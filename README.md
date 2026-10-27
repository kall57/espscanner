# ESP SCANNER — 2 × Seeed XIAO ESP32-C6

> **EN —** Passive drone **Remote ID** detector (ASTM F3411 / ASD-STAN, incl. French electronic signalling) on two Seeed XIAO ESP32-C6 boards: ESP 1 listens (BLE 4/5 incl. Coded PHY, Wi-Fi beacon/NAN), ESP 2 serves the web UI, journal, MQTT/Home Assistant, syslog, OTA/GitHub updates and an optional WireGuard client. Build with PlatformIO (pioarduino). Documentation below is in French; issues and pull requests in English or French are welcome.

> **Avertissement.** Ce projet reçoit *passivement* des diffusions radio publiques (Remote ID). Il est fourni « tel quel », sans garantie, pour un usage personnel et légal. Les données reçues (position d'un opérateur, numéros d'enregistrement, identifiants) peuvent être des données personnelles : ne les republiez pas et respectez la réglementation de votre pays. L'indicateur météo est indicatif et ne remplace ni la notice du drone ni la réglementation. La détection de traceurs Bluetooth est une aide, pas une preuve.

> **Sécurité.** Le point d'accès de secours est `espscanner` / `12345678` : **changez ce mot de passe** (Réglages) et définissez un utilisateur et un mot de passe pour la page. L'identification de la page passe en clair (http) : n'exposez pas la carte sur Internet sans le VPN WireGuard.

**ESP 1 (scanner)** écoute en continu les balises Remote ID (Bluetooth 4/5 + Wi-Fi, y compris le signalement électronique français) et envoie chaque drone décodé à l'ESP 2 par liaison série. Il relève aussi les réseaux Wi-Fi et les appareils Bluetooth autour de lui (priorité toujours aux drones).
**ESP 2 (passerelle)** héberge la page web, tient le journal (100 lignes en flash) et publie la liste en MQTT.

## Câblage (câbles de 10 cm)
| ESP 1 | ESP 2 |
|---|---|
| D6 (TX, GPIO16) | D7 (RX, GPIO17) |
| D7 (RX, GPIO17) | D6 (TX, GPIO16) |
| GND | GND |

Niveau 3,3 V uniquement. Antenne externe U.FL branchée sur **les deux** cartes (`ANTENNA_EXTERNAL 1` dans `include/config.h`). Alimentez chaque carte par son USB.
Au démarrage, la ROM de l'ESP 1 peut émettre quelques caractères parasites : le protocole (CRC16) les ignore.

## Flasher
Une carte à la fois sur le port USB :
```
pio run -e scanner -t upload     # ESP 1
pio run -e gateway -t upload     # ESP 2
pio device monitor
```
**Première fois avec cette version : flash USB obligatoire sur les deux cartes** (nouvelle table de partitions à deux emplacements, nécessaire à la mise à jour OTA). Le journal enregistré en flash est remis à zéro une fois ; les réglages (Wi-Fi, MQTT…) sont conservés.

## Mise à jour OTA (sans câble USB)
Page web → onglet *Technique* → *Mise à jour du firmware*. Compilez (`pio run -e gateway`, `pio run -e scanner`), puis choisissez `.pio/build/gateway/firmware.bin` (ESP 2) ou `.pio/build/scanner/firmware.bin` (ESP 1). Jamais `bootloader.bin` ni `partitions.bin`.
- ESP 2 : écrit directement, redémarre seul.
- ESP 1 : l'ESP 2 relaie le fichier sur la liaison série (blocs de 384 octets avec accusé et nouvelle tentative, environ 1 minute). L'ESP 1 arrête l'écoute pendant le transfert, valide l'image puis redémarre. Si le transfert est interrompu, l'ancien firmware reste en place.
- Mettez un mot de passe à la page (Réglages → Protection) : l'OTA est sinon ouverte à tout le réseau.
- Si le firmware de l'ESP 1 dépasse 1,6 Mo, la compilation échoue (« region overflow ») : dites-le, on réduit les options.

