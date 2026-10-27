// ============================================================================
//  ESP 1 — SCANNER : écoute en continu les balises Remote ID et les envoie à l'ESP 2 par liaison série.
//  Seeed XIAO ESP32-C6
//
//  Sources décodées :
//    - Bluetooth 4 (advertising) et Bluetooth 5 (étendu, PHY 1M + Coded) : Open Drone ID
//    - Wi-Fi : trames « beacon » Open Drone ID (élément vendeur FA:0B:BC / 0x0D)
//    - Wi-Fi : trames NAN « service discovery » Open Drone ID (canal 6)
//    - Wi-Fi : signalement électronique français (beacon, élément vendeur 6A:5C:35 / 0x01)
//  En plus des drones, relève les réseaux Wi-Fi et les appareils Bluetooth captés (priorité toujours aux drones).
//  Réception passive uniquement : la carte n'émet rien sur la radio.
//  Le Wi-Fi reste sur le canal 6 (balises Remote ID) la moitié du temps ; les autres canaux sont visités à tour de rôle.
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
#include <Update.h>

#include "opendroneid.h"
#include "config.h"
#include "proto.h"
#include "frid.h"

enum Kind : uint8_t { K_BLE = 1, K_BEACON = 2, K_NAN = 3, K_FR = 4 };
static uint8_t srcBit(uint8_t kind) { return kind == K_BLE ? 1 : (kind == K_BEACON ? 2 : (kind == K_NAN ? 4 : 8)); }

static Drone drones[MAX_DRONES];
static SemaphoreHandle_t dMutex;
static volatile uint32_t statBle = 0, statWifi = 0, statDropped = 0;
static Net nets[MAX_NETS];
static Bt bts[MAX_BT];
static Sens sens[MAX_SENS];
static ScanCmd cur;                 // réglages courants, commandés par l'ESP 2
static volatile bool surveyWifi = true, surveyBt = true;
static volatile int rssiMin = DEFAULT_RSSI_MIN;
static volatile uint8_t fixedChan = 0;
static volatile int bleDuty = DEFAULT_BLE_DUTY;
static uint8_t scanMode = DEFAULT_SCAN_MODE;   // 0 BLE+Wi-Fi, 1 BLE, 2 Wi-Fi ; commandé par l'ESP 2
static volatile uint32_t statNetDrop = 0;
static uint32_t sensSeenAt[256];   // limitation de débit des capteurs (200 ms par adresse)
static uint32_t seenAt[256];        // limitation de débit des relevés, par hachage de l'adresse (callbacks radio)

static Drone* getDrone(const uint8_t* mac) {   // dMutex pris par l'appelant
  Drone* freeSlot = nullptr;
  Drone* oldest = nullptr;
  for (int i = 0; i < MAX_DRONES; i++) {
    Drone& d = drones[i];
    if (!d.used) { if (!freeSlot) freeSlot = &d; continue; }
    if (memcmp(d.mac, mac, 6) == 0) return &d;
    if (!oldest || d.lastMs < oldest->lastMs) oldest = &d;
  }
  Drone* slot = freeSlot ? freeSlot : oldest;
  if (!slot) return nullptr;
  *slot = Drone();
  slot->used = true;
  memcpy(slot->mac, mac, 6);
  slot->firstMs = millis();
  return slot;
}

