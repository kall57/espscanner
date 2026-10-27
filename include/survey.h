#pragma once
// ============================================================================
//  Relevé des réseaux Wi-Fi et des appareils Bluetooth captés par l'ESP 1 (hors drones).
//  Aucune dépendance à Arduino : testé sur PC.
// ============================================================================
#include "drone.h"

struct Net {
  bool used = false;
  uint8_t bssid[6] = {0, 0, 0, 0, 0, 0};
  char ssid[33] = "";
  uint8_t ch = 0, enc = 0;   // enc : 0 ouvert, 1 WEP/privé, 2 WPA, 3 WPA2/WPA3
  int8_t rssi = -127;
  uint32_t frames = 0, firstMs = 0, lastMs = 0, sentMs = 0;
  bool dirty = false;
  bool logged = false;   // passerelle : déjà signalé en syslog
};

struct Bt {
  bool used = false;
  uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
  uint8_t atype = 0;   // 0 publique, 1 aléatoire
  char name[33] = "";
  int32_t mf = -1;     // identifiant constructeur Bluetooth (-1 = inconnu)
  uint8_t tag = 0;     // type reconnu : traceur ou balise (voir btTagText), 0 = aucun
  int8_t rssi = -127;
  uint32_t frames = 0, firstMs = 0, lastMs = 0, sentMs = 0;
  bool dirty = false;
  bool logged = false;
};

// Traceurs et balises reconnus dans les annonces (relevé passif)
//   1 AirTag, 2 accessoire Find My tiers, 3 autre appareil Apple Find My, 4 Tile, 5 Samsung SmartTag, 6 iBeacon, 7 Eddystone
static inline const char* btTagText(uint8_t t) {
  switch (t) {
    case 1: return "AirTag (Find My)";
    case 2: return "Accessoire Find My tiers";
    case 3: return "Appareil Apple Find My";
    case 4: return "Tile";
    case 5: return "Samsung SmartTag";
    case 6: return "iBeacon";
    case 7: return "Eddystone";
  }
  return "";
}

// type = type de la structure d'annonce (0xFF données constructeur, 0x16 données de service, 0x02/0x03 liste d'UUID 16 bits),
// d = octets qui suivent le type, n = leur nombre. Retourne le type de traceur/balise, 0 sinon.
// Find My (état « séparé ») : 1E FF 4C 00 12 19 + octet d'état (bits 2-3 : 1 AirTag, 2 accessoire tiers). Tile et Samsung : d'après leurs UUID de service.
static inline uint8_t btTagFromAd(uint8_t type, const uint8_t* d, size_t n) {
  if (type == 0xFF && n >= 5 && d[0] == 0x4C && d[1] == 0x00) {
    if (d[2] == 0x12 && d[3] == 0x19 && n == 29) {
      uint8_t k = (d[4] >> 2) & 3;
      return k == 1 ? 1 : k == 2 ? 2 : 3;
    }
    if (d[2] == 0x02 && d[3] == 0x15 && n >= 25) return 6;
    return 0;
  }
  if (type == 0x16 && n >= 2) {
    uint16_t u = (uint16_t)(d[0] | (d[1] << 8));
    if (u == 0xFEED || u == 0xFD84) return 4;
    if (u == 0xFD5A) return 5;
    if (u == 0xFEAA) return 7;
    return 0;
  }
  if ((type == 0x02 || type == 0x03) && n >= 2) {
    for (size_t i = 0; i + 1 < n; i += 2) {
      uint16_t u = (uint16_t)(d[i] | (d[i + 1] << 8));
      if (u == 0xFEED || u == 0xFD84) return 4;
      if (u == 0xFD5A) return 5;
    }
  }
  return 0;
}

static inline const char* encText(uint8_t e) {
  switch (e) {
    case 0: return "Ouvert";
    case 1: return "WEP";
    case 2: return "WPA";
    default: return "WPA2/WPA3";
  }
}

static inline const char* companyName(int32_t id) {
  switch (id) {
    case 0x0006: return "Microsoft";
    case 0x004C: return "Apple";
    case 0x0075: return "Samsung";
    case 0x00E0: return "Google";
    case 0x0087: return "Garmin";
    case 0x0059: return "Nordic Semiconductor";
    case 0x02E5: return "Espressif";
    case 0x0157: return "Huami (Xiaomi)";
    case 0x038F: return "Xiaomi";
    case 0x0499: return "Ruuvi";
    case 0x00D2: return "Dialog Semiconductor";
    case 0x0171: return "Amazon";
    default: return "";
  }
}

static inline size_t netJson(const Net& n, uint32_t ageSec, char* out, size_t cap) {
  char mac[18];
  jnlMac(n.bssid, mac, sizeof mac);
  JBuf b(out, cap);
  b.add("{\"ssid\":");
  b.jstr(n.ssid);
  b.add(",\"bssid\":\"%s\",\"ch\":%u,\"enc\":%u,\"enc_txt\":\"%s\",\"rssi\":%d,\"age\":%lu,\"frames\":%lu}", mac, (unsigned)n.ch,
        (unsigned)n.enc, encText(n.enc), (int)n.rssi, (unsigned long)ageSec, (unsigned long)n.frames);
  return b.len;
}

static inline size_t btJson(const Bt& d, uint32_t ageSec, char* out, size_t cap) {
  char mac[18];
  jnlMac(d.mac, mac, sizeof mac);
  JBuf b(out, cap);
  b.add("{\"name\":");
  b.jstr(d.name);
  b.add(",\"mac\":\"%s\",\"addr_type\":\"%s\"", mac, d.atype ? "aléatoire" : "publique");
  if (d.tag) b.add(",\"tag\":\"%s\",\"tag_id\":%u", btTagText(d.tag), (unsigned)d.tag);
  if (d.mf >= 0) {
    b.add(",\"mf\":%ld,\"mf_name\":", (long)d.mf);
    b.jstr(companyName(d.mf));
  }
  b.add(",\"rssi\":%d,\"age\":%lu,\"frames\":%lu}", (int)d.rssi, (unsigned long)ageSec, (unsigned long)d.frames);
  return b.len;
}