## Page web (ESP 2) — types de détection séparés
| Onglet | Contenu |
|---|---|
| 🛩 Drones | drones Remote ID en direct (filtre Bluetooth / Wi-Fi), carte |
| 📒 Journal drones | historique conservé en flash, export CSV |
| 📶 Réseaux Wi-Fi | réseaux captés par l'ESP 1 : nom, BSSID, canal, sécurité, signal |
| 🔵 Appareils Bluetooth | appareils captés : nom, adresse, type d'adresse, constructeur, signal |
| ⚙️ Réglages | voir ci-dessous |
| 🔧 Technique | état des deux cartes, liaison, journal technique |

## Réglages
Valeurs par défaut dans `include/config.h` ; tout est ensuite modifiable dans l'onglet Réglages (la valeur enregistrée l'emporte) :
- **Wi-Fi ESP 2** : réseau, mot de passe, nom d'hôte, délai avant de considérer le réseau perdu
- **Point d'accès** : secours seulement / toujours / jamais, nom, mot de passe
- **Scanner (ESP 1)** : écoute des drones (BT+Wi-Fi / BT / Wi-Fi), relevé des réseaux Wi-Fi, relevé des appareils Bluetooth, canal Wi-Fi fixe ou saut automatique, signal minimum, antenne de chaque carte (interne/externe)
- **MQTT** : serveur, port, utilisateur, mot de passe, sujet de base
- **Publication MQTT** : listes drones / Wi-Fi / Bluetooth, un message par drone, messages conservés, fréquences
- **Listes et journal** : durée d'oubli (drones, réseaux/appareils), absence avant nouvelle ligne de journal, fréquence d'écriture en flash
- **Syslog** : serveur, port, format, contenu, facility, familles envoyées
- **Protection de la page** : utilisateur / mot de passe

## Signalement électronique français
L'identifiant français fait 30 caractères : trigramme constructeur (3) + modèle (3) + numéro de série (24). Il est affiché découpé dans la page, et publié en MQTT / syslog sous `fr_manufacturer`, `fr_model`, `fr_serial`. Si le drone envoie aussi l'identifiant ANSI/CTA-2063 (système de signalement non intégré), il est conservé à part sous `ansi_id`.

## Syslog (option)
Réglages → *Syslog* : l'ESP 2 envoie en UDP (RFC 5424 ou 3164, contenu JSON ou texte `clé="valeur"`) vers rsyslog, Graylog, Synology Log Center, ELK…
Types de messages : `drone_new` (première détection), `drone` (mise à jour, au plus une ligne par drone toutes les N secondes), `drone_lost`, `wifi_new`, `bluetooth_new`, `system` (journal technique). Chaque famille s'active séparément ; le bouton *Tester syslog* envoie un message de test (UDP : pas d'accusé de réception, vérifiez côté serveur). L'adresse du serveur doit être une IP.

## Mise à jour depuis GitHub (dépôt privé)
1. Sur GitHub : créez un dépôt privé. Compte → Settings → Developer settings → *Fine-grained personal access tokens* : jeton limité à ce dépôt, permission **Contents : lecture seule**, avec expiration.
2. À chaque nouvelle version : changez `FW_VERSION` dans `include/config.h` (ex. `"1.2.0"`), compilez les deux environnements, puis créez une *Release* dont le **tag** est `v1.2.0` et joignez `.pio/build/gateway/firmware.bin` renommé **`gateway.bin`** et `.pio/build/scanner/firmware.bin` renommé **`scanner.bin`**.
3. Page de l'ESP 2 → Réglages → *Mise à jour depuis GitHub* : saisissez `compte/dépôt` et le jeton, enregistrez.
4. Onglet *Technique* → *Rechercher une mise à jour* → *Installer sur l'ESP 2* / *Installer sur l'ESP 1*. L'ESP 2 télécharge en HTTPS (certificats vérifiés ; le jeton n'est jamais envoyé au serveur de téléchargement), installe puis redémarre ; l'ESP 1 passe par la liaison série comme pour l'OTA manuel.
Le jeton est conservé en clair dans la mémoire de l'ESP 2 : limitez-le à la lecture de ce seul dépôt. Les installations sont toujours lancées à la main.