static void idToStr(const ODID_BasicID_data& b, char* out, size_t outSize) {
  if (b.IDType == ODID_IDTYPE_UTM_ASSIGNED_UUID) {   // 16 octets binaires -> hexadécimal
    size_t n = 0;
    for (int i = 0; i < 16 && n + 2 < outSize; i++) n += snprintf(out + n, outSize - n, "%02X", (uint8_t)b.UASID[i]);
    out[n] = 0;
    return;
  }
  size_t n = 0;
  for (int i = 0; i < ODID_ID_SIZE && b.UASID[i] && n + 1 < outSize; i++) {
    uint8_t c = (uint8_t)b.UASID[i];
    out[n++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
  }
  out[n] = 0;
}

static void copyPrintable(char* dst, size_t dstSize, const char* src, size_t srcMax) {
  size_t n = 0;
  for (size_t i = 0; i < srcMax && src[i] && n + 1 < dstSize; i++) {
    uint8_t c = (uint8_t)src[i];
    dst[n++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
  }
  dst[n] = 0;
}

static void applyUas(Drone& d, const ODID_UAS_Data& u) {
  for (int i = 0; i < ODID_BASIC_ID_MAX_MESSAGES; i++) {
    if (!u.BasicIDValid[i]) continue;
    const ODID_BasicID_data& b = u.BasicID[i];
    if (b.IDType == ODID_IDTYPE_CAA_REGISTRATION_ID) {
      idToStr(b, d.reg, sizeof d.reg);
      d.hasReg = true;
    } else {
      idToStr(b, d.id, sizeof d.id);
      d.idType = (uint8_t)b.IDType;
      d.uaType = (uint8_t)b.UAType;
      d.hasId = true;
    }
  }
  if (u.LocationValid) {
    const ODID_Location_data& L = u.Location;
    d.hasLoc = true;
    d.status = (uint8_t)L.Status;
    d.odTs = (L.TimeStamp >= 0.0f && L.TimeStamp <= (float)MAX_TIMESTAMP) ? L.TimeStamp : NAN;
    if (L.Latitude != 0.0 || L.Longitude != 0.0) {
      d.hasPos = true;
      d.lat = L.Latitude;
      d.lon = L.Longitude;
    }
    d.dir = (L.Direction <= 360.0f) ? L.Direction : NAN;
    d.spdH = (L.SpeedHorizontal < 254.5f) ? L.SpeedHorizontal : NAN;
    d.spdV = (L.SpeedVertical < 62.5f) ? L.SpeedVertical : NAN;
    d.altBaro = (L.AltitudeBaro > -999.0f) ? L.AltitudeBaro : NAN;
    d.altGeo = (L.AltitudeGeo > -999.0f) ? L.AltitudeGeo : NAN;
    d.height = (L.Height > -999.0f) ? L.Height : NAN;
  }
  if (u.SystemValid) {
    if (u.System.OperatorLatitude != 0.0 || u.System.OperatorLongitude != 0.0) {
      d.hasOp = true;
      d.opLat = u.System.OperatorLatitude;
      d.opLon = u.System.OperatorLongitude;
    }
  }
  if (u.SelfIDValid) copyPrintable(d.desc, sizeof d.desc, u.SelfID.Desc, ODID_STR_SIZE);
  if (u.OperatorIDValid) copyPrintable(d.opId, sizeof d.opId, u.OperatorID.OperatorId, ODID_ID_SIZE);
}

static void applyFr(Drone& d, const FrData& f) {
  // identifiant français (30 caractères) et/ou identifiant ANSI/CTA-2063 : on garde les deux s'ils sont reçus
  if (f.hasId) {
    copyPrintable(d.id, sizeof d.id, f.id, 30);
    d.hasId = true;
  }
  if (f.hasAnsi) {
    copyPrintable(d.ansi, sizeof d.ansi, f.ansi, 47);
    if (!f.hasId) {   // sans identifiant français, l'identifiant ANSI sert d'identifiant principal
      copyPrintable(d.id, sizeof d.id, f.ansi, 40);
      d.hasId = true;
    }
  }
  if (f.hasPos && (f.lat != 0.0 || f.lon != 0.0)) {
    d.hasPos = true;
    d.lat = f.lat;
    d.lon = f.lon;
  }
  if (f.hasAlt) d.altGeo = (float)f.altitude;
  if (f.hasHeight) d.height = (float)f.height;
  if (f.hasSpeed) d.spdH = (float)f.speed;
  if (f.hasCourse && f.course >= 0 && f.course <= 360) d.dir = (float)f.course;
  if (f.hasTakeoff && (f.toLat != 0.0 || f.toLon != 0.0)) {   // point de décollage = position de l'opérateur
    d.hasOp = true;
    d.opLat = f.toLat;
    d.opLon = f.toLon;
  }
}

// ----------------------------------------------------------------------------
//  Réception : file d'attente entre les callbacks radio et le décodage
// ----------------------------------------------------------------------------
struct Item {
  uint8_t kind;
  int8_t rssi;
  uint16_t len;
  uint8_t mac[6];
  uint8_t data[300];
};
static QueueHandle_t rxQueue;

static void enqueue(uint8_t kind, const uint8_t* mac, int rssi, const uint8_t* data, size_t len) {
  if (len == 0 || len > sizeof(((Item*)0)->data)) return;
  Item it;
  it.kind = kind;
  it.rssi = (int8_t)rssi;
  it.len = (uint16_t)len;
  memcpy(it.mac, mac, 6);
  memcpy(it.data, data, len);
  if (xQueueSend(rxQueue, &it, 0) != pdTRUE) statDropped++;
}

static bool packDecode(ODID_UAS_Data* u, const uint8_t* b, size_t n) {
  if (n < 3 || (b[0] >> 4) != ODID_MESSAGETYPE_PACKED) return false;
  uint8_t size = b[1], count = b[2];
  if (size != ODID_MESSAGE_SIZE || count == 0 || count > ODID_PACK_MAX_MESSAGES) return false;
  if (n < 3u + (size_t)ODID_MESSAGE_SIZE * count) return false;
  for (int i = 0; i < count; i++) decodeOpenDroneID(u, b + 3 + ODID_MESSAGE_SIZE * i);
  return true;
}

// Trame NAN « service discovery » complète (en-tête 802.11 inclus)
static bool parseNan(const uint8_t* f, size_t len, ODID_UAS_Data* u) {
  static const uint8_t serviceId[6] = {0x88, 0x69, 0x19, 0x9D, 0x92, 0x09};
  if (len < 24 + 6 + 13 + 4) return false;
  const uint8_t* nsd = f + 24;   // catégorie 4, action 9, OUI 50:6F:9A, type 0x13
  if (nsd[0] != 0x04 || nsd[1] != 0x09 || nsd[2] != 0x50 || nsd[3] != 0x6F || nsd[4] != 0x9A || nsd[5] != 0x13)
    return false;
  const uint8_t* a = nsd + 6;    // attribut « service descriptor »
  if (a[0] != 0x03 || memcmp(a + 3, serviceId, 6) != 0) return false;
  size_t infoLen = a[12];
  const uint8_t* info = a + 13;
  if (infoLen < 1 + 3 || info + infoLen > f + len) return false;
  return packDecode(u, info + 1, infoLen - 1);   // info[0] = compteur
}

// ----------------------------------------------------------------------------
//  Relevé des réseaux Wi-Fi et appareils Bluetooth (hors drones) : basse priorité
// ----------------------------------------------------------------------------
struct Obs {
  uint8_t type;   // 'w' réseau Wi-Fi, 'b' appareil Bluetooth
  uint8_t mac[6];
  int8_t rssi;
  uint8_t ch, enc, atype;
  int32_t mf;
  char name[33];
  uint8_t tag;       // traceur / balise reconnu (btTagText)
  uint16_t sduuid;   // capteur : UUID de service (0xFCD2 BTHome, 0xFE95 Xiaomi) et données brutes
  uint8_t sdl;
  uint8_t sd[28];
};
static QueueHandle_t obsQueue;

// true si cette adresse peut être relevée maintenant (au plus une fois toutes les 1,5 s)
static bool surveyAllowed(const uint8_t* mac, uint32_t now) {
  uint8_t i = (uint8_t)(mac[3] * 31 + mac[4] * 7 + mac[5]);
  if (now - seenAt[i] < 1500) return false;
  seenAt[i] = now;
  return true;
}

static void enqueueObs(const Obs& o) {
  if (xQueueSend(obsQueue, &o, 0) != pdTRUE) statNetDrop++;
}

static void obsTask(void*) {
  Obs o;
  for (;;) {
    if (xQueueReceive(obsQueue, &o, portMAX_DELAY) != pdTRUE) continue;
    uint32_t now = millis();
    xSemaphoreTake(dMutex, portMAX_DELAY);
    if (o.type == 's') {
      Sens t;
      if (sensDecode(o.sduuid, o.sd, o.sdl, t)) {
        Sens* slot = nullptr;
        Sens* oldest = nullptr;
        for (auto& x : sens) {
          if (!x.used) { if (!slot) slot = &x; continue; }
          if (memcmp(x.mac, o.mac, 6) == 0) { slot = &x; oldest = nullptr; break; }
          if (!oldest || x.lastMs < oldest->lastMs) oldest = &x;
        }
        if (!slot) slot = oldest;
        if (slot) {
          if (!slot->used || memcmp(slot->mac, o.mac, 6) != 0) {
            *slot = Sens();
            slot->used = true;
            memcpy(slot->mac, o.mac, 6);
            slot->firstMs = now;
          }
          sensMerge(*slot, t);
          if (o.name[0]) strlcpy(slot->name, o.name, sizeof slot->name);
          slot->rssi = o.rssi;
          slot->frames++;
          slot->lastMs = now;
          slot->dirty = true;
        }
      }
    } else if (o.type == 'w') {
      Net* slot = nullptr;
      Net* oldest = nullptr;
      for (auto& n : nets) {
        if (!n.used) { if (!slot) slot = &n; continue; }
        if (memcmp(n.bssid, o.mac, 6) == 0) { slot = &n; oldest = nullptr; break; }
        if (!oldest || n.lastMs < oldest->lastMs) oldest = &n;
      }
      if (!slot) slot = oldest;
      if (slot) {
        if (!slot->used || memcmp(slot->bssid, o.mac, 6) != 0) {
          *slot = Net();
          slot->used = true;
          memcpy(slot->bssid, o.mac, 6);
          slot->firstMs = now;
        }
        strlcpy(slot->ssid, o.name, sizeof slot->ssid);
        slot->ch = o.ch;
        slot->enc = o.enc;
        slot->rssi = o.rssi;
        slot->frames++;
        slot->lastMs = now;
        slot->dirty = true;
      }
    } else {
      Bt* slot = nullptr;
      Bt* oldest = nullptr;
      for (auto& d : bts) {
        if (!d.used) { if (!slot) slot = &d; continue; }
        if (memcmp(d.mac, o.mac, 6) == 0) { slot = &d; oldest = nullptr; break; }
        if (!oldest || d.lastMs < oldest->lastMs) oldest = &d;
      }
      if (!slot) slot = oldest;
      if (slot) {
        if (!slot->used || memcmp(slot->mac, o.mac, 6) != 0) {
          *slot = Bt();
          slot->used = true;
          memcpy(slot->mac, o.mac, 6);
          slot->firstMs = now;
        }
        if (o.name[0]) strlcpy(slot->name, o.name, sizeof slot->name);
        if (o.mf >= 0) slot->mf = o.mf;
        if (o.tag) slot->tag = o.tag;
        slot->atype = o.atype;
        slot->rssi = o.rssi;
        slot->frames++;
        slot->lastMs = now;
        slot->dirty = true;
      }
    }
    xSemaphoreGive(dMutex);
  }
}

static void ridTask(void*) {
  static ODID_UAS_Data uas;
  static FrData fr;
  Item it;
  for (;;) {
    if (xQueueReceive(rxQueue, &it, portMAX_DELAY) != pdTRUE) continue;

    bool ok = false;
    if (it.kind == K_FR) {
      ok = parseFr(it.data, it.len, &fr);
    } else {
      odid_initUasData(&uas);
      if (it.kind == K_NAN) {
        ok = parseNan(it.data, it.len, &uas);
      } else if ((it.data[0] >> 4) == ODID_MESSAGETYPE_PACKED) {
        ok = packDecode(&uas, it.data, it.len);
      } else if (it.len >= ODID_MESSAGE_SIZE && (it.data[0] >> 4) <= ODID_MESSAGETYPE_OPERATOR_ID) {
        ok = decodeOpenDroneID(&uas, it.data) != ODID_MESSAGETYPE_INVALID;
      }
    }
    if (!ok) continue;

    xSemaphoreTake(dMutex, portMAX_DELAY);
    Drone* d = getDrone(it.mac);
    if (d) {
      if (it.kind == K_FR) applyFr(*d, fr); else applyUas(*d, uas);
      d->src |= srcBit(it.kind);
      d->rssi = it.rssi;
      d->lastMs = millis();
      d->frames++;
      d->dirty = true;
    }
    xSemaphoreGive(dMutex);
  }
}

// ---- Bluetooth ----
class RidScanCallbacks : public NimBLEScanCallbacks {
 public:
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    const std::vector<uint8_t>& p = dev->getPayload();
    const uint8_t* v = dev->getAddress().getVal();   // octet de poids faible en premier
    uint8_t mac[6];
    for (int k = 0; k < 6; k++) mac[k] = v[5 - k];
    bool isDrone = false;
    uint8_t tag = 0;
    uint16_t sduuid = 0;
    uint8_t sdl = 0, sd[28];
    char name[33] = "";
    int32_t mf = -1;
    size_t i = 0, n = p.size();
    while (i + 1 < n) {
      uint8_t l = p[i];
      if (l == 0 || i + 1 + l > n) break;
      uint8_t t = p[i + 1];
      if (!tag) tag = btTagFromAd(t, &p[i + 2], (size_t)l - 1);
      // Service Data (0x16), UUID 0xFFFA, code d'application 0x0D, compteur, message(s)
      if (t == 0x16 && l >= 6 && p[i + 2] == 0xFA && p[i + 3] == 0xFF && p[i + 4] == 0x0D) {
        enqueue(K_BLE, mac, dev->getRSSI(), &p[i + 6], l - 5);
        statBle++;
        isDrone = true;
      } else if (t == 0x16 && l >= 5 && ((p[i + 2] == 0xD2 && p[i + 3] == 0xFC) || (p[i + 2] == 0x95 && p[i + 3] == 0xFE) || (p[i + 2] == 0x1A && p[i + 3] == 0x18))) {
        // capteur : BTHome (UUID 0xFCD2), MiBeacon Xiaomi (0xFE95) ou firmware ATC/pvvx (0x181A)
        sduuid = (uint16_t)(p[i + 2] | (p[i + 3] << 8));
        sdl = (uint8_t)((l - 3) > sizeof sd ? sizeof sd : (l - 3));
        memcpy(sd, &p[i + 4], sdl);
      } else if (t == 0x16 && l >= 17 && p[i + 2] == 0xAA && p[i + 3] == 0xFE && p[i + 4] == 0x20) {
        // Eddystone TLM : tension de pile et température de la balise
        sduuid = 0xFEAA;
        sdl = (uint8_t)((l - 3) > sizeof sd ? sizeof sd : (l - 3));
        memcpy(sd, &p[i + 4], sdl);
      } else if ((t == 0x09 || (t == 0x08 && !name[0])) && l >= 2) {   // nom complet / abrégé
        size_t k = 0;
        for (; k + 1 < (size_t)l && k + 1 < sizeof name; k++) {
          uint8_t c = p[i + 2 + k];
          name[k] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
        }
        name[k] = 0;
      } else if (t == 0xFF && l >= 3) {   // données constructeur : identifiant sur 2 octets
        mf = (int32_t)(p[i + 2] | (p[i + 3] << 8));
        if (mf == 0x0499 && l >= 27 && p[i + 4] == 5 && !sduuid) {   // Ruuvi, format 5 (24 octets)
          sduuid = 0x0499;
          sdl = 24;
          memcpy(sd, &p[i + 4], sdl);
        }
      }
      i += l + 1;
    }
    if (sduuid) {
      uint32_t now = millis();
      uint8_t h = (uint8_t)(mac[3] * 31 + mac[4] * 7 + mac[5]);
      if (now - sensSeenAt[h] >= 200) {
        sensSeenAt[h] = now;
        Obs so;
        memset(&so, 0, sizeof so);
        so.type = 's';
        memcpy(so.mac, mac, 6);
        so.rssi = (int8_t)dev->getRSSI();
        so.sduuid = sduuid;
        so.sdl = sdl;
        memcpy(so.sd, sd, sdl);
        strlcpy(so.name, name, sizeof so.name);
        enqueueObs(so);
      }
    }
    if (isDrone || !surveyBt) return;
    int rssi = dev->getRSSI();
    if (rssi < rssiMin || !surveyAllowed(mac, millis())) return;
    Obs o;
    memset(&o, 0, sizeof o);
    o.type = 'b';
    memcpy(o.mac, mac, 6);
    o.rssi = (int8_t)rssi;
    o.atype = dev->getAddress().getType() ? 1 : 0;
    o.mf = mf;
    o.tag = tag;
    strlcpy(o.name, name, sizeof o.name);
    enqueueObs(o);
  }
};
static RidScanCallbacks bleCallbacks;
static bool bleScanning = false, wifiSniffing = false;

static void bleStart() {
  if (bleScanning) return;
  NimBLEScan* s = NimBLEDevice::getScan();
  s->setScanCallbacks(&bleCallbacks, true);   // true : recevoir aussi les répétitions
  s->setActiveScan(false);                    // écoute passive
  s->setDuplicateFilter(0);
  s->setMaxResults(0);                        // ne pas conserver les résultats en mémoire
  s->setInterval(100);
  // Bluetooth seul : écoute quasi continue. Bluetooth + Wi-Fi : le Bluetooth ne garde que bleDuty % du temps,
  // sinon il occupe l'antenne et le Wi-Fi ne reçoit presque rien.
  int duty = (scanMode == 0) ? bleDuty : 99;
  s->setWindow(constrain(duty, 10, 99));
#if MYNEWT_VAL(BLE_EXT_ADV)
  s->setPhy(NimBLEScan::SCAN_ALL);            // 1M + Coded (Bluetooth 5 longue portée)
#endif
  bleScanning = s->start(0);                  // 0 = jusqu'à l'arrêt
  Serial.printf("BLE : scan %s\n", bleScanning ? "démarré" : "ÉCHEC");
}

static void bleStop() {
  if (bleScanning) NimBLEDevice::getScan()->stop();
  bleScanning = false;
}

// ---- Wi-Fi (mode promiscuous) ----
static void promiscCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* f = pkt->payload;
  int len = (int)pkt->rx_ctrl.sig_len - 4;   // sig_len inclut le FCS
  if (len < 36) return;
  const uint8_t* sa = f + 10;

  if (f[0] == 0x80 || f[0] == 0x50) {   // beacon ou réponse de sonde : parcourt les éléments
    const uint8_t* ie = f + 36;
    const uint8_t* end = f + len;
    char ssid[33] = "";
    uint8_t ch = pkt->rx_ctrl.channel, enc = (f[34] & 0x10) ? 1 : 0;
    while (ie + 2 <= end) {
      uint8_t t = ie[0], l = ie[1];
      if (ie + 2 + l > end) break;
      if (t == 221) {
        if (f[0] == 0x80 && l >= 5 + 1 + 3 && ie[2] == 0xFA && ie[3] == 0x0B && ie[4] == 0xBC && ie[5] == 0x0D) {
          // Open Drone ID : ie[6] = compteur, puis le paquet de messages
          enqueue(K_BEACON, sa, pkt->rx_ctrl.rssi, ie + 7, l - 5);
          statWifi++;
          return;
        }
        if (f[0] == 0x80 && l >= 6 && ie[2] == 0x6A && ie[3] == 0x5C && ie[4] == 0x35 && ie[5] == 0x01) {
          // signalement électronique français : champs TLV après le type
          enqueue(K_FR, sa, pkt->rx_ctrl.rssi, ie + 6, l - 4);
          statWifi++;
          return;
        }
        if (l >= 8 && ie[2] == 0x00 && ie[3] == 0x50 && ie[4] == 0xF2 && ie[5] == 0x01 && enc < 2) enc = 2;   // WPA
      } else if (t == 0 && l <= 32) {   // SSID
        size_t k = 0;
        for (; k < l; k++) {
          uint8_t c = ie[2 + k];
          ssid[k] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
        }
        ssid[k] = 0;
      } else if (t == 3 && l == 1) {    // canal
        ch = ie[2];
      } else if (t == 48) {             // RSN : WPA2 / WPA3
        enc = 3;
      }
      ie += 2 + l;
    }
    if (surveyWifi && pkt->rx_ctrl.rssi >= rssiMin && surveyAllowed(f + 16, millis())) {
      Obs o;
      memset(&o, 0, sizeof o);
      o.type = 'w';
      memcpy(o.mac, f + 16, 6);   // BSSID
      o.rssi = (int8_t)pkt->rx_ctrl.rssi;
      o.ch = ch;
      o.enc = enc;
      strlcpy(o.name, ssid, sizeof o.name);
      enqueueObs(o);
    }
  } else if (f[0] == 0xD0) {   // action : NAN, destination 51:6F:9A:01:00:00
    static const uint8_t nanDa[6] = {0x51, 0x6F, 0x9A, 0x01, 0x00, 0x00};
    if (memcmp(f + 4, nanDa, 6) == 0 && f[24] == 0x04 && f[25] == 0x09) {
      enqueue(K_NAN, sa, pkt->rx_ctrl.rssi, f, (size_t)len);
      statWifi++;
    }
  }
}

