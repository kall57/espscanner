// ============================================================================
//  ESP 2 — PASSERELLE : reçoit les drones de l'ESP 1 par liaison série, héberge la page web,
//  tient le journal (mémoire flash) et publie la liste des équipements détectés en MQTT.
//  Seeed XIAO ESP32-C6
//
//  Wi-Fi : l'ESP 2 rejoint VOTRE réseau. Son propre point d'accès ne s'ouvre que si ce réseau
//  est absent ou inaccessible, et se referme dès que le réseau est revenu (sauf client connecté).
//  Tâches : liaison série (uartTask), MQTT (mqttTask), page web + Wi-Fi + journal (loop).
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <LittleFS.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

#include "config.h"
#include "proto.h"
#include "web_ui_gz.h"
#include <WireGuard-ESP32.h>

// ----------------------------------------------------------------------------
//  Réglages (persistants)
// ----------------------------------------------------------------------------
struct Settings {
  // réseau
  String wifiSsid = DEFAULT_WIFI_SSID, wifiPass = DEFAULT_WIFI_PASS;
  String hostname = HOSTNAME;
  uint8_t apMode = 0;                    // 0 secours seulement, 1 toujours actif, 2 jamais
  String apName = "";                    // vide = AP_NAME
  String apPass = AP_PASS;
  uint16_t staGiveupSec = STA_GIVEUP_MS / 1000;
  // MQTT
  bool mqttEnabled = true;
  String mqttHost = DEFAULT_MQTT_HOST, mqttUser = DEFAULT_MQTT_USER, mqttPass = DEFAULT_MQTT_PASS, mqttBase = DEFAULT_MQTT_BASE;
  uint16_t mqttPort = DEFAULT_MQTT_PORT;
  bool mqttRetain = true;                // messages « liste » conservés par le broker
  uint16_t mqttDroneSec = MQTT_DRONE_MIN_MS / 1000;   // un message par drone au plus toutes les N secondes
  uint16_t mqttListSec = MQTT_LIST_EVERY_MS / 1000;   // listes complètes toutes les N secondes
  bool pubDrones = true, pubList = true, pubWifi = true, pubBt = true, pubSens = true;
  // ESP 1
  uint8_t mode = DEFAULT_SCAN_MODE;      // 0 BLE+Wi-Fi, 1 BLE, 2 Wi-Fi
  bool wifiSurvey = true, btSurvey = true;
  uint8_t chan = 0;                      // 0 = saut automatique
  int16_t rssiMin = DEFAULT_RSSI_MIN;
  uint8_t bleDuty = DEFAULT_BLE_DUTY;
  uint8_t antScanner = ANTENNA_EXTERNAL;
  // ESP 2
  uint8_t antGateway = ANTENNA_EXTERNAL;
  // listes et journal
  uint16_t expireMin = DEFAULT_EXPIRE_MIN;        // drones
  uint16_t expireNetMin = DEFAULT_EXPIRE_NET_MIN; // réseaux Wi-Fi et appareils Bluetooth
  uint16_t gapMin = JOURNAL_GAP_SEC / 60;
  uint16_t saveSec = JOURNAL_SAVE_EVERY_SEC;
  // alertes, Home Assistant, traces
  bool alertOn = true, alertMqtt = true, alertSyslog = true;
  bool haOn = true;
  String haPrefix = "homeassistant";
  bool trackOn = true;
  uint16_t trackMinM = 3;       // un point de trace tous les N mètres au moins
  uint16_t trackKeepMin = 60;   // une trace est oubliée N minutes après le dernier point
  // mise à jour depuis GitHub
  String updRepo = "kall57/espscanner", updToken = "";   // dépôt public : le jeton est inutile
  // VPN WireGuard (client)
  bool vpnOn = false, vpnFull = false;
  String vpnIp = "", vpnMask = "255.255.255.0", vpnPriv = "", vpnPeerPub = "", vpnHost = "", vpnPsk = "";
  uint16_t vpnPort = 51820;
  // syslog (UDP)
  bool syslogOn = false;
  String syslogHost = DEFAULT_SYSLOG_HOST;
  uint16_t syslogPort = DEFAULT_SYSLOG_PORT;
  uint8_t syslogFmt = 0;      // 0 RFC 5424, 1 RFC 3164
  uint8_t syslogBody = 0;     // 0 JSON, 1 texte clé=valeur
  uint8_t syslogFac = 16;     // 16..23 = local0..local7
  bool slDrones = true, slWifi = false, slBt = false, slSystem = true;
  uint16_t slDroneSec = 30;   // un même drone : une ligne au plus toutes les N secondes
  // interface
  String webUser = WEB_USER, webPass = WEB_PASS;
};
static Settings cfg;
static Preferences prefs;
static uint16_t bootId = 1;   // numéro de démarrage (sert à dater le journal)

// Les réglages sont lus par plusieurs tâches : on prend un instantané des chaînes MQTT sous verrou.
static SemaphoreHandle_t cfgMutex, dMutex, logMutex;

static void loadCfg() {
  prefs.begin("dgw", true);
  cfg.wifiSsid = prefs.getString("wssid", cfg.wifiSsid);
  cfg.wifiPass = prefs.getString("wpass", cfg.wifiPass);
  cfg.hostname = prefs.getString("host", cfg.hostname);
  cfg.apMode = prefs.getUChar("apmode", cfg.apMode);
  cfg.apName = prefs.getString("apname", cfg.apName);
  cfg.apPass = prefs.getString("appass", cfg.apPass);
  cfg.staGiveupSec = prefs.getUShort("giveup", cfg.staGiveupSec);
  cfg.mqttEnabled = prefs.getBool("mqen", cfg.mqttEnabled);
  cfg.mqttHost = prefs.getString("mqhost", cfg.mqttHost);
  cfg.mqttPort = prefs.getUShort("mqport", cfg.mqttPort);
  cfg.mqttUser = prefs.getString("mquser", cfg.mqttUser);
  cfg.mqttPass = prefs.getString("mqpass", cfg.mqttPass);
  cfg.mqttBase = prefs.getString("mqbase", cfg.mqttBase);
  cfg.mqttRetain = prefs.getBool("mqret", cfg.mqttRetain);
  cfg.mqttDroneSec = prefs.getUShort("mqdsec", cfg.mqttDroneSec);
  cfg.mqttListSec = prefs.getUShort("mqlsec", cfg.mqttListSec);
  cfg.pubDrones = prefs.getBool("pdr", cfg.pubDrones);
  cfg.pubList = prefs.getBool("plist", cfg.pubList);
  cfg.pubWifi = prefs.getBool("pwifi", cfg.pubWifi);
  cfg.pubBt = prefs.getBool("pbt", cfg.pubBt);
  cfg.pubSens = prefs.getBool("psn", cfg.pubSens);
  cfg.mode = prefs.getUChar("mode", cfg.mode);
  cfg.wifiSurvey = prefs.getBool("swifi", cfg.wifiSurvey);
  cfg.btSurvey = prefs.getBool("sbt", cfg.btSurvey);
  cfg.chan = prefs.getUChar("chan", cfg.chan);
  cfg.rssiMin = prefs.getShort("rssimin", cfg.rssiMin);
  cfg.bleDuty = prefs.getUChar("bleduty", cfg.bleDuty);
  cfg.antScanner = prefs.getUChar("ant1", cfg.antScanner);
  cfg.antGateway = prefs.getUChar("ant2", cfg.antGateway);
  cfg.expireMin = prefs.getUShort("exp", cfg.expireMin);
  cfg.expireNetMin = prefs.getUShort("expnet", cfg.expireNetMin);
  cfg.gapMin = prefs.getUShort("gap", cfg.gapMin);
  cfg.saveSec = prefs.getUShort("savesec", cfg.saveSec);
  cfg.alertOn = prefs.getBool("alon", cfg.alertOn);
  cfg.alertMqtt = prefs.getBool("almq", cfg.alertMqtt);
  cfg.alertSyslog = prefs.getBool("alsl", cfg.alertSyslog);
  cfg.haOn = prefs.getBool("haon", cfg.haOn);
  cfg.haPrefix = prefs.getString("hapfx", cfg.haPrefix);
  cfg.trackOn = prefs.getBool("tron", cfg.trackOn);
  cfg.trackMinM = prefs.getUShort("trmin", cfg.trackMinM);
  cfg.trackKeepMin = prefs.getUShort("trkeep", cfg.trackKeepMin);
  cfg.updRepo = prefs.getString("urepo", cfg.updRepo);
  cfg.updToken = prefs.getString("utok", cfg.updToken);
  cfg.vpnOn = prefs.getBool("vpon", cfg.vpnOn);
  cfg.vpnFull = prefs.getBool("vpfull", cfg.vpnFull);
  cfg.vpnIp = prefs.getString("vpip", cfg.vpnIp);
  cfg.vpnMask = prefs.getString("vpmask", cfg.vpnMask);
  cfg.vpnPriv = prefs.getString("vppriv", cfg.vpnPriv);
  cfg.vpnPeerPub = prefs.getString("vppub", cfg.vpnPeerPub);
  cfg.vpnHost = prefs.getString("vphost", cfg.vpnHost);
  cfg.vpnPort = prefs.getUShort("vpport", cfg.vpnPort);
  cfg.vpnPsk = prefs.getString("vppsk", cfg.vpnPsk);
  cfg.syslogOn = prefs.getBool("slon", cfg.syslogOn);
  cfg.syslogHost = prefs.getString("slhost", cfg.syslogHost);
  cfg.syslogPort = prefs.getUShort("slport", cfg.syslogPort);
  cfg.syslogFmt = prefs.getUChar("slfmt", cfg.syslogFmt);
  cfg.syslogBody = prefs.getUChar("slbody", cfg.syslogBody);
  cfg.syslogFac = prefs.getUChar("slfac", cfg.syslogFac);
  cfg.slDrones = prefs.getBool("sldr", cfg.slDrones);
  cfg.slWifi = prefs.getBool("slwifi", cfg.slWifi);
  cfg.slBt = prefs.getBool("slbt", cfg.slBt);
  cfg.slSystem = prefs.getBool("slsys", cfg.slSystem);
  cfg.slDroneSec = prefs.getUShort("sldsec", cfg.slDroneSec);
  cfg.webUser = prefs.getString("wuser", cfg.webUser);
  cfg.webPass = prefs.getString("wpw", cfg.webPass);
  prefs.end();
}

static void saveCfg() {
  prefs.begin("dgw", false);
  prefs.putString("wssid", cfg.wifiSsid);
  prefs.putString("wpass", cfg.wifiPass);
  prefs.putString("host", cfg.hostname);
  prefs.putUChar("apmode", cfg.apMode);
  prefs.putString("apname", cfg.apName);
  prefs.putString("appass", cfg.apPass);
  prefs.putUShort("giveup", cfg.staGiveupSec);
  prefs.putBool("mqen", cfg.mqttEnabled);
  prefs.putString("mqhost", cfg.mqttHost);
  prefs.putUShort("mqport", cfg.mqttPort);
  prefs.putString("mquser", cfg.mqttUser);
  prefs.putString("mqpass", cfg.mqttPass);
  prefs.putString("mqbase", cfg.mqttBase);
  prefs.putBool("mqret", cfg.mqttRetain);
  prefs.putUShort("mqdsec", cfg.mqttDroneSec);
  prefs.putUShort("mqlsec", cfg.mqttListSec);
  prefs.putBool("pdr", cfg.pubDrones);
  prefs.putBool("plist", cfg.pubList);
  prefs.putBool("pwifi", cfg.pubWifi);
  prefs.putBool("pbt", cfg.pubBt);
  prefs.putBool("psn", cfg.pubSens);
  prefs.putUChar("mode", cfg.mode);
  prefs.putBool("swifi", cfg.wifiSurvey);
  prefs.putBool("sbt", cfg.btSurvey);
  prefs.putUChar("chan", cfg.chan);
  prefs.putShort("rssimin", cfg.rssiMin);
  prefs.putUChar("bleduty", cfg.bleDuty);
  prefs.putUChar("ant1", cfg.antScanner);
  prefs.putUChar("ant2", cfg.antGateway);
  prefs.putUShort("exp", cfg.expireMin);
  prefs.putUShort("expnet", cfg.expireNetMin);
  prefs.putUShort("gap", cfg.gapMin);
  prefs.putUShort("savesec", cfg.saveSec);
  prefs.putBool("alon", cfg.alertOn);
  prefs.putBool("almq", cfg.alertMqtt);
  prefs.putBool("alsl", cfg.alertSyslog);
  prefs.putBool("haon", cfg.haOn);
  prefs.putString("hapfx", cfg.haPrefix);
  prefs.putBool("tron", cfg.trackOn);
  prefs.putUShort("trmin", cfg.trackMinM);
  prefs.putUShort("trkeep", cfg.trackKeepMin);
  prefs.putString("urepo", cfg.updRepo);
  prefs.putString("utok", cfg.updToken);
  prefs.putBool("vpon", cfg.vpnOn);
  prefs.putBool("vpfull", cfg.vpnFull);
  prefs.putString("vpip", cfg.vpnIp);
  prefs.putString("vpmask", cfg.vpnMask);
  prefs.putString("vppriv", cfg.vpnPriv);
  prefs.putString("vppub", cfg.vpnPeerPub);
  prefs.putString("vphost", cfg.vpnHost);
  prefs.putUShort("vpport", cfg.vpnPort);
  prefs.putString("vppsk", cfg.vpnPsk);
  prefs.putBool("slon", cfg.syslogOn);
  prefs.putString("slhost", cfg.syslogHost);
  prefs.putUShort("slport", cfg.syslogPort);
  prefs.putUChar("slfmt", cfg.syslogFmt);
  prefs.putUChar("slbody", cfg.syslogBody);
  prefs.putUChar("slfac", cfg.syslogFac);
  prefs.putBool("sldr", cfg.slDrones);
  prefs.putBool("slwifi", cfg.slWifi);
  prefs.putBool("slbt", cfg.slBt);
  prefs.putBool("slsys", cfg.slSystem);
  prefs.putUShort("sldsec", cfg.slDroneSec);
  prefs.putString("wuser", cfg.webUser);
  prefs.putString("wpw", cfg.webPass);
  prefs.end();
}

static void nextBoot() {
  prefs.begin("dgw", false);
  bootId = (uint16_t)(prefs.getUShort("boot", 0) + 1);
  if (bootId == 0) bootId = 1;
  prefs.putUShort("boot", bootId);
  prefs.end();
}

// ----------------------------------------------------------------------------
//  Journal technique (RAM) affiché dans la page web ; utilisable depuis toutes les tâches
// ----------------------------------------------------------------------------
#define LOG_LINES 50
#define LOG_LEN 130
static char logBuf[LOG_LINES][LOG_LEN];
static uint32_t logSeq = 0;
static char mqttErr[200] = "";   // protégé par logMutex
// Lignes du journal technique à transmettre en syslog (anneau de 16, protégé par logMutex)
static char sysQ[16][LOG_LEN];
static uint32_t sysQHead = 0, sysQTail = 0;

static void dlog(const char* fmt, ...) {
  char msg[LOG_LEN - 12];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  xSemaphoreTake(logMutex, portMAX_DELAY);
  char* line = logBuf[logSeq % LOG_LINES];
  snprintf(line, LOG_LEN, "[%5lu s] %s", (unsigned long)(millis() / 1000), msg);
  Serial.println(line);
  logSeq++;
  if (cfg.syslogOn && cfg.slSystem) strlcpy(sysQ[sysQHead++ % 16], msg, LOG_LEN);
  xSemaphoreGive(logMutex);
}

static void setMqttErr(const String& s) {
  xSemaphoreTake(logMutex, portMAX_DELAY);
  strlcpy(mqttErr, s.c_str(), sizeof mqttErr);
  xSemaphoreGive(logMutex);
}
static String getMqttErr() {
  xSemaphoreTake(logMutex, portMAX_DELAY);
  String s(mqttErr);
  xSemaphoreGive(logMutex);
  return s;
}

static bool timeSynced() { return time(nullptr) > 1700000000; }

// ----------------------------------------------------------------------------
//  Table des drones (vue « en direct »), journal des détections (persistant), état du scanner
// ----------------------------------------------------------------------------
static Drone drones[MAX_DRONES];
static Journal journal;
static bool fsOk = false, timeFixed = false;

static Net nets[MAX_NETS];
static Bt bts[MAX_BT];
static Sens sens[MAX_SENS];

// Drones disparus (pour le syslog), protégé par dMutex
static char lostQ[8][2][41];   // [i][0] identifiant, [i][1] adresse
static uint32_t lostHead = 0, lostTail = 0;

