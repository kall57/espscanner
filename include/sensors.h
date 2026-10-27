#pragma once
// ============================================================================
//  Capteurs Bluetooth lus dans leurs annonces (sans connexion) :
//    - BTHome v2 (Shelly BLU Door/Window, H&T, Motion, boutons…), en clair
//    - MiBeacon de la Xiaomi Flora (HHCCJCY01), en clair
//  Aucune dépendance à Arduino : testé sur PC.
// ============================================================================
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "survey.h"

struct Sens {
  bool used = false, dirty = false;
  uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
  uint8_t ty = 0;      // 1 BTHome, 2 Xiaomi Flora, 3 thermomètre Xiaomi (MiBeacon), 4 Xiaomi firmware ATC/pvvx, 5 Ruuvi, 6 Eddystone TLM
  uint8_t enc = 0;     // 1 : annonces chiffrées (non lisibles sans clé)
  char name[24] = "";
  int8_t rssi = -127;
  uint32_t frames = 0, firstMs = 0, lastMs = 0, sentMs = 0;
  float temp = NAN, hum = NAN, lux = NAN, rot = NAN, pres = NAN;   // pression en hPa
  int16_t batt = -1, moist = -1;   // %
  int16_t volt = -1;               // tension de pile en mV
  int32_t cond = -1;               // µS/cm
  int8_t win = -1, door = -1, motion = -1;   // -1 inconnu, 0 fermé/aucun, 1 ouvert/mouvement
  int16_t button = -1;             // dernier évènement bouton (BTHome 0x3A)
  uint16_t haMask = 0;             // passerelle : entités Home Assistant déjà déclarées
};

// bits des mesures présentes
enum { SB_TEMP = 1, SB_HUM = 2, SB_LUX = 4, SB_BATT = 8, SB_MOIST = 16, SB_COND = 32, SB_WIN = 64, SB_DOOR = 128, SB_MOTION = 256, SB_BUTTON = 512, SB_ROT = 1024, SB_PRES = 2048, SB_VOLT = 4096 };

static inline uint16_t sensMask(const Sens& s) {
  uint16_t m = 0;
  if (!isnan(s.temp)) m |= SB_TEMP;
  if (!isnan(s.hum)) m |= SB_HUM;
  if (!isnan(s.lux)) m |= SB_LUX;
  if (s.batt >= 0) m |= SB_BATT;
  if (s.moist >= 0) m |= SB_MOIST;
  if (s.cond >= 0) m |= SB_COND;
  if (s.win >= 0) m |= SB_WIN;
  if (s.door >= 0) m |= SB_DOOR;
  if (s.motion >= 0) m |= SB_MOTION;
  if (s.button >= 0) m |= SB_BUTTON;
  if (!isnan(s.rot)) m |= SB_ROT;
  if (!isnan(s.pres)) m |= SB_PRES;
  if (s.volt >= 0) m |= SB_VOLT;
  return m;
}

// recopie dans « dst » les seules mesures présentes dans « t » (un paquet ne contient qu'une partie des mesures)
static inline void sensMerge(Sens& dst, const Sens& t) {
  if (!isnan(t.temp)) dst.temp = t.temp;
  if (!isnan(t.hum)) dst.hum = t.hum;
  if (!isnan(t.lux)) dst.lux = t.lux;
  if (!isnan(t.rot)) dst.rot = t.rot;
  if (!isnan(t.pres)) dst.pres = t.pres;
  if (t.volt >= 0) dst.volt = t.volt;
  if (t.batt >= 0) dst.batt = t.batt;
  if (t.moist >= 0) dst.moist = t.moist;
  if (t.cond >= 0) dst.cond = t.cond;
  if (t.win >= 0) dst.win = t.win;
  if (t.door >= 0) dst.door = t.door;
  if (t.motion >= 0) dst.motion = t.motion;
  if (t.button >= 0) dst.button = t.button;
  if (t.ty) dst.ty = t.ty;
  if (t.enc) dst.enc = 1;
}

