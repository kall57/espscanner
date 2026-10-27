#pragma once
// ============================================================================
//  Journal des détections de drones.
//  Aucune dépendance à Arduino : ce fichier est testé sur PC (voir README).
//
//  Une ligne = une « observation » d'un drone : première et dernière détection,
//  dernière position connue, etc. Un drone revu moins de JOURNAL_GAP_SEC secondes
//  après sa dernière détection met à jour sa ligne ; au-delà, une nouvelle ligne est créée.
// ============================================================================
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

#ifndef JOURNAL_MAX
#define JOURNAL_MAX 100
#endif

#define JNL_UNKNOWN INT16_MIN
#define JNL_MAGIC 0x314A4E44u

enum { JF_POS = 1, JF_OP = 2 };

struct JEntry {
  uint32_t firstT, lastT;     // heure UTC (secondes depuis 1970), 0 = inconnue
  uint32_t firstUp, lastUp;   // secondes depuis le démarrage de la carte
  uint32_t frames;
  int32_t lat6, lon6, opLat6, opLon6;   // micro-degrés
  uint16_t boot;              // numéro de démarrage de la carte
  int16_t altGeo, height, speed10, dir;   // m, m, 0,1 m/s, degrés (JNL_UNKNOWN / -1 = inconnu)
  uint8_t mac[6];
  uint8_t src, status, uaType, flags;
  int8_t rssiLast, rssiBest;
  char id[33], reg[21], opId[21];
};

// Observation à fusionner dans le journal (NAN = valeur inconnue)
struct JObs {
  const char* id;
  const char* reg;
  const char* opId;
  uint8_t mac[6];
  uint8_t src, status, uaType;
  bool hasPos, hasOp;
  double lat, lon, opLat, opLon;
  float altGeo, height, speed, dir;
  int rssi;
};

struct JHeader {
  uint32_t magic;
  uint16_t count;
  uint16_t entrySize;
};

class Journal {
 public:
  JEntry e[JOURNAL_MAX];
  int n = 0;
  bool dirty = false;
  uint32_t rev = 0;   // incrémenté à chaque modification (la page web recharge si ça change)

  void clear() { n = 0; dirty = true; rev++; }

  static int32_t micro(double v) { return (int32_t)llround(v * 1e6); }
  static int16_t meters(float v) {
    if (isnan(v)) return JNL_UNKNOWN;
    if (v > 32000.0f) v = 32000.0f;
    if (v < -1000.0f) v = -1000.0f;
    return (int16_t)lroundf(v);
  }

  // Même drone ? Deux identifiants renseignés => on compare les identifiants (un drone vu en
  // Bluetooth et en Wi-Fi est ainsi fusionné). Sinon on compare l'adresse MAC.
  static bool sameDrone(const JEntry& x, const JObs& o) {
    bool xi = x.id[0] != 0, oi = o.id && o.id[0];
    if (xi && oi) return strcmp(x.id, o.id) == 0;
    return memcmp(x.mac, o.mac, 6) == 0;
  }

  static bool recent(const JEntry& x, uint32_t up, uint32_t epoch, uint16_t boot, uint32_t gap) {
    if (x.boot == boot) return up < x.lastUp || (up - x.lastUp) <= gap;
    if (epoch && x.lastT) return epoch < x.lastT || (epoch - x.lastT) <= gap;
    return false;
  }