// Traces de vol (RAM, protégées par dMutex) : anneau de points par drone
#define MAX_TRACKS 12
#define TRACK_PTS 120
struct TrackPt {
  int32_t lat6, lon6;   // micro-degrés
  int16_t alt;          // mètres (altitude géométrique, sinon hauteur), -32768 = inconnue
  uint32_t up;          // secondes depuis le démarrage
};
struct Track {
  bool used = false;
  uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
  char id[41] = "";
  TrackPt* pts = nullptr;
  uint16_t n = 0, head = 0;
  uint32_t lastMs = 0;
};
static Track tracks[MAX_TRACKS];

static Heartbeat scHb;                       // dernier battement de cœur de l'ESP 1 (dMutex)
static uint32_t scHbMs = 0;                  // millis() de sa réception (0 = jamais)
static uint32_t linkLines = 0, linkBad = 0;  // lignes valides / rejetées (dMutex)
static volatile bool mqttForce = false;      // demande de publication immédiate

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

// Reporte l'état courant d'un drone dans le journal (dMutex pris par l'appelant)
static void journalTouch(const Drone& d) {
  JObs o;
  memset(&o, 0, sizeof o);
  o.id = d.hasId ? d.id : "";
  o.reg = d.hasReg ? d.reg : "";
  o.opId = d.opId;
  memcpy(o.mac, d.mac, 6);
  o.src = d.src;
  o.status = d.status;
  o.uaType = d.uaType;
  o.hasPos = d.hasPos;
  o.lat = d.lat;
  o.lon = d.lon;
  o.hasOp = d.hasOp;
  o.opLat = d.opLat;
  o.opLon = d.opLon;
  o.altGeo = !isnan(d.altGeo) ? d.altGeo : d.altBaro;
  o.height = d.height;
  o.speed = d.spdH;
  o.dir = d.dir;
  o.rssi = d.rssi;
  journal.update(o, millis() / 1000, timeSynced() ? (uint32_t)time(nullptr) : 0, bootId, (uint32_t)cfg.gapMin * 60);
}

// Ajoute un point à la trace du drone si la position a changé (dMutex pris par l'appelant)
static void trackAdd(const Drone& d) {
  if (!cfg.trackOn || !d.hasPos) return;
  Track* t = nullptr;
  Track* oldest = nullptr;
  Track* freeSlot = nullptr;
  for (auto& x : tracks) {
    if (!x.used) { if (!freeSlot) freeSlot = &x; continue; }
    if (memcmp(x.mac, d.mac, 6) == 0) { t = &x; break; }
    if (!oldest || x.lastMs < oldest->lastMs) oldest = &x;
  }
  if (!t) {
    t = freeSlot ? freeSlot : oldest;
    if (!t) return;
    if (t->pts) { free(t->pts); t->pts = nullptr; }
    *t = Track();
    t->pts = (TrackPt*)malloc(sizeof(TrackPt) * TRACK_PTS);
    if (!t->pts) return;
    t->used = true;
    memcpy(t->mac, d.mac, 6);
  }
  if (d.hasId) strlcpy(t->id, d.id, sizeof t->id);
  int32_t la = (int32_t)lround(d.lat * 1e6), lo = (int32_t)lround(d.lon * 1e6);
  uint32_t up = millis() / 1000;
  if (t->n) {
    const TrackPt& l = t->pts[(t->head + TRACK_PTS - 1) % TRACK_PTS];
    float dy = (float)(la - l.lat6) * 0.11132f;
    float dx = (float)(lo - l.lon6) * 0.11132f * cosf((float)d.lat * 0.0174533f);
    float minM = (float)cfg.trackMinM;
    if (dx * dx + dy * dy < minM * minM && up - l.up < 30) { t->lastMs = millis(); return; }
  }
  TrackPt& p = t->pts[t->head];
  p.lat6 = la;
  p.lon6 = lo;
  float a = !isnan(d.altGeo) ? d.altGeo : (!isnan(d.altBaro) ? d.altBaro : d.height);
  p.alt = isnan(a) ? (int16_t)-32768 : (int16_t)constrain((long)lroundf(a), -32767L, 32767L);
  p.up = up;
  t->head = (t->head + 1) % TRACK_PTS;
  if (t->n < TRACK_PTS) t->n++;
  t->lastMs = millis();
}

// Intègre un drone reçu de l'ESP 1 (dMutex pris par l'appelant)
static void ingestDrone(const Drone& in) {
  Drone* d = getDrone(in.mac);
  if (!d) return;
  uint32_t first = d->firstMs, lms = d->logMs;
  bool alertKeep = d->alertSeen, alertKeepSl = d->alertSl, emKeep = d->alertEm, emKeepSl = d->alertEmSl;
  bool lseen = d->logSeen && d->used && memcmp(d->mac, in.mac, 6) == 0;
  *d = in;
  d->used = true;
  d->firstMs = first;
  d->logMs = lseen ? lms : 0;
  d->logSeen = lseen;
  d->alertSeen = alertKeep;
  d->alertSl = alertKeepSl;
  d->alertEm = in.status >= 3 && emKeep;        // réarmé quand l'urgence cesse
  d->alertEmSl = in.status >= 3 && emKeepSl;
  d->lastMs = millis();
  d->dirty = true;
  journalTouch(*d);
  trackAdd(*d);
}

// Réseaux Wi-Fi et appareils Bluetooth reçus de l'ESP 1 (dMutex pris par l'appelant)
static void ingestNet(const Net& in) {
  Net* slot = nullptr;
  Net* oldest = nullptr;
  for (auto& n : nets) {
    if (!n.used) { if (!slot) slot = &n; continue; }
    if (memcmp(n.bssid, in.bssid, 6) == 0) { slot = &n; oldest = nullptr; break; }
    if (!oldest || n.lastMs < oldest->lastMs) oldest = &n;
  }
  if (!slot) slot = oldest;
  if (!slot) return;
  bool sameNet = slot->used && memcmp(slot->bssid, in.bssid, 6) == 0;
  uint32_t first = sameNet ? slot->firstMs : millis();
  bool wasLogged = sameNet && slot->logged;
  *slot = in;
  slot->logged = wasLogged;
  slot->used = true;
  slot->firstMs = first;
  slot->lastMs = millis();
  slot->dirty = true;
}

static void ingestBt(const Bt& in) {
  Bt* slot = nullptr;
  Bt* oldest = nullptr;
  for (auto& d : bts) {
    if (!d.used) { if (!slot) slot = &d; continue; }
    if (memcmp(d.mac, in.mac, 6) == 0) { slot = &d; oldest = nullptr; break; }
    if (!oldest || d.lastMs < oldest->lastMs) oldest = &d;
  }
  if (!slot) slot = oldest;
  if (!slot) return;
  bool same = slot->used && memcmp(slot->mac, in.mac, 6) == 0;
  uint32_t first = same ? slot->firstMs : millis();
  char keepName[33];
  strlcpy(keepName, same ? slot->name : "", sizeof keepName);
  int32_t keepMf = same ? slot->mf : -1;
  bool wasLogged = same && slot->logged;
  *slot = in;
  slot->logged = wasLogged;
  if (!slot->name[0]) strlcpy(slot->name, keepName, sizeof slot->name);   // un nom déjà connu n'est pas oublié
  if (slot->mf < 0) slot->mf = keepMf;
  slot->used = true;
  slot->firstMs = first;
  slot->lastMs = millis();
  slot->dirty = true;
}

// Capteurs Bluetooth (BTHome, Flora) : l'ESP 1 envoie l'état fusionné (dMutex pris par l'appelant)
static void ingestSens(const Sens& in) {
  Sens* slot = nullptr;
  Sens* oldest = nullptr;
  for (auto& x : sens) {
    if (!x.used) { if (!slot) slot = &x; continue; }
    if (memcmp(x.mac, in.mac, 6) == 0) { slot = &x; oldest = nullptr; break; }
    if (!oldest || x.lastMs < oldest->lastMs) oldest = &x;
  }
  if (!slot) slot = oldest;
  if (!slot) return;
  bool same = slot->used && memcmp(slot->mac, in.mac, 6) == 0;
  uint32_t first = same ? slot->firstMs : millis();
  uint16_t ha = same ? slot->haMask : 0;
  *slot = in;
  slot->used = true;
  slot->firstMs = first;
  slot->haMask = ha;
  slot->lastMs = millis();
  slot->dirty = true;
}

// ----------------------------------------------------------------------------
//  Persistance du journal (LittleFS). L'écriture se fait HORS verrou, sur une copie.
// ----------------------------------------------------------------------------
static bool saveJournal(bool force = false) {
  if (!fsOk) return false;
  JEntry* copy = nullptr;
  int n = 0;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  bool need = force || journal.dirty;
  if (need) {
    n = journal.n;
    if (n > 0) {
      copy = (JEntry*)malloc(sizeof(JEntry) * (size_t)n);
      if (copy) memcpy(copy, journal.e, sizeof(JEntry) * (size_t)n);
      else need = false;
    }
    if (need) journal.dirty = false;
  }
  xSemaphoreGive(dMutex);
  if (!need) return true;

  bool ok = true;
  File f = LittleFS.open("/journal.tmp", "w");
  ok = (bool)f;
  if (ok) {
    JHeader h = Journal::makeHeader(n);
    ok = f.write((const uint8_t*)&h, sizeof h) == sizeof h;
    size_t bytes = sizeof(JEntry) * (size_t)n;
    if (ok && bytes) ok = f.write((const uint8_t*)copy, bytes) == bytes;
    f.close();
  }
  if (ok) {
    LittleFS.remove("/journal.bin");
    ok = LittleFS.rename("/journal.tmp", "/journal.bin");
  }
  free(copy);
  if (!ok) {
    xSemaphoreTake(dMutex, portMAX_DELAY);
    journal.dirty = true;   // on réessaiera
    xSemaphoreGive(dMutex);
  }
  dlog(ok ? "Journal enregistré en flash (%d ligne(s))" : "ERREUR : écriture du journal impossible (%d ligne(s))", n);
  return ok;
}

static void loadJournal() {
  if (!fsOk) return;
  const char* path = LittleFS.exists("/journal.bin") ? "/journal.bin" : (LittleFS.exists("/journal.tmp") ? "/journal.tmp" : nullptr);
  if (!path) return;
  File f = LittleFS.open(path, "r");
  JHeader h;
  if (!f || f.read((uint8_t*)&h, sizeof h) != sizeof h || !Journal::acceptHeader(h)) {
    dlog("Journal en flash illisible ou d'une autre version : ignoré");
    return;
  }
  size_t bytes = sizeof(JEntry) * (size_t)h.count;
  if (f.size() != sizeof h + bytes || (bytes && f.read((uint8_t*)journal.e, bytes) != bytes)) {
    dlog("Journal en flash tronqué : ignoré");
    return;
  }
  journal.loaded(h.count);
  dlog("Journal rechargé : %d ligne(s)", journal.n);
}

// L'heure vient d'être connue (NTP ou navigateur) : on date les lignes créées depuis le démarrage
static void applyTimeFix() {
  if (!timeSynced()) return;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  bool changed = journal.fixTimes((uint32_t)time(nullptr), millis() / 1000, bootId);
  xSemaphoreGive(dMutex);
  timeFixed = true;
  if (changed) dlog("Heure connue : journal daté");
}

// ----------------------------------------------------------------------------
//  Liaison série avec l'ESP 1
// ----------------------------------------------------------------------------
// État de la mise à jour OTA de l'ESP 1 (relayée par la liaison série)
static volatile bool otaRelay = false;                 // true : uartTask n'envoie plus de commande
static volatile uint32_t otaAckIdx = 0xFFFFFFFEu;      // dernier accusé reçu (0xFFFFFFFF = début accepté)
static volatile int otaResultCode = -1;                // résultat final de l'ESP 1 (0 = succès)
static const char GW_FW[] = FW_VERSION " " __DATE__ " " __TIME__;

