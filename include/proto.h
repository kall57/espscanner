#pragma once
// ============================================================================
//  Liaison série ESP 1 -> ESP 2 (et commandes ESP 2 -> ESP 1)
//  Une ligne = un objet JSON plat, suivi de « *XXXX » (CRC16-CCITT en hexadécimal) et d'un retour à la ligne :
//     {"k":"d","mac":"AABBCC...","id":"...","lat":48.1,...}*3FA2\n
//  Types : k=d drone, k=h battement de cœur (statistiques de l'ESP 1), k=c commande (ESP 2 -> ESP 1).
//  Les lignes abîmées (bruit au démarrage, câble) sont rejetées par le CRC.
//  Aucune dépendance à Arduino : testé sur PC.
// ============================================================================
#include <stdlib.h>
#include "survey.h"
#include "sensors.h"

static inline uint16_t crc16(const char* p, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint16_t)((uint8_t)p[i]) << 8;
    for (int k = 0; k < 8; k++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}

// Ajoute « *XXXX\n » à un JSON déjà écrit dans out (longueur len). Retourne la longueur totale.
static inline size_t protoSeal(char* out, size_t cap, size_t len) {
  if (len + 7 > cap) return 0;
  int k = snprintf(out + len, cap - len, "*%04X\n", crc16(out, len));
  return k > 0 ? len + (size_t)k : 0;
}

static inline size_t protoDrone(const Drone& d, char* out, size_t cap) {
  char mac[13];
  snprintf(mac, sizeof mac, "%02X%02X%02X%02X%02X%02X", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);
  JBuf b(out, cap);
  b.add("{\"k\":\"d\",\"mac\":\"%s\",\"s\":%u,\"fr\":%lu,\"rs\":%d", mac, (unsigned)d.src, (unsigned long)d.frames, (int)d.rssi);
  if (d.hasId) { b.add(",\"id\":"); b.jstr(d.id); b.add(",\"it\":%u,\"ut\":%u", (unsigned)d.idType, (unsigned)d.uaType); }
  if (d.hasReg) { b.add(",\"reg\":"); b.jstr(d.reg); }
  if (d.hasLoc) b.add(",\"st\":%u", (unsigned)d.status);
  if (!isnan(d.odTs)) b.add(",\"tm\":%.1f", d.odTs);
  if (d.hasPos) b.add(",\"lat\":%.7f,\"lon\":%.7f", d.lat, d.lon);
  if (!isnan(d.altGeo)) b.add(",\"ag\":%.1f", d.altGeo);
  if (!isnan(d.altBaro)) b.add(",\"ab\":%.1f", d.altBaro);
  if (!isnan(d.height)) b.add(",\"h\":%.1f", d.height);
  if (!isnan(d.spdH)) b.add(",\"sp\":%.2f", d.spdH);
  if (!isnan(d.spdV)) b.add(",\"vs\":%.2f", d.spdV);
  if (!isnan(d.dir)) b.add(",\"dir\":%.0f", d.dir);
  if (d.hasOp) b.add(",\"ola\":%.7f,\"olo\":%.7f", d.opLat, d.opLon);
  if (d.opId[0]) { b.add(",\"oid\":"); b.jstr(d.opId); }
  if (d.desc[0]) { b.add(",\"ds\":"); b.jstr(d.desc); }
  if (d.ansi[0]) { b.add(",\"an\":"); b.jstr(d.ansi); }
  b.add("}");
  return protoSeal(out, cap, b.len);
}

struct Heartbeat {
  uint32_t up = 0, ble = 0, wifi = 0, drop = 0, heap = 0, tracked = 0, nets = 0, bts = 0;
  int ch = 0, mode = 0;
  bool bleOn = false, wifiOn = false;
  char fw[28] = "";   // date de compilation du firmware
};

// Réglages de l'ESP 1, commandés par l'ESP 2 (renvoyés toutes les 5 s)
struct ScanCmd {
  int mode = 0;          // 0 BLE+Wi-Fi, 1 BLE, 2 Wi-Fi
  bool wifiSurvey = true, btSurvey = true;
  int chan = 0;          // 0 = saut de canaux automatique, 1..13 = canal fixe
  int rssiMin = -95;     // réseaux / appareils plus faibles que ce seuil ignorés
  int ant = 1;           // 0 céramique, 1 externe
  int bleDuty = 50;      // % du temps d'antenne donné au Bluetooth quand les deux radios écoutent
};