static const uint8_t HOP_OTHERS[] = {1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13};
static uint8_t hopCur = 6, hopIdx = 0;
static uint32_t lastHop = 0;

static void wifiSniffStart() {
  if (wifiSniffing) return;
  WiFi.mode(WIFI_STA);
  wifi_country_t c;
  memset(&c, 0, sizeof c);
  memcpy(c.cc, "FR", 2);
  c.schan = 1;
  c.nchan = 13;
  c.policy = WIFI_COUNTRY_POLICY_MANUAL;
  esp_wifi_set_country(&c);
  wifi_promiscuous_filter_t flt;
  flt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&flt);
  esp_wifi_set_promiscuous_rx_cb(&promiscCb);
  esp_wifi_set_promiscuous(true);
  hopCur = 6;
  esp_wifi_set_channel(hopCur, WIFI_SECOND_CHAN_NONE);
  lastHop = millis();
  wifiSniffing = true;
  Serial.println("Wi-Fi : écoute démarrée");
}

static void wifiSniffStop() {
  if (!wifiSniffing) return;
  esp_wifi_set_promiscuous(false);
  wifiSniffing = false;
}

// Le canal 6 est visité une fois sur deux ; les autres canaux à tour de rôle.
static void wifiHop(uint32_t now) {
  if (!wifiSniffing) return;
  if (fixedChan) {   // canal fixe demandé depuis la page web
    if (hopCur != fixedChan) {
      hopCur = fixedChan;
      esp_wifi_set_channel(hopCur, WIFI_SECOND_CHAN_NONE);
    }
    return;
  }
  uint32_t dwell = (hopCur == 6) ? 400 : 150;
  if (now - lastHop < dwell) return;
  lastHop = now;
  if (hopCur == 6) {
    hopCur = HOP_OTHERS[hopIdx];
    hopIdx = (hopIdx + 1) % sizeof HOP_OTHERS;
  } else {
    hopCur = 6;
  }
  esp_wifi_set_channel(hopCur, WIFI_SECOND_CHAN_NONE);
}