static void uartTask(void*) {
  static LineReader rd;
  static LinkMsg m;
  uint32_t lastCmd = 0;
  for (;;) {
    int avail = Serial1.available();
    if (avail <= 0) {
      uint32_t now = millis();
      if (!otaRelay && now - lastCmd >= 5000) {   // l'ESP 1 reçoit régulièrement le mode d'écoute voulu
        lastCmd = now;
        char buf[160];
        ScanCmd c;
        c.mode = cfg.mode;
        c.wifiSurvey = cfg.wifiSurvey;
        c.btSurvey = cfg.btSurvey;
        c.chan = cfg.chan;
        c.rssiMin = cfg.rssiMin;
        c.ant = cfg.antScanner;
        c.bleDuty = cfg.bleDuty;
        size_t n = protoCommand(c, buf, sizeof buf);
        if (n && Serial1.availableForWrite() >= (int)n) Serial1.write((const uint8_t*)buf, n);
      }
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    while (avail-- > 0) {
      const char* line;
      size_t len;
      if (!rd.feed((uint8_t)Serial1.read(), &line, &len)) continue;
      if (len > 1 && line[0] == '~') {   // réponse de mise à jour OTA
        static OtaMsg om;
        if (otaParse(line, len, &om) == 0) {
          if (om.t == 'a') otaAckIdx = om.a;
          else if (om.t == 'r') otaResultCode = (int)om.a;
        }
        continue;
      }
      int r = protoParse(line, len, &m);
      xSemaphoreTake(dMutex, portMAX_DELAY);
      if (r != 0) {
        linkBad++;
      } else if (m.kind == 'd') {
        linkLines++;
        ingestDrone(m.d);
      } else if (m.kind == 'w') {
        linkLines++;
        ingestNet(m.net);
      } else if (m.kind == 'b') {
        linkLines++;
        ingestBt(m.bt);
      } else if (m.kind == 's') {
        linkLines++;
        ingestSens(m.sens);
      } else if (m.kind == 'h') {
        linkLines++;
        scHb = m.hb;
        scHbMs = millis() ? millis() : 1;
      }
      xSemaphoreGive(dMutex);
    }
  }
}

static bool scannerAlive() {   // dMutex pris par l'appelant ou lecture tolérante
  return scHbMs != 0 && millis() - scHbMs < 6000;
}

// ----------------------------------------------------------------------------
//  Wi-Fi : réseau de l'utilisateur d'abord, point d'accès de secours seulement si nécessaire
// ----------------------------------------------------------------------------
static WebServer server(80);
static String apName, apPassNow;
static bool apOn = false, mdnsOn = false, ntpOn = false, staLogged = false;
static uint32_t wifiTryMs = 0, staUpMs = 0, staDownMs = 0;

static const char* wifiStatusText(int s) {
  switch (s) {
    case WL_CONNECTED: return "connecté";
    case WL_NO_SSID_AVAIL: return "réseau introuvable (nom incorrect, hors de portée ou en 5 GHz uniquement)";
    case WL_CONNECT_FAILED: return "connexion refusée (mot de passe probablement incorrect)";
    case WL_CONNECTION_LOST: return "connexion perdue";
    case WL_DISCONNECTED: return "non connecté";
    case WL_IDLE_STATUS: return "connexion en cours";
    default: return "état inconnu";
  }
}

static void apStart() {
  if (apOn) return;
  WiFi.mode(WIFI_AP_STA);
  apPassNow = cfg.apPass.length() >= 8 ? cfg.apPass : String(AP_PASS);
  WiFi.softAP(apName.c_str(), apPassNow.c_str());
  apOn = true;
  if (cfg.apMode == 1) dlog("Point d'accès « %s » ouvert (mode « toujours actif », http://%s)", apName.c_str(), WiFi.softAPIP().toString().c_str());
  else dlog("Réseau « %s » indisponible : point d'accès de secours « %s » ouvert (http://%s)", cfg.wifiSsid.c_str(), apName.c_str(),
            WiFi.softAPIP().toString().c_str());
}

static void apStop() {
  if (!apOn) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apOn = false;
  dlog("Réseau « %s » revenu : point d'accès de secours fermé", cfg.wifiSsid.c_str());
}

static void wifiBegin() {
  WiFi.disconnect(false);
  if (cfg.wifiSsid.length()) WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
  wifiTryMs = millis();
  staLogged = false;
}

static void wifiTick(uint32_t now) {
  bool up = WiFi.status() == WL_CONNECTED;
  if (up) {
    staDownMs = 0;
    if (!staUpMs) staUpMs = now ? now : 1;
    if (!staLogged) {
      staLogged = true;
      dlog("Wi-Fi connecté à « %s » : %s (%d dBm)", cfg.wifiSsid.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    if (!mdnsOn && MDNS.begin(cfg.hostname.c_str())) {
      MDNS.addService("http", "tcp", 80);
      mdnsOn = true;
    }
    if (!ntpOn) {
      configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
      ntpOn = true;
    }
    if (!timeFixed && timeSynced()) applyTimeFix();
    // le réseau est là depuis 5 s et personne n'est sur le point d'accès : on le ferme
    if (cfg.apMode == 1 && !apOn) apStart();
    if (cfg.apMode == 2 && apOn) apStop();
    if (cfg.apMode == 0 && apOn && now - staUpMs > 5000 && WiFi.softAPgetStationNum() == 0) apStop();
  } else {
    staUpMs = 0;
    if (!staDownMs) staDownMs = now ? now : 1;
    if (staLogged) {   // la connexion vient d'être perdue
      staLogged = false;
      ntpOn = false;
      dlog("Wi-Fi « %s » perdu (%s)", cfg.wifiSsid.c_str(), wifiStatusText(WiFi.status()));
    }
    bool noNet = cfg.wifiSsid.length() == 0;
    uint32_t giveup = (uint32_t)cfg.staGiveupSec * 1000UL;
    if (cfg.apMode == 2 && apOn) apStop();
    // mode « jamais » : le point d'accès ne s'ouvre pas, même sans réseau (l'interface n'est alors joignable que par le Wi-Fi de la maison)
    if (!apOn && cfg.apMode != 2 && (cfg.apMode == 1 || ((noNet || now - staDownMs > giveup) && now - wifiTryMs > giveup))) apStart();
    if (!noNet && now - wifiTryMs > 30000) {   // on retente régulièrement le réseau de l'utilisateur
      wifiTryMs = now;
      WiFi.reconnect();
    }
  }
}

// ----------------------------------------------------------------------------
//  VPN WireGuard (client) : seul le trafic du réseau du tunnel passe par le VPN,
//  l'heure (NTP), GitHub et le MQTT local restent sur le Wi-Fi (sauf option « tout le trafic »)
// ----------------------------------------------------------------------------
static WireGuard wg;
static volatile uint8_t vpnSt = 0;   // 0 désactivé, 1 attente, 2 démarrage, 3 poignée de main, 4 connecté, 5 erreur
static String vpnMsg;
static volatile bool vpnBusy = false;
static bool vpnRestart = false;
static uint32_t vpnRetryMs = 0, vpnStartMs = 0;
struct VpnJob { String ip, mask, priv, pub, host, psk; uint16_t port; bool full; bool ok; String err; };
static VpnJob vpnJob;

static bool vpnKeyOk(const String& k) {   // clé WireGuard : 32 octets en base64 = 44 caractères terminés par « = »
  if (k.length() != 44 || k[43] != '=') return false;
  for (int i = 0; i < 43; i++) {
    char c = k[i];
    if (!(isalnum((unsigned char)c) || c == '+' || c == '/')) return false;
  }
  return true;
}
static bool vpnConfigured() {
  IPAddress a, m;
  return cfg.vpnOn && cfg.vpnHost.length() && vpnKeyOk(cfg.vpnPriv) && vpnKeyOk(cfg.vpnPeerPub) && (!cfg.vpnPsk.length() || vpnKeyOk(cfg.vpnPsk)) && a.fromString(cfg.vpnIp) && m.fromString(cfg.vpnMask);
}

static void vpnTask(void*) {   // wg.begin() peut attendre le DNS plusieurs secondes : hors de la boucle principale
  IPAddress ip, mask, gw(0, 0, 0, 0);
  ip.fromString(vpnJob.ip);
  mask.fromString(vpnJob.mask);
  vpnJob.ok = wg.begin(ip, mask, gw, vpnJob.priv.c_str(), vpnJob.host.c_str(), vpnJob.pub.c_str(), vpnJob.port, vpnJob.full, vpnJob.psk.c_str());
  if (!vpnJob.ok) vpnJob.err = "serveur « " + vpnJob.host + " » introuvable (DNS) ou interface refusée";
  vpnJob.priv = "";
  vpnJob.psk = "";
  vpnBusy = false;
  vTaskDelete(NULL);
}

static void vpnTick(uint32_t now) {
  if (vpnBusy) { vpnSt = 2; return; }
  if (vpnSt == 2) {   // la tâche de démarrage vient de se terminer
    if (vpnJob.ok) { vpnSt = 3; vpnStartMs = now; vpnMsg = "poignée de main en cours"; dlog("VPN : tunnel démarré vers %s:%u (%s)", cfg.vpnHost.c_str(), cfg.vpnPort, cfg.vpnIp.c_str()); }
    else { vpnSt = 5; vpnMsg = vpnJob.err; vpnRetryMs = now ? now : 1; dlog("VPN : %s", vpnMsg.c_str()); }
  }
  bool want = vpnConfigured();
  bool wifi = WiFi.status() == WL_CONNECTED;
  if (wg.is_initialized() && (vpnRestart || !want || !wifi)) {
    wg.end();
    dlog("VPN arrêté (%s)", !want ? "désactivé ou incomplet" : !wifi ? "Wi-Fi perdu" : "réglages modifiés");
    vpnRetryMs = 0;
  }
  vpnRestart = false;
  if (!want) { vpnSt = 0; vpnMsg = cfg.vpnOn ? "réglages incomplets ou clés invalides" : ""; return; }
  if (wg.is_initialized()) {
    if (wg.peer_up()) {
      if (vpnSt != 4) { vpnSt = 4; vpnMsg = ""; dlog("VPN connecté à %s (adresse du tunnel %s)", cfg.vpnHost.c_str(), cfg.vpnIp.c_str()); }
    } else {
      vpnSt = 3;
      vpnMsg = now - vpnStartMs > 60000 ? "aucune réponse du serveur depuis 60 s : vérifiez les clés, l'adresse, le port UDP ouvert et l'heure"
                                        : "poignée de main en cours";
    }
    return;
  }
  if (!wifi) { vpnSt = 1; vpnMsg = "attente du Wi-Fi"; return; }
  if (!timeSynced()) { vpnSt = 1; vpnMsg = "attente de l'heure (NTP) : la poignée de main WireGuard est horodatée"; return; }
  if (vpnRetryMs && now - vpnRetryMs < 30000) return;
  vpnJob.ip = cfg.vpnIp; vpnJob.mask = cfg.vpnMask; vpnJob.priv = cfg.vpnPriv; vpnJob.pub = cfg.vpnPeerPub; vpnJob.psk = cfg.vpnPsk;
  vpnJob.host = cfg.vpnHost; vpnJob.port = cfg.vpnPort; vpnJob.full = cfg.vpnFull; vpnJob.ok = false; vpnJob.err = "";
  vpnBusy = true;
  vpnSt = 2;
  vpnMsg = "démarrage";
  if (xTaskCreate(vpnTask, "vpn", 8192, nullptr, 1, nullptr) != pdPASS) { vpnBusy = false; vpnSt = 5; vpnMsg = "mémoire insuffisante"; vpnRetryMs = now ? now : 1; }
}

// ----------------------------------------------------------------------------
//  MQTT (tâche dédiée : une connexion lente ne bloque ni la page web ni la liaison série)
// ----------------------------------------------------------------------------
static volatile bool mqttConnected = false;
static volatile uint32_t publishedTotal = 0;

static const char* mqttStateText(int st) {
  switch (st) {
    case -4: return "délai dépassé : le broker ne répond pas";
    case -3: return "connexion perdue";
    case -2: return "broker injoignable (adresse ou port incorrects, broker éteint, pare-feu, ou broker qui n'écoute que sur localhost)";
    case -1: return "déconnecté";
    case 0: return "connecté";
    case 1: return "version du protocole refusée par le broker";
    case 2: return "identifiant client refusé";
    case 3: return "broker indisponible";
    case 4: return "utilisateur ou mot de passe incorrect";
    case 5: return "non autorisé (identifiants manquants ou droits insuffisants)";
    default: return "erreur inconnue";
  }
}

static String clientId(const char* suffix) {
  uint8_t m[6];
  WiFi.macAddress(m);
  char b[48];
  snprintf(b, sizeof b, "dronescan-%02x%02x%02x%s", m[3], m[4], m[5], suffix);
  return String(b);
}

struct MqttCfg {
  bool enabled;
  String host, user, pass, base;
  uint16_t port;
  bool retain, pubDrones, pubList, pubWifi, pubBt, pubSens;
  uint32_t droneMs, listMs;
  bool alert, ha;
  String haPrefix;
};
static MqttCfg mqttSnapshot() {
  MqttCfg c;
  xSemaphoreTake(cfgMutex, portMAX_DELAY);
  c.enabled = cfg.mqttEnabled;
  c.host = cfg.mqttHost;
  c.user = cfg.mqttUser;
  c.pass = cfg.mqttPass;
  c.base = cfg.mqttBase;
  c.port = cfg.mqttPort;
  c.retain = cfg.mqttRetain;
  c.pubDrones = cfg.pubDrones;
  c.pubList = cfg.pubList;
  c.pubWifi = cfg.pubWifi;
  c.pubBt = cfg.pubBt;
  c.pubSens = cfg.pubSens;
  c.droneMs = (uint32_t)cfg.mqttDroneSec * 1000UL;
  c.listMs = (uint32_t)cfg.mqttListSec * 1000UL;
  c.alert = cfg.alertOn && cfg.alertMqtt;
  c.ha = cfg.haOn;
  c.haPrefix = cfg.haPrefix;
  xSemaphoreGive(cfgMutex);
  return c;
}

static void publishStatus(PubSubClient& mq, const MqttCfg& c) {
  char buf[260];
  JBuf b(buf, sizeof buf);
  int n = 0;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& d : drones) n += d.used;
  bool alive = scannerAlive();
  int jn = journal.n;
  xSemaphoreGive(dMutex);
  int nw = 0, nb = 0;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& x : nets) nw += x.used;
  for (auto& x : bts) nb += x.used;
  xSemaphoreGive(dMutex);
  b.add("{\"online\":true,\"scanner\":%s,\"drones\":%d,\"wifi_networks\":%d,\"bluetooth_devices\":%d,\"journal\":%d,\"uptime\":%lu,\"heap\":%lu}",
        alive ? "true" : "false", n, nw, nb, jn, (unsigned long)(millis() / 1000), (unsigned long)ESP.getFreeHeap());
  mq.publish((c.base + "/status").c_str(), (const uint8_t*)buf, b.len, true);
}

// Publication en flux d'un message possiblement long (pas de gros tampon)
static bool streamPublish(PubSubClient& mq, const String& topic, bool retain, const String& body) {
  if (!mq.beginPublish(topic.c_str(), body.length(), retain)) return false;
  const uint8_t* p = (const uint8_t*)body.c_str();
  size_t left = body.length();
  while (left) {
    size_t k = left > 256 ? 256 : left;
    if (mq.write(p, k) != k) return false;
    p += k;
    left -= k;
  }
  return mq.endPublish() != 0;
}

// Liste complète des drones détectés
static bool publishList(PubSubClient& mq, const MqttCfg& c) {
  static Drone snap[MAX_DRONES];
  int n = 0;
  bool alive;
  uint32_t now = millis();
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& d : drones)
    if (d.used) snap[n++] = d;
  alive = scannerAlive();
  xSemaphoreGive(dMutex);

  String body;
  body.reserve(256 + 220 * (size_t)n);
  long ts = timeSynced() ? (long)time(nullptr) : 0;
  body += "{\"ts\":" + String(ts) + ",\"scanner\":" + (alive ? "true" : "false") + ",\"count\":" + String(n) + ",\"drones\":[";
  char buf[420];
  for (int i = 0; i < n; i++) {
    size_t len = droneJsonCompact(snap[i], (now - snap[i].lastMs) / 1000, buf, sizeof buf);
    if (i) body += ',';
    body.concat(buf, (unsigned int)len);
  }
  body += "]}";
  return streamPublish(mq, c.base + "/list", c.retain, body);
}

// Réseaux Wi-Fi captés par l'ESP 1
static bool publishNets(PubSubClient& mq, const MqttCfg& c) {
  static Net snap[MAX_NETS];
  int n = 0;
  uint32_t now = millis();
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& x : nets)
    if (x.used) snap[n++] = x;
  xSemaphoreGive(dMutex);
  String body;
  body.reserve(128 + 150 * (size_t)n);
  body += "{\"ts\":" + String(timeSynced() ? (long)time(nullptr) : 0L) + ",\"count\":" + String(n) + ",\"networks\":[";
  char buf[300];
  for (int i = 0; i < n; i++) {
    size_t len = netJson(snap[i], (now - snap[i].lastMs) / 1000, buf, sizeof buf);
    if (i) body += ',';
    body.concat(buf, (unsigned int)len);
  }
  body += "]}";
  return streamPublish(mq, c.base + "/wifi", c.retain, body);
}

// Appareils Bluetooth captés par l'ESP 1
static bool publishBts(PubSubClient& mq, const MqttCfg& c) {
  static Bt snap[MAX_BT];
  int n = 0;
  uint32_t now = millis();
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& x : bts)
    if (x.used) snap[n++] = x;
  xSemaphoreGive(dMutex);
  String body;
  body.reserve(128 + 150 * (size_t)n);
  body += "{\"ts\":" + String(timeSynced() ? (long)time(nullptr) : 0L) + ",\"count\":" + String(n) + ",\"devices\":[";
  char buf[300];
  for (int i = 0; i < n; i++) {
    size_t len = btJson(snap[i], (now - snap[i].lastMs) / 1000, buf, sizeof buf);
    if (i) body += ',';
    body.concat(buf, (unsigned int)len);
  }
  body += "]}";
  return streamPublish(mq, c.base + "/bluetooth", c.retain, body);
}

static volatile bool haRedo = false;   // vrai : redéclarer les capteurs Home Assistant

// Alertes « nouveau drone » : un message par apparition sur <base>/alert (non conservé)
static bool publishAlerts(PubSubClient& mq, const MqttCfg& c) {
  static Drone snap;
  for (int i = 0; i < MAX_DRONES; i++) {
    bool want = false;
    uint32_t now = millis();
    xSemaphoreTake(dMutex, portMAX_DELAY);
    Drone& d = drones[i];
    if (d.used && !d.alertSeen) {
      d.alertSeen = true;
      if (c.alert) { snap = d; want = true; }
    }
    xSemaphoreGive(dMutex);
    if (!want) continue;
    char j[900], who[48], buf[1200];
    droneJsonCompact(snap, (now - snap.lastMs) / 1000, j, sizeof j);
    snprintf(who, sizeof who, "%s", snap.hasId && snap.id[0] ? snap.id : "inconnu");
    JBuf b(buf, sizeof buf);
    b.add("{\"event\":\"drone_new\",\"title\":\"Drone détecté\",\"message\":");
    char msg[120];
    snprintf(msg, sizeof msg, "Drone %s détecté (signal %d dBm)", who, (int)snap.rssi);
    b.jstr(msg);
    b.add(",\"drone\":%s}", j);
    if (!streamPublish(mq, c.base + "/alert", false, String(buf))) return false;
    publishedTotal++;
  }
  // alertes « urgence » : le drone signale un état d'urgence (3) ou une panne du Remote ID (4)
  for (int i = 0; i < MAX_DRONES; i++) {
    bool want = false;
    uint32_t now = millis();
    xSemaphoreTake(dMutex, portMAX_DELAY);
    Drone& d = drones[i];
    if (d.used && d.hasLoc && d.status >= 3 && !d.alertEm) {
      d.alertEm = true;
      if (c.alert) { snap = d; want = true; }
    }
    xSemaphoreGive(dMutex);
    if (!want) continue;
    char j[900], who[48], buf[1200], msg[140];
    droneJsonCompact(snap, (now - snap.lastMs) / 1000, j, sizeof j);
    snprintf(who, sizeof who, "%s", snap.hasId && snap.id[0] ? snap.id : "inconnu");
    snprintf(msg, sizeof msg, "Drone %s : %s", who, snap.status == 3 ? "état d'urgence signalé" : "panne du système Remote ID signalée");
    JBuf b(buf, sizeof buf);
    b.add("{\"event\":\"drone_emergency\",\"title\":\"Urgence drone\",\"message\":");
    b.jstr(msg);
    b.add(",\"status\":%u,\"drone\":%s}", (unsigned)snap.status, j);
    if (!streamPublish(mq, c.base + "/alert", false, String(buf))) return false;
    publishedTotal++;
  }
  return true;
}