  void update(const JObs& o, uint32_t up, uint32_t epoch, uint16_t boot, uint32_t gapSec) {
    int idx = -1;
    for (int i = n - 1; i >= 0; i--) {
      if (sameDrone(e[i], o) && recent(e[i], up, epoch, boot, gapSec)) { idx = i; break; }
    }
    if (idx < 0) {
      if (n >= JOURNAL_MAX) {   // plein : la plus ancienne ligne disparaît
        memmove(&e[0], &e[1], sizeof(JEntry) * (JOURNAL_MAX - 1));
        n = JOURNAL_MAX - 1;
      }
      idx = n++;
      JEntry& z = e[idx];
      memset(&z, 0, sizeof z);
      z.firstT = epoch;
      z.firstUp = up;
      z.boot = boot;
      z.altGeo = z.height = z.speed10 = JNL_UNKNOWN;
      z.dir = -1;
      z.rssiBest = -127;
      memcpy(z.mac, o.mac, 6);
    }
    JEntry& x = e[idx];
    x.lastT = epoch;
    x.lastUp = up;
    x.frames++;
    x.src |= o.src;
    x.rssiLast = (int8_t)o.rssi;
    if ((int8_t)o.rssi > x.rssiBest) x.rssiBest = (int8_t)o.rssi;
    if (o.uaType) x.uaType = o.uaType;
    if (o.status) x.status = o.status;
    if (o.id && o.id[0] && !x.id[0]) { strncpy(x.id, o.id, sizeof x.id - 1); x.id[sizeof x.id - 1] = 0; }
    if (o.reg && o.reg[0]) { strncpy(x.reg, o.reg, sizeof x.reg - 1); x.reg[sizeof x.reg - 1] = 0; }
    if (o.opId && o.opId[0]) { strncpy(x.opId, o.opId, sizeof x.opId - 1); x.opId[sizeof x.opId - 1] = 0; }
    if (o.hasPos) { x.lat6 = micro(o.lat); x.lon6 = micro(o.lon); x.flags |= JF_POS; }
    if (o.hasOp) { x.opLat6 = micro(o.opLat); x.opLon6 = micro(o.opLon); x.flags |= JF_OP; }
    if (!isnan(o.altGeo)) x.altGeo = meters(o.altGeo);
    if (!isnan(o.height)) x.height = meters(o.height);
    if (!isnan(o.speed)) {
      float s = o.speed * 10.0f;
      if (s < 0) s = 0;
      if (s > 2550) s = 2550;
      x.speed10 = (int16_t)lroundf(s);
    }
    if (!isnan(o.dir) && o.dir >= 0 && o.dir <= 360) x.dir = (int16_t)(lroundf(o.dir)) % 360;
    dirty = true;
    rev++;
  }

  // L'heure vient d'être connue : on date les lignes créées depuis le démarrage.
  bool fixTimes(uint32_t epochNow, uint32_t upNow, uint16_t boot) {
    bool changed = false;
    for (int i = 0; i < n; i++) {
      JEntry& x = e[i];
      if (x.boot != boot) continue;
      if (!x.firstT && upNow >= x.firstUp) { x.firstT = epochNow - (upNow - x.firstUp); changed = true; }
      if (!x.lastT && upNow >= x.lastUp) { x.lastT = epochNow - (upNow - x.lastUp); changed = true; }
    }
    if (changed) { dirty = true; rev++; }
    return changed;
  }

  // --- sauvegarde / chargement (le fichier est géré par l'appelant) ---
  static JHeader makeHeader(int count) {
    JHeader h;
    h.magic = JNL_MAGIC;
    h.count = (uint16_t)count;
    h.entrySize = (uint16_t)sizeof(JEntry);
    return h;
  }
  static bool acceptHeader(const JHeader& h) {
    return h.magic == JNL_MAGIC && h.entrySize == sizeof(JEntry) && h.count <= JOURNAL_MAX;
  }
  void loaded(int count) {   // après lecture des entrées brutes dans e[]
    n = count;
    for (int i = 0; i < n; i++) {   // garde-fou : chaînes toujours terminées
      e[i].id[sizeof e[i].id - 1] = 0;
      e[i].reg[sizeof e[i].reg - 1] = 0;
      e[i].opId[sizeof e[i].opId - 1] = 0;
    }
    dirty = false;
    rev++;
  }
};