static void applyMode(uint8_t m) {
  if (m > 2) return;
  cur.mode = m;
  scanMode = m;
  bleStop();   // redémarré avec la fenêtre d'écoute adaptée au mode
  if (m != 2) bleStart();
  if (m == 1) wifiSniffStop(); else wifiSniffStart();
  Serial.printf("Mode d'écoute : %u (0 = BLE+Wi-Fi, 1 = BLE, 2 = Wi-Fi)\n", m);
}

static void setAntenna(int ext) {
#if XIAO_RF_SWITCH
  pinMode(14, OUTPUT);
  digitalWrite(14, ext ? HIGH : LOW);
#endif
}

static void applyCmd(const ScanCmd& c) {
  if (c.mode != cur.mode) applyMode((uint8_t)c.mode);
  if (c.chan != cur.chan) {
    fixedChan = (c.chan >= 1 && c.chan <= 13) ? (uint8_t)c.chan : 0;
    Serial.printf("Canal Wi-Fi : %s\n", fixedChan ? String(fixedChan).c_str() : "saut automatique");
  }
  if (c.ant != cur.ant) {
    setAntenna(c.ant);
    Serial.printf("Antenne : %s\n", c.ant ? "externe (U.FL)" : "céramique intégrée");
  }
  int newDuty = constrain(c.bleDuty, 10, 100);
  if (newDuty != bleDuty) {
    bleDuty = newDuty;
    if (bleScanning && scanMode == 0) { bleStop(); bleStart(); }   // la nouvelle fenêtre d'écoute s'applique
    Serial.printf("Bluetooth : %d %% du temps d'antenne quand le Wi-Fi écoute aussi\n", newDuty);
  }
  surveyWifi = c.wifiSurvey;
  surveyBt = c.btSurvey;
  rssiMin = constrain(c.rssiMin, -120, -20);
  cur = c;
}