static inline size_t protoHeartbeat(const Heartbeat& h, char* out, size_t cap) {
  JBuf b(out, cap);
  b.add("{\"k\":\"h\",\"up\":%lu,\"ble\":%lu,\"wifi\":%lu,\"drop\":%lu,\"heap\":%lu,\"n\":%lu,\"nw\":%lu,\"nb\":%lu,\"ch\":%d,\"mode\":%d,\"bo\":%d,\"wo\":%d,\"fw\":\"%s\"}",
        (unsigned long)h.up, (unsigned long)h.ble, (unsigned long)h.wifi, (unsigned long)h.drop, (unsigned long)h.heap,
        (unsigned long)h.tracked, (unsigned long)h.nets, (unsigned long)h.bts, h.ch, h.mode, h.bleOn ? 1 : 0, h.wifiOn ? 1 : 0, h.fw);
  return protoSeal(out, cap, b.len);
}

static inline size_t protoCommand(const ScanCmd& c, char* out, size_t cap) {
  JBuf b(out, cap);
  b.add("{\"k\":\"c\",\"mode\":%d,\"wn\":%d,\"bn\":%d,\"chn\":%d,\"rm\":%d,\"ant\":%d,\"bd\":%d}", c.mode, c.wifiSurvey ? 1 : 0,
        c.btSurvey ? 1 : 0, c.chan, c.rssiMin, c.ant, c.bleDuty);
  return protoSeal(out, cap, b.len);
}

static inline size_t protoNet(const Net& n, char* out, size_t cap) {
  char mac[13];
  snprintf(mac, sizeof mac, "%02X%02X%02X%02X%02X%02X", n.bssid[0], n.bssid[1], n.bssid[2], n.bssid[3], n.bssid[4], n.bssid[5]);
  JBuf b(out, cap);
  b.add("{\"k\":\"w\",\"mac\":\"%s\",\"ss\":", mac);
  b.jstr(n.ssid);
  b.add(",\"c\":%u,\"en\":%u,\"rs\":%d,\"fr\":%lu}", (unsigned)n.ch, (unsigned)n.enc, (int)n.rssi, (unsigned long)n.frames);
  return protoSeal(out, cap, b.len);
}

static inline size_t protoBt(const Bt& d, char* out, size_t cap) {
  char mac[13];
  snprintf(mac, sizeof mac, "%02X%02X%02X%02X%02X%02X", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);
  JBuf b(out, cap);
  b.add("{\"k\":\"b\",\"mac\":\"%s\",\"at\":%u,\"nm\":", mac, (unsigned)d.atype);
  b.jstr(d.name);
  b.add(",\"mf\":%ld,\"tg\":%u,\"rs\":%d,\"fr\":%lu}", (long)d.mf, (unsigned)d.tag, (int)d.rssi, (unsigned long)d.frames);
  return protoSeal(out, cap, b.len);
}

// capteur Bluetooth (BTHome, Xiaomi Flora) : seules les mesures connues sont envoyées
static inline size_t protoSens(const Sens& d, char* out, size_t cap) {
  char mac[13];
  snprintf(mac, sizeof mac, "%02X%02X%02X%02X%02X%02X", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);
  JBuf b(out, cap);
  b.add("{\"k\":\"s\",\"mac\":\"%s\",\"ty\":%u,\"en\":%u,\"nm\":", mac, (unsigned)d.ty, (unsigned)d.enc);
  b.jstr(d.name);
  b.add(",\"rs\":%d,\"fr\":%lu", (int)d.rssi, (unsigned long)d.frames);
  if (!isnan(d.temp)) b.add(",\"te\":%.2f", d.temp);
  if (!isnan(d.hum)) b.add(",\"hu\":%.2f", d.hum);
  if (!isnan(d.lux)) b.add(",\"lx\":%.2f", d.lux);
  if (!isnan(d.rot)) b.add(",\"ro\":%.1f", d.rot);
  if (!isnan(d.pres)) b.add(",\"pr\":%.1f", d.pres);
  if (d.volt >= 0) b.add(",\"vo\":%d", (int)d.volt);
  if (d.batt >= 0) b.add(",\"ba\":%d", (int)d.batt);
  if (d.moist >= 0) b.add(",\"mo\":%d", (int)d.moist);
  if (d.cond >= 0) b.add(",\"ec\":%ld", (long)d.cond);
  if (d.win >= 0) b.add(",\"wi\":%d", (int)d.win);
  if (d.door >= 0) b.add(",\"do\":%d", (int)d.door);
  if (d.motion >= 0) b.add(",\"mt\":%d", (int)d.motion);
  if (d.button >= 0) b.add(",\"bu\":%d", (int)d.button);
  b.add("}");
  return protoSeal(out, cap, b.len);
}