## Statistiques et test d'antenne
- **Journal drones** : graphiques des détections par jour (14 jours) et par heure de la journée, calculés dans votre navigateur à partir du journal (seules les lignes datées comptent : l'heure doit être connue).
- **Test d'antenne** : onglet dédié. Choisissez une source (réseau Wi-Fi, appareil ou capteur Bluetooth, drone) et lancez la mesure : la page trace le signal dans le temps (un point par nouvelle réception, les trous sont des pertes) avec moyenne, min, max, trames par minute et plus long silence. « Garder la courbe » conserve la mesure (4 courbes au plus, perdues au rechargement de la page) pour comparer antenne externe / céramique ou deux emplacements. L'ESP 1 ne renvoie une valeur que toutes les 4 s au plus : mesure grossière mais comparable d'une courbe à l'autre.
- Rien de tout cela n'alourdit le firmware : tout est calculé dans la page.

## Nom et logo
La page s'appelle « ESP SCANNER » avec un nouveau logo (radar). Le point d'accès de secours s'appelle `espscanner` (si vous n'avez pas fixé de nom) et l'appareil Home Assistant « ESP SCANNER ». Le nom réseau (`dronescan.local`), le préfixe MQTT `dronescan` et les identifiants Home Assistant n'ont **pas** changé pour ne rien casser : ils se règlent dans la page (Réglages).

## Traceurs et balises Bluetooth, canaux Wi-Fi
- **Traceurs / balises** (onglet *Appareils Bluetooth*, colonne « Constructeur / type », case « Traceurs et balises seulement », champ `tag` en MQTT/syslog) : AirTag et accessoires Apple « Find My » (annonces en état **séparé**, c'est-à-dire éloignés de leur propriétaire depuis un moment : un AirTag près de son iPhone n'est pas reconnu), iBeacon, Eddystone, et, d'après leurs identifiants de service, **Tile** et **Samsung SmartTag** (ces deux derniers : identifiants non vérifiés auprès d'une source officielle, donc à confirmer sur vos propres appareils). C'est un **indice**, pas une preuve : les adresses changent régulièrement. Il faut que « Relever les appareils Bluetooth » soit activé.
- **Eddystone TLM** : tension de pile et température de la balise apparaissent aussi dans l'onglet *Capteurs*.
- **Occupation des canaux Wi-Fi** (onglet *Réseaux Wi-Fi*) : nombre de réseaux et signal le plus fort par canal, liste des canaux sans réseau relevé. Le scanner ne voit un réseau que sur les canaux qu'il visite.
- **Pas encore pris en charge** : Govee (passifs), SwitchBot, Qingping. Je n'ai pas pu vérifier leurs formats dans des sources fiables ; pour en ajouter un, il faut le modèle exact et une annonce réelle ou la documentation du format.

## Capteurs Bluetooth : Shelly BLU (BTHome) et Xiaomi Flora
L'ESP 1 lit les annonces Bluetooth de ces capteurs, sans connexion : onglet **Capteurs** de la page, MQTT `<base>/sensors/<MAC>` (JSON : température, humidité, luminosité, sol, fertilité, batterie, fenêtre, porte, mouvement, bouton, selon le modèle) et déclaration automatique Home Assistant (un appareil par capteur).
- **Écoute passive uniquement** (aucune demande envoyée aux capteurs). Formats lus, en clair :
  - **BTHome v2** : Shelly BLU (Door/Window, H&T, Motion, bouton…), et tout appareil BTHome, y compris les thermomètres Xiaomi dont le firmware alternatif ATC/pvvx est réglé en « BTHome » (réglage recommandé par ce projet).
  - **Xiaomi MiBeacon** : Flora HHCCJCY01, LYWSDCGQ (sonde ronde), HHCCPOT002, CGG1, LYWSD02, MHO-C303/C401. La **LYWSD03MMC d'origine** (et tout appareil au firmware récent) est chiffrée : elle est signalée « chiffrée » et illisible.
  - **Firmware alternatif ATC1441 et pvvx « custom »** des thermomètres Xiaomi (température, humidité, batterie, tension).
  - **Ruuvi** (RuuviTag, format 5 : température, humidité, pression, tension de pile).
