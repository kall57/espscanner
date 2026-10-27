#pragma once
// ============================================================================
//  Décodage du signalement électronique français (Wi-Fi).
//  Trame « beacon » avec un élément vendeur (IE 221) : OUI 6A:5C:35, type 0x01,
//  suivi de champs TLV (1 octet type, 1 octet longueur, valeur) :
//    01 version   02 identifiant (30 octets)   03 identifiant ANSI/CTA-2063
//    04/05 latitude/longitude (int32 big-endian, 1e-5 degré)
//    06 altitude (int16, m)   07 hauteur (int16, m)
//    08/09 latitude/longitude du point de décollage (int32, 1e-5 degré)
//    0A vitesse horizontale (int8, m/s)   0B cap (int16, degrés)
//  Format déduit de la bibliothèque opendroneid-core-c (frdid_build).
//  Aucune dépendance à Arduino : testé sur PC.
// ============================================================================
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct FrData {
  char id[31];
  bool hasId;
  char ansi[48];
  bool hasAnsi;
  bool hasPos;
  double lat, lon;
  bool hasAlt;
  int altitude;
  bool hasHeight;
  int height;
  bool hasTakeoff;
  double toLat, toLon;
  bool hasSpeed;
  int speed;
  bool hasCourse;
  int course;
};

static inline int16_t frBe16(const uint8_t* p) { return (int16_t)(((unsigned)p[0] << 8) | p[1]); }
static inline int32_t frBe32(const uint8_t* p) {
  return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

static inline void frCopy(char* dst, size_t dstSize, const uint8_t* src, size_t srcLen) {
  size_t n = 0;
  for (size_t i = 0; i < srcLen && src[i] && n + 1 < dstSize; i++) {
    uint8_t c = src[i];
    dst[n++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
  }
  dst[n] = 0;
}

// b = suite de champs TLV (après le type d'élément vendeur). Retourne true si une donnée utile a été lue.
static inline bool parseFr(const uint8_t* b, size_t n, FrData* o) {
  memset(o, 0, sizeof *o);
  bool hasLat = false, hasLon = false, hasTLat = false, hasTLon = false;
  size_t i = 0;
  while (i + 2 <= n) {
    uint8_t t = b[i], l = b[i + 1];
    if (i + 2 + l > n) break;
    const uint8_t* v = b + i + 2;
    switch (t) {
      case 0x02:
        frCopy(o->id, sizeof o->id, v, l);
        o->hasId = o->id[0] != 0;
        break;
      case 0x03:
        frCopy(o->ansi, sizeof o->ansi, v, l);
        o->hasAnsi = o->ansi[0] != 0;
        break;
      case 0x04: if (l == 4) { o->lat = frBe32(v) / 1e5; hasLat = true; } break;
      case 0x05: if (l == 4) { o->lon = frBe32(v) / 1e5; hasLon = true; } break;
      case 0x06: if (l == 2) { o->altitude = frBe16(v); o->hasAlt = true; } break;
      case 0x07: if (l == 2) { o->height = frBe16(v); o->hasHeight = true; } break;
      case 0x08: if (l == 4) { o->toLat = frBe32(v) / 1e5; hasTLat = true; } break;
      case 0x09: if (l == 4) { o->toLon = frBe32(v) / 1e5; hasTLon = true; } break;
      case 0x0A: if (l == 1) { o->speed = (int8_t)v[0]; o->hasSpeed = true; } break;
      case 0x0B: if (l == 2) { o->course = frBe16(v); o->hasCourse = true; } break;
      default: break;   // version et champs inconnus ignorés
    }
    i += 2 + l;
  }
  o->hasPos = hasLat && hasLon;
  o->hasTakeoff = hasTLat && hasTLon;
  return o->hasId || o->hasAnsi || o->hasPos;
}