// ---------------------------------------------------------------------------
//  Lecture d'un JSON plat : "clé":valeur (chaîne, nombre, true/false). Pas d'imbrication.
// ---------------------------------------------------------------------------
struct JKV {
  char key[8];
  char val[96];
  bool isStr;
};

// p avance après la paire lue. Retourne false à la fin ou si le texte est invalide.
static inline bool jsonNext(const char*& p, JKV* kv) {
  while (*p == '{' || *p == ',' || *p == ' ') p++;
  if (*p != '"') return false;
  p++;
  size_t n = 0;
  while (*p && *p != '"') {
    if (n + 1 < sizeof kv->key) kv->key[n++] = *p;
    p++;
  }
  kv->key[n] = 0;
  if (*p != '"') return false;
  p++;
  if (*p != ':') return false;
  p++;
  n = 0;
  if (*p == '"') {
    kv->isStr = true;
    p++;
    while (*p && *p != '"') {
      char c = *p++;
      if (c == '\\' && *p) {
        c = *p++;
        if (c == 'u') {   // \u00XX
          unsigned v = 0;
          for (int i = 0; i < 4 && *p; i++, p++) {
            char h = *p;
            v = v * 16 + (unsigned)((h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10 : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : 0);
          }
          c = (v >= 0x20 && v < 0x7F) ? (char)v : '?';
        }
      }
      if (n + 1 < sizeof kv->val) kv->val[n++] = c;
    }
    if (*p != '"') return false;
    p++;
  } else {
    kv->isStr = false;
    while (*p && *p != ',' && *p != '}') {
      if (n + 1 < sizeof kv->val) kv->val[n++] = *p;
      p++;
    }
  }
  kv->val[n] = 0;
  return true;
}

struct LinkMsg {
  char kind = 0;   // 'd' drone, 'w' réseau Wi-Fi, 'b' appareil Bluetooth, 's' capteur Bluetooth, 'h' battement de cœur, 'c' commande
  Drone d;
  Net net;
  Bt bt;
  Sens sens;
  Heartbeat hb;
  ScanCmd cmd;
};

static inline int hexv(char c) { return (c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1; }

static inline void protoCopy(char* dst, size_t cap, const char* s) {
  size_t n = 0;
  for (; s[n] && n + 1 < cap; n++) dst[n] = s[n];
  dst[n] = 0;
}

// Décode une ligne (sans retour à la ligne). Retourne 0 si correcte, 1 si CRC faux, 2 si illisible.
static inline int protoParse(const char* line, size_t len, LinkMsg* m) {
  while (len && (line[len - 1] == '\r' || line[len - 1] == '\n')) len--;
  if (len < 8 || line[0] != '{' || line[len - 5] != '*') return 2;
  int h0 = hexv(line[len - 4]), h1 = hexv(line[len - 3]), h2 = hexv(line[len - 2]), h3 = hexv(line[len - 1]);
  if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) return 2;
  uint16_t want = (uint16_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
  if (crc16(line, len - 5) != want) return 1;

  char body[880];
  size_t bl = len - 5;
  if (bl >= sizeof body) return 2;
  memcpy(body, line, bl);
  body[bl] = 0;

  *m = LinkMsg();
  uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
  int rs = -127, c = 0, en = 0, at = 0;
  long mf = -1;
  int tg = 0;
  uint32_t fr = 0;
  char str[33] = "";
  const char* p = body;
  JKV kv;
  while (jsonNext(p, &kv)) {
    const char* k = kv.key;
    const char* v = kv.val;
    double x = kv.isStr ? 0 : atof(v);
    Drone& d = m->d;
    Heartbeat& h = m->hb;
    if (!strcmp(k, "k")) m->kind = v[0];
    else if (!strcmp(k, "mac")) {
      for (int i = 0; i < 6; i++) {
        int a = hexv(v[2 * i]), b = a < 0 ? -1 : hexv(v[2 * i + 1]);
        if (b < 0) return 2;
        mac[i] = (uint8_t)(a * 16 + b);
      }
    }
    else if (!strcmp(k, "s")) d.src = (uint8_t)x;
    else if (!strcmp(k, "fr")) fr = (uint32_t)x;
    else if (!strcmp(k, "rs")) rs = (int)x;
    else if (!strcmp(k, "id")) { protoCopy(d.id, sizeof d.id, v); d.hasId = true; }
    else if (!strcmp(k, "it")) d.idType = (uint8_t)x;
    else if (!strcmp(k, "ut")) d.uaType = (uint8_t)x;
    else if (!strcmp(k, "reg")) { protoCopy(d.reg, sizeof d.reg, v); d.hasReg = true; }
    else if (!strcmp(k, "st")) { d.status = (uint8_t)x; d.hasLoc = true; }
    else if (!strcmp(k, "tm")) d.odTs = (float)x;
    else if (!strcmp(k, "lat")) { d.lat = x; d.hasPos = true; }
    else if (!strcmp(k, "lon")) d.lon = x;
    else if (!strcmp(k, "ag")) d.altGeo = (float)x;
    else if (!strcmp(k, "ab")) d.altBaro = (float)x;
    else if (!strcmp(k, "h")) d.height = (float)x;
    else if (!strcmp(k, "sp")) d.spdH = (float)x;
    else if (!strcmp(k, "vs")) d.spdV = (float)x;
    else if (!strcmp(k, "dir")) d.dir = (float)x;
    else if (!strcmp(k, "ola")) { d.opLat = x; d.hasOp = true; }
    else if (!strcmp(k, "olo")) d.opLon = x;
    else if (!strcmp(k, "oid")) protoCopy(d.opId, sizeof d.opId, v);
    else if (!strcmp(k, "ds")) protoCopy(d.desc, sizeof d.desc, v);
    else if (!strcmp(k, "an")) protoCopy(d.ansi, sizeof d.ansi, v);
    else if (!strcmp(k, "ss") || !strcmp(k, "nm")) protoCopy(str, sizeof str, v);
    else if (!strcmp(k, "c")) c = (int)x;
    else if (!strcmp(k, "en")) en = (int)x;
    else if (!strcmp(k, "at")) at = (int)x;
    else if (!strcmp(k, "mf")) mf = (long)x;
    else if (!strcmp(k, "tg")) tg = (int)x;
    else if (!strcmp(k, "up")) h.up = (uint32_t)x;
    else if (!strcmp(k, "ble")) h.ble = (uint32_t)x;
    else if (!strcmp(k, "wifi")) h.wifi = (uint32_t)x;
    else if (!strcmp(k, "drop")) h.drop = (uint32_t)x;
    else if (!strcmp(k, "heap")) h.heap = (uint32_t)x;
    else if (!strcmp(k, "n")) h.tracked = (uint32_t)x;
    else if (!strcmp(k, "nw")) h.nets = (uint32_t)x;
    else if (!strcmp(k, "nb")) h.bts = (uint32_t)x;
    else if (!strcmp(k, "ch")) h.ch = (int)x;
    else if (!strcmp(k, "mode")) { h.mode = (int)x; m->cmd.mode = (int)x; }
    else if (!strcmp(k, "bo")) h.bleOn = x != 0;
    else if (!strcmp(k, "wo")) h.wifiOn = x != 0;
    else if (!strcmp(k, "fw")) protoCopy(h.fw, sizeof h.fw, v);
    else if (!strcmp(k, "wn")) m->cmd.wifiSurvey = x != 0;
    else if (!strcmp(k, "bn")) m->cmd.btSurvey = x != 0;
    else if (!strcmp(k, "chn")) m->cmd.chan = (int)x;
    else if (!strcmp(k, "rm")) m->cmd.rssiMin = (int)x;
    else if (!strcmp(k, "ant")) m->cmd.ant = (int)x;
    else if (!strcmp(k, "bd")) m->cmd.bleDuty = (int)x;
    else if (!strcmp(k, "ty")) m->sens.ty = (uint8_t)x;
    else if (!strcmp(k, "te")) m->sens.temp = (float)x;
    else if (!strcmp(k, "hu")) m->sens.hum = (float)x;
    else if (!strcmp(k, "lx")) m->sens.lux = (float)x;
    else if (!strcmp(k, "ro")) m->sens.rot = (float)x;
    else if (!strcmp(k, "pr")) m->sens.pres = (float)x;
    else if (!strcmp(k, "vo")) m->sens.volt = (int16_t)x;
    else if (!strcmp(k, "ba")) m->sens.batt = (int16_t)x;
    else if (!strcmp(k, "mo")) m->sens.moist = (int16_t)x;
    else if (!strcmp(k, "ec")) m->sens.cond = (int32_t)x;
    else if (!strcmp(k, "wi")) m->sens.win = (int8_t)x;
    else if (!strcmp(k, "do")) m->sens.door = (int8_t)x;
    else if (!strcmp(k, "mt")) m->sens.motion = (int8_t)x;
    else if (!strcmp(k, "bu")) m->sens.button = (int16_t)x;
  }
  switch (m->kind) {
    case 'd':
      memcpy(m->d.mac, mac, 6);
      m->d.frames = fr;
      m->d.rssi = (int8_t)rs;
      break;
    case 'w':
      memcpy(m->net.bssid, mac, 6);
      protoCopy(m->net.ssid, sizeof m->net.ssid, str);
      m->net.ch = (uint8_t)c;
      m->net.enc = (uint8_t)en;
      m->net.rssi = (int8_t)rs;
      m->net.frames = fr;
      break;
    case 'b':
      memcpy(m->bt.mac, mac, 6);
      protoCopy(m->bt.name, sizeof m->bt.name, str);
      m->bt.atype = (uint8_t)at;
      m->bt.mf = (int32_t)mf;
      m->bt.tag = (uint8_t)tg;
      m->bt.rssi = (int8_t)rs;
      m->bt.frames = fr;
      break;
    case 's':
      memcpy(m->sens.mac, mac, 6);
      protoCopy(m->sens.name, sizeof m->sens.name, str);
      m->sens.enc = (uint8_t)en;
      m->sens.rssi = (int8_t)rs;
      m->sens.frames = fr;
      break;
    case 'h':
    case 'c':
      break;
    default:
      return 2;
  }
  return 0;
}


// ---------------------------------------------------------------------------
//  Mise à jour OTA de l'ESP 1 relayée par l'ESP 2 (lignes « ~O… », CRC comme les autres lignes)
//    ESP 2 -> ESP 1 :  ~Ob,<taille>        début          ~Od,<n>,<base64>   morceau n (384 octets)       ~Oe   fin
//    ESP 1 -> ESP 2 :  ~Oa,<n>             morceau n reçu ~Or,<code>         résultat final (0 = réussi)
//  Codes d'erreur : 1 début impossible (place insuffisante), 2 écriture, 3 validation finale, 4 ordre inattendu
// ---------------------------------------------------------------------------
#define OTA_CHUNK 384

static inline size_t b64enc(const uint8_t* in, size_t n, char* out, size_t cap) {
  static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)in[i] << 16;
    if (i + 1 < n) v |= (uint32_t)in[i + 1] << 8;
    if (i + 2 < n) v |= in[i + 2];
    if (o + 4 >= cap) return 0;
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    out[o++] = i + 1 < n ? T[(v >> 6) & 63] : '=';
    out[o++] = i + 2 < n ? T[v & 63] : '=';
  }
  out[o] = 0;
  return o;
}