// Découverte automatique Home Assistant : capteurs publiés (conservés) à chaque connexion MQTT
static bool haSensor(PubSubClient& mq, const MqttCfg& c, const char* kind, const char* key, const char* name,
                     const char* topicSuffix, const char* tpl, const String& extra) {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  String uid = "dronescan_" + mac;
  String t = c.haPrefix + "/" + kind + "/" + uid + "/" + key + "/config";
  String s = "{\"name\":\"";
  s += name;
  s += "\",\"unique_id\":\"" + uid + "_" + key + "\",\"state_topic\":\"" + c.base + topicSuffix + "\",\"value_template\":\"";
  s += tpl;
  s += "\",\"availability_topic\":\"" + c.base + "/status\",\"availability_template\":\"{{ 'online' if value_json.online else 'offline' }}\"";
  s += ",\"device\":{\"identifiers\":[\"" + uid + "\"],\"name\":\"ESP SCANNER\",\"manufacturer\":\"DIY\",\"model\":\"ESP32-C6 Remote ID\"}";
  s += extra;
  s += "}";
  return streamPublish(mq, t, true, s);
}
static bool publishHA(PubSubClient& mq, const MqttCfg& c) {
  bool ok = true;
  const char* st = "/status";
  ok = haSensor(mq, c, "sensor", "drones", "Drones détectés", st, "{{ value_json.drones }}", ",\"icon\":\"mdi:quadcopter\",\"state_class\":\"measurement\"") && ok;
  ok = haSensor(mq, c, "sensor", "wifi", "Réseaux Wi-Fi vus", st, "{{ value_json.wifi_networks }}", ",\"icon\":\"mdi:wifi\",\"state_class\":\"measurement\"") && ok;
  ok = haSensor(mq, c, "sensor", "bt", "Appareils Bluetooth vus", st, "{{ value_json.bluetooth_devices }}", ",\"icon\":\"mdi:bluetooth\",\"state_class\":\"measurement\"") && ok;
  ok = haSensor(mq, c, "binary_sensor", "drone", "Drone en vue", st, "{{ 'ON' if value_json.drones > 0 else 'OFF' }}", ",\"device_class\":\"occupancy\"") && ok;
  ok = haSensor(mq, c, "binary_sensor", "scanner", "Scanner (ESP 1)", st, "{{ 'ON' if value_json.scanner else 'OFF' }}", ",\"device_class\":\"connectivity\"") && ok;
  ok = haSensor(mq, c, "sensor", "alert", "Dernière alerte drone", "/alert", "{{ value_json.message }}", String(",\"icon\":\"mdi:alert\",\"json_attributes_topic\":\"") + c.base + "/alert\"") && ok;
  return ok;
}

// Capteurs Bluetooth : une fiche Home Assistant par mesure (déclarée à sa première apparition) + l'état sur <base>/sensors/<MAC>
static bool haSens(PubSubClient& mq, const MqttCfg& c, const Sens& s, uint16_t bit) {
  const char *kind = "sensor", *key = "", *name = "", *tpl = "", *extra = "";
  switch (bit) {
    case SB_TEMP: key = "temperature"; name = "Température"; tpl = "{{ value_json.temperature }}"; extra = ",\"device_class\":\"temperature\",\"unit_of_measurement\":\"°C\",\"state_class\":\"measurement\""; break;
    case SB_HUM: key = "humidity"; name = "Humidité"; tpl = "{{ value_json.humidity }}"; extra = ",\"device_class\":\"humidity\",\"unit_of_measurement\":\"%\",\"state_class\":\"measurement\""; break;
    case SB_LUX: key = "illuminance"; name = "Luminosité"; tpl = "{{ value_json.illuminance }}"; extra = ",\"device_class\":\"illuminance\",\"unit_of_measurement\":\"lx\",\"state_class\":\"measurement\""; break;
    case SB_BATT: key = "battery"; name = "Batterie"; tpl = "{{ value_json.battery }}"; extra = ",\"device_class\":\"battery\",\"unit_of_measurement\":\"%\",\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\""; break;
    case SB_MOIST: key = "moisture"; name = "Humidité du sol"; tpl = "{{ value_json.moisture }}"; extra = ",\"device_class\":\"moisture\",\"unit_of_measurement\":\"%\",\"state_class\":\"measurement\""; break;
    case SB_COND: key = "conductivity"; name = "Fertilité (conductivité)"; tpl = "{{ value_json.conductivity }}"; extra = ",\"unit_of_measurement\":\"µS/cm\",\"state_class\":\"measurement\",\"icon\":\"mdi:flower\""; break;
    case SB_WIN: kind = "binary_sensor"; key = "window"; name = "Fenêtre"; tpl = "{{ 'ON' if value_json.window == 'open' else 'OFF' }}"; extra = ",\"device_class\":\"window\""; break;
    case SB_DOOR: kind = "binary_sensor"; key = "door"; name = "Porte"; tpl = "{{ 'ON' if value_json.door == 'open' else 'OFF' }}"; extra = ",\"device_class\":\"door\""; break;
    case SB_MOTION: kind = "binary_sensor"; key = "motion"; name = "Mouvement"; tpl = "{{ 'ON' if value_json.motion else 'OFF' }}"; extra = ",\"device_class\":\"motion\""; break;
    case SB_BUTTON: key = "button"; name = "Dernier évènement bouton"; tpl = "{{ value_json.button }}"; extra = ",\"icon\":\"mdi:gesture-tap-button\""; break;
    case SB_PRES: key = "pressure"; name = "Pression"; tpl = "{{ value_json.pressure }}"; extra = ",\"device_class\":\"pressure\",\"unit_of_measurement\":\"hPa\",\"state_class\":\"measurement\""; break;
    case SB_VOLT: key = "battery_voltage"; name = "Tension de la pile"; tpl = "{{ value_json.battery_voltage }}"; extra = ",\"device_class\":\"voltage\",\"unit_of_measurement\":\"mV\",\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\""; break;
    case SB_ROT: key = "rotation"; name = "Rotation"; tpl = "{{ value_json.rotation }}"; extra = ",\"unit_of_measurement\":\"°\",\"state_class\":\"measurement\""; break;
    default: return true;
  }
  char hex[13], mac[18];
  snprintf(hex, sizeof hex, "%02x%02x%02x%02x%02x%02x", s.mac[0], s.mac[1], s.mac[2], s.mac[3], s.mac[4], s.mac[5]);
  jnlMac(s.mac, mac, sizeof mac);
  String gw = WiFi.macAddress();
  gw.replace(":", "");
  gw.toLowerCase();
  String uid = String("dronescan_s_") + hex;
  String dev = s.name[0] ? String(s.name) : String(s.ty == 2 ? "Flora " : s.ty == 5 ? "Ruuvi " : s.ty == 6 ? "Eddystone " : s.ty == 1 ? "BTHome " : "Xiaomi ") + (hex + 8);
  String topic = c.haPrefix + "/" + kind + "/" + uid + "/" + key + "/config";
  String j = String("{\"name\":\"") + name + "\",\"unique_id\":\"" + uid + "_" + key + "\",\"state_topic\":\"" + c.base + "/sensors/" + hex +
             "\",\"value_template\":\"" + tpl + "\",\"availability_topic\":\"" + c.base +
             "/status\",\"availability_template\":\"{{ 'online' if value_json.online else 'offline' }}\"" + extra +
             ",\"device\":{\"identifiers\":[\"" + uid + "\"],\"name\":";
  j += '"';
  for (const char* q = dev.c_str(); *q; q++) {
    if (*q == '"' || *q == '\\') j += '\\';
    if ((uint8_t)*q >= 0x20) j += *q;
  }
  j += '"';
  j += String(",\"manufacturer\":\"") + (s.ty == 1 ? "BTHome" : s.ty == 5 ? "Ruuvi" : s.ty == 6 ? "Eddystone" : "Xiaomi") + "\",\"model\":\"" + sensTypeText(s) + "\",\"connections\":[[\"bluetooth\",\"" + mac +
       "\"]],\"via_device\":\"dronescan_" + gw + "\"}}";
  return streamPublish(mq, topic, true, j);
}

static bool publishSensors(PubSubClient& mq, const MqttCfg& c) {
  static Sens snap;
  for (int i = 0; i < MAX_SENS; i++) {
    bool want = false;
    uint32_t now = millis();
    xSemaphoreTake(dMutex, portMAX_DELAY);
    if (sens[i].used && sens[i].dirty) {
      snap = sens[i];
      sens[i].dirty = false;
      want = true;
    }
    xSemaphoreGive(dMutex);
    if (!want) continue;
    bool ok = true;
    uint16_t add = sensMask(snap) & ~snap.haMask;
    if (c.ha && add) {
      uint16_t done = 0;
      for (uint16_t bit = 1; bit <= SB_VOLT; bit <<= 1)
        if (add & bit) {
          if (haSens(mq, c, snap, bit)) done |= bit;
          else ok = false;
          mq.loop();
        }
      xSemaphoreTake(dMutex, portMAX_DELAY);
      if (sens[i].used && memcmp(sens[i].mac, snap.mac, 6) == 0) sens[i].haMask |= done;
      xSemaphoreGive(dMutex);
    }
    char hex[13], buf[560];
    snprintf(hex, sizeof hex, "%02X%02X%02X%02X%02X%02X", snap.mac[0], snap.mac[1], snap.mac[2], snap.mac[3], snap.mac[4], snap.mac[5]);
    size_t len = sensJson(snap, (now - snap.lastMs) / 1000, buf, sizeof buf);
    if (ok && streamPublish(mq, c.base + "/sensors/" + hex, c.retain, String(buf))) publishedTotal++;
    else ok = false;
    if (!ok) {
      xSemaphoreTake(dMutex, portMAX_DELAY);
      if (sens[i].used && memcmp(sens[i].mac, snap.mac, 6) == 0) sens[i].dirty = true;
      xSemaphoreGive(dMutex);
      return false;
    }
  }
  return true;
}

// à chaque (re)connexion : tout est redéclaré et republié
static void sensorsRepublishAll() {
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& x : sens)
    if (x.used) { x.haMask = 0; x.dirty = true; }
  xSemaphoreGive(dMutex);
}

static bool publishAllLists(PubSubClient& mq, const MqttCfg& c) {
  bool ok = true;
  if (c.pubList) ok = publishList(mq, c) && ok;
  if (c.pubWifi) ok = publishNets(mq, c) && ok;
  if (c.pubBt) ok = publishBts(mq, c) && ok;
  return ok;
}

static void mqttTask(void*) {
  static WiFiClient net;
  static PubSubClient mq(net);
  static char hostBuf[96];   // PubSubClient ne copie pas le nom du serveur : tampon stable
  uint32_t nextTry = 0, lastList = 0, lastStatus = 0;
  uint32_t lastCfgSig = 0;
  bool wasConnected = false;
  mq.setBufferSize(1536);
  mq.setSocketTimeout(4);
  mq.setKeepAlive(30);

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(50));
    uint32_t now = millis();
    MqttCfg c = mqttSnapshot();

    if (!c.enabled || !c.host.length() || WiFi.status() != WL_CONNECTED) {
      if (mq.connected()) mq.disconnect();
      mqttConnected = false;
      wasConnected = false;
      continue;
    }

    // les réglages ont changé : on se reconnecte
    uint32_t sig = crc16(c.host.c_str(), c.host.length()) ^ (crc16(c.user.c_str(), c.user.length()) << 1) ^
                   (crc16(c.pass.c_str(), c.pass.length()) << 2) ^ (uint32_t)c.port * 2654435761u;
    if (sig != lastCfgSig) {
      lastCfgSig = sig;
      if (mq.connected()) mq.disconnect();
      nextTry = 0;
    }

    if (!mq.connected()) {
      mqttConnected = false;
      if (wasConnected) { wasConnected = false; dlog("MQTT : connexion perdue (%s)", mqttStateText(mq.state())); }
      if ((int32_t)(now - nextTry) < 0) continue;
      nextTry = now + 6000;   // nouvelle tentative toutes les 6 s, sans limite
      strlcpy(hostBuf, c.host.c_str(), sizeof hostBuf);
      mq.setServer(hostBuf, c.port);
      String cid = clientId("");
      String will = c.base + "/status";
      bool ok = c.user.length()
                    ? mq.connect(cid.c_str(), c.user.c_str(), c.pass.c_str(), will.c_str(), 1, true, "{\"online\":false}")
                    : mq.connect(cid.c_str(), nullptr, nullptr, will.c_str(), 1, true, "{\"online\":false}");
      if (ok) {
        setMqttErr("");
        wasConnected = true;
        mqttConnected = true;
        dlog("MQTT connecté à %s:%u", hostBuf, c.port);
        publishStatus(mq, c);
        if (c.ha) { if (publishHA(mq, c)) dlog("Home Assistant : capteurs déclarés"); else dlog("Home Assistant : déclaration incomplète"); }
        sensorsRepublishAll();
        publishAllLists(mq, c);
        lastStatus = lastList = now;
        mqttForce = true;   // publie aussi chaque drone une première fois
      } else {
        int st = mq.state();
        String e = String("code ") + st + " : " + mqttStateText(st);
        setMqttErr(e);
        dlog("MQTT échec (%s:%u) : %s", hostBuf, c.port, e.c_str());
      }
      continue;
    }

    mq.loop();
    if (haRedo) {
      haRedo = false;
      if (c.ha) publishHA(mq, c);
    }

    // un message par drone modifié (au plus un toutes les MQTT_DRONE_MIN_MS)
    bool all = mqttForce;
    mqttForce = false;
    bool failed = false;
    for (int i = 0; i < MAX_DRONES && !failed; i++) {
      Drone d;
      bool want = false;
      xSemaphoreTake(dMutex, portMAX_DELAY);
      if (c.pubDrones && drones[i].used && (all || (drones[i].dirty && now - drones[i].sentMs >= c.droneMs))) {
        d = drones[i];
        want = true;
      }
      xSemaphoreGive(dMutex);
      if (!want) continue;
      char topicId[48], payload[1100];
      droneTopicId(d, topicId, sizeof topicId);
      size_t len = droneJson(d, (now - d.lastMs) / 1000, timeSynced() ? (long)time(nullptr) - (long)((now - d.lastMs) / 1000) : 0,
                             payload, sizeof payload);
      String topic = c.base + "/drones/" + topicId;
      if (mq.publish(topic.c_str(), (const uint8_t*)payload, len, false)) {
        xSemaphoreTake(dMutex, portMAX_DELAY);
        if (drones[i].used && memcmp(drones[i].mac, d.mac, 6) == 0 && drones[i].frames == d.frames) drones[i].dirty = false;
        drones[i].sentMs = now;
        xSemaphoreGive(dMutex);
        publishedTotal++;
      } else {
        setMqttErr("publication refusée (connexion perdue ou message trop gros)");
        dlog("MQTT : publication refusée, reconnexion");
        mq.disconnect();
        nextTry = now + 3000;
        failed = true;
      }
      mq.loop();
    }
    if (failed) continue;

    if (c.pubSens && !publishSensors(mq, c)) { setMqttErr("publication d'un capteur refusée"); mq.disconnect(); nextTry = now + 3000; continue; }
    if (!publishAlerts(mq, c)) { setMqttErr("publication d'une alerte refusée"); mq.disconnect(); nextTry = now + 3000; continue; }

    if (all || now - lastList >= c.listMs) {
      lastList = now;
      if (publishAllLists(mq, c)) publishedTotal++;
      else { setMqttErr("publication d'une liste refusée"); dlog("MQTT : liste refusée"); mq.disconnect(); nextTry = now + 3000; continue; }
    }
    if (now - lastStatus >= 30000) {
      lastStatus = now;
      publishStatus(mq, c);
    }
  }
}

// ----------------------------------------------------------------------------
//  Syslog (UDP) : détections et événements envoyés à un serveur syslog (rsyslog, Graylog, Synology, ELK…)
//  Chaque message porte un type : drone, drone_lost, wifi_new, bluetooth_new, system.
// ----------------------------------------------------------------------------
static volatile uint32_t syslogSent = 0;
static char syslogErr[140] = "";   // protégé par logMutex

struct SlCfg {
  bool on;
  String host;
  uint16_t port;
  uint8_t fmt, body, fac;
  bool drones, wifi, bt, system;
  uint16_t droneSec;
  bool alert;
  String hostname;
};
static SlCfg slSnapshot() {
  SlCfg c;
  xSemaphoreTake(cfgMutex, portMAX_DELAY);
  c.on = cfg.syslogOn;
  c.host = cfg.syslogHost;
  c.port = cfg.syslogPort;
  c.fmt = cfg.syslogFmt;
  c.body = cfg.syslogBody;
  c.fac = cfg.syslogFac;
  c.drones = cfg.slDrones;
  c.wifi = cfg.slWifi;
  c.bt = cfg.slBt;
  c.system = cfg.slSystem;
  c.droneSec = cfg.slDroneSec;
  c.alert = cfg.alertOn && cfg.alertSyslog;
  c.hostname = cfg.hostname;
  xSemaphoreGive(cfgMutex);
  return c;
}

static void setSyslogErr(const char* s) {
  xSemaphoreTake(logMutex, portMAX_DELAY);
  strlcpy(syslogErr, s, sizeof syslogErr);
  xSemaphoreGive(logMutex);
}

static bool slResolve(const String& host, IPAddress& ip) {
  if (ip.fromString(host)) return true;
  return WiFi.hostByName(host.c_str(), ip) == 1;
}

