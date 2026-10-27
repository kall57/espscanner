#pragma once
// ============================================================================
//  Réglages communs aux deux cartes (ESP 1 « scanner » et ESP 2 « passerelle »)
//  Tout ce qui est marqué « modifiable depuis la page web » peut aussi être changé
//  dans l'interface de l'ESP 2 (la valeur enregistrée dans la page l'emporte alors).
// ============================================================================

// ---- Liaison série entre les deux cartes (câbles de 10 cm) ----
//  ESP 1 D6 (TX, GPIO16) -> ESP 2 D7 (RX, GPIO17)
//  ESP 1 D7 (RX, GPIO17) <- ESP 2 D6 (TX, GPIO16)
//  GND <-> GND  (obligatoire)      Niveau logique : 3,3 V (ne pas relier de 5 V)
#define LINK_TX_PIN 16
#define LINK_RX_PIN 17
#define LINK_BAUD 921600

// ---- XIAO ESP32-C6 : commutateur d'antenne (GPIO3 / GPIO14) ----
#define XIAO_RF_SWITCH 1       // mettre 0 sur une autre carte
#define ANTENNA_EXTERNAL 1     // 0 = céramique intégrée, 1 = connecteur U.FL externe

// ---- ESP 1 (scanner) ----
#define DEFAULT_SCAN_MODE 0    // 0 = Bluetooth + Wi-Fi, 1 = Bluetooth seul, 2 = Wi-Fi seul (commandable depuis la page web)
#define MAX_DRONES 32          // drones suivis en mémoire
#define MAX_NETS 64            // réseaux Wi-Fi suivis en mémoire
#define MAX_SENS 24            // capteurs Bluetooth (BTHome, Flora) suivis en mémoire
#define SENS_SEND_MS 1000      // un même capteur est renvoyé à l'ESP 2 au plus une fois par seconde
#define MAX_BT 64              // appareils Bluetooth suivis en mémoire
#define NET_SEND_MS 4000       // un même réseau / appareil n'est renvoyé à l'ESP 2 qu'une fois toutes les 4 s
#define DEFAULT_BLE_DUTY 50    // part du temps d'antenne donnée au Bluetooth quand Bluetooth ET Wi-Fi écoutent (%) ; le reste va au Wi-Fi
#define DEFAULT_RSSI_MIN -95   // signaux plus faibles ignorés pour les réseaux / appareils (dBm)
#define DEFAULT_EXPIRE_NET_MIN 5  // un réseau Wi-Fi / appareil Bluetooth non revu depuis ce délai (min) disparaît de la liste
#define SEND_MIN_MS 1000       // un même drone n'est renvoyé à l'ESP 2 qu'une fois par seconde au plus

// ---- ESP 2 (passerelle) : Wi-Fi ----
//  L'ESP 2 rejoint ce réseau. Il ne crée son propre point d'accès QUE si ce réseau est
//  absent ou inaccessible, et le referme dès que le réseau est revenu (sauf si quelqu'un y est connecté).
#define DEFAULT_WIFI_SSID ""   // modifiable depuis la page web
#define DEFAULT_WIFI_PASS ""
#define AP_NAME "espscanner"              // nom du point d'accès de secours par défaut (modifiable dans Réglages)
#define AP_PASS "12345678"              // mot de passe par défaut du point d'accès de secours (8 caractères minimum) : changez-le dans Réglages
#define STA_GIVEUP_MS 20000             // sans connexion au bout de ce délai, le point d'accès de secours s'ouvre
#define HOSTNAME "dronescan"            // http://dronescan.local/ sur votre réseau

// ---- ESP 2 (passerelle) : MQTT ----
#define DEFAULT_MQTT_HOST ""                 // vide = MQTT inactif ; à renseigner depuis la page web
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_USER ""                 // laisser vide si le broker n'exige pas d'identifiants
#define DEFAULT_MQTT_PASS ""
#define DEFAULT_MQTT_BASE "dronescan"
#define MQTT_DRONE_MIN_MS 2000     // un message par drone au plus toutes les 2 s
#define MQTT_LIST_EVERY_MS 10000   // liste complète des équipements détectés, toutes les 10 s

// ---- ESP 2 (passerelle) : liste, journal ----
#define DEFAULT_EXPIRE_MIN 10      // un drone non vu depuis ce délai disparaît de la liste « en direct »
#define JOURNAL_MAX 100            // lignes conservées dans le journal (en flash)
#define JOURNAL_GAP_SEC 300        // drone absent plus de 5 min = nouvelle ligne du journal
#define JOURNAL_SAVE_EVERY_SEC 120 // écriture du journal en flash au plus toutes les 2 min

// ---- ESP 2 (passerelle) : syslog (UDP) ----
#define DEFAULT_SYSLOG_HOST ""       // vide = désactivé ; modifiable depuis la page web
#define DEFAULT_SYSLOG_PORT 514

// Protection optionnelle de l'interface web (HTTP Basic). Laisser WEB_USER vide pour désactiver.
#define WEB_USER ""
#define WEB_PASS ""

// Numéro de version du projet (identique pour les deux cartes ; à augmenter à chaque publication GitHub)
#define FW_VERSION "1.8.3"