// ----------------------------------------------------------------------------
//  Liaison vers l'ESP 2
// ----------------------------------------------------------------------------
static uint32_t txSkipped = 0, rxBad = 0, lastHb = 0;
static LineReader cmdReader;

static void linkSend(const char* line, size_t len) {
  if (!len) return;
  if (Serial1.availableForWrite() < (int)len) { txSkipped++; return; }   // ne jamais bloquer la réception
  Serial1.write((const uint8_t*)line, len);
}

static void sendDrones(uint32_t now) {
  static char buf[940];
  static int cursor = 0;
  // un seul passage sur la table par appel, pour garder la boucle réactive
  for (int k = 0; k < MAX_DRONES; k++) {
    int i = (cursor + k) % MAX_DRONES;
    size_t n = 0;
    xSemaphoreTake(dMutex, portMAX_DELAY);
    Drone& d = drones[i];
    if (d.used && d.dirty && now - d.sentMs >= SEND_MIN_MS) {
      n = protoDrone(d, buf, sizeof buf);
      d.dirty = false;
      d.sentMs = now;
    }
    xSemaphoreGive(dMutex);
    if (n) linkSend(buf, n);
  }
  cursor = (cursor + 1) % MAX_DRONES;
}

// Réseaux Wi-Fi et appareils Bluetooth : basse priorité, seulement si la liaison a de la place
static void sendSurvey(uint32_t now) {
  static char buf[300];
  static int cn = 0, cb = 0;
  if (Serial1.availableForWrite() < 2000) return;
  int budget = 3;   // lignes par passage
  for (int k = 0; k < MAX_NETS && budget > 0; k++) {
    int i = (cn + k) % MAX_NETS;
    size_t n = 0;
    xSemaphoreTake(dMutex, portMAX_DELAY);
    Net& x = nets[i];
    if (x.used && x.dirty && now - x.sentMs >= NET_SEND_MS) {
      n = protoNet(x, buf, sizeof buf);
      x.dirty = false;
      x.sentMs = now;
    }
    xSemaphoreGive(dMutex);
    if (n) { linkSend(buf, n); budget--; cn = (i + 1) % MAX_NETS; }
  }
  for (int k = 0; k < MAX_BT && budget > 0; k++) {
    int i = (cb + k) % MAX_BT;
    size_t n = 0;
    xSemaphoreTake(dMutex, portMAX_DELAY);
    Bt& x = bts[i];
    if (x.used && x.dirty && now - x.sentMs >= NET_SEND_MS) {
      n = protoBt(x, buf, sizeof buf);
      x.dirty = false;
      x.sentMs = now;
    }
    xSemaphoreGive(dMutex);
    if (n) { linkSend(buf, n); budget--; cb = (i + 1) % MAX_BT; }
  }
}