// ----------------------------------------------------------------------------
//  Mise en forme (JSON / CSV)
// ----------------------------------------------------------------------------
struct JBuf {
  char* p;
  size_t cap, len;
  JBuf(char* b, size_t c) : p(b), cap(c), len(0) { if (cap) p[0] = 0; }
  void add(const char* fmt, ...) {
    if (len + 1 >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(p + len, cap - len, fmt, ap);
    va_end(ap);
    if (k < 0) return;
    if ((size_t)k >= cap - len) len = cap - 1; else len += (size_t)k;
  }
  void jstr(const char* s) {   // chaîne JSON échappée
    add("\"");
    for (; *s; s++) {
      unsigned char c = (unsigned char)*s;
      if (c == '"' || c == '\\') add("\\%c", c);
      else if (c < 0x20) add("\\u%04x", c);
      else add("%c", c);
    }
    add("\"");
  }
  void cstr(const char* s) {   // champ CSV entre guillemets
    add("\"");
    for (; *s; s++) {
      if (*s == '"') add("\"\"");
      else add("%c", *s);
    }
    add("\"");
  }
};

// Source(s) : 1 Bluetooth, 2/4 Wi-Fi (ASTM), 8 Wi-Fi format français, 16 simulation
static inline void srcText(uint8_t s, char* out, size_t cap) {
  char tmp[48];
  tmp[0] = 0;
  size_t n = 0;
  auto add = [&](const char* t) { n += (size_t)snprintf(tmp + n, sizeof tmp - n, "%s%s", n ? "+" : "", t); };
  if (s & 1) add("ble");
  if (s & 6) add("wifi");
  if (s & 8) add("wifi-fr");
  if (s & 16) add("test");
  if (!n) add("?");
  snprintf(out, cap, "%s", tmp);
}

static inline void isoTime(uint32_t t, char* out, size_t cap) {
  if (!t) { if (cap) out[0] = 0; return; }
  time_t tt = (time_t)t;
  struct tm tmv;
  gmtime_r(&tt, &tmv);
  strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

static inline void jnlMac(const uint8_t* m, char* out, size_t cap) {
  snprintf(out, cap, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static inline long jnlDuration(const JEntry& x) {
  if (x.firstT && x.lastT && x.lastT >= x.firstT) return (long)(x.lastT - x.firstT);
  return x.lastUp >= x.firstUp ? (long)(x.lastUp - x.firstUp) : 0L;
}

static inline size_t jnlJson(const JEntry& x, char* out, size_t cap) {
  JBuf b(out, cap);
  char src[32], mac[18];
  srcText(x.src, src, sizeof src);
  jnlMac(x.mac, mac, sizeof mac);
  b.add("{\"first\":%lu,\"last\":%lu,\"boot\":%u,\"dur\":%ld,\"id\":", (unsigned long)x.firstT,
        (unsigned long)x.lastT, (unsigned)x.boot, jnlDuration(x));
  b.jstr(x.id);
  if (x.reg[0]) { b.add(",\"reg\":"); b.jstr(x.reg); }
  b.add(",\"mac\":\"%s\",\"src\":\"%s\",\"status\":%u,\"ua_type\":%u", mac, src, (unsigned)x.status, (unsigned)x.uaType);
  if (x.flags & JF_POS) b.add(",\"lat\":%.6f,\"lon\":%.6f", x.lat6 / 1e6, x.lon6 / 1e6);
  if (x.flags & JF_OP) b.add(",\"op_lat\":%.6f,\"op_lon\":%.6f", x.opLat6 / 1e6, x.opLon6 / 1e6);
  if (x.altGeo != JNL_UNKNOWN) b.add(",\"alt_geo\":%d", (int)x.altGeo);
  if (x.height != JNL_UNKNOWN) b.add(",\"height\":%d", (int)x.height);
  if (x.speed10 != JNL_UNKNOWN) b.add(",\"speed\":%.1f", x.speed10 / 10.0);
  if (x.dir >= 0) b.add(",\"dir\":%d", (int)x.dir);
  b.add(",\"rssi\":%d,\"rssi_best\":%d,\"frames\":%lu", (int)x.rssiLast, (int)x.rssiBest, (unsigned long)x.frames);
  if (x.opId[0]) { b.add(",\"op_id\":"); b.jstr(x.opId); }
  b.add("}");
  return b.len;
}

static inline const char* jnlCsvHeader() {
  return "first_utc,last_utc,duration_s,id,registration,mac,source,status,ua_type,lat,lon,"
         "operator_lat,operator_lon,alt_geo_m,height_m,speed_ms,heading_deg,rssi_best,frames,operator_id\n";
}

static inline size_t jnlCsv(const JEntry& x, char* out, size_t cap) {
  JBuf b(out, cap);
  char src[32], mac[18], t1[24], t2[24];
  srcText(x.src, src, sizeof src);
  jnlMac(x.mac, mac, sizeof mac);
  isoTime(x.firstT, t1, sizeof t1);
  isoTime(x.lastT, t2, sizeof t2);
  b.add("%s,%s,%ld,", t1, t2, jnlDuration(x));
  b.cstr(x.id); b.add(",");
  b.cstr(x.reg); b.add(",%s,%s,%u,%u,", mac, src, (unsigned)x.status, (unsigned)x.uaType);
  if (x.flags & JF_POS) b.add("%.6f,%.6f,", x.lat6 / 1e6, x.lon6 / 1e6); else b.add(",,");
  if (x.flags & JF_OP) b.add("%.6f,%.6f,", x.opLat6 / 1e6, x.opLon6 / 1e6); else b.add(",,");
  if (x.altGeo != JNL_UNKNOWN) b.add("%d,", (int)x.altGeo); else b.add(",");
  if (x.height != JNL_UNKNOWN) b.add("%d,", (int)x.height); else b.add(",");
  if (x.speed10 != JNL_UNKNOWN) b.add("%.1f,", x.speed10 / 10.0); else b.add(",");
  if (x.dir >= 0) b.add("%d,", (int)x.dir); else b.add(",");
  b.add("%d,%lu,", (int)x.rssiBest, (unsigned long)x.frames);
  b.cstr(x.opId);
  b.add("\n");
  return b.len;
}