// longueur des données d'un objet BTHome v2 (0 = identifiant inconnu : on arrête la lecture)
static inline int bthomeLen(uint8_t id) {
  if (id == 0x00 || id == 0x01 || id == 0x09 || (id >= 0x0F && id <= 0x11) || (id >= 0x15 && id <= 0x2F) || id == 0x3A || id == 0x46 ||
      id == 0x57 || id == 0x58 || id == 0x59 || id == 0x60)
    return 1;
  switch (id) {
    case 0x02: case 0x03: case 0x06: case 0x07: case 0x08: case 0x0C: case 0x0D: case 0x0E: case 0x12: case 0x13: case 0x14:
    case 0x3C: case 0x3D: case 0x3F: case 0x40: case 0x41: case 0x43: case 0x44: case 0x45: case 0x47: case 0x48: case 0x49:
    case 0x4A: case 0x51: case 0x52: case 0x56: case 0x5A: case 0x5D: case 0x5E: case 0x5F: case 0x61: case 0xF0:
      return 2;
    case 0x04: case 0x05: case 0x0A: case 0x0B: case 0x42: case 0x4B: case 0xF2:
      return 3;
    case 0x3E: case 0x4C: case 0x4D: case 0x4E: case 0x4F: case 0x50: case 0x55: case 0x5B: case 0x5C: case 0x62: case 0x63: case 0xF1:
      return 4;
  }
  return 0;
}

static inline uint32_t leU(const uint8_t* v, int n) {
  uint32_t x = 0;
  for (int i = n - 1; i >= 0; i--) x = (x << 8) | v[i];
  return x;
}
static inline int leS16(const uint8_t* v) { return (int16_t)(uint16_t)(v[0] | (v[1] << 8)); }

// d = données de service après l'UUID 0xFCD2
static inline bool sensDecodeBthome(const uint8_t* d, size_t n, Sens& s) {
  if (n < 2) return false;
  uint8_t info = d[0];
  if (((info >> 5) & 7) != 2) return false;   // BTHome v2 seulement
  s.ty = 1;
  if (info & 1) { s.enc = 1; return true; }
  size_t i = 1;
  if (info & 2) i += 6;
  while (i < n) {
    uint8_t id = d[i++];
    int len = bthomeLen(id);
    if (len <= 0 || i + (size_t)len > n) break;
    const uint8_t* v = d + i;
    switch (id) {
      case 0x01: s.batt = v[0] > 100 ? 100 : v[0]; break;
      case 0x02: s.temp = leS16(v) * 0.01f; break;
      case 0x45: s.temp = leS16(v) * 0.1f; break;
      case 0x03: s.hum = leU(v, 2) * 0.01f; break;
      case 0x2E: s.hum = (float)v[0]; break;
      case 0x05: s.lux = leU(v, 3) * 0.01f; break;
      case 0x14: s.moist = (int16_t)lroundf(leU(v, 2) * 0.01f); break;
      case 0x2F: s.moist = v[0]; break;
      case 0x56: s.cond = (int32_t)leU(v, 2); break;
      case 0x2D: s.win = v[0] ? 1 : 0; break;
      case 0x1A: s.door = v[0] ? 1 : 0; break;
      case 0x21: s.motion = v[0] ? 1 : 0; break;
      case 0x3A: s.button = v[0]; break;
      case 0x3F: s.rot = leS16(v) * 0.1f; break;
      default: break;
    }
    i += (size_t)len;
  }
  return true;
}