// capteurs Bluetooth : valeurs fusionnées, renvoyées à l'ESP 2 au plus une fois par seconde et par capteur
static void sendSensors(uint32_t now) {
  static char buf[400];
  static int cs = 0;
  if (Serial1.availableForWrite() < 1500) return;
  for (int k = 0; k < MAX_SENS; k++) {
    int i = (cs + k) % MAX_SENS;
    size_t n = 0;
    xSemaphoreTake(dMutex, portMAX_DELAY);
    Sens& x = sens[i];
    if (x.used && x.dirty && now - x.sentMs >= SENS_SEND_MS) {
      n = protoSens(x, buf, sizeof buf);
      x.dirty = false;
      x.sentMs = now;
    }
    xSemaphoreGive(dMutex);
    if (n) { linkSend(buf, n); cs = (i + 1) % MAX_SENS; return; }
  }
}

static Heartbeat lastHbData;

static void sendHeartbeat(uint32_t now) {
  Heartbeat h;
  h.up = now / 1000;
  h.ble = statBle;
  h.wifi = statWifi;
  h.drop = statDropped + txSkipped + statNetDrop;
  h.heap = ESP.getFreeHeap();
  h.ch = wifiSniffing ? hopCur : 0;
  h.mode = scanMode;
  h.bleOn = bleScanning;
  h.wifiOn = wifiSniffing;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& d : drones) h.tracked += d.used;
  for (auto& x : nets) h.nets += x.used;
  for (auto& x : bts) h.bts += x.used;
  xSemaphoreGive(dMutex);
  strlcpy(h.fw, FW_VERSION " " __DATE__ " " __TIME__, sizeof h.fw);
  lastHbData = h;
  char buf[360];
  linkSend(buf, protoHeartbeat(h, buf, sizeof buf));
}