// Assemble et envoie un paquet syslog. sev : 0 urgence … 4 avertissement, 5 notice, 6 info
static bool slSend(WiFiUDP& udp, const IPAddress& ip, const SlCfg& c, int sev, const char* msgid, const char* body) {
  static char pkt[1500];
  int pri = (int)c.fac * 8 + sev;
  time_t t = time(nullptr);
  struct tm tmv;
  bool haveTime = t > 1700000000;
  if (haveTime) gmtime_r(&t, &tmv);
  size_t n;
  if (c.fmt == 0) {   // RFC 5424
    char ts[32] = "-";
    if (haveTime) strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &tmv);
    n = (size_t)snprintf(pkt, sizeof pkt, "<%d>1 %s %s dronescan - %s - %s", pri, ts, c.hostname.c_str(), msgid, body);
  } else {            // RFC 3164
    char ts[32] = "Jan  1 00:00:00";
    if (haveTime) strftime(ts, sizeof ts, "%b %e %H:%M:%S", &tmv);
    n = (size_t)snprintf(pkt, sizeof pkt, "<%d>%s %s dronescan: %s %s", pri, ts, c.hostname.c_str(), msgid, body);
  }
  if (n >= sizeof pkt) n = sizeof pkt - 1;
  if (!udp.beginPacket(ip, c.port)) return false;
  udp.write((const uint8_t*)pkt, n);
  return udp.endPacket() == 1;
}

// valeur de texte « clé="valeur" » avec guillemets et barres obliques échappés
static void slKv(JBuf& b, const char* key, const char* val) {
  b.add(" %s=\"", key);
  for (; *val; val++) {
    if (*val == '"' || *val == '\\') b.add("\\%c", *val);
    else b.add("%c", *val);
  }
  b.add("\"");
}

static void slBodyDrone(const Drone& d, const char* event, uint32_t ageSec, const SlCfg& c, char* out, size_t cap) {
  if (c.body == 0) {
    char j[1100];
    droneJson(d, ageSec, 0, j, sizeof j);
    snprintf(out, cap, "{\"event\":\"%s\",\"drone\":%s}", event, j);
    return;
  }
  char mac[18], src[32], man[4], mod[4], ser[25];
  jnlMac(d.mac, mac, sizeof mac);
  srcText(d.src, src, sizeof src);
  JBuf b(out, cap);
  b.add("event=%s", event);
  slKv(b, "id", d.hasId ? d.id : "");
  b.add(" mac=%s src=%s", mac, src);
  if (frSplit(d, man, mod, ser)) {
    slKv(b, "fr_manufacturer", man);
    slKv(b, "fr_model", mod);
    slKv(b, "fr_serial", ser);
  }
  if (d.ansi[0] && strcmp(d.ansi, d.id) != 0) slKv(b, "ansi_id", d.ansi);
  if (d.hasReg) slKv(b, "reg", d.reg);
  if (d.hasPos) b.add(" lat=%.6f lon=%.6f", d.lat, d.lon);
  if (!isnan(d.altGeo)) b.add(" alt_geo=%.0f", d.altGeo);
  if (!isnan(d.height)) b.add(" height=%.0f", d.height);
  if (!isnan(d.spdH)) b.add(" speed=%.1f", d.spdH);
  if (!isnan(d.dir)) b.add(" dir=%.0f", d.dir);
  if (d.hasOp) b.add(" op_lat=%.6f op_lon=%.6f", d.opLat, d.opLon);
  if (d.opId[0]) slKv(b, "op_id", d.opId);
  b.add(" rssi=%d", (int)d.rssi);
}

static void slBodyNet(const Net& n, const SlCfg& c, char* out, size_t cap) {
  if (c.body == 0) {
    char j[300];
    netJson(n, 0, j, sizeof j);
    snprintf(out, cap, "{\"event\":\"wifi_new\",\"network\":%s}", j);
    return;
  }
  char mac[18];
  jnlMac(n.bssid, mac, sizeof mac);
  JBuf b(out, cap);
  b.add("event=wifi_new");
  slKv(b, "ssid", n.ssid);
  b.add(" bssid=%s channel=%u security=\"%s\" rssi=%d", mac, (unsigned)n.ch, encText(n.enc), (int)n.rssi);
}

static void slBodyBt(const Bt& d, const SlCfg& c, char* out, size_t cap) {
  if (c.body == 0) {
    char j[300];
    btJson(d, 0, j, sizeof j);
    snprintf(out, cap, "{\"event\":\"bluetooth_new\",\"device\":%s}", j);
    return;
  }
  char mac[18];
  jnlMac(d.mac, mac, sizeof mac);
  JBuf b(out, cap);
  b.add("event=bluetooth_new");
  slKv(b, "name", d.name);
  b.add(" mac=%s", mac);
  if (d.mf >= 0) {
    slKv(b, "manufacturer", companyName(d.mf)[0] ? companyName(d.mf) : "");
    b.add(" manufacturer_id=%ld", (long)d.mf);
  }
  b.add(" rssi=%d", (int)d.rssi);
}

static void slBodySystem(const char* msg, const SlCfg& c, char* out, size_t cap) {
  JBuf b(out, cap);
  if (c.body == 0) {
    b.add("{\"event\":\"system\",\"message\":");
    b.jstr(msg);
    b.add("}");
  } else {
    b.add("event=system");
    slKv(b, "message", msg);
  }
}

static void syslogTask(void*) {
  static WiFiUDP udp;
  static Drone dsnap;
  static Net nsnap;
  static Bt bsnap;
  IPAddress ip;
  String resolvedFor;
  uint32_t resolvedAt = 0;
  char body[1400];
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(300));
    SlCfg c = slSnapshot();
    if (!c.on || !c.host.length() || WiFi.status() != WL_CONNECTED) {
      // rien n'est envoyé : on vide les files pour ne pas envoyer de vieux messages plus tard
      xSemaphoreTake(logMutex, portMAX_DELAY);
      sysQTail = sysQHead;
      xSemaphoreGive(logMutex);
      continue;
    }
    uint32_t now = millis();
    if (resolvedFor != c.host || now - resolvedAt > 300000UL || ip == IPAddress((uint32_t)0)) {
      resolvedFor = c.host;
      resolvedAt = now;
      ip = IPAddress((uint32_t)0);
      if (!slResolve(c.host, ip)) {
        setSyslogErr("nom du serveur syslog introuvable (DNS) : utilisez son adresse IP");
        resolvedAt = now - 290000UL;   // nouvel essai dans 10 s
        continue;
      }
    }
    bool failed = false;
    int budget = 8;   // messages par passage

    {   // alertes : première apparition d'un drone (gravité « warning », identifiant ALERT)
      for (int i = 0; i < MAX_DRONES && budget > 0; i++) {
        bool want = false;
        xSemaphoreTake(dMutex, portMAX_DELAY);
        Drone& d = drones[i];
        if (d.used && !d.alertSl) {
          d.alertSl = true;
          if (c.alert) { dsnap = d; want = true; }
        }
        xSemaphoreGive(dMutex);
        if (!want) continue;
        slBodyDrone(dsnap, "drone_alert", (now - dsnap.lastMs) / 1000, c, body, sizeof body);
        if (slSend(udp, ip, c, 4, "ALERT", body)) { syslogSent++; budget--; } else failed = true;
      }
      for (int i = 0; i < MAX_DRONES && budget > 0; i++) {   // urgence signalée par le drone (gravité « critical »)
        bool want = false;
        xSemaphoreTake(dMutex, portMAX_DELAY);
        Drone& d = drones[i];
        if (d.used && d.hasLoc && d.status >= 3 && !d.alertEmSl) {
          d.alertEmSl = true;
          if (c.alert) { dsnap = d; want = true; }
        }
        xSemaphoreGive(dMutex);
        if (!want) continue;
        slBodyDrone(dsnap, "drone_emergency", (now - dsnap.lastMs) / 1000, c, body, sizeof body);
        if (slSend(udp, ip, c, 2, "ALERT", body)) { syslogSent++; budget--; } else failed = true;
      }
    }
    if (c.drones) {
      for (int i = 0; i < MAX_DRONES && budget > 0; i++) {
        bool want = false, fresh = false;
        xSemaphoreTake(dMutex, portMAX_DELAY);
        Drone& d = drones[i];
        if (d.used && (!d.logSeen || now - d.logMs >= (uint32_t)c.droneSec * 1000UL)) {
          dsnap = d;
          fresh = !d.logSeen;
          d.logSeen = true;
          d.logMs = now;
          want = true;
        }
        xSemaphoreGive(dMutex);
        if (!want) continue;
        slBodyDrone(dsnap, fresh ? "drone_new" : "drone", (now - dsnap.lastMs) / 1000, c, body, sizeof body);
        if (slSend(udp, ip, c, 5, "DRONE", body)) { syslogSent++; budget--; } else failed = true;
      }
      for (;;) {   // drones disparus
        char id[41], mac[41];
        bool have = false;
        xSemaphoreTake(dMutex, portMAX_DELAY);
        if (lostTail != lostHead) {
          if (lostHead - lostTail > 8) lostTail = lostHead - 8;
          strlcpy(id, lostQ[lostTail % 8][0], sizeof id);
          strlcpy(mac, lostQ[lostTail % 8][1], sizeof mac);
          lostTail++;
          have = true;
        }
        xSemaphoreGive(dMutex);
        if (!have) break;
        JBuf b(body, sizeof body);
        if (c.body == 0) {
          b.add("{\"event\":\"drone_lost\",\"id\":");
          b.jstr(id);
          b.add(",\"mac\":\"%s\"}", mac);
        } else {
          b.add("event=drone_lost");
          slKv(b, "id", id);
          b.add(" mac=%s", mac);
        }
        if (slSend(udp, ip, c, 5, "DRONE", body)) syslogSent++; else failed = true;
      }
    }
    if (c.wifi) {
      for (int i = 0; i < MAX_NETS && budget > 0; i++) {
        bool want = false;
        xSemaphoreTake(dMutex, portMAX_DELAY);
        if (nets[i].used && !nets[i].logged) {
          nsnap = nets[i];
          nets[i].logged = true;
          want = true;
        }
        xSemaphoreGive(dMutex);
        if (!want) continue;
        slBodyNet(nsnap, c, body, sizeof body);
        if (slSend(udp, ip, c, 6, "WIFI", body)) { syslogSent++; budget--; } else failed = true;
      }
    }
    if (c.bt) {
      for (int i = 0; i < MAX_BT && budget > 0; i++) {
        bool want = false;
        xSemaphoreTake(dMutex, portMAX_DELAY);
        if (bts[i].used && !bts[i].logged) {
          bsnap = bts[i];
          bts[i].logged = true;
          want = true;
        }
        xSemaphoreGive(dMutex);
        if (!want) continue;
        slBodyBt(bsnap, c, body, sizeof body);
        if (slSend(udp, ip, c, 6, "BLUETOOTH", body)) { syslogSent++; budget--; } else failed = true;
      }
    }
    for (;;) {   // journal technique
      char line[LOG_LEN];
      bool have = false;
      xSemaphoreTake(logMutex, portMAX_DELAY);
      if (sysQTail != sysQHead) {
        if (sysQHead - sysQTail > 16) sysQTail = sysQHead - 16;
        strlcpy(line, sysQ[sysQTail % 16], sizeof line);
        sysQTail++;
        have = true;
      }
      xSemaphoreGive(logMutex);
      if (!have) break;
      slBodySystem(line, c, body, sizeof body);
      bool bad = strstr(line, "ERREUR") || strstr(line, "échec") || strstr(line, "refus");
      if (slSend(udp, ip, c, bad ? 4 : 6, "SYSTEM", body)) syslogSent++; else failed = true;
    }
    setSyslogErr(failed ? "envoi UDP impossible (réseau ou adresse du serveur)" : "");
  }
}

// ----------------------------------------------------------------------------
//  Interface web
// ----------------------------------------------------------------------------
#define AUTH()                                                                   \
  do {                                                                           \
    if (cfg.webUser.length() && !server.authenticate(cfg.webUser.c_str(), cfg.webPass.c_str())) { \
      server.requestAuthentication();                                            \
      return;                                                                    \
    }                                                                            \
  } while (0)

static void jstr(String& o, const char* s) {
  o += '"';
  for (; *s; s++) {
    uint8_t c = (uint8_t)*s;
    if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
    else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
    else o += (char)c;
  }
  o += '"';
}

static void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}
static void ok() { sendJson(200, "{\"ok\":true}"); }

static void handleRoot() {
  AUTH();
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html; charset=utf-8", (const char*)INDEX_GZ, INDEX_GZ_LEN);
}

static int countDrones() {
  int n = 0;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& d : drones) n += d.used;
  xSemaphoreGive(dMutex);
  return n;
}

static void handleStatus() {
  AUTH();
  uint32_t now = millis();
  Heartbeat hb;
  uint32_t hbMs, lines, bad, jrev;
  int jn, nd = 0, nw = 0, nb = 0, ns = 0;
  bool alive;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  hb = scHb;
  hbMs = scHbMs;
  lines = linkLines;
  bad = linkBad;
  jrev = journal.rev;
  jn = journal.n;
  alive = scannerAlive();
  for (auto& d : drones) nd += d.used;
  for (auto& x : nets) nw += x.used;
  for (auto& x : bts) nb += x.used;
  for (auto& x : sens) ns += x.used;
  xSemaphoreGive(dMutex);

  bool sta = WiFi.status() == WL_CONNECTED;
  String s;
  s.reserve(1100);
  s += "{\"drones\":" + String(nd) + ",\"sens\":" + String(ns) + ",\"nets\":" + String(nw) + ",\"bts\":" + String(nb) + ",\"journal\":" + String(jn) +
       ",\"journalRev\":" + String(jrev) + ",\"boot\":" + String(bootId) + ",\"mode\":" + String(cfg.mode) + ",\"heap\":" +
       String(ESP.getFreeHeap()) + ",\"uptime\":" + String(now / 1000) + ",\"timeOk\":" + (timeSynced() ? "true" : "false") +
       ",\"fs\":" + (fsOk ? "true" : "false");
  s += ",\"scanner\":{\"alive\":" + String(alive ? "true" : "false") + ",\"ago\":" + String(hbMs ? (now - hbMs) / 1000 : -1) +
       ",\"up\":" + String(hb.up) + ",\"ble\":" + String(hb.ble) + ",\"wifi\":" + String(hb.wifi) + ",\"drop\":" + String(hb.drop) +
       ",\"heap\":" + String(hb.heap) + ",\"ch\":" + String(hb.ch) + ",\"mode\":" + String(hb.mode) + ",\"bleOn\":" +
       (hb.bleOn ? "true" : "false") + ",\"wifiOn\":" + (hb.wifiOn ? "true" : "false") + ",\"tracked\":" + String(hb.tracked) +
       ",\"nets\":" + String(hb.nets) + ",\"bts\":" + String(hb.bts) + ",\"lines\":" + String(lines) + ",\"bad\":" + String(bad) + "}";
  s += ",\"vpn\":{\"on\":" + String(cfg.vpnOn ? "true" : "false") + ",\"state\":" + String(vpnSt) + ",\"msg\":";
  jstr(s, vpnMsg.c_str());
  s += ",\"ip\":";
  jstr(s, cfg.vpnIp.c_str());
  s += "}";
  s += ",\"fw\":\"" + String(GW_FW) + "\",\"scannerFw\":";
  jstr(s, hb.fw);
  s += ",\"wifiInfo\":{\"sta\":" + String(sta ? "true" : "false") + ",\"ssid\":";
  jstr(s, cfg.wifiSsid.c_str());
  s += ",\"ip\":";
  jstr(s, WiFi.localIP().toString().c_str());
  s += ",\"rssi\":" + String(sta ? WiFi.RSSI() : 0) + ",\"state\":";
  jstr(s, wifiStatusText(WiFi.status()));
  s += ",\"apOn\":" + String(apOn ? "true" : "false") + ",\"ap\":";
  jstr(s, apName.c_str());
  s += ",\"apIp\":";
  jstr(s, apOn ? WiFi.softAPIP().toString().c_str() : "");
  s += ",\"apClients\":" + String(apOn ? WiFi.softAPgetStationNum() : 0) + "}";
  MqttCfg c = mqttSnapshot();
  s += ",\"mqttInfo\":{\"enabled\":" + String(c.enabled ? "true" : "false") + ",\"host\":";
  jstr(s, c.host.c_str());
  s += ",\"connected\":" + String(mqttConnected ? "true" : "false") + ",\"published\":" + String(publishedTotal) + ",\"error\":";
  jstr(s, getMqttErr().c_str());
  s += "},\"syslog\":{\"enabled\":" + String(cfg.syslogOn ? "true" : "false") + ",\"host\":";
  jstr(s, cfg.syslogHost.c_str());
  s += ",\"sent\":" + String(syslogSent) + ",\"error\":";
  xSemaphoreTake(logMutex, portMAX_DELAY);
  String slErr(syslogErr);
  xSemaphoreGive(logMutex);
  jstr(s, slErr.c_str());
  s += "}}";
  sendJson(200, s);
}