// d = données de service après l'UUID 0xFE95 (MiBeacon). Seule la Flora HHCCJCY01 (0x0098) est lue.
static inline bool sensDecodeMiBeacon(const uint8_t* d, size_t n, Sens& s) {
  if (n < 5 || !(d[0] & 0x40)) return false;
  uint16_t dev = (uint16_t)(d[2] | (d[3] << 8));
  switch (dev) {   // appareils dont les mesures sont lisibles ou signalées chiffrées (même disposition, d'après ESPHome)
    case 0x0098: s.ty = 2; break;   // Flora HHCCJCY01
    case 0x01AA: case 0x015D: case 0x0347: case 0x0B48: case 0x055B: case 0x045B: case 0x06D3: case 0x0387:
      s.ty = 3;                     // LYWSDCGQ, HHCCPOT002, CGG1, LYWSD03MMC, LYWSD02, MHO-C303/C401
      break;
    default: return false;
  }
  if (d[0] & 0x08) { s.enc = 1; return true; }
  size_t off = (d[0] & 0x20) ? 12 : 11;
  while (off + 3 < n) {
    uint16_t type = (uint16_t)(d[off] | (d[off + 1] << 8));
    uint8_t len = d[off + 2];
    if (len < 1 || len > 4 || off + 3 + len > n) break;
    const uint8_t* v = d + off + 3;
    switch (type) {
      case 0x1004: if (len == 2) s.temp = leS16(v) / 10.0f; break;
      case 0x1006: if (len == 2) s.hum = leS16(v) / 10.0f; break;
      case 0x1007: if (len == 3) s.lux = (float)leU(v, 3); break;
      case 0x1008: if (len == 1) s.moist = v[0]; break;
      case 0x1009: if (len == 2) s.cond = (int32_t)leU(v, 2); break;
      case 0x100A: if (len == 1) s.batt = v[0] > 100 ? 100 : v[0]; break;
      case 0x100D: if (len == 4) { s.temp = leS16(v) / 10.0f; s.hum = leS16(v + 2) / 10.0f; } break;
      default: break;
    }
    off += 3 + len;
  }
  return true;
}

// d = données de service après l'UUID 0x181A : firmware alternatif ATC1441 (13 octets) ou pvvx « custom » (15 octets) des thermomètres Xiaomi
static inline bool sensDecodeAtc(const uint8_t* d, size_t n, Sens& s) {
  if (n == 13) {   // MAC(6) · température int16 gros-boutiste /10 · humidité % · batterie % · pile mV gros-boutiste · compteur
    s.temp = (int16_t)(uint16_t)((d[6] << 8) | d[7]) / 10.0f;
    s.hum = (float)d[8];
    s.batt = d[9] > 100 ? 100 : d[9];
    s.volt = (int16_t)((d[10] << 8) | d[11]);
  } else if (n == 15) {   // MAC(6) · température int16 /100 · humidité u16 /100 · pile mV · batterie % · compteur · drapeaux (petit-boutiste)
    s.temp = leS16(d + 6) / 100.0f;
    s.hum = leU(d + 8, 2) / 100.0f;
    s.volt = (int16_t)leU(d + 10, 2);
    s.batt = d[12] > 100 ? 100 : d[12];
  } else {
    return false;
  }
  if (s.hum < 0 || s.hum > 100 || s.temp < -60 || s.temp > 120) { s.temp = NAN; s.hum = NAN; return false; }
  s.ty = 4;
  return true;
}

// d = données constructeur Ruuvi (société 0x0499) après l'identifiant de société : format 5 (RAWv2), 24 octets, gros-boutiste
static inline bool sensDecodeRuuvi(const uint8_t* d, size_t n, Sens& s) {
  if (n < 24 || d[0] != 5) return false;
  int t = (int16_t)(uint16_t)((d[1] << 8) | d[2]);
  unsigned h = (unsigned)((d[3] << 8) | d[4]), p = (unsigned)((d[5] << 8) | d[6]), pw = (unsigned)((d[13] << 8) | d[14]);
  if ((uint16_t)t != 0x8000) s.temp = t * 0.005f;
  if (h != 0xFFFF) s.hum = h * 0.0025f;
  if (p != 0xFFFF) s.pres = (p + 50000) / 100.0f;
  if ((pw >> 5) != 2047) s.volt = (int16_t)((pw >> 5) + 1600);
  s.ty = 5;
  return true;
}