// ----------------------------------------------------------------------------
//  Mise à jour OTA reçue de l'ESP 2 (arrêt de l'écoute pendant le transfert)
// ----------------------------------------------------------------------------
static bool otaActive = false;
static uint32_t otaExpect = 0, otaLastMs = 0, otaSize = 0;

static void otaReply(const char* line, size_t n) {
  if (Serial1.availableForWrite() >= (int)n) Serial1.write((const uint8_t*)line, n);
}
static void otaResult(int code) {
  char b[40];
  otaReply(b, otaLineResult(code, b, sizeof b));
  Serial1.flush();
}
static void otaAbort(int code) {
  if (otaActive) Update.abort();
  otaActive = false;
  otaResult(code);
  applyMode(scanMode);   // reprise de l'écoute
}

static void otaHandle(const OtaMsg& m) {
  char b[40];
  otaLastMs = millis();
  if (m.t == 'b') {
    if (otaActive && otaExpect == 0 && m.a == otaSize) {   // début répété (accusé tardif) : on ne recommence pas l'effacement
      otaReply(b, otaLineAck(0xFFFFFFFFu, b, sizeof b));
      return;
    }
    if (otaActive) Update.abort();
    bleStop();
    wifiSniffStop();
    otaActive = false;
    if (!Update.begin(m.a)) {   // m.a = taille du firmware
      otaResult(1);
      applyMode(scanMode);
      return;
    }
    otaActive = true;
    otaExpect = 0;
    otaSize = m.a;
    otaLastMs = millis();   // l'effacement de la partition peut durer plusieurs secondes
    Serial.printf("OTA : réception de %lu octets\n", (unsigned long)m.a);
    otaReply(b, otaLineAck(0xFFFFFFFFu, b, sizeof b));   // accusé du début
  } else if (m.t == 'd') {
    if (!otaActive) return otaResult(4);
    if (otaExpect > 0 && m.a == otaExpect - 1) {   // morceau répété (accusé perdu) : on ré-accuse sans réécrire
      otaReply(b, otaLineAck(m.a, b, sizeof b));
      return;
    }
    if (m.a != otaExpect) return otaAbort(4);
    static uint8_t raw[OTA_CHUNK + 4];
    int n = b64dec(m.b64, m.b64n, raw, sizeof raw);
    if (n <= 0 || Update.write(raw, (size_t)n) != (size_t)n) return otaAbort(2);
    otaExpect++;
    otaReply(b, otaLineAck(m.a, b, sizeof b));
  } else if (m.t == 'e') {
    if (!otaActive) return otaResult(4);
    if (!Update.end(true)) return otaAbort(3);
    otaActive = false;
    otaResult(0);
    Serial.println("OTA : terminé, redémarrage");
    delay(400);
    ESP.restart();
  }
}