static void handleDrones() {
  AUTH();
  uint32_t now = millis();
  static Drone snap[MAX_DRONES];
  int n = 0;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& d : drones)
    if (d.used) snap[n++] = d;
  xSemaphoreGive(dMutex);
  String out = "[";
  char buf[1100];
  for (int i = 0; i < n; i++) {
    uint32_t age = (now - snap[i].lastMs) / 1000;
    size_t len = droneJson(snap[i], age, timeSynced() ? (long)time(nullptr) - (long)age : 0, buf, sizeof buf);
    if (i) out += ',';
    out.concat(buf, (unsigned int)len);
  }
  out += "]";
  sendJson(200, out);
}

static void handleWifiList() {
  AUTH();
  uint32_t now = millis();
  static Net snap[MAX_NETS];
  int n = 0;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& x : nets)
    if (x.used) snap[n++] = x;
  xSemaphoreGive(dMutex);
  String out = "[";
  char buf[300];
  for (int i = 0; i < n; i++) {
    size_t len = netJson(snap[i], (now - snap[i].lastMs) / 1000, buf, sizeof buf);
    if (i) out += ',';
    out.concat(buf, (unsigned int)len);
  }
  out += "]";
  sendJson(200, out);
}

static void handleBtList() {
  AUTH();
  uint32_t now = millis();
  static Bt snap[MAX_BT];
  int n = 0;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& x : bts)
    if (x.used) snap[n++] = x;
  xSemaphoreGive(dMutex);
  String out = "[";
  char buf[300];
  for (int i = 0; i < n; i++) {
    size_t len = btJson(snap[i], (now - snap[i].lastMs) / 1000, buf, sizeof buf);
    if (i) out += ',';
    out.concat(buf, (unsigned int)len);
  }
  out += "]";
  sendJson(200, out);
}

static void handleSensList() {
  AUTH();
  uint32_t now = millis();
  static Sens snap[MAX_SENS];
  int n = 0;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& x : sens)
    if (x.used) snap[n++] = x;
  xSemaphoreGive(dMutex);
  String out = "[";
  char buf[560];
  for (int i = 0; i < n; i++) {
    size_t len = sensJson(snap[i], (now - snap[i].lastMs) / 1000, buf, sizeof buf);
    if (i) out += ',';
    out.concat(buf, (unsigned int)len);
  }
  out += "]";
  sendJson(200, out);
}

// Journal : réponse envoyée par morceaux (une ligne à la fois) pour ne pas saturer la mémoire
static void handleJournal() {
  AUTH();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "");
  server.sendContent("[");
  char buf[700];
  bool first = true;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  int n = journal.n;
  xSemaphoreGive(dMutex);
  for (int i = n - 1; i >= 0; i--) {   // du plus récent au plus ancien
    JEntry x;
    bool have;
    xSemaphoreTake(dMutex, portMAX_DELAY);
    have = i < journal.n;
    if (have) x = journal.e[i];
    xSemaphoreGive(dMutex);
    if (!have) continue;
    size_t len = jnlJson(x, buf, sizeof buf);
    if (!first) server.sendContent(",");
    first = false;
    server.sendContent(buf, len);
  }
  server.sendContent("]");
  server.sendContent("");
}

static void handleJournalCsv() {
  AUTH();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Content-Disposition", "attachment; filename=\"journal-drones.csv\"");
  server.send(200, "text/csv; charset=utf-8", "");
  server.sendContent(jnlCsvHeader());
  char buf[700];
  xSemaphoreTake(dMutex, portMAX_DELAY);
  int n = journal.n;
  xSemaphoreGive(dMutex);
  for (int i = n - 1; i >= 0; i--) {
    JEntry x;
    bool have;
    xSemaphoreTake(dMutex, portMAX_DELAY);
    have = i < journal.n;
    if (have) x = journal.e[i];
    xSemaphoreGive(dMutex);
    if (!have) continue;
    size_t len = jnlCsv(x, buf, sizeof buf);
    server.sendContent(buf, len);
  }
  server.sendContent("");
}

static void handleJournalClear() {
  AUTH();
  xSemaphoreTake(dMutex, portMAX_DELAY);
  journal.clear();
  xSemaphoreGive(dMutex);
  saveJournal(true);
  dlog("Journal vidé");
  ok();
}

static void handleLog() {
  AUTH();
  String o = "[";
  xSemaphoreTake(logMutex, portMAX_DELAY);
  uint32_t from = logSeq > LOG_LINES ? logSeq - LOG_LINES : 0;
  for (uint32_t s = from; s < logSeq; s++) {
    if (s != from) o += ',';
    jstr(o, logBuf[s % LOG_LINES]);
  }
  xSemaphoreGive(logMutex);
  o += "]";
  sendJson(200, o);
}

static void handleTime() {
  AUTH();
  long t = server.arg("t").toInt();
  if (!timeSynced() && t > 1700000000L && t < 4102444800L) {
    struct timeval tv;
    tv.tv_sec = (time_t)t;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    dlog("Heure réglée depuis le navigateur");
    applyTimeFix();
  }
  ok();
}

static void setAntenna(int ext) {
#if XIAO_RF_SWITCH
  pinMode(14, OUTPUT);
  digitalWrite(14, ext ? HIGH : LOW);
#endif
}

static void handleConfigGet() {
  AUTH();
  MqttCfg c = mqttSnapshot();
  auto B = [](bool v) { return v ? "true" : "false"; };
  String s = "{\"wifiSsid\":";
  jstr(s, cfg.wifiSsid.c_str());
  s += ",\"wifiPassSet\":" + String(B(cfg.wifiPass.length()));
  s += ",\"hostname\":";
  jstr(s, cfg.hostname.c_str());
  s += ",\"apMode\":" + String(cfg.apMode) + ",\"apName\":";
  jstr(s, cfg.apName.c_str());
  s += ",\"apNameNow\":";
  jstr(s, apName.c_str());
  s += ",\"apPassSet\":" + String(B(cfg.apPass.length())) + ",\"staGiveupSec\":" + String(cfg.staGiveupSec);
  s += ",\"mqttEnabled\":" + String(B(c.enabled)) + ",\"mqttHost\":";
  jstr(s, c.host.c_str());
  s += ",\"mqttPort\":" + String(c.port) + ",\"mqttUser\":";
  jstr(s, c.user.c_str());
  s += ",\"mqttPassSet\":" + String(B(c.pass.length())) + ",\"mqttBase\":";
  jstr(s, c.base.c_str());
  s += ",\"mqttRetain\":" + String(B(c.retain)) + ",\"mqttDroneSec\":" + String(cfg.mqttDroneSec) + ",\"mqttListSec\":" +
       String(cfg.mqttListSec) + ",\"pubDrones\":" + String(B(c.pubDrones)) + ",\"pubList\":" + String(B(c.pubList)) +
       ",\"pubWifi\":" + String(B(c.pubWifi)) + ",\"pubBt\":" + String(B(c.pubBt)) + ",\"pubSens\":" + String(B(c.pubSens));
  s += ",\"mode\":" + String(cfg.mode) + ",\"wifiSurvey\":" + String(B(cfg.wifiSurvey)) + ",\"btSurvey\":" + String(B(cfg.btSurvey)) +
       ",\"chan\":" + String(cfg.chan) + ",\"rssiMin\":" + String(cfg.rssiMin) + ",\"bleDuty\":" + String(cfg.bleDuty) + ",\"antScanner\":" + String(cfg.antScanner) +
       ",\"antGateway\":" + String(cfg.antGateway);
  s += ",\"expireMin\":" + String(cfg.expireMin) + ",\"expireNetMin\":" + String(cfg.expireNetMin) + ",\"gapMin\":" +
       String(cfg.gapMin) + ",\"saveSec\":" + String(cfg.saveSec);
  s += ",\"syslogOn\":" + String(B(cfg.syslogOn)) + ",\"syslogHost\":";
  jstr(s, cfg.syslogHost.c_str());
  s += ",\"syslogPort\":" + String(cfg.syslogPort) + ",\"syslogFmt\":" + String(cfg.syslogFmt) + ",\"syslogBody\":" + String(cfg.syslogBody) +
       ",\"syslogFac\":" + String(cfg.syslogFac) + ",\"slDrones\":" + String(B(cfg.slDrones)) + ",\"slWifi\":" + String(B(cfg.slWifi)) +
       ",\"slBt\":" + String(B(cfg.slBt)) + ",\"slSystem\":" + String(B(cfg.slSystem)) + ",\"slDroneSec\":" + String(cfg.slDroneSec);
  s += ",\"alertOn\":" + String(B(cfg.alertOn)) + ",\"alertMqtt\":" + String(B(cfg.alertMqtt)) + ",\"alertSyslog\":" + String(B(cfg.alertSyslog)) +
       ",\"haOn\":" + String(B(cfg.haOn)) + ",\"haPrefix\":";
  jstr(s, cfg.haPrefix.c_str());
  s += ",\"trackOn\":" + String(B(cfg.trackOn)) + ",\"trackMinM\":" + String(cfg.trackMinM) + ",\"trackKeepMin\":" + String(cfg.trackKeepMin);
  s += ",\"updRepo\":";
  jstr(s, cfg.updRepo.c_str());
  s += ",\"updTokenSet\":" + String(B(cfg.updToken.length()));
  s += ",\"vpnOn\":" + String(B(cfg.vpnOn)) + ",\"vpnFull\":" + String(B(cfg.vpnFull)) + ",\"vpnPort\":" + String(cfg.vpnPort) + ",\"vpnIp\":";
  jstr(s, cfg.vpnIp.c_str());
  s += ",\"vpnMask\":";
  jstr(s, cfg.vpnMask.c_str());
  s += ",\"vpnPeerPub\":";
  jstr(s, cfg.vpnPeerPub.c_str());
  s += ",\"vpnHost\":";
  jstr(s, cfg.vpnHost.c_str());
  s += ",\"vpnPrivSet\":" + String(B(cfg.vpnPriv.length())) + ",\"vpnPskSet\":" + String(B(cfg.vpnPsk.length()));
  s += ",\"webUser\":";
  jstr(s, cfg.webUser.c_str());
  s += ",\"webPassSet\":" + String(B(cfg.webPass.length())) + "}";
  sendJson(200, s);
}

static void handleConfigPost() {
  AUTH();
  bool wifiChanged = false, mqttChanged = false, apChanged = false;
  auto arg = [&](const char* k) { String v = server.arg(k); v.trim(); return v; };
  auto num = [&](const char* k, long lo, long hi, long cur) { return server.hasArg(k) ? (long)constrain(arg(k).toInt(), lo, hi) : cur; };
  auto flag = [&](const char* k, bool cur) { return server.hasArg(k) ? arg(k) == "1" : cur; };

  xSemaphoreTake(cfgMutex, portMAX_DELAY);
  if (server.hasArg("wifiSsid")) {
    String v = arg("wifiSsid");
    if (v != cfg.wifiSsid) { cfg.wifiSsid = v; wifiChanged = true; }
  }
  if (server.hasArg("wifiPass") && server.arg("wifiPass").length()) {   // vide = inchangé
    cfg.wifiPass = server.arg("wifiPass");
    wifiChanged = true;
  }
  if (server.hasArg("hostname")) {
    String h = arg("hostname");
    if (h.length() && h.length() <= 32) cfg.hostname = h;
  }
  uint8_t apMode = (uint8_t)num("apMode", 0, 2, cfg.apMode);
  if (apMode != cfg.apMode) { cfg.apMode = apMode; apChanged = true; }
  if (server.hasArg("apName")) {
    String v = arg("apName");
    if (v != cfg.apName) { cfg.apName = v; apChanged = true; }
  }
  if (server.hasArg("apPass") && server.arg("apPass").length() >= 8 && server.arg("apPass").length() <= 63) {
    cfg.apPass = server.arg("apPass");
    apChanged = true;
  }
  cfg.staGiveupSec = (uint16_t)num("staGiveupSec", 5, 600, cfg.staGiveupSec);

  if (server.hasArg("mqttEnabled")) { cfg.mqttEnabled = arg("mqttEnabled") == "1"; mqttChanged = true; }
  if (server.hasArg("mqttHost")) { cfg.mqttHost = arg("mqttHost"); mqttChanged = true; }
  if (server.hasArg("mqttPort")) { cfg.mqttPort = (uint16_t)constrain(arg("mqttPort").toInt(), 1, 65535); mqttChanged = true; }
  if (server.hasArg("mqttUser")) { cfg.mqttUser = arg("mqttUser"); mqttChanged = true; }
  if (server.hasArg("mqttPass") && server.arg("mqttPass").length()) { cfg.mqttPass = server.arg("mqttPass"); mqttChanged = true; }
  if (server.hasArg("mqttBase")) {
    String b = arg("mqttBase");
    while (b.endsWith("/")) b.remove(b.length() - 1);
    if (b.length()) cfg.mqttBase = b;
    mqttChanged = true;
  }
  cfg.mqttRetain = flag("mqttRetain", cfg.mqttRetain);
  cfg.mqttDroneSec = (uint16_t)num("mqttDroneSec", 1, 600, cfg.mqttDroneSec);
  cfg.mqttListSec = (uint16_t)num("mqttListSec", 2, 3600, cfg.mqttListSec);
  cfg.pubDrones = flag("pubDrones", cfg.pubDrones);
  cfg.pubList = flag("pubList", cfg.pubList);
  cfg.pubWifi = flag("pubWifi", cfg.pubWifi);
  cfg.pubBt = flag("pubBt", cfg.pubBt);
  cfg.pubSens = flag("pubSens", cfg.pubSens);

  cfg.mode = (uint8_t)num("mode", 0, 2, cfg.mode);
  cfg.wifiSurvey = flag("wifiSurvey", cfg.wifiSurvey);
  cfg.btSurvey = flag("btSurvey", cfg.btSurvey);
  cfg.chan = (uint8_t)num("chan", 0, 13, cfg.chan);
  cfg.rssiMin = (int16_t)num("rssiMin", -120, -20, cfg.rssiMin);
  cfg.bleDuty = (uint8_t)num("bleDuty", 10, 100, cfg.bleDuty);
  cfg.antScanner = (uint8_t)num("antScanner", 0, 1, cfg.antScanner);
  cfg.antGateway = (uint8_t)num("antGateway", 0, 1, cfg.antGateway);

  cfg.expireMin = (uint16_t)num("expireMin", 1, 1440, cfg.expireMin);
  cfg.expireNetMin = (uint16_t)num("expireNetMin", 1, 1440, cfg.expireNetMin);
  cfg.gapMin = (uint16_t)num("gapMin", 1, 1440, cfg.gapMin);
  cfg.saveSec = (uint16_t)num("saveSec", 30, 3600, cfg.saveSec);

  cfg.syslogOn = flag("syslogOn", cfg.syslogOn);
  if (server.hasArg("syslogHost")) cfg.syslogHost = arg("syslogHost");
  cfg.syslogPort = (uint16_t)num("syslogPort", 1, 65535, cfg.syslogPort);
  cfg.syslogFmt = (uint8_t)num("syslogFmt", 0, 1, cfg.syslogFmt);
  cfg.syslogBody = (uint8_t)num("syslogBody", 0, 1, cfg.syslogBody);
  cfg.syslogFac = (uint8_t)num("syslogFac", 0, 23, cfg.syslogFac);
  cfg.slDrones = flag("slDrones", cfg.slDrones);
  cfg.slWifi = flag("slWifi", cfg.slWifi);
  cfg.slBt = flag("slBt", cfg.slBt);
  cfg.slSystem = flag("slSystem", cfg.slSystem);
  cfg.slDroneSec = (uint16_t)num("slDroneSec", 1, 3600, cfg.slDroneSec);
  cfg.alertOn = flag("alertOn", cfg.alertOn);
  cfg.alertMqtt = flag("alertMqtt", cfg.alertMqtt);
  cfg.alertSyslog = flag("alertSyslog", cfg.alertSyslog);
  {
    bool ha = flag("haOn", cfg.haOn);
    String pf = server.hasArg("haPrefix") ? arg("haPrefix") : cfg.haPrefix;
    while (pf.endsWith("/")) pf.remove(pf.length() - 1);
    if (!pf.length()) pf = "homeassistant";
    if (ha != cfg.haOn || pf != cfg.haPrefix) haRedo = true;   // déclaration à refaire
    cfg.haOn = ha;
    cfg.haPrefix = pf;
  }
  cfg.trackOn = flag("trackOn", cfg.trackOn);
  cfg.trackMinM = (uint16_t)num("trackMinM", 1, 500, cfg.trackMinM);
  cfg.trackKeepMin = (uint16_t)num("trackKeepMin", 5, 1440, cfg.trackKeepMin);
  if (server.hasArg("updRepo")) cfg.updRepo = arg("updRepo");
  if (server.hasArg("updToken") && server.arg("updToken").length()) cfg.updToken = arg("updToken");
  if (server.hasArg("updTokenClear") && arg("updTokenClear") == "1") cfg.updToken = "";
  {
    bool vOn = flag("vpnOn", cfg.vpnOn), vFull = flag("vpnFull", cfg.vpnFull);
    String vIp = server.hasArg("vpnIp") ? arg("vpnIp") : cfg.vpnIp, vMask = server.hasArg("vpnMask") ? arg("vpnMask") : cfg.vpnMask;
    String vHost = server.hasArg("vpnHost") ? arg("vpnHost") : cfg.vpnHost, vPub = server.hasArg("vpnPeerPub") ? arg("vpnPeerPub") : cfg.vpnPeerPub;
    String vPriv = server.hasArg("vpnPriv") && server.arg("vpnPriv").length() ? arg("vpnPriv") : cfg.vpnPriv;
    uint16_t vPort = (uint16_t)num("vpnPort", 1, 65535, cfg.vpnPort);
    String vPsk = server.hasArg("vpnPsk") && server.arg("vpnPsk").length() ? arg("vpnPsk") : cfg.vpnPsk;
    if (server.hasArg("vpnPskClear") && arg("vpnPskClear") == "1") vPsk = "";
    if (vOn != cfg.vpnOn || vFull != cfg.vpnFull || vIp != cfg.vpnIp || vMask != cfg.vpnMask || vHost != cfg.vpnHost || vPub != cfg.vpnPeerPub ||
        vPriv != cfg.vpnPriv || vPsk != cfg.vpnPsk || vPort != cfg.vpnPort)
      vpnRestart = true;
    cfg.vpnOn = vOn; cfg.vpnFull = vFull; cfg.vpnIp = vIp; cfg.vpnMask = vMask; cfg.vpnHost = vHost; cfg.vpnPeerPub = vPub; cfg.vpnPriv = vPriv; cfg.vpnPsk = vPsk; cfg.vpnPort = vPort;
  }
  if (server.hasArg("webUser")) cfg.webUser = arg("webUser");
  if (server.hasArg("webPass") && server.arg("webPass").length()) cfg.webPass = server.arg("webPass");
  saveCfg();
  xSemaphoreGive(cfgMutex);
  setAntenna(cfg.antGateway);
  dlog("Réglages enregistrés%s%s%s", wifiChanged ? " (Wi-Fi modifié)" : "", mqttChanged ? " (MQTT modifié)" : "",
       apChanged ? " (point d'accès modifié)" : "");

  if (wifiChanged) wifiBegin();
  if (apChanged) {   // le point d'accès repart avec son nouveau nom / mot de passe
    if (apOn) {
      WiFi.softAPdisconnect(true);
      apOn = false;
    }
    char nm[40];
    uint8_t m[6];
    WiFi.macAddress(m);
    snprintf(nm, sizeof nm, "%s", AP_NAME);
    apName = cfg.apName.length() ? cfg.apName : String(nm);
  }
  if (mqttChanged || wifiChanged) { setMqttErr(""); mqttForce = true; }
  ok();
}