// d = données de service après l'UUID 0xFEAA, trame TLM d'Eddystone : 0x20, version 0, tension de pile mV (2 octets), température 8.8 signée (2 octets),
// compteurs. Octets gros-boutistes ; 0 (tension) et 0x8000 (température) = non pris en charge.
static inline bool sensDecodeEddystone(const uint8_t* d, size_t n, Sens& s) {
  if (n < 14 || d[0] != 0x20 || d[1] != 0x00) return false;
  unsigned mv = (unsigned)((d[2] << 8) | d[3]);
  int t = (int16_t)(uint16_t)((d[4] << 8) | d[5]);
  if (mv) s.volt = (int16_t)(mv > 32767 ? 32767 : mv);
  if ((uint16_t)t != 0x8000) s.temp = t / 256.0f;
  s.ty = 6;
  return true;
}

// uuid : 0xFCD2 (BTHome), 0xFE95 (Xiaomi MiBeacon), 0x181A (ATC/pvvx), 0x0499 (société Ruuvi, données constructeur)
static inline bool sensDecode(uint16_t uuid, const uint8_t* d, size_t n, Sens& s) {
  if (uuid == 0xFCD2) return sensDecodeBthome(d, n, s);
  if (uuid == 0xFE95) return sensDecodeMiBeacon(d, n, s);
  if (uuid == 0x181A) return sensDecodeAtc(d, n, s);
  if (uuid == 0x0499) return sensDecodeRuuvi(d, n, s);
  if (uuid == 0xFEAA) return sensDecodeEddystone(d, n, s);
  return false;
}

static inline const char* sensTypeText(const Sens& s) {
  switch (s.ty) {
    case 2: return "Xiaomi Flora";
    case 3: return "Xiaomi (MiBeacon)";
    case 4: return "Xiaomi (firmware ATC/pvvx)";
    case 5: return "Ruuvi";
    case 6: return "Eddystone TLM";
    default: return "BTHome";
  }
}

// JSON d'un capteur (page web, MQTT) : seules les mesures connues sont présentes
static inline size_t sensJson(const Sens& s, uint32_t ageSec, char* out, size_t cap) {
  char mac[18];
  jnlMac(s.mac, mac, sizeof mac);
  JBuf b(out, cap);
  b.add("{\"name\":");
  b.jstr(s.name[0] ? s.name : (s.ty == 2 ? "Flora" : s.ty == 5 ? "Ruuvi" : s.ty == 6 ? "Eddystone" : s.ty == 1 ? "BTHome" : "Xiaomi"));
  b.add(",\"mac\":\"%s\",\"type\":\"%s\"", mac, sensTypeText(s));
  if (s.enc) b.add(",\"encrypted\":true");
  if (!isnan(s.temp)) b.add(",\"temperature\":%.1f", s.temp);
  if (!isnan(s.hum)) b.add(",\"humidity\":%.1f", s.hum);
  if (!isnan(s.lux)) b.add(",\"illuminance\":%.0f", s.lux);
  if (s.moist >= 0) b.add(",\"moisture\":%d", (int)s.moist);
  if (s.cond >= 0) b.add(",\"conductivity\":%ld", (long)s.cond);
  if (!isnan(s.pres)) b.add(",\"pressure\":%.1f", s.pres);
  if (s.batt >= 0) b.add(",\"battery\":%d", (int)s.batt);
  if (s.volt >= 0) b.add(",\"battery_voltage\":%d", (int)s.volt);
  if (s.win >= 0) b.add(",\"window\":\"%s\"", s.win ? "open" : "closed");
  if (s.door >= 0) b.add(",\"door\":\"%s\"", s.door ? "open" : "closed");
  if (s.motion >= 0) b.add(",\"motion\":%s", s.motion ? "true" : "false");
  if (s.button >= 0) b.add(",\"button\":%d", (int)s.button);
  if (!isnan(s.rot)) b.add(",\"rotation\":%.1f", s.rot);
  b.add(",\"rssi\":%d,\"age\":%lu,\"frames\":%lu}", (int)s.rssi, (unsigned long)ageSec, (unsigned long)s.frames);
  return b.len;
}