static void readCommands() {
  while (Serial1.available()) {
    const char* line;
    size_t len;
    if (!cmdReader.feed((uint8_t)Serial1.read(), &line, &len)) continue;
    if (len > 0 && line[0] == '~') {   // ligne de mise à jour OTA
      static OtaMsg om;
      if (otaParse(line, len, &om) != 0) { rxBad++; continue; }
      otaHandle(om);
      continue;
    }
    static LinkMsg m;
    if (protoParse(line, len, &m) != 0) { rxBad++; continue; }
    if (m.kind == 'c' && m.cmd.mode >= 0 && m.cmd.mode <= 2) applyCmd(m.cmd);
  }
}

static void expireDrones(uint32_t now) {
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& d : drones)
    if (d.used && now - d.lastMs > 180000UL) d.used = false;   // 3 min sans réception : on oublie
  for (auto& x : nets)
    if (x.used && now - x.lastMs > 180000UL) x.used = false;
  for (auto& x : bts)
    if (x.used && now - x.lastMs > 180000UL) x.used = false;
  for (auto& x : sens)
    if (x.used && now - x.lastMs > 86400000UL) x.used = false;   // un capteur peut se taire longtemps : 24 h
  xSemaphoreGive(dMutex);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n== ESP 1 — SCANNER Remote ID ==");

#if XIAO_RF_SWITCH
  // XIAO ESP32-C6 : GPIO3 à LOW active le commutateur RF, GPIO14 choisit l'antenne
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);
  delay(100);
  pinMode(14, OUTPUT);
  digitalWrite(14, ANTENNA_EXTERNAL ? HIGH : LOW);
  Serial.printf("Antenne : %s\n", ANTENNA_EXTERNAL ? "externe (U.FL)" : "céramique intégrée");
#endif

  Serial1.setTxBufferSize(4096);
  Serial1.setRxBufferSize(2048);
  Serial1.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);

  dMutex = xSemaphoreCreateMutex();
  rxQueue = xQueueCreate(16, sizeof(Item));
  obsQueue = xQueueCreate(24, sizeof(Obs));
  xTaskCreate(ridTask, "rid", 6144, nullptr, 2, nullptr);
  xTaskCreate(obsTask, "obs", 3072, nullptr, 1, nullptr);   // priorité plus basse que les drones
  cur.mode = DEFAULT_SCAN_MODE;
  cur.ant = ANTENNA_EXTERNAL;
  cur.rssiMin = DEFAULT_RSSI_MIN;
  cur.bleDuty = DEFAULT_BLE_DUTY;
  rssiMin = DEFAULT_RSSI_MIN;

  NimBLEDevice::init("");
  applyMode(scanMode);
  Serial.printf("RAM libre : %u octets\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
  uint32_t now = millis();
  wifiHop(now);
  if (!otaActive) {
    sendDrones(now);
    sendSurvey(now);
    sendSensors(now);
  }
  readCommands();
  if (otaActive && millis() - otaLastMs > 20000) {   // l'ESP 2 ne répond plus : on abandonne et on reprend l'écoute
    Update.abort();
    otaActive = false;
    applyMode(scanMode);
    Serial.println("OTA : abandon (plus de données reçues)");
  }
  if (now - lastHb >= 2000) {
    lastHb = now;
    sendHeartbeat(now);
    expireDrones(now);
    static uint32_t lastLog = 0;
    if (now - lastLog >= 10000) {
      lastLog = now;
      Serial.printf("[%lus] BLE %lu  Wi-Fi %lu  perdus %lu  canal %u  réseaux %lu  appareils BT %lu  RAM %u  CRC-liaison-faux %lu\n",
                    (unsigned long)(now / 1000), (unsigned long)statBle, (unsigned long)statWifi,
                    (unsigned long)(statDropped + txSkipped + statNetDrop), hopCur, (unsigned long)lastHbData.nets, (unsigned long)lastHbData.bts,
                    (unsigned)ESP.getFreeHeap(), (unsigned long)rxBad);
    }
  }
  delay(5);
}