// Envoie un message de test au serveur syslog (UDP : aucune confirmation de réception possible)
static void handleSyslogTest() {
  AUTH();
  SlCfg c = slSnapshot();
  if (!c.host.length()) return sendJson(200, "{\"ok\":false,\"msg\":\"Aucun serveur syslog configuré : renseignez l'adresse puis enregistrez.\"}");
  if (WiFi.status() != WL_CONNECTED) return sendJson(200, "{\"ok\":false,\"msg\":\"La carte n'est pas connectée à votre réseau.\"}");
  IPAddress ip;
  if (!slResolve(c.host, ip)) return sendJson(200, "{\"ok\":false,\"msg\":\"Serveur introuvable (DNS). Utilisez son adresse IP.\"}");
  WiFiUDP udp;
  char body[200];
  slBodySystem("message de test ESP SCANNER", c, body, sizeof body);
  bool good = slSend(udp, ip, c, 6, "TEST", body);
  if (good) syslogSent++;
  String m = good ? String("Message envoyé à ") + ip.toString() + ":" + c.port + " (UDP : aucune confirmation possible, vérifiez sa réception sur le serveur)."
                  : String("Envoi impossible vers ") + ip.toString() + ":" + c.port + ".";
  String out = String("{\"ok\":") + (good ? "true" : "false") + ",\"msg\":";
  jstr(out, m.c_str());
  out += "}";
  sendJson(200, out);
}

static void handlePublish() {
  AUTH();
  if (!mqttConnected) return sendJson(409, "{\"error\":\"MQTT non connecté : voir le journal technique ou « Tester MQTT »\"}");
  mqttForce = true;
  dlog("MQTT : publication complète demandée");
  ok();
}

// Test MQTT pas à pas avec une connexion indépendante : Wi-Fi, port TCP, identification, publication
static void handleMqttTest() {
  AUTH();
  MqttCfg c = mqttSnapshot();
  String steps = "[";
  auto add = [&](bool good, const String& msg) {
    if (steps.length() > 1) steps += ',';
    steps += "{\"ok\":";
    steps += good ? "true" : "false";
    steps += ",\"msg\":";
    jstr(steps, msg.c_str());
    steps += "}";
  };
  bool all = false;

  if (!c.host.length()) {
    add(false, "Aucun serveur MQTT configuré : renseignez « Serveur MQTT » puis enregistrez.");
  } else if (WiFi.status() != WL_CONNECTED) {
    add(false, String("La carte n'est pas connectée à votre réseau (« ") + cfg.wifiSsid + " » : " + wifiStatusText(WiFi.status()) +
                   "). Sans lui, MQTT ne peut pas fonctionner.");
  } else {
    add(true, String("Wi-Fi OK : ") + WiFi.localIP().toString() + " (passerelle " + WiFi.gatewayIP().toString() + ")");
    WiFiClient probe;
    uint32_t t0 = millis();
    bool tcp = probe.connect(c.host.c_str(), c.port, 4000);
    uint32_t dt = millis() - t0;
    probe.stop();
    if (!tcp) {
      add(false, String("Port TCP ") + c.port + " de " + c.host + " injoignable après " + dt +
                     " ms. Vérifiez l'adresse IP, le port, que le broker tourne, le pare-feu de la machine, et qu'il écoute sur le réseau "
                     "(Mosquitto 2 n'écoute par défaut que sur localhost : ajoutez « listener 1883 »).");
    } else {
      add(true, String("Port TCP ") + c.port + " joignable (" + dt + " ms)");
      WiFiClient tn;
      PubSubClient tm(tn);
      char hostBuf[96];
      strlcpy(hostBuf, c.host.c_str(), sizeof hostBuf);
      tm.setServer(hostBuf, c.port);
      tm.setSocketTimeout(4);
      String cid = clientId("-test");
      bool good = c.user.length() ? tm.connect(cid.c_str(), c.user.c_str(), c.pass.c_str()) : tm.connect(cid.c_str());
      if (!good) {
        int st = tm.state();
        add(false, String("Connexion MQTT refusée, code ") + st + " : " + mqttStateText(st));
      } else {
        add(true, String("Identification MQTT acceptée") + (c.user.length() ? " (utilisateur « " + c.user + " »)" : " (sans utilisateur)"));
        String topic = c.base + "/test";
        String payload = String("{\"test\":true,\"uptime\":") + (millis() / 1000) + "}";
        if (tm.publish(topic.c_str(), payload.c_str(), false)) {
          add(true, String("Message de test publié sur « ") + topic + " ». Vérifiez-le côté broker (mosquitto_sub -t '" + c.base + "/#' -v).");
          all = true;
          mqttForce = true;
        } else {
          add(false, "Publication refusée par la connexion.");
        }
        tm.loop();
        tm.disconnect();
      }
    }
  }
  steps += "]";
  sendJson(200, String("{\"ok\":") + (all ? "true" : "false") + ",\"steps\":" + steps + "}");
}

// Ajoute un drone fictif (liste en direct + journal + MQTT) pour tester toute la chaîne sans drone réel
static void handleTestDrone() {
  AUTH();
  Drone t;
  uint8_t mac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
  memcpy(t.mac, mac, 6);
  strlcpy(t.id, "TEST-DRONE-0001", sizeof t.id);
  t.hasId = true;
  t.idType = 1;
  t.uaType = 2;
  t.hasLoc = true;
  t.status = 2;
  t.hasPos = true;
  t.lat = 48.8566 + random(400) / 100000.0;
  t.lon = 2.3522 + random(400) / 100000.0;
  t.altGeo = 120;
  t.height = 60;
  t.spdH = 8.5f;
  t.dir = 90;
  t.hasOp = true;
  t.opLat = 48.8566;
  t.opLon = 2.3522;
  strlcpy(t.opId, "FRATEST1234", sizeof t.opId);
  strlcpy(t.desc, "Drone de test", sizeof t.desc);
  t.src = 16;
  t.rssi = -60;
  t.frames = 1;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  ingestDrone(t);
  xSemaphoreGive(dMutex);
  mqttForce = true;
  dlog("Drone de test ajouté");
  ok();
}

static void handleClear() {
  AUTH();
  xSemaphoreTake(dMutex, portMAX_DELAY);
  String what = server.arg("what");   // drones (défaut), wifi, bt
  if (what == "wifi") { for (auto& x : nets) x.used = false; }
  else if (what == "bt") { for (auto& x : bts) x.used = false; }
  else if (what == "sens") { for (auto& x : sens) x.used = false; }
  else { for (auto& d : drones) d.used = false; }
  xSemaphoreGive(dMutex);
  mqttForce = true;
  ok();
}

// ----------------------------------------------------------------------------
//  Traces de vol
// ----------------------------------------------------------------------------
static void handleTracks() {
  AUTH();
  bool geo = server.uri().endsWith(".geojson");
  String s;
  s.reserve(8192);
  xSemaphoreTake(dMutex, portMAX_DELAY);
  char mac[18];
  bool first = true;
  s += geo ? "{\"type\":\"FeatureCollection\",\"features\":[" : "{\"tracks\":[";
  for (auto& t : tracks) {
    if (!t.used || t.n == 0) continue;
    jnlMac(t.mac, mac, sizeof mac);
    if (!first) s += ",";
    first = false;
    uint16_t start = (t.head + TRACK_PTS - t.n) % TRACK_PTS;
    char buf[64];
    if (geo) {
      if (t.n >= 2) {
        s += "{\"type\":\"Feature\",\"properties\":{\"id\":";
        jstr(s, t.id);
        s += ",\"mac\":\"" + String(mac) + "\",\"points\":" + String(t.n) + "},\"geometry\":{\"type\":\"LineString\",\"coordinates\":[";
        for (uint16_t i = 0; i < t.n; i++) {
          const TrackPt& p = t.pts[(start + i) % TRACK_PTS];
          if (p.alt == -32768) snprintf(buf, sizeof buf, "%s[%.6f,%.6f]", i ? "," : "", p.lon6 / 1e6, p.lat6 / 1e6);
          else snprintf(buf, sizeof buf, "%s[%.6f,%.6f,%d]", i ? "," : "", p.lon6 / 1e6, p.lat6 / 1e6, (int)p.alt);
          s += buf;
        }
        s += "]}},";
      }
      const TrackPt& l = t.pts[(t.head + TRACK_PTS - 1) % TRACK_PTS];
      s += "{\"type\":\"Feature\",\"properties\":{\"id\":";
      jstr(s, t.id);
      s += ",\"mac\":\"" + String(mac) + "\"},\"geometry\":{\"type\":\"Point\",\"coordinates\":";
      snprintf(buf, sizeof buf, "[%.6f,%.6f]}}", l.lon6 / 1e6, l.lat6 / 1e6);
      s += buf;
    } else {
      s += "{\"id\":";
      jstr(s, t.id);
      s += ",\"mac\":\"" + String(mac) + "\",\"pts\":[";
      for (uint16_t i = 0; i < t.n; i++) {
        const TrackPt& p = t.pts[(start + i) % TRACK_PTS];
        snprintf(buf, sizeof buf, "%s[%.6f,%.6f,%d,%lu]", i ? "," : "", p.lat6 / 1e6, p.lon6 / 1e6, p.alt == -32768 ? 0 : (int)p.alt,
                 (unsigned long)p.up);
        s += buf;
      }
      s += "]}";
    }
  }
  xSemaphoreGive(dMutex);
  s += "]}";
  if (geo) server.sendHeader("Content-Disposition", "attachment; filename=\"dronescan-traces.geojson\"");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, geo ? "application/geo+json" : "application/json", s);
}

static void handleTracksClear() {
  AUTH();
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& t : tracks) {
    if (t.pts) free(t.pts);
    t = Track();
  }
  xSemaphoreGive(dMutex);
  ok();
}

// ----------------------------------------------------------------------------
//  Mise à jour OTA par la page web : ESP 2 directement, ESP 1 relayée par la liaison série
// ----------------------------------------------------------------------------
static volatile uint32_t rebootAt = 0;
static struct {
  bool active = false, authFail = false, bad = false;
  int target = 0;           // 2 = cette carte, 1 = scanner
  uint32_t size = 0, got = 0, sent = 0;
  uint8_t chunk[OTA_CHUNK];
  size_t clen = 0;
  uint32_t idx = 0;
  char msg[96] = "";
} ota;

static void otaFail(const char* m) {
  ota.bad = true;
  strlcpy(ota.msg, m, sizeof ota.msg);
  if (ota.target == 2) Update.abort();
  if (ota.target == 1) otaRelay = false;
}

static bool otaLinkSend(const char* line, size_t n) {
  if (!n) return false;
  Serial1.write((const uint8_t*)line, n);
  return true;
}

// envoie une ligne et attend l'accusé voulu (3 essais de 2 s)
static bool otaLinkExchange(const char* line, size_t n, uint32_t wantAck) {
  for (int t = 0; t < 3; t++) {
    otaAckIdx = 0xFFFFFFFEu;
    otaResultCode = -1;
    otaLinkSend(line, n);
    uint32_t t0 = millis();
    while (millis() - t0 < 5000) {
      if (otaAckIdx == wantAck) return true;
      if (otaResultCode > 0) return false;   // erreur annoncée par l'ESP 1
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
  return false;
}

static bool otaSendChunk() {
  static char line[OTA_CHUNK * 4 / 3 + 48];
  size_t n = otaLineData(ota.idx, ota.chunk, ota.clen, line, sizeof line);
  if (!otaLinkExchange(line, n, ota.idx)) return false;
  ota.idx++;
  ota.sent += ota.clen;
  ota.clen = 0;
  return true;
}

// Trois temps, utilisés par l'envoi depuis la page ET par le téléchargement GitHub
static bool otaStart(int target, uint32_t size) {
  ota = decltype(ota)();
  ota.target = target;
  ota.active = true;
  ota.size = size;
  if (target == 1) {
    if (size < 100000 || size > 0x1A0000) { otaFail("taille du fichier incorrecte pour l'ESP 1"); return false; }
    if (!scannerAlive()) { otaFail("l'ESP 1 ne répond pas (liaison coupée)"); return false; }
    otaRelay = true;
    vTaskDelay(pdMS_TO_TICKS(30));
    static char line[48];
    size_t n = otaLineBegin(size, line, sizeof line);
    otaAckIdx = 0xFFFFFFFEu;
    otaResultCode = -1;
    otaLinkSend(line, n);
    uint32_t t0 = millis();
    while (otaAckIdx != 0xFFFFFFFFu && otaResultCode <= 0 && millis() - t0 < 60000) vTaskDelay(pdMS_TO_TICKS(20));   // effacement de la partition : jusqu'à ~30 s
    if (otaAckIdx != 0xFFFFFFFFu) { otaFail("l'ESP 1 refuse la mise à jour (pas de place, ou ancienne version sans OTA)"); return false; }
  }
  dlog("OTA %s : début (%lu octets)", target == 1 ? "ESP 1" : "ESP 2", (unsigned long)size);
  return true;
}

static bool otaPush(const uint8_t* buf, size_t len) {
  if (!ota.active || ota.bad) return false;
  if (ota.got == 0) {   // en-tête d'image ESP32 : 0xE9, puce ESP32-C6 (13) à l'offset 12
    if (len < 16 || buf[0] != 0xE9 || buf[12] != 13 || buf[13] != 0) {
      otaFail("ce fichier n'est pas un firmware pour ESP32-C6 (utilisez firmware.bin, pas bootloader.bin)");
      return false;
    }
    if (ota.target == 2 && !Update.begin(UPDATE_SIZE_UNKNOWN)) { otaFail("pas de place pour la mise à jour (partition OTA absente : flashez d'abord par USB)"); return false; }
  }
  if (ota.target == 2) {
    if (Update.write((uint8_t*)buf, len) != len) { otaFail("écriture en mémoire flash refusée"); return false; }
  } else {
    size_t off = 0;
    while (off < len) {
      size_t k = (size_t)OTA_CHUNK - ota.clen;
      if (k > len - off) k = len - off;
      memcpy(ota.chunk + ota.clen, buf + off, k);
      ota.clen += k;
      off += k;
      if (ota.clen == OTA_CHUNK && !otaSendChunk()) { otaFail("transfert vers l'ESP 1 interrompu (liaison série)"); return false; }
    }
  }
  ota.got += len;
  return true;
}

static bool otaEnd() {
  if (!ota.active || ota.bad) return false;
  if (ota.target == 2) {
    if (!Update.end(true)) { otaFail("le firmware reçu est invalide ou incomplet"); return false; }
    rebootAt = millis() + 1500;
  } else {
    if (ota.clen && !otaSendChunk()) { otaFail("transfert vers l'ESP 1 interrompu (fin)"); return false; }
    static char line[24];
    size_t n = otaLineEnd(line, sizeof line);
    otaResultCode = -1;
    otaLinkSend(line, n);
    uint32_t t0 = millis();
    while (otaResultCode < 0 && millis() - t0 < 8000) vTaskDelay(pdMS_TO_TICKS(5));
    otaRelay = false;
    if (otaResultCode != 0) { otaFail("l'ESP 1 a refusé le firmware (contrôle final)"); return false; }
  }
  dlog("OTA %s : terminé (%lu octets)", ota.target == 1 ? "ESP 1" : "ESP 2", (unsigned long)ota.got);
  return true;
}

static void otaUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    if (cfg.webUser.length() && !server.authenticate(cfg.webUser.c_str(), cfg.webPass.c_str())) {
      ota = decltype(ota)();
      ota.authFail = true;
      return;
    }
    otaStart(server.uri().endsWith("/scanner") ? 1 : 2, (uint32_t)server.arg("size").toInt());
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!ota.authFail) otaPush(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (!ota.authFail) otaEnd();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (ota.target == 2) Update.abort();
    otaRelay = false;
    ota.bad = true;
    strlcpy(ota.msg, "envoi interrompu", sizeof ota.msg);
  }
}

