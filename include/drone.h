#pragma once
// ============================================================================
//  Structure « drone » et sérialisation JSON, partagées par les deux cartes.
//  Aucune dépendance à Arduino : testé sur PC.
// ============================================================================
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "journal.h"

// bits de « src » : 1 Bluetooth, 2 Wi-Fi beacon, 4 Wi-Fi NAN, 8 Wi-Fi français, 16 simulation
struct Drone {
  bool used = false;
  uint8_t src = 0;
  uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
  char id[41] = "";
  char reg[41] = "";
  uint8_t idType = 0, uaType = 0;
  bool hasId = false, hasReg = false, hasPos = false, hasOp = false, hasLoc = false;
  uint8_t status = 0;
  double lat = 0, lon = 0, opLat = 0, opLon = 0;
  float altGeo = NAN, altBaro = NAN, height = NAN, spdH = NAN, spdV = NAN, dir = NAN;
  float odTs = NAN;      // horodatage de l'émetteur : secondes écoulées depuis le début de l'heure (UTC)
  char desc[24] = "";
  char opId[21] = "";
  char ansi[48] = "";   // identifiant ANSI/CTA-2063 (système de signalement non intégré), s'il est reçu en plus de l'identifiant
  uint32_t logMs = 0;    // passerelle : dernier envoi syslog
  bool logSeen = false;  // passerelle : première détection déjà signalée en syslog
  bool alertSl = false;   // passerelle : alerte syslog déjà envoyée
  bool alertSeen = false; // passerelle : alerte « nouveau drone » déjà publiée en MQTT
  bool alertEm = false, alertEmSl = false; // passerelle : alerte « urgence » déjà envoyée (MQTT / syslog)
  int8_t rssi = -127;
  uint32_t firstMs = 0, lastMs = 0, frames = 0;
  bool dirty = false;
  uint32_t sentMs = 0;   // scanner : dernier envoi vers l'ESP 2
};

// Identifiant français de 30 caractères : trigramme constructeur (3) + modèle (3) + numéro de série (24).
// Retourne true si l'identifiant a cette forme (signalement électronique français).
static inline bool frSplit(const Drone& d, char man[4], char mod[4], char ser[25]) {
  if (!(d.src & 8) || !d.hasId || strlen(d.id) != 30) return false;
  memcpy(man, d.id, 3);
  man[3] = 0;
  memcpy(mod, d.id + 3, 3);
  mod[3] = 0;
  memcpy(ser, d.id + 6, 24);
  ser[24] = 0;
  return true;
}

// Sérialisation pour la page web et pour MQTT (age = secondes depuis la dernière réception)
static inline size_t droneJson(const Drone& d, uint32_t ageSec, long seenEpoch, char* out, size_t cap) {
  char src[32], mac[18];
  srcText(d.src, src, sizeof src);
  jnlMac(d.mac, mac, sizeof mac);
  JBuf b(out, cap);
  b.add("{\"id\":");
  b.jstr(d.hasId ? d.id : "");
  if (d.hasReg) { b.add(",\"reg\":"); b.jstr(d.reg); }
  b.add(",\"mac\":\"%s\",\"src\":\"%s\"", mac, src);
  if (d.hasId) b.add(",\"id_type\":%u,\"ua_type\":%u", (unsigned)d.idType, (unsigned)d.uaType);
  {
    char man[4], mod[4], ser[25];
    if (frSplit(d, man, mod, ser)) {
      b.add(",\"fr_manufacturer\":\"%s\",\"fr_model\":\"%s\",\"fr_serial\":", man, mod);
      b.jstr(ser);
    }
    if (d.ansi[0] && strcmp(d.ansi, d.id) != 0) { b.add(",\"ansi_id\":"); b.jstr(d.ansi); }
  }
  if (d.hasLoc) b.add(",\"status\":%u", (unsigned)d.status);
  if (!isnan(d.odTs)) b.add(",\"od_ts\":%.1f", d.odTs);
  if (d.hasPos) b.add(",\"lat\":%.6f,\"lon\":%.6f", d.lat, d.lon);
  if (!isnan(d.altGeo)) b.add(",\"alt_geo\":%.1f", d.altGeo);
  if (!isnan(d.altBaro)) b.add(",\"alt_baro\":%.1f", d.altBaro);
  if (!isnan(d.height)) b.add(",\"height\":%.1f", d.height);
  if (!isnan(d.spdH)) b.add(",\"speed\":%.1f", d.spdH);
  if (!isnan(d.spdV)) b.add(",\"vspeed\":%.1f", d.spdV);
  if (!isnan(d.dir)) b.add(",\"dir\":%.0f", d.dir);
  if (d.hasOp) b.add(",\"op_lat\":%.6f,\"op_lon\":%.6f", d.opLat, d.opLon);
  if (d.opId[0]) { b.add(",\"op_id\":"); b.jstr(d.opId); }
  if (d.desc[0]) { b.add(",\"desc\":"); b.jstr(d.desc); }
  b.add(",\"rssi\":%d,\"age\":%lu,\"frames\":%lu", (int)d.rssi, (unsigned long)ageSec, (unsigned long)d.frames);
  if (seenEpoch > 0) b.add(",\"seen\":%ld", seenEpoch);
  b.add("}");
  return b.len;
}

// Version compacte pour la liste MQTT
static inline size_t droneJsonCompact(const Drone& d, uint32_t ageSec, char* out, size_t cap) {
  char src[32], mac[18];
  srcText(d.src, src, sizeof src);
  jnlMac(d.mac, mac, sizeof mac);
  JBuf b(out, cap);
  b.add("{\"id\":");
  b.jstr(d.hasId ? d.id : "");
  b.add(",\"mac\":\"%s\",\"src\":\"%s\"", mac, src);
  {
    char man[4], mod[4], ser[25];
    if (frSplit(d, man, mod, ser)) b.add(",\"fr_manufacturer\":\"%s\",\"fr_model\":\"%s\",\"fr_serial\":\"%s\"", man, mod, ser);
  }
  if (d.hasPos) b.add(",\"lat\":%.6f,\"lon\":%.6f", d.lat, d.lon);
  if (!isnan(d.altGeo)) b.add(",\"alt\":%.0f", d.altGeo);
  if (!isnan(d.height)) b.add(",\"height\":%.0f", d.height);
  if (!isnan(d.spdH)) b.add(",\"speed\":%.1f", d.spdH);
  if (d.hasOp) b.add(",\"op_lat\":%.6f,\"op_lon\":%.6f", d.opLat, d.opLon);
  if (d.opId[0]) { b.add(",\"op_id\":"); b.jstr(d.opId); }
  b.add(",\"rssi\":%d,\"age\":%lu}", (int)d.rssi, (unsigned long)ageSec);
  return b.len;
}

// Identifiant utilisable dans un sujet MQTT : id du drone, sinon adresse MAC
static inline void droneTopicId(const Drone& d, char* out, size_t cap) {
  size_t n = 0;
  if (d.hasId && d.id[0]) {
    for (const char* p = d.id; *p && n + 1 < cap; p++) {
      bool okc = (*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '-' || *p == '_';
      out[n++] = okc ? *p : '_';
    }
    out[n] = 0;
  } else {
    snprintf(out, cap, "%02X%02X%02X%02X%02X%02X", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);
  }
}