static inline int b64val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

// Retourne le nombre d'octets décodés, ou -1 si le texte est invalide
static inline int b64dec(const char* in, size_t n, uint8_t* out, size_t cap) {
  if (n % 4) return -1;
  size_t o = 0;
  for (size_t i = 0; i < n; i += 4) {
    int a = b64val(in[i]), b = b64val(in[i + 1]);
    int c = in[i + 2] == '=' ? -2 : b64val(in[i + 2]);
    int d = in[i + 3] == '=' ? -2 : b64val(in[i + 3]);
    if (a < 0 || b < 0 || c == -1 || d == -1 || (c == -2 && d != -2)) return -1;
    if (o + 3 > cap) return -1;
    uint32_t v = ((uint32_t)a << 18) | ((uint32_t)b << 12) | ((uint32_t)(c < 0 ? 0 : c) << 6) | (uint32_t)(d < 0 ? 0 : d);
    out[o++] = (uint8_t)(v >> 16);
    if (c != -2) out[o++] = (uint8_t)(v >> 8);
    if (d != -2) out[o++] = (uint8_t)v;
  }
  return (int)o;
}

static inline size_t otaLineBegin(uint32_t size, char* out, size_t cap) {
  int k = snprintf(out, cap, "~Ob,%lu", (unsigned long)size);
  return k > 0 ? protoSeal(out, cap, (size_t)k) : 0;
}
static inline size_t otaLineData(uint32_t idx, const uint8_t* d, size_t n, char* out, size_t cap) {
  int k = snprintf(out, cap, "~Od,%lu,", (unsigned long)idx);
  if (k <= 0 || (size_t)k >= cap) return 0;
  size_t e = b64enc(d, n, out + k, cap - (size_t)k);
  return e ? protoSeal(out, cap, (size_t)k + e) : 0;
}
static inline size_t otaLineEnd(char* out, size_t cap) {
  int k = snprintf(out, cap, "~Oe");
  return protoSeal(out, cap, (size_t)k);
}
static inline size_t otaLineAck(uint32_t idx, char* out, size_t cap) {
  int k = snprintf(out, cap, "~Oa,%lu", (unsigned long)idx);
  return protoSeal(out, cap, (size_t)k);
}
static inline size_t otaLineResult(int code, char* out, size_t cap) {
  int k = snprintf(out, cap, "~Or,%d", code);
  return protoSeal(out, cap, (size_t)k);
}