- **Non pris en charge** : les Govee H5074, H5075 et H5129 (ils exigent une écoute active), les annonces chiffrées (aucune clé de déchiffrement), et tous les modèles non listés ci-dessus.
- Ces capteurs émettent rarement : pour une bonne réception, réglez l'ESP 1 sur « Bluetooth seulement » (ou augmentez la part Bluetooth), sinon beaucoup d'annonces sont manquées pendant l'écoute Wi-Fi.
- Un capteur est oublié après 24 h sans réception. Les valeurs affichées sont les dernières reçues : l'âge indique le dernier paquet.
- Format vérifié d'après les spécifications publiques (BTHome, décodeur ESPHome) et des paquets de test, **pas encore avec vos capteurs réels**.

## Alertes, Home Assistant, traces de vol
- **Alerte** à la première apparition d'un drone : MQTT `<base>/alert` (JSON `event`, `title`, `message`, `drone`) et/ou syslog gravité *warning*, identifiant `ALERT`. À brancher sur une automatisation Home Assistant, ntfy, Telegram… (Réglages → *Alertes et Home Assistant*).
- **Home Assistant** : découverte automatique MQTT (préfixe `homeassistant`), appareil « DroneScan » avec capteurs *Drones détectés*, *Réseaux Wi-Fi vus*, *Appareils Bluetooth vus*, *Drone en vue*, *Scanner (ESP 1)*, *Dernière alerte drone*. Déclarés à chaque connexion MQTT. Désactiver l'option ne supprime pas les entités déjà créées (à retirer dans Home Assistant).
- **Traces de vol** : chaque drone positionné laisse un parcours sur la carte (jusqu'à 12 drones × 120 points, en RAM, perdu au redémarrage). Export GeoJSON : bouton sur la carte ou `/api/tracks.geojson` (une ligne par drone + dernier point, altitude en 3ᵉ coordonnée).

## Interface : thèmes clair et sombre

Le bouton en haut à droite de la page bascule entre le thème sombre et le thème clair (le choix est mémorisé dans le navigateur ; au premier affichage, c'est celui de votre appareil). Le menu est en haut, les drones s'affichent en fiches à côté de la carte, et un bandeau signale un nouveau drone apparu pendant que la page est ouverte.

## Météo locale, horodatage et urgence

- **Météo locale** (onglet Drones, colonne de gauche) : choisissez une ville (ou « Ma position » si votre navigateur l'autorise) ; le lieu est mémorisé dans le navigateur. Les données viennent d'Open-Meteo, directement depuis votre navigateur (accès Internet requis sur l'appareil qui affiche la page, rien ne passe par l'ESP). Un indicateur signale rafales, pluie ou visibilité réduite ; il est indicatif. La taille de la carte se choisit (réduite, moyenne, complète) et est mémorisée.
- **Horodatage de l'émetteur** : affiché dans la fiche du drone (heure déduite des secondes écoulées depuis le début de l'heure, avec le décalage par rapport à l'heure de la page).
- **Alerte d'urgence** : quand un drone signale un état d'urgence ou une panne du Remote ID, un message `drone_emergency` part sur `<base>/alert` (MQTT) et en syslog (gravité critique), une fois par épisode.

## VPN WireGuard (ESP 2)

L'ESP 2 peut se connecter à votre serveur WireGuard (Réglages → « VPN (WireGuard) »). Sur le serveur, créez un pair pour l'ESP 2 (adresse de tunnel unique, ex. `10.8.0.2`), puis saisissez dans la page : adresse et port UDP du serveur, clé publique du serveur, clé privée de l'ESP 2, adresse de l'ESP 2 dans le tunnel et masque du réseau du tunnel. L'état s'affiche dans l'onglet Technique.

- Seul le trafic du réseau du tunnel passe par le VPN : l'heure (NTP), GitHub et le MQTT local restent sur le Wi-Fi (option « Tout le trafic par le VPN » pour changer).
- La poignée de main est horodatée : le VPN démarre une fois l'heure obtenue par NTP.
- Clé pré-partagée non prise en charge. IPv4 uniquement.
- La bibliothèque WireGuard (BSD-3, Kenta Ida) est incluse dans `lib/WireGuard-ESP32`, adaptée à Arduino-ESP32 3.x.

## Page web compressée

La page est compressée en gzip à chaque compilation (`tools/gzip_ui.py`, lancé automatiquement par PlatformIO) : environ 35 Ko de flash gagnés sur l'ESP 2. Le fichier source reste `src/gateway/web_ui.h` ; `web_ui_gz.h` est généré.

## Wi-Fi de l'ESP 2
Il rejoint votre réseau. En mode « secours » (par défaut), si celui-ci est absent ou inaccessible pendant 20 s (ou si aucun SSID n'est saisi), il ouvre le point d'accès `espscanner` (mot de passe `12345678` : changez-le dans Réglages, page sur http://192.168.4.1). Dès que votre réseau revient, le point d'accès se ferme (sauf si quelqu'un y est connecté). Sur votre réseau : http://dronescan.local/ ou l'IP affichée dans le moniteur série.

## MQTT

Par défaut, aucune adresse de serveur n'est renseignée : MQTT reste inactif tant que vous n'en saisissez pas une dans Réglages.
| Sujet | Contenu |
|---|---|
| `<base>/list` | liste complète des **drones** détectés, retenue, toutes les 10 s |
| `<base>/drones/<id>` | un drone (détail complet), au plus un message / 2 s |
| `<base>/wifi` | liste des **réseaux Wi-Fi** captés par l'ESP 1 |
| `<base>/bluetooth` | liste des **appareils Bluetooth** captés par l'ESP 1 |
| `<base>/status` | état des deux cartes (retenu ; `online:false` si l'ESP 2 disparaît) |

Chaque famille se désactive séparément dans les réglages.
Si rien n'arrive : *Réglages* → **Tester MQTT** (Wi-Fi, port TCP, identification, publication, avec la cause exacte), puis onglet *Technique*. Causes fréquentes : Mosquitto 2 n'écoute que sur localhost par défaut (ajouter `listener 1883` et un `password_file`, ou `allow_anonymous true`), mauvais utilisateur/mot de passe (code 4/5), pare-feu.
Test sans drone : bouton **Simuler un drone**.

## Limites connues
- Non testé sur matériel : mise à jour OTA, alertes, découverte Home Assistant, traces, réception BLE Coded PHY, effet du commutateur d'antenne, format français face à de vrais équipements.
- Seuls les drones qui émettent leur Remote ID sont visibles.
- Le relevé Wi-Fi se fait sur les canaux visités par le saut automatique : un réseau peut mettre quelques dizaines de secondes à apparaître. Les adresses Bluetooth aléatoires (téléphones) changent régulièrement.
- Si le point d'accès est modifié depuis la page connectée à ce point d'accès, la connexion est coupée quelques secondes.

## Crédits et licences

Ce projet est distribué sous licence MIT (voir `LICENSE`). Il utilise :

| Composant | Licence | Remarque |
|---|---|---|
| [opendroneid-core-c](https://github.com/opendroneid/opendroneid-core-c) (`lib/opendroneid`) | Apache-2.0 | décodage Open Drone ID, copie incluse |
| [WireGuard-ESP32-Arduino](https://github.com/ciniml/WireGuard-ESP32-Arduino) de Kenta Ida (`lib/WireGuard-ESP32`) | BSD-3-Clause | copie **modifiée** : adaptation à Arduino-ESP32 3.x (`esp_netif`), clé pré-partagée, état du tunnel, prototype manquant |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | Apache-2.0 | téléchargé par PlatformIO |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | MIT | téléchargé par PlatformIO |
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32) / [pioarduino](https://github.com/pioarduino/platform-espressif32) | LGPL-2.1 / Apache-2.0 | plateforme |
| [Leaflet](https://leafletjs.com/) | BSD-2-Clause | chargé depuis unpkg par le navigateur |
| Fond de carte © [OpenStreetMap](https://www.openstreetmap.org/copyright) contributors | ODbL | tuiles chargées par le navigateur |
| Météo [Open-Meteo.com](https://open-meteo.com/) | CC BY 4.0 | interrogée par le navigateur, usage non commercial |

## Contribuer

Les compilations se font avec `pio run -e scanner` et `pio run -e gateway`. Le fichier source de la page est `src/gateway/web_ui.h` ; `src/gateway/web_ui_gz.h` en est la version compressée, régénérée par `tools/gzip_ui.py` à chaque compilation (à valider avec le reste).