static void handleOtaDone() {
  if (ota.authFail || (cfg.webUser.length() && !server.authenticate(cfg.webUser.c_str(), cfg.webPass.c_str()))) {
    server.requestAuthentication();
    return;
  }
  if (!ota.active || ota.bad) {
    String s = "{\"ok\":false,\"error\":";
    jstr(s, ota.msg[0] ? ota.msg : "aucun fichier reçu");
    s += "}";
    if (ota.bad) dlog("OTA échec : %s", ota.msg);
    sendJson(200, s);
    return;
  }
  sendJson(200, String("{\"ok\":true,\"target\":") + ota.target + ",\"bytes\":" + ota.got + "}");
}

// ----------------------------------------------------------------------------
//  Mise à jour depuis GitHub (dépôt privé, jeton en lecture seule) : vérifier puis installer
// ----------------------------------------------------------------------------
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");
template <class C>
static auto setBundle(C& c, int) -> decltype(c.setCACertBundle(rootca_crt_bundle_start, (size_t)(rootca_crt_bundle_end - rootca_crt_bundle_start)), void()) {
  c.setCACertBundle(rootca_crt_bundle_start, (size_t)(rootca_crt_bundle_end - rootca_crt_bundle_start));
}
template <class C>
static auto setBundle(C& c, long) -> decltype(c.setCACertBundle(rootca_crt_bundle_start), void()) {
  c.setCACertBundle(rootca_crt_bundle_start);
}

static struct {
  String latest, notes, urlGw, urlSc, err;
  bool checked = false;
} upd;

// compare « v1.2.3 » et « 1.10.0 » : >0 si a est plus récent que b
static int verCmp(const char* a, const char* b) {
  if (*a == 'v' || *a == 'V') a++;
  if (*b == 'v' || *b == 'V') b++;
  for (int i = 0; i < 3; i++) {
    long x = strtol(a, (char**)&a, 10), y = strtol(b, (char**)&b, 10);
    if (x != y) return x > y ? 1 : -1;
    if (*a == '.') a++;
    if (*b == '.') b++;
  }
  return 0;
}

// valeur d'une clé texte JSON à partir de la position « from » (sans échappements exotiques)
static String jsonStr(const String& j, const char* key, int from = 0, int* endPos = nullptr) {
  String k = String("\"") + key + "\":\"";
  int i = j.indexOf(k, from);
  if (i < 0) return "";
  i += k.length();
  String out;
  for (; i < (int)j.length(); i++) {
    char c = j[i];
    if (c == '"') break;
    if (c == '\\' && i + 1 < (int)j.length()) {
      char n = j[++i];
      out += n == 'n' ? ' ' : (n == 'r' || n == 't') ? ' ' : n;
    } else out += c;
  }
  if (endPos) *endPos = i;
  return out;
}

// URL de l'API d'un fichier de la publication : « "url" » précède « "name" » dans chaque objet
static String assetUrl(const String& j, const char* name) {
  int n = j.indexOf(String("\"name\":\"") + name + "\"");
  if (n < 0) return "";
  int u = j.lastIndexOf("\"url\":\"", n);
  if (u < 0) return "";
  return jsonStr(j, "url", u);
}

static const char* updRepoPath(String& out) {
  out = cfg.updRepo;
  out.trim();
  if (out.startsWith("https://github.com/")) out.remove(0, 19);
  while (out.endsWith("/")) out.remove(out.length() - 1);
  if (out.endsWith(".git")) out.remove(out.length() - 4);
  return out.indexOf('/') > 0 ? out.c_str() : nullptr;
}

static bool updBegin(HTTPClient& h, WiFiClientSecure& cl, const String& url, const char* accept, bool withAuth) {
  setBundle(cl, 0);
  cl.setTimeout(15);
  if (!h.begin(cl, url)) return false;
  h.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);   // le jeton ne doit pas suivre vers un autre serveur
  h.setUserAgent("DroneScan");
  h.addHeader("Accept", accept);
  if (withAuth && cfg.updToken.length()) h.addHeader("Authorization", "Bearer " + cfg.updToken);
  h.addHeader("X-GitHub-Api-Version", "2022-11-28");
  return true;
}

static String updCheck() {
  upd = decltype(upd)();
  upd.checked = true;
  String repo;
  if (!updRepoPath(repo)) { upd.err = "dépôt non renseigné (Réglages → Mise à jour : « compte/dépôt »)"; return upd.err; }
  if (WiFi.status() != WL_CONNECTED) { upd.err = "pas de connexion à votre réseau Wi-Fi"; return upd.err; }
  static WiFiClientSecure cl;
  HTTPClient h;
  if (!updBegin(h, cl, String("https://api.github.com/repos/") + repo + "/releases/latest", "application/vnd.github+json", true)) {
    upd.err = "connexion impossible";
    return upd.err;
  }
  int code = h.GET();
  if (code != 200) {
    upd.err = code == 404 ? "dépôt ou publication introuvable (jeton sans accès, ou aucune publication)"
              : code == 401 ? "jeton refusé (expiré ou incorrect)"
              : code > 0 ? String("GitHub a répondu ") + code
                         : String("connexion refusée : ") + HTTPClient::errorToString(code);
    h.end();
    return upd.err;
  }
  String body = h.getString();
  h.end();
  upd.latest = jsonStr(body, "tag_name");
  upd.notes = jsonStr(body, "body");
  if (upd.notes.length() > 300) upd.notes = upd.notes.substring(0, 300);
  upd.urlGw = assetUrl(body, "gateway.bin");
  upd.urlSc = assetUrl(body, "scanner.bin");
  if (!upd.latest.length()) upd.err = "réponse GitHub illisible";
  return upd.err;
}

// télécharge un fichier de la publication et l'installe (target 2 = cette carte, 1 = scanner)
static String updInstall(int target) {
  const String& api = target == 1 ? upd.urlSc : upd.urlGw;
  if (!api.length()) return "fichier absent de la publication (gateway.bin / scanner.bin) : relancez la recherche";
  static WiFiClientSecure cl;
  HTTPClient h;
  if (!updBegin(h, cl, api, "application/octet-stream", true)) return "connexion impossible";
  const char* want[] = {"Location"};
  h.collectHeaders(want, 1);
  int code = h.GET();
  String url = api;
  if (code == 302 || code == 301 || code == 307) {   // adresse signée : on la suit SANS le jeton
    url = h.header("Location");
    h.end();
    cl.stop();
    if (!url.startsWith("https://")) return "redirection GitHub inattendue";
    static WiFiClientSecure cl2;
    if (!updBegin(h, cl2, url, "application/octet-stream", false)) return "connexion impossible (téléchargement)";
    code = h.GET();
  }
  if (code != 200) {
    String e = code > 0 ? String("téléchargement refusé (HTTP ") + code + ")" : String("téléchargement impossible : ") + HTTPClient::errorToString(code);
    h.end();
    return e;
  }
  int total = h.getSize();
  if (total < 100000) { h.end(); return "taille du fichier inattendue"; }
  if (!otaStart(target, (uint32_t)total)) { h.end(); return ota.msg; }
  WiFiClient* st = h.getStreamPtr();
  static uint8_t buf[1024];
  int left = total;
  uint32_t last = millis();
  while (left > 0 && !ota.bad) {
    size_t want_n = left > (int)sizeof buf ? sizeof buf : (size_t)left;
    size_t got = st->readBytes(buf, want_n);
    if (got == 0) {
      if (millis() - last > 15000) { otaFail("téléchargement interrompu"); break; }
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    last = millis();
    if (!otaPush(buf, got)) break;
    left -= (int)got;
    vTaskDelay(1);
  }
  h.end();
  if (!ota.bad && left == 0) otaEnd();
  else if (!ota.bad) otaFail("fichier incomplet");
  return ota.bad ? String(ota.msg) : String("");
}

static void handleUpdCheck() {
  AUTH();
  String e = updCheck();
  String s = "{\"ok\":" + String(e.length() ? "false" : "true") + ",\"current\":";
  jstr(s, FW_VERSION);
  s += ",\"latest\":";
  jstr(s, upd.latest.c_str());
  char scv[28] = "";
  bool scAlive;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  strlcpy(scv, scHb.fw, sizeof scv);
  scAlive = scannerAlive();
  xSemaphoreGive(dMutex);
  for (char* q = scv; *q; q++) if (*q == ' ') { *q = 0; break; }   // « 1.2.0 Oct 27 2026 … » -> « 1.2.0 »
  s += ",\"scCurrent\":";
  jstr(s, scAlive ? scv : "");
  s += ",\"newer\":" + String(!e.length() && verCmp(upd.latest.c_str(), FW_VERSION) > 0 ? "true" : "false");
  s += ",\"newerSc\":" + String(!e.length() && scAlive && verCmp(upd.latest.c_str(), scv) > 0 ? "true" : "false");
  s += ",\"hasGw\":" + String(upd.urlGw.length() ? "true" : "false") + ",\"hasSc\":" + String(upd.urlSc.length() ? "true" : "false") + ",\"notes\":";
  jstr(s, upd.notes.c_str());
  s += ",\"error\":";
  jstr(s, e.c_str());
  s += "}";
  sendJson(200, s);
}

static void handleUpdInstall() {
  AUTH();
  int target = server.arg("target") == "scanner" ? 1 : 2;
  if (!upd.checked) { sendJson(200, "{\"ok\":false,\"error\":\"Lancez d'abord la recherche de mise à jour.\"}"); return; }
  String e = updInstall(target);
  String s = "{\"ok\":" + String(e.length() ? "false" : "true") + ",\"error\":";
  jstr(s, e.c_str());
  s += "}";
  if (e.length()) dlog("Mise à jour GitHub : %s", e.c_str());
  sendJson(200, s);
}

static void handleRestart() {
  AUTH();
  ok();
  saveJournal();
  delay(300);
  ESP.restart();
}

static void expireDrones() {
  uint32_t now = millis(), maxAge = (uint32_t)cfg.expireMin * 60000UL, maxNet = (uint32_t)cfg.expireNetMin * 60000UL;
  xSemaphoreTake(dMutex, portMAX_DELAY);
  for (auto& d : drones)
    if (d.used && now - d.lastMs > maxAge) {
      d.used = false;
      if (d.logSeen) {
        strlcpy(lostQ[lostHead % 8][0], d.hasId ? d.id : "", 41);
        jnlMac(d.mac, lostQ[lostHead % 8][1], 41);
        lostHead++;
      }
    }
  for (auto& x : tracks)
    if (x.used && now - x.lastMs > (uint32_t)cfg.trackKeepMin * 60000UL) {
      free(x.pts);
      x = Track();
    }
  for (auto& n : nets)
    if (n.used && now - n.lastMs > maxNet) n.used = false;
  for (auto& d : bts)
    if (d.used && now - d.lastMs > maxNet) d.used = false;
  for (auto& x : sens)
    if (x.used && now - x.lastMs > 86400000UL) x.used = false;   // un capteur peut se taire longtemps : 24 h
  xSemaphoreGive(dMutex);
}

// ----------------------------------------------------------------------------
//  setup / loop
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n== ESP 2 — PASSERELLE ESP SCANNER ==");

#if XIAO_RF_SWITCH
  // XIAO ESP32-C6 : GPIO3 à LOW active le commutateur RF, GPIO14 choisit l'antenne (réglée plus bas)
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);
  delay(100);
#endif

  dMutex = xSemaphoreCreateMutex();
  cfgMutex = xSemaphoreCreateMutex();
  logMutex = xSemaphoreCreateMutex();

  loadCfg();
  nextBoot();
  setAntenna(cfg.antGateway);
  dlog("Antenne : %s", cfg.antGateway ? "externe (U.FL)" : "céramique intégrée");

  fsOk = LittleFS.begin(true);
  if (!fsOk) dlog("ERREUR : système de fichiers inutilisable, le journal ne sera pas conservé au redémarrage");
  loadJournal();
  dlog("Démarrage n°%u, RAM libre %u octets", bootId, (unsigned)ESP.getFreeHeap());

  Serial1.setRxBufferSize(8192);
  Serial1.setTxBufferSize(256);
  Serial1.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(cfg.hostname.c_str());
  WiFi.mode(WIFI_STA);
  uint8_t m[6];
  WiFi.macAddress(m);
  char nm[40];
  snprintf(nm, sizeof nm, "%s", AP_NAME);
  apName = cfg.apName.length() ? cfg.apName : String(nm);
  wifiBegin();
  if (cfg.wifiSsid.length()) dlog("Connexion au Wi-Fi « %s »…", cfg.wifiSsid.c_str());
  else dlog("Aucun Wi-Fi configuré : point d'accès de secours");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/drones", HTTP_GET, handleDrones);
  server.on("/api/wifi", HTTP_GET, handleWifiList);
  server.on("/api/ble", HTTP_GET, handleBtList);
  server.on("/api/sensors", HTTP_GET, handleSensList);
  server.on("/api/journal", HTTP_GET, handleJournal);
  server.on("/api/journal.csv", HTTP_GET, handleJournalCsv);
  server.on("/api/journal/clear", HTTP_POST, handleJournalClear);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/time", HTTP_POST, handleTime);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/publish", HTTP_POST, handlePublish);
  server.on("/api/mqtt/test", HTTP_POST, handleMqttTest);
  server.on("/api/syslog/test", HTTP_POST, handleSyslogTest);
  server.on("/api/test/drone", HTTP_POST, handleTestDrone);
  server.on("/api/clear", HTTP_POST, handleClear);
  server.on("/api/tracks", HTTP_GET, handleTracks);
  server.on("/api/tracks.geojson", HTTP_GET, handleTracks);
  server.on("/api/tracks/clear", HTTP_POST, handleTracksClear);
  server.on("/api/update/check", HTTP_POST, handleUpdCheck);
  server.on("/api/update/install", HTTP_POST, handleUpdInstall);
  server.on("/api/ota/gateway", HTTP_POST, handleOtaDone, otaUpload);
  server.on("/api/ota/scanner", HTTP_POST, handleOtaDone, otaUpload);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.onNotFound([] { server.send(404, "text/plain", "404"); });
  server.begin();

  xTaskCreate(uartTask, "uart", 6144, nullptr, 3, nullptr);
  xTaskCreate(mqttTask, "mqtt", 10240, nullptr, 1, nullptr);
  xTaskCreate(syslogTask, "syslog", 8192, nullptr, 1, nullptr);
}

void loop() {
  uint32_t now = millis();
  server.handleClient();
  wifiTick(now);
  vpnTick(now);
  if (rebootAt && (int32_t)(now - rebootAt) >= 0) {
    saveJournal();
    ESP.restart();
  }

  static uint32_t lastHousekeeping = 0, lastSave = 0;
  if (now - lastHousekeeping >= 5000) {
    lastHousekeeping = now;
    expireDrones();
  }
  if (now - lastSave >= (uint32_t)cfg.saveSec * 1000UL) {
    lastSave = now;
    saveJournal();   // n'écrit que si le journal a changé
  }
  delay(2);
}