struct OtaMsg {
  char t = 0;                  // b début, d données, e fin, a accusé, r résultat
  uint32_t a = 0;              // taille (b), numéro (d, a) ou code (r)
  const char* b64 = nullptr;   // données encodées (d)
  size_t b64n = 0;
};

// Retourne 0 si correcte, 1 si CRC faux, 2 si illisible. La ligne doit rester valide pendant l'usage de msg.
static inline int otaParse(const char* line, size_t len, OtaMsg* m) {
  while (len && (line[len - 1] == '\r' || line[len - 1] == '\n')) len--;
  if (len < 8 || line[0] != '~' || line[1] != 'O' || line[len - 5] != '*') return 2;
  int h0 = hexv(line[len - 4]), h1 = hexv(line[len - 3]), h2 = hexv(line[len - 2]), h3 = hexv(line[len - 1]);
  if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) return 2;
  if (crc16(line, len - 5) != (uint16_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3)) return 1;
  *m = OtaMsg();
  m->t = line[2];
  const char* p = line + 3;
  const char* end = line + len - 5;
  if (m->t == 'e') return 0;
  if (p >= end || *p != ',') return 2;
  p++;
  m->a = (uint32_t)strtoul(p, nullptr, 10);
  if (m->t == 'd') {
    while (p < end && *p != ',') p++;
    if (p >= end) return 2;
    m->b64 = p + 1;
    m->b64n = (size_t)(end - (p + 1));
  } else if (m->t != 'b' && m->t != 'a' && m->t != 'r') {
    return 2;
  }
  return 0;
}

// Assemble les octets reçus en lignes. Ignore le bruit : une ligne trop longue est abandonnée.
struct LineReader {
  char buf[940];
  size_t n = 0;
  bool overflow = false;
  // Retourne true quand une ligne complète est disponible (line/len valides jusqu'au prochain feed)
  bool feed(uint8_t c, const char** line, size_t* len) {
    if (c == '\n') {
      bool good = !overflow && n > 0;
      size_t len0 = n;
      n = 0;
      overflow = false;
      if (!good) return false;
      // du bruit peut précéder la ligne (démarrage de la carte) : on repart du dernier « {"k": »
      size_t start = 0;
      for (size_t i = 0; i + 5 <= len0; i++)
        if (buf[i] == '{' && buf[i + 1] == '"' && buf[i + 2] == 'k' && buf[i + 3] == '"' && buf[i + 4] == ':') start = i;
      *line = buf + start;
      *len = len0 - start;
      return true;
    }
    if (n < sizeof buf) buf[n++] = (char)c; else overflow = true;
    return false;
  }
};
