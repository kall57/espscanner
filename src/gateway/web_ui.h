#pragma once
#include <pgmspace.h>

// Interface web (page unique). Les données reçues par radio ne sont jamais insérées comme HTML (textContent uniquement).
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<script>try{var t=localStorage.getItem("th");if(!t)t=matchMedia("(prefers-color-scheme: light)").matches?"light":"dark";document.documentElement.dataset.theme=t}catch(e){document.documentElement.dataset.theme="dark"}</script>
<title>ESP SCANNER</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns=%27http://www.w3.org/2000/svg%27 viewBox=%270 0 32 32%27%3E%3Crect width=%2732%27 height=%2732%27 rx=%277%27 fill=%27%230e1217%27/%3E%3Ccircle cx=%2716%27 cy=%2716%27 r=%2712%27 fill=%27none%27 stroke=%27%233ea6ff%27 stroke-width=%272.4%27/%3E%3Ccircle cx=%2716%27 cy=%2716%27 r=%276.5%27 fill=%27none%27 stroke=%27%233ea6ff%27 stroke-width=%272.4%27 opacity=%27.6%27/%3E%3Cpath d=%27M16 16L26 9%27 stroke=%27%233fb950%27 stroke-width=%273%27 stroke-linecap=%27round%27/%3E%3Ccircle cx=%2716%27 cy=%2716%27 r=%272.8%27 fill=%27%233ea6ff%27/%3E%3C/svg%3E">
<style>
:root{color-scheme:dark;--bg:#0e1217;--card:#161c24;--line:#252f3b;--fg:#e6edf3;--mut:#8b98a8;--acc:#3ea6ff;--ok:#3fb950;--warn:#d29922;--err:#f85149;--pur:#bc8cff;--onacc:#04131f;--btn:#222b36;--inp:#0e1217;--hdr:#111820;--mapbg:#0a0e13;--navon:#16314d;--offbg:#3a2a05;--offfg:#ffd88a;--toast:#2b3542;--errbg:#2a1514;--errbd:#5a2623;--errfg:#ffd0cc}
:root[data-theme=light]{color-scheme:light;--bg:#f3f5f8;--card:#ffffff;--line:#dde3ea;--fg:#17202b;--mut:#566475;--acc:#1d6fd6;--ok:#1a7f37;--warn:#8a5a00;--err:#c62828;--pur:#6b3fb8;--onacc:#ffffff;--btn:#f1f4f8;--inp:#ffffff;--hdr:#ffffff;--mapbg:#eef2f6;--navon:#e6f0fc;--offbg:#fdf1d6;--offfg:#6b4500;--toast:#17202b;--errbg:#fdeceb;--errbd:#f1b8b4;--errfg:#8c1d18}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:14.5px/1.45 Inter,system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:1280px;margin:0 auto;padding:16px 20px 40px}
.top{background:var(--hdr);border-bottom:1px solid var(--line);display:flex;flex-wrap:wrap;align-items:center;gap:8px 20px;padding:10px 24px}
.brand{display:flex;gap:9px;align-items:center;font-weight:700;font-size:15.5px;letter-spacing:.02em}
.stat{display:flex;gap:6px;flex-wrap:wrap;align-items:center}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);margin:0 0 10px}
.chip{font-size:12px;padding:3px 10px;border-radius:99px;border:1px solid var(--line);color:var(--mut);font-weight:500}
.chip.ok{color:var(--ok);border-color:var(--ok)}.chip.warn{color:var(--warn);border-color:var(--warn)}.chip.err{color:var(--err);border-color:var(--err)}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:14px}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.grow{flex:1;min-width:0}
button{background:var(--btn);color:var(--fg);border:1px solid var(--line);border-radius:9px;padding:7px 12px;font:inherit;cursor:pointer}
button:hover{border-color:var(--acc)}button.pri{background:var(--acc);border-color:var(--acc);color:var(--onacc);font-weight:600}button.dng:hover{border-color:var(--err);color:var(--err)}
input,select{width:100%;background:var(--inp);color:var(--fg);border:1px solid var(--line);border-radius:9px;padding:7px 9px;font:inherit}
label{display:block;font-size:12px;color:var(--mut);margin:0 0 3px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:10px}
.tabs{display:flex;gap:2px;flex:1 1 420px;flex-wrap:wrap}.tabs button{background:none;border:0;color:var(--mut);padding:8px 12px;border-radius:9px}.tabs button:hover{color:var(--fg)}.tabs button.on{background:var(--navon);color:var(--acc);font-weight:600}
.tabs .n{display:inline-block;min-width:20px;margin-left:5px;padding:0 6px;border-radius:99px;background:var(--bg);color:var(--fg);font-size:12px;font-weight:600}
.tab{display:none}.tab.on{display:block}
details{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:0 14px;margin-bottom:10px}
summary{cursor:pointer;padding:11px 0;font-weight:600}details[open] summary{border-bottom:1px solid var(--line);margin-bottom:11px}details>.grid{padding-bottom:13px}
.hint{font-size:12px;color:var(--mut);margin:2px 0 12px}
#map{height:430px;border-radius:10px;border:1px solid var(--line);background:var(--mapbg)}
table{width:100%;border-collapse:collapse;font-size:13px}th{color:var(--mut);font-weight:500;text-align:left;border-bottom:1px solid var(--line);padding:5px 6px;white-space:nowrap}
td{padding:6px;border-bottom:1px solid var(--line);vertical-align:top}.wrap{overflow-x:auto}
.pre{white-space:pre-line}.mut{color:var(--mut)}.mono{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:12.5px}
.bar{display:inline-block;height:7px;border-radius:4px;background:var(--acc);vertical-align:middle;margin-right:6px}
.step{padding:5px 0}.step.ok::before{content:"✔ ";color:var(--ok)}.step.ko::before{content:"✖ ";color:var(--err)}
#log{max-height:340px;overflow:auto;white-space:pre-wrap;font-size:12px}
#off{display:none;background:var(--offbg);border:1px solid var(--warn);color:var(--offfg);border-radius:10px;padding:9px 12px;margin-bottom:14px}
#toast{position:fixed;left:50%;bottom:16px;transform:translateX(-50%);background:var(--toast);color:#fff;border:1px solid var(--line);padding:8px 13px;border-radius:9px;opacity:0;pointer-events:none;transition:opacity .25s;max-width:90vw}#toast.on{opacity:1}
.alert{display:none;align-items:center;gap:10px;flex-wrap:wrap;background:var(--errbg);border:1px solid var(--errbd);color:var(--errfg);border-radius:12px;padding:10px 14px;margin-bottom:14px}.alert b{color:var(--errfg)}
.split{display:flex;flex-wrap:wrap;gap:14px;align-items:flex-start}.split>.cards{flex:1 1 320px;min-width:0;display:flex;flex-direction:column;gap:12px}
.dcard{background:var(--card);border:1px solid var(--line);border-left:5px solid var(--acc);border-radius:14px;padding:13px 15px;display:flex;flex-direction:column;gap:9px}
.dh{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.dh b{flex:1;min-width:0;overflow-wrap:anywhere}
.ds{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}.ds span{display:block;font-size:11.5px;color:var(--mut)}
.dm{font-size:12.5px;color:var(--mut);white-space:pre-line;overflow-wrap:anywhere}.df{display:flex;gap:8px;align-items:center;font-size:12.5px;color:var(--mut)}
.split>.lcol{flex:2 1 500px;min-width:0}.split>.mapc{margin:0}
.wxg{display:grid;grid-template-columns:repeat(auto-fit,minmax(112px,1fr));gap:8px;margin:10px 0}.wxg span{display:block;font-size:11.5px;color:var(--mut)}
.wxt{font-size:32px;font-weight:700;line-height:1}.wxh{display:flex;gap:12px;align-items:center;flex-wrap:wrap}
#wxchart{max-width:460px}
#wx[data-sz=s] .wxg,#wx[data-sz=s] .hint,#wx[data-sz=s] #wxchart,#wx[data-sz=m] .hint,#wx[data-sz=m] #wxchart{display:none}#wx[data-sz=s] .wxt{font-size:22px}#wx[data-sz=s]{padding:9px 14px}#wx select{width:auto;padding:4px 8px}
.dcard.em{border-left-color:var(--err);border-color:var(--err)}
.chip.s-ble{color:var(--pur);border-color:var(--pur)}.chip.s-wifi{color:var(--acc);border-color:var(--acc)}
</style></head><body>
<header class="top">
<div class="brand"><svg viewBox="0 0 32 32" width="28" height="28" aria-hidden="true"><circle cx="16" cy="16" r="14" fill="none" stroke="#3ea6ff" stroke-width="2"/><circle cx="16" cy="16" r="8" fill="none" stroke="#3ea6ff" stroke-width="2" opacity=".6"/><path d="M16 16L27 8" stroke="#3fb950" stroke-width="2.6" stroke-linecap="round"/><circle cx="16" cy="16" r="2.6" fill="#3ea6ff"/><circle cx="24" cy="21" r="2" fill="#f85149"/></svg><span>ESP SCANNER</span></div>
<nav class="tabs" id="tabs" aria-label="Navigation">
  <button data-t="live" class="on">Drones<span class="n" id="n-live">0</span></button>
  <button data-t="jnl">Journal<span class="n" id="n-jnl">0</span></button>
  <button data-t="wifi">Wi-Fi<span class="n" id="n-wifi">0</span></button>
  <button data-t="ble">Bluetooth<span class="n" id="n-ble">0</span></button>
  <button data-t="sens">Capteurs<span class="n" id="n-sens">0</span></button>
  <button data-t="ant">Antenne</button>
  <button data-t="set">Réglages</button>
  <button data-t="tech">Technique</button>
</nav>
<div class="stat"><span class="chip" id="csc">ESP 1</span><span class="chip" id="cwf">Wi-Fi</span><span class="chip" id="cmq">MQTT</span><button type="button" id="bth" title="Basculer entre thème clair et sombre">Thème</button></div>
</header>
<main>
<div id="off"></div>

<div class="tab on" id="t-live">
  <div class="alert" id="newdr" role="alert"></div>
  <section class="card"><div class="row"><div class="grow"><b id="cnt">0 drone</b> <span class="mut" id="stats"></span></div>
    <select id="fsrc" style="width:auto"><option value="">Toutes les sources</option><option value="ble">Bluetooth seulement</option><option value="wifi">Wi-Fi seulement</option></select>
    <button id="bpub">Publier tout en MQTT</button><button id="bsim">Simuler un drone</button><button id="bclr" class="dng">Vider</button></div></section>
  <div class="split">
  <div class="lcol">
  <section class="card" id="wx" data-sz="m"><div class="row"><h2 class="grow" style="margin:0">Météo locale</h2><span class="mut" id="wxplace"></span><select id="wxsz" aria-label="Taille de la carte météo"><option value="s">Réduite</option><option value="m">Moyenne</option><option value="l">Complète</option></select><button type="button" id="wxset">Choisir le lieu</button></div>
    <div id="wxform" class="row" style="display:none;margin-top:8px"><input id="wxq" placeholder="Ville (ex. Metz)" style="flex:1;min-width:160px"><button type="button" id="wxgo" class="pri">Chercher</button><button type="button" id="wxgeo">Ma position</button></div>
    <div id="wxres" class="row" style="margin-top:6px"></div>
    <div id="wxbody" class="mut" style="margin-top:8px">Choisissez un lieu pour afficher la météo (données Open-Meteo, accès Internet requis sur votre appareil).</div>
    <div id="wxchart"></div><div class="hint" style="margin:8px 0 0">Données météo : <a href="https://open-meteo.com/" target="_blank" rel="noopener">Open-Meteo.com</a> (CC BY 4.0)</div></section>
  <section class="card mapc"><div class="row"><h2 class="grow" style="margin:0">Carte</h2><label class="mut" style="display:flex;gap:6px;align-items:center;margin:0"><input type="checkbox" id="ctr" checked style="width:auto"> Traces de vol</label><a href="/api/tracks.geojson" download="dronescan-traces.geojson"><button type="button">Exporter GeoJSON</button></a><button type="button" id="btrc" class="dng">Effacer les traces</button></div><div id="map" style="margin-top:8px"></div><div class="mut" id="mapmsg" style="margin-top:6px"></div></section>
  </div>
  <section class="cards" aria-label="Drones détectés"><div id="dc" style="display:flex;flex-direction:column;gap:12px"></div><p class="mut card" id="empty">Aucun drone pour le moment. Seuls les drones qui émettent leur identification (Remote ID) sont visibles.</p></section>
  </div>
</div>

<div class="tab" id="t-jnl">
  <section class="card"><h2>Statistiques</h2><div id="jsum" class="mut" style="margin-bottom:8px"></div><div class="grid" style="grid-template-columns:repeat(auto-fit,minmax(280px,1fr))"><div id="jday"></div><div id="jhour"></div></div></section>
  <section class="card"><div class="row"><div class="grow"><b id="jcnt">0</b> <span class="mut">ligne(s) de drones · conservées après redémarrage</span></div>
    <a href="/api/journal.csv"><button>Exporter CSV</button></a><button id="bjclr" class="dng">Vider le journal</button></div></section>
  <section class="card"><div class="wrap"><table><thead><tr><th>Première vue</th><th>Dernière vue</th><th>Durée</th><th>Identifiant</th><th>Source</th><th>Dernière position</th><th>Alt. max</th><th>Trames</th><th>RSSI</th></tr></thead><tbody id="jtb"></tbody></table></div>
  <p class="mut" id="jempty">Journal vide.</p></section>
</div>

<div class="tab" id="t-wifi">
  <section class="card"><div class="row"><div class="grow"><b id="wcnt">0 réseau</b> <span class="mut">captés par l'ESP 1 (hors drones)</span></div>
    <input id="wq" placeholder="Filtrer (nom, adresse…)" style="width:200px"><button id="bwclr" class="dng">Vider</button></div></section>
  <section class="card"><h2>Occupation des canaux</h2><div class="grid" style="grid-template-columns:repeat(auto-fit,minmax(280px,1fr))"><div id="wch1"></div><div id="wch2"></div></div>
    <p class="hint" style="margin-top:8px" id="wchh"></p></section>
  <section class="card"><div class="wrap"><table><thead><tr><th>Nom (SSID)</th><th>Adresse (BSSID)</th><th>Canal</th><th>Sécurité</th><th>Signal</th><th>Vu il y a</th><th>Trames</th></tr></thead><tbody id="wtb"></tbody></table></div>
  <p class="mut" id="wempty">Aucun réseau. Vérifiez que « Relever les réseaux Wi-Fi » est activé dans les réglages.</p></section>
</div>

<div class="tab" id="t-ble">
  <section class="card"><div class="row"><div class="grow"><b id="bcnt">0 appareil</b> <span class="mut">captés par l'ESP 1 (hors drones)</span></div>
    <label class="mut" style="display:flex;gap:6px;align-items:center;margin:0"><input type="checkbox" id="btrk" style="width:auto"> Traceurs et balises seulement</label>
    <input id="bq" placeholder="Filtrer (nom, adresse…)" style="width:200px"><button id="bbclr" class="dng">Vider</button></div></section>
  <section class="card"><div class="wrap"><table><thead><tr><th>Nom</th><th>Adresse</th><th>Type d'adresse</th><th>Constructeur / type</th><th>Signal</th><th>Vu il y a</th><th>Trames</th></tr></thead><tbody id="btb"></tbody></table></div>
  <p class="mut" id="bempty">Aucun appareil. Vérifiez que « Relever les appareils Bluetooth » est activé dans les réglages.</p></section>
</div>

<div class="tab" id="t-sens">
  <section class="card"><div class="row"><div class="grow"><b id="scnt">0 capteur</b> <span class="mut">Capteurs Bluetooth lus dans leurs annonces : Shelly BLU (BTHome) et Xiaomi Flora (HHCCJCY01). Sans connexion, donc sans pile supplémentaire consommée.</span></div><button id="bsclr" class="dng">Vider</button></div></section>
  <section class="card"><div class="wrap"><table><thead><tr><th>Capteur</th><th>Adresse</th><th>Mesures</th><th>Batterie</th><th>Signal</th><th>Vu il y a</th><th>Trames</th></tr></thead><tbody id="stb"></tbody></table></div>
  <p class="mut" id="sempty">Aucun capteur reçu. Mettez l'ESP 1 en mode « Bluetooth seulement » (Réglages → Scanner) pour une meilleure réception : ces capteurs n'émettent qu'à intervalles espacés. Les sondes chiffrées ne sont pas lisibles.</p></section>
</div>

<div class="tab" id="t-ant">
  <section class="card"><h2>Test d'antenne</h2>
    <p class="hint">Mesure le niveau de signal d'une source précise dans le temps. Pour comparer deux antennes (ou deux emplacements) : choisissez une source stable et proche (un réseau Wi-Fi voisin ou un capteur Bluetooth), fixez le canal Wi-Fi dans Réglages → Scanner si c'est un réseau, enregistrez 2 minutes, cliquez sur <b>Garder la courbe</b>, changez d'antenne (Réglages → Scanner) ou déplacez la carte, puis recommencez. Les courbes gardées restent affichées pour comparaison. Un point est tracé à chaque nouvelle réception : un trou dans la courbe est une perte de réception. Mesure grossière : l'ESP 1 renvoie une valeur toutes les 4 s au plus.</p>
    <div class="grid"><div><label>Type de source</label><select id="asrc"><option value="wifi">Réseau Wi-Fi</option><option value="ble">Appareil Bluetooth</option><option value="sens">Capteur Bluetooth</option><option value="drone">Drone</option></select></div>
      <div><label>Source à mesurer</label><select id="atg"></select></div>
      <div><label>Nom de la courbe</label><input id="alab" autocomplete="off" placeholder="ex. antenne externe, près de la fenêtre"></div></div>
    <div class="row" style="margin-top:10px"><button id="aref">Actualiser la liste</button><button id="ago" class="pri">Démarrer</button><button id="akeep">Garder la courbe</button><button id="aclr" class="dng">Effacer les courbes gardées</button></div>
    <div id="achart" style="margin-top:10px"></div><div id="astat" class="mono" style="white-space:pre-wrap;margin-top:6px"></div></section>
</div>

<div class="tab" id="t-set"><form id="f">
  <details open><summary>Wi-Fi de l'ESP 2</summary><div class="grid">
    <div><label>Votre réseau Wi-Fi (SSID)</label><input id="wifiSsid" autocomplete="off"></div>
    <div><label>Mot de passe Wi-Fi</label><input id="wifiPass" type="password" autocomplete="new-password" placeholder="(inchangé)"></div>
    <div><label>Nom d'hôte (redémarrage requis)</label><input id="hostname" autocomplete="off"></div>
    <div><label>Délai avant de considérer le réseau perdu (s)</label><input id="staGiveupSec" type="number" min="5" max="600"></div>
  </div></details>
  <details><summary>Point d'accès propre à l'ESP 2</summary><p class="hint">Par défaut il ne s'ouvre que si votre réseau est inaccessible, et se referme dès son retour.</p><div class="grid">
    <div><label>Mode</label><select id="apMode"><option value="0">Secours seulement (recommandé)</option><option value="1">Toujours actif</option><option value="2">Jamais</option></select></div>
    <div><label>Nom du point d'accès (vide = automatique)</label><input id="apName" autocomplete="off" placeholder="espscanner"></div>
    <div><label>Mot de passe (8 caractères min.)</label><input id="apPass" type="password" autocomplete="new-password" placeholder="(inchangé)"></div>
  </div></details>
  <details><summary>Scanner (ESP 1)</summary><p class="hint">Appliqué à l'ESP 1 dans les 5 secondes.</p><div class="grid">
    <div><label>Écoute des drones</label><select id="mode"><option value="0">Bluetooth + Wi-Fi</option><option value="1">Bluetooth seul</option><option value="2">Wi-Fi seul</option></select></div>
    <div><label>Relever les réseaux Wi-Fi</label><select id="wifiSurvey"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Relever les appareils Bluetooth</label><select id="btSurvey"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Canal Wi-Fi</label><select id="chan"><option value="0">Saut automatique (canal 6 prioritaire)</option><option value="1">1</option><option value="2">2</option><option value="3">3</option><option value="4">4</option><option value="5">5</option><option value="6">6 (balises et NAN)</option><option value="7">7</option><option value="8">8</option><option value="9">9</option><option value="10">10</option><option value="11">11</option><option value="12">12</option><option value="13">13</option></select></div>
    <div><label>Part d'antenne donnée au Bluetooth si Bluetooth + Wi-Fi (%)</label><input id="bleDuty" type="number" min="10" max="100"></div>
    <div><label>Signal minimum des réseaux / appareils (dBm)</label><input id="rssiMin" type="number" min="-120" max="-20"></div>
    <div><label>Antenne de l'ESP 1</label><select id="antScanner"><option value="1">Externe (U.FL)</option><option value="0">Céramique intégrée</option></select></div>
    <div><label>Antenne de l'ESP 2</label><select id="antGateway"><option value="1">Externe (U.FL)</option><option value="0">Céramique intégrée</option></select></div>
  </div></details>
  <details open><summary>MQTT</summary><div class="grid">
    <div><label>MQTT activé</label><select id="mqttEnabled"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Serveur (hôte / IP)</label><input id="mqttHost" autocomplete="off"></div>
    <div><label>Port</label><input id="mqttPort" type="number" min="1" max="65535"></div>
    <div><label>Utilisateur</label><input id="mqttUser" autocomplete="off"></div>
    <div><label>Mot de passe</label><input id="mqttPass" type="password" autocomplete="new-password" placeholder="(inchangé)"></div>
    <div><label>Sujet de base</label><input id="mqttBase" autocomplete="off"></div>
  </div></details>
  <details><summary>Ce que l'ESP 2 publie en MQTT</summary><p class="hint">Sujets : <span class="mono">&lt;base&gt;/list</span> (liste des drones) · <span class="mono">&lt;base&gt;/drones/&lt;id&gt;</span> (un drone) · <span class="mono">&lt;base&gt;/wifi</span> · <span class="mono">&lt;base&gt;/bluetooth</span> · <span class="mono">&lt;base&gt;/status</span></p><div class="grid">
    <div><label>Liste des drones</label><select id="pubList"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Un message par drone</label><select id="pubDrones"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Liste des réseaux Wi-Fi</label><select id="pubWifi"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Liste des appareils Bluetooth</label><select id="pubBt"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Capteurs Bluetooth (<span class="mono">&lt;base&gt;/sensors/&lt;MAC&gt;</span>)</label><select id="pubSens"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Messages conservés par le broker</label><select id="mqttRetain"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Listes : toutes les (s)</label><input id="mqttListSec" type="number" min="2" max="3600"></div>
    <div><label>Un même drone : au plus toutes les (s)</label><input id="mqttDroneSec" type="number" min="1" max="600"></div>
  </div></details>
  <details><summary>Syslog (envoi vers un serveur de journaux)</summary><p class="hint">Messages UDP (RFC 5424 ou 3164) vers rsyslog, Graylog, Synology, ELK… Types : <span class="mono">drone_new</span>, <span class="mono">drone</span>, <span class="mono">drone_lost</span>, <span class="mono">wifi_new</span>, <span class="mono">bluetooth_new</span>, <span class="mono">system</span>.</p><div class="grid">
    <div><label>Syslog activé</label><select id="syslogOn"><option value="0">Non</option><option value="1">Oui</option></select></div>
    <div><label>Serveur (adresse IP)</label><input id="syslogHost" autocomplete="off" placeholder="192.168.10.x"></div>
    <div><label>Port (UDP)</label><input id="syslogPort" type="number" min="1" max="65535"></div>
    <div><label>Format du paquet</label><select id="syslogFmt"><option value="0">RFC 5424 (moderne)</option><option value="1">RFC 3164 (classique)</option></select></div>
    <div><label>Contenu du message</label><select id="syslogBody"><option value="0">JSON</option><option value="1">Texte clé="valeur"</option></select></div>
    <div><label>Facility</label><select id="syslogFac"><option value="16">local0</option><option value="17">local1</option><option value="18">local2</option><option value="19">local3</option><option value="20">local4</option><option value="21">local5</option><option value="22">local6</option><option value="23">local7</option></select></div>
    <div><label>Envoyer les drones</label><select id="slDrones"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Un même drone : une ligne toutes les (s)</label><input id="slDroneSec" type="number" min="1" max="3600"></div>
    <div><label>Envoyer les nouveaux réseaux Wi-Fi</label><select id="slWifi"><option value="0">Non</option><option value="1">Oui</option></select></div>
    <div><label>Envoyer les nouveaux appareils Bluetooth</label><select id="slBt"><option value="0">Non</option><option value="1">Oui</option></select></div>
    <div><label>Envoyer le journal technique</label><select id="slSystem"><option value="1">Oui</option><option value="0">Non</option></select></div>
  </div><div class="row" style="padding-bottom:13px"><button type="button" id="bsl">Tester syslog</button><span class="mut" id="slres"></span></div></details>
  <details><summary>Alertes et Home Assistant</summary><p class="hint">Une alerte part à la <b>première apparition</b> d'un drone : message MQTT sur <span class="mono">&lt;base&gt;/alert</span> (à brancher sur une automatisation Home Assistant, ntfy, Telegram…) et/ou ligne syslog de gravité « warning » (identifiant <span class="mono">ALERT</span>). La découverte Home Assistant crée seule les capteurs (drones, réseaux Wi-Fi, appareils Bluetooth, drone en vue, état de l'ESP 1, dernière alerte) à chaque connexion MQTT.</p><div class="grid">
    <div><label>Alertes activées</label><select id="alertOn"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Alerte par MQTT</label><select id="alertMqtt"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Alerte par syslog</label><select id="alertSyslog"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Découverte automatique Home Assistant</label><select id="haOn"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Préfixe de découverte</label><input id="haPrefix" autocomplete="off" placeholder="homeassistant"></div>
  </div></details>
  <details><summary>Traces de vol</summary><p class="hint">Chaque drone positionné laisse un parcours sur la carte (gardé en mémoire vive, perdu au redémarrage). Export : bouton « Exporter GeoJSON » sur la carte.</p><div class="grid">
    <div><label>Enregistrer les traces</label><select id="trackOn"><option value="1">Oui</option><option value="0">Non</option></select></div>
    <div><label>Un point tous les (m) au moins</label><input id="trackMinM" type="number" min="1" max="500"></div>
    <div><label>Oubli d'une trace après (min)</label><input id="trackKeepMin" type="number" min="5" max="1440"></div>
  </div></details>
  <details><summary>Listes et journal</summary><div class="grid">
    <div><label>Oubli d'un drone non revu depuis (min)</label><input id="expireMin" type="number" min="1" max="1440"></div>
    <div><label>Oubli d'un réseau / appareil non revu depuis (min)</label><input id="expireNetMin" type="number" min="1" max="1440"></div>
    <div><label>Nouvelle ligne de journal après une absence de (min)</label><input id="gapMin" type="number" min="1" max="1440"></div>
    <div><label>Écriture du journal en flash toutes les (s)</label><input id="saveSec" type="number" min="30" max="3600"></div>
  </div></details>
  <details><summary>Mise à jour depuis GitHub</summary><p class="hint">Dépôt public : laissez le jeton vide. Dépôt privé : créez sur GitHub (Settings → Developer settings → Fine-grained tokens) un jeton limité à CE dépôt, avec la seule permission « Contents : lecture seule », et une date d'expiration. Chaque publication (Release) doit contenir <span class="mono">gateway.bin</span> et <span class="mono">scanner.bin</span>, avec un numéro de version comme nom de tag (ex. <span class="mono">v1.2.0</span>). Le jeton est stocké dans la mémoire de l'ESP 2 (visible par qui le relie en USB). La recherche et l'installation se lancent depuis l'onglet Technique.</p><div class="grid">
    <div><label>Dépôt (compte/nom)</label><input id="updRepo" autocomplete="off" placeholder="kall57/espscanner"></div>
    <div><label>Jeton d'accès (lecture seule)</label><input id="updToken" type="password" autocomplete="new-password" placeholder="(aucun)"></div>
    <div><label>Effacer le jeton enregistré</label><select id="updTokenClear"><option value="0">Non</option><option value="1">Oui (dépôt public)</option></select></div>
  </div></details>
  <details><summary>VPN (WireGuard)</summary><p class="hint">L'ESP 2 se connecte à VOTRE serveur WireGuard (routeur, box compatible, Raspberry Pi…) : aucun port à ouvrir côté ESP. Créez sur le serveur un « pair » pour l'ESP 2 (clé privée générée par l'outil WireGuard, adresse de tunnel unique, ex. 10.8.0.2), puis recopiez ici sa clé privée et la clé PUBLIQUE du serveur. Par défaut seul le trafic du réseau du tunnel passe par le VPN ; l'heure, GitHub et MQTT local restent sur le Wi-Fi. La clé pré-partagée est facultative. L'état s'affiche dans l'onglet Technique.</p><div class="grid">
    <div><label>VPN actif</label><select id="vpnOn"><option value="0">Non</option><option value="1">Oui</option></select></div>
    <div><label>Adresse du serveur (nom ou IP)</label><input id="vpnHost" autocomplete="off" placeholder="vpn.exemple.fr"></div>
    <div><label>Port UDP du serveur</label><input id="vpnPort" type="number" min="1" max="65535" placeholder="51820"></div>
    <div><label>Clé publique du serveur</label><input id="vpnPeerPub" autocomplete="off" placeholder="44 caractères, terminés par ="></div>
    <div><label>Clé privée de l'ESP 2</label><input id="vpnPriv" type="password" autocomplete="new-password" placeholder="(aucune)"></div>
    <div><label>Clé pré-partagée (facultative)</label><input id="vpnPsk" type="password" autocomplete="new-password" placeholder="(aucune)"></div>
    <div><label>Effacer la clé pré-partagée</label><select id="vpnPskClear"><option value="0">Non</option><option value="1">Oui</option></select></div>
    <div><label>Adresse de l'ESP 2 dans le tunnel</label><input id="vpnIp" autocomplete="off" placeholder="10.8.0.2"></div>
    <div><label>Masque du réseau du tunnel</label><input id="vpnMask" autocomplete="off" placeholder="255.255.255.0"></div>
    <div><label>Tout le trafic par le VPN</label><select id="vpnFull"><option value="0">Non (recommandé)</option><option value="1">Oui</option></select></div>
  </div></details>
  <details><summary>Protection de la page</summary><p class="hint">Si vous renseignez un utilisateur, le navigateur demandera ce nom et ce mot de passe. Pour désactiver, videz l'utilisateur.</p><div class="grid">
    <div><label>Utilisateur</label><input id="webUser" autocomplete="off"></div>
    <div><label>Mot de passe</label><input id="webPass" type="password" autocomplete="new-password" placeholder="(inchangé)"></div>
  </div></details>
  <div class="row"><button class="pri" type="submit">Enregistrer</button><button type="button" id="bmq">Tester MQTT</button><button type="button" id="brs" class="dng">Redémarrer l'ESP 2</button></div>
  <div id="mqres" style="margin-top:10px"></div>
</form></div>

<div class="tab" id="t-tech">
  <section class="card"><h2>État</h2><div id="info" class="mono" style="white-space:pre-wrap"></div></section>
  <section class="card"><h2>Mise à jour depuis GitHub</h2>
    <div class="row"><button type="button" id="bupc">Rechercher une mise à jour</button><button type="button" id="bupg" style="display:none">Installer sur l'ESP 2</button><button type="button" id="bups" style="display:none">Installer sur l'ESP 1</button></div>
    <div id="upm" class="mut" style="margin-top:8px;white-space:pre-wrap"></div>
  </section>
  <section class="card"><h2>Mise à jour du firmware (OTA)</h2>
    <p class="hint">Compilez avec PlatformIO puis choisissez le fichier <span class="mono">.pio/build/gateway/firmware.bin</span> (ESP 2) ou <span class="mono">.pio/build/scanner/firmware.bin</span> (ESP 1). Ne choisissez jamais <span class="mono">bootloader.bin</span> ni <span class="mono">partitions.bin</span>. La carte redémarre seule ; ne coupez pas l'alimentation pendant le transfert (ESP 1 : environ 1 minute). Pensez à protéger la page par un mot de passe.</p>
    <div id="fwv" class="mono" style="white-space:pre-wrap;margin-bottom:10px"></div>
    <div class="grid">
      <div><label>Firmware de l'ESP 2 (passerelle)</label><input type="file" id="fw2" accept=".bin"><div class="row" style="padding:8px 0 0"><button type="button" id="bfw2">Mettre à jour l'ESP 2</button></div></div>
      <div><label>Firmware de l'ESP 1 (scanner)</label><input type="file" id="fw1" accept=".bin"><div class="row" style="padding:8px 0 0"><button type="button" id="bfw1">Mettre à jour l'ESP 1</button></div></div>
    </div>
    <progress id="fwp" value="0" max="100" style="width:100%;display:none;margin-top:10px"></progress><div id="fwm" class="mut" style="margin-top:6px"></div>
  </section>
  <section class="card"><h2>Journal technique</h2><pre id="log" class="mono"></pre></section>
</div>
<div id="toast"></div>
</main>
<script>
const $=id=>document.getElementById(id);
let AR=null,AT=null,ARUN=[],SN=[],trN=0,S=null,cfgLoaded=false,offSince=0,map=null,layer=null,fitted=false,tried=false,tab='live',lastRev=-1,DR=[],WF=[],BT=[];
function toast(m){const t=$('toast');t.textContent=m;t.classList.add('on');setTimeout(()=>t.classList.remove('on'),2800)}
async function api(u,o){const r=await fetch(u,o);let j=null;try{j=await r.json()}catch(e){}if(!r.ok)throw new Error((j&&j.error)||('HTTP '+r.status));return j}
function post(u,b){return api(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(b||{})})}
function td(tr,t,cls){const c=document.createElement('td');c.textContent=t;if(cls)c.className=cls;tr.appendChild(c)}
function tdBar(tr,rssi){const c=document.createElement('td');const b=document.createElement('span');b.className='bar';b.style.width=Math.max(4,Math.min(60,(rssi+100)))+'px';c.appendChild(b);c.appendChild(document.createTextNode(rssi+' dBm'));tr.appendChild(c)}
function setChip(el,t,cls){el.textContent=t;el.className='chip '+cls}
function dur(s){if(s<60)return s+' s';if(s<3600)return Math.floor(s/60)+' min '+(s%60)+' s';return Math.floor(s/3600)+' h '+Math.floor(s%3600/60)+' min'}
function fdate(t){if(!t)return '—';const d=new Date(t*1000);return d.toLocaleString('fr-FR',{day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit',second:'2-digit'})}
function plural(n,a,b){return n+' '+(n>1?b:a)}
document.querySelectorAll('#tabs button').forEach(b=>b.onclick=()=>{tab=b.dataset.t;document.querySelectorAll('#tabs button').forEach(x=>x.classList.toggle('on',x===b));document.querySelectorAll('.tab').forEach(x=>x.classList.toggle('on',x.id==='t-'+tab));
  if(tab==='jnl')loadJournal();if(tab==='tech')loadLog();if(tab==='wifi')loadWifi();if(tab==='ble')loadBt();if(tab==='sens')loadSens();if(tab==='ant')loadTargets();if(tab==='live'&&map)setTimeout(()=>map.invalidateSize(),50)});

function setTheme(t){document.documentElement.dataset.theme=t;try{localStorage.setItem('th',t)}catch(e){}$('bth').textContent=t==='light'?'Thème sombre':'Thème clair';
  if(tab==='ant')drawAnt();if(tab==='wifi')renderWifi();if(tab==='jnl')loadJournal()}
$('bth').onclick=()=>setTheme(document.documentElement.dataset.theme==='light'?'dark':'light');
$('bth').textContent=document.documentElement.dataset.theme==='light'?'Thème sombre':'Thème clair';

const WXC={0:'Ciel dégagé',1:'Plutôt dégagé',2:'Partiellement nuageux',3:'Couvert',45:'Brouillard',48:'Brouillard givrant',51:'Bruine légère',53:'Bruine',55:'Bruine dense',56:'Bruine verglaçante',57:'Bruine verglaçante',61:'Pluie faible',63:'Pluie',65:'Forte pluie',66:'Pluie verglaçante',67:'Pluie verglaçante',71:'Neige faible',73:'Neige',75:'Forte neige',77:'Grains de neige',80:'Averses faibles',81:'Averses',82:'Fortes averses',85:'Averses de neige',86:'Fortes averses de neige',95:'Orage',96:'Orage et grêle',99:'Orage violent et grêle'};
const WXD=['N','NE','E','SE','S','SO','O','NO'];
let WXP=null,WXT=0;
try{WXP=JSON.parse(localStorage.getItem('wxp'))}catch(e){}
function wxMsg(t){const b=$('wxbody');b.textContent=t;b.className='mut'}
function wxSave(p){WXP=p;try{localStorage.setItem('wxp',JSON.stringify(p))}catch(e){}$('wxform').style.display='none';$('wxres').textContent='';wxFetch()}
async function wxFetch(){if(!WXP){return}$('wxplace').textContent=WXP.name;clearTimeout(WXT);WXT=setTimeout(wxFetch,600000);
  try{const u='https://api.open-meteo.com/v1/forecast?latitude='+WXP.lat+'&longitude='+WXP.lon+'&current=temperature_2m,apparent_temperature,relative_humidity_2m,precipitation,weather_code,cloud_cover,pressure_msl,wind_speed_10m,wind_direction_10m,wind_gusts_10m&hourly=wind_gusts_10m,visibility,precipitation_probability&wind_speed_unit=ms&timezone=auto&forecast_days=2';
    const r=await fetch(u);if(!r.ok)throw 0;const j=await r.json();const c=j.current,h=j.hourly;let i=h.time.findIndex(t=>t>=c.time);if(i<0)i=0;
    const vis=h.visibility[i],gust=c.wind_gusts_10m,pr=c.precipitation;
    let lvl=0,why=[];if(gust>=12){lvl=2;why.push('rafales fortes')}else if(gust>=8){lvl=1;why.push('rafales')}
    if(pr>=0.5){lvl=2;why.push('précipitations')}else if(pr>0){lvl=Math.max(lvl,1);why.push('pluie faible')}
    if(vis!==undefined&&vis<1500){lvl=2;why.push('visibilité faible')}else if(vis!==undefined&&vis<5000){lvl=Math.max(lvl,1);why.push('visibilité réduite')}
    const b=$('wxbody');b.textContent='';b.className='';
    const top=el('div','wxh');top.appendChild(el('span','wxt',Math.round(c.temperature_2m)+' °C'));top.appendChild(el('span','',(WXC[c.weather_code]||'—')+' · ressenti '+Math.round(c.apparent_temperature)+' °C'));
    top.appendChild(el('span','',' · vent '+c.wind_speed_10m.toFixed(1)+' m/s · rafales '+gust.toFixed(1)+' m/s'));top.appendChild(el('span','chip '+['ok','warn','err'][lvl],lvl===0?'Conditions favorables au vol':lvl===1?'Prudence : '+why.join(', '):'Déconseillé : '+why.join(', ')));b.appendChild(top);
    const g=el('div','wxg');[['Vent',c.wind_speed_10m.toFixed(1)+' m/s'],['Rafales',gust.toFixed(1)+' m/s'],['Direction (d\'où il vient)',WXD[Math.round(c.wind_direction_10m/45)%8]+' '+Math.round(c.wind_direction_10m)+'°'],['Visibilité',vis!==undefined?(vis>=10000?(vis/1000).toFixed(0)+' km':(vis/1000).toFixed(1)+' km'):'—'],['Précipitations',pr+' mm'],['Nuages',c.cloud_cover+' %'],['Humidité',c.relative_humidity_2m+' %'],['Pression',Math.round(c.pressure_msl)+' hPa']].forEach(x=>{const w=el('div');w.appendChild(el('span','',x[0]));w.appendChild(el('b','',x[1]));g.appendChild(w)});b.appendChild(g);
    b.appendChild(el('div','hint','Indicateur indicatif (rafales ≥ 8 m/s, pluie, visibilité < 5 km) : il ne remplace ni la notice du drone ni la réglementation.'));
    const n=8,gv=h.wind_gusts_10m.slice(i,i+n),lb=h.time.slice(i,i+n).map(t=>t.slice(11,13)+'h');barChart($('wxchart'),gv,lb,'Rafales prévues (m/s)',null,v=>v.toFixed(0))}
  catch(e){wxMsg('Météo indisponible : pas d\'accès à Internet depuis cet appareil, ou service momentanément injoignable.')}}
$('wxset').onclick=()=>{const f=$('wxform');f.style.display=f.style.display==='none'?'flex':'none';if(f.style.display==='flex')$('wxq').focus()};
$('wxgo').onclick=async()=>{const q=$('wxq').value.trim(),r=$('wxres');r.textContent='';if(!q)return;
  try{const j=await(await fetch('https://geocoding-api.open-meteo.com/v1/search?name='+encodeURIComponent(q)+'&count=5&language=fr&format=json')).json();
    if(!j.results||!j.results.length){r.textContent='Aucun lieu trouvé.';return}
    j.results.forEach(x=>{const b=el('button','',x.name+(x.admin1?', '+x.admin1:'')+(x.country?' ('+x.country+')':''));b.type='button';b.onclick=()=>wxSave({name:x.name,lat:+x.latitude.toFixed(3),lon:+x.longitude.toFixed(3)});r.appendChild(b)})}
  catch(e){r.textContent='Recherche impossible (pas d\'accès à Internet ?).'}};
$('wxq').onkeydown=e=>{if(e.key==='Enter')$('wxgo').click()};
$('wxgeo').onclick=()=>{const r=$('wxres');if(!navigator.geolocation){r.textContent='Position indisponible sur ce navigateur : saisissez une ville.';return}
  navigator.geolocation.getCurrentPosition(p=>wxSave({name:'Ma position',lat:+p.coords.latitude.toFixed(3),lon:+p.coords.longitude.toFixed(3)}),()=>{r.textContent='Position refusée ou indisponible (le navigateur l\'interdit sur une page http) : saisissez une ville.'})};
function wxSize(v){if(['s','m','l'].indexOf(v)<0)v='m';$('wx').dataset.sz=v;$('wxsz').value=v;try{localStorage.setItem('wxsz',v)}catch(e){}}
$('wxsz').onchange=()=>wxSize($('wxsz').value);
function wxInit(){let v='m';try{v=localStorage.getItem('wxsz')||'m'}catch(e){}wxSize(v);if(WXP)wxFetch()}
function loadMap(){if(tried)return;tried=true;
  const css=document.createElement('link');css.rel='stylesheet';css.href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css';document.head.appendChild(css);
  const js=document.createElement('script');js.src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js';
  js.onload=()=>{map=L.map('map').setView([46.6,2.4],5);L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'© OpenStreetMap'}).addTo(map);layer=L.layerGroup().addTo(map);renderMap(DR);loadTracks()};
  js.onerror=()=>{$('mapmsg').textContent='Carte indisponible : votre appareil n\'a pas accès à Internet (la liste reste utilisable).'};
  document.head.appendChild(js)}
let TR=[];
function tcol(k){let h=0;for(let i=0;i<k.length;i++)h=(h*31+k.charCodeAt(i))>>>0;return 'hsl('+(h%360)+',80%,60%)'}
async function loadTracks(){if(!map||!$('ctr').checked){TR=[];if(map)renderMap(DR);return}try{TR=(await api('/api/tracks')).tracks;renderMap(DR)}catch(e){}}
function renderMap(ds){if(!map)return;layer.clearLayers();const pts=[];
  if($('ctr').checked)TR.forEach(t=>{if(t.pts.length>1)L.polyline(t.pts.map(p=>[p[0],p[1]]),{color:tcol(t.mac),weight:3,opacity:.8}).bindTooltip('Trace de '+(t.id||t.mac)+' · '+t.pts.length+' points').addTo(layer)});
  ds.forEach(d=>{if(d.lat===undefined)return;const p=[d.lat,d.lon];pts.push(p);L.circleMarker(p,{radius:8,color:'#3ea6ff',fillOpacity:.7}).bindTooltip((d.id||d.mac)+(d.alt_geo!==undefined?' · '+Math.round(d.alt_geo)+' m':'')).addTo(layer);
    if(d.op_lat!==undefined){const o=[d.op_lat,d.op_lon];L.circleMarker(o,{radius:5,color:'#d29922',fillOpacity:.8}).bindTooltip('Opérateur de '+(d.id||d.mac)).addTo(layer);L.polyline([p,o],{color:'#d29922',weight:1,dashArray:'4'}).addTo(layer)}});
  if(pts.length&&!fitted){map.fitBounds(pts,{maxZoom:15,padding:[30,30]});fitted=true}}

function el(t,c,x){const e=document.createElement(t);if(c)e.className=c;if(x!==undefined)e.textContent=x;return e}
let SEEN=null,NDT=0;
const UAT=['non précisé','avion','hélicoptère / multirotor','autogire','hybride','ornithoptère','planeur','cerf-volant','ballon libre','ballon captif','dirigeable','parachute','fusée','motorisé captif','obstacle au sol','autre'];
const IDT=['non précisé','numéro de série (ANSI/CTA-2063-A)','numéro d\'enregistrement (CAA)','identifiant UTM','identifiant de session'];
const STT=['état non déclaré','au sol','en vol','URGENCE','panne du Remote ID'];
function odTime(ts){const n=Date.now();let t=Math.floor(n/3600000)*3600000+ts*1000;if(t>n+1800000)t-=3600000;const dt=Math.round((n-t)/1000);
  return new Date(t).toLocaleTimeString('fr-FR')+' (décalage avec cette page : '+(dt>0?'+':'')+dt+' s)'}
function dcard(d){const c=el('article','dcard'+(d.status>=3?' em':''));const h=el('div','dh');h.appendChild(el('b','mono',d.id||'(sans id)'));
  if(d.status!==undefined)h.appendChild(el('span','chip'+(d.status>=3?' err':d.status===2?' ok':''),STT[d.status]||('état '+d.status)));
  h.appendChild(el('span','chip '+(d.src.indexOf('ble')>=0?'s-ble':'s-wifi'),d.src));c.appendChild(h);
  const g=el('div','ds');const v=(x,u,n)=>x!==undefined?(n===undefined?Math.round(x):x.toFixed(n))+' '+u:'—';
  [['Altitude (GPS)',v(d.alt_geo,'m')],['Altitude baro.',v(d.alt_baro,'m')],['Hauteur / sol',v(d.height,'m')],['Vitesse',v(d.speed,'m/s',1)],['Vitesse vert.',v(d.vspeed,'m/s',1)],['Cap',d.dir!==undefined?d.dir+'°':'—']].forEach(x=>{const w=el('div');w.appendChild(el('span','',x[0]));w.appendChild(el('b','',x[1]));g.appendChild(w)});c.appendChild(g);
  const m=[];
  m.push('Position : '+(d.lat!==undefined?d.lat.toFixed(5)+', '+d.lon.toFixed(5):'—'));
  m.push('Pilote / décollage : '+(d.op_lat!==undefined?d.op_lat.toFixed(5)+', '+d.op_lon.toFixed(5):'—'));
  m.push('N° d\'enregistrement opérateur : '+(d.op_id||'—'));
  if(d.reg)m.push('Enregistrement (CAA) : '+d.reg);
  if(d.id_type!==undefined)m.push('Type d\'identifiant : '+(IDT[d.id_type]||d.id_type)+' · aéronef : '+(UAT[d.ua_type]||d.ua_type));
  if(d.fr_manufacturer)m.push('Constructeur '+d.fr_manufacturer+' · modèle '+d.fr_model+' · série '+d.fr_serial);
  if(d.ansi_id)m.push('ANSI/CTA-2063 : '+d.ansi_id);
  if(d.desc)m.push('Description : '+d.desc);
  if(d.od_ts!==undefined)m.push('Horodatage émetteur : '+odTime(d.od_ts));
  c.appendChild(el('div','dm mono',m.join('\n')));
  const f=el('div','df');const b=el('span','bar');b.style.width=Math.max(4,Math.min(60,d.rssi+100))+'px';f.appendChild(b);f.appendChild(el('span','',d.rssi+' dBm'));
  const ag=el('span','','il y a '+d.age+' s');ag.style.marginLeft='auto';f.appendChild(ag);c.appendChild(f);return c}
function newDrones(ds){const keys=ds.map(d=>d.mac||d.id);
  if(SEEN===null){SEEN=new Set(keys);return}
  ds.forEach(d=>{const k=d.mac||d.id;if(SEEN.has(k))return;SEEN.add(k);const a=$('newdr');a.textContent='';a.appendChild(el('b','','Nouveau drone détecté'));a.appendChild(el('span','grow',(d.id||d.mac)+' · '+d.src+' · '+d.rssi+' dBm'));
    const x=el('button','','Fermer');x.onclick=()=>{a.style.display='none'};a.appendChild(x);a.style.display='flex';clearTimeout(NDT);NDT=setTimeout(()=>{a.style.display='none'},60000)})}
function renderDrones(){const f=$('fsrc').value;const ds=DR.filter(d=>!f||d.src.split('+').includes(f)||(f==='wifi'&&d.src.indexOf('wifi')>=0));
  newDrones(DR);const dc=$('dc');dc.textContent='';
  ds.slice().sort((a,b)=>a.age-b.age).forEach(d=>dc.appendChild(dcard(d)));
  $('empty').style.display=ds.length?'none':'block';$('cnt').textContent=plural(ds.length,'drone','drones');renderMap(ds)}
$('fsrc').onchange=renderDrones;
$('ctr').onchange=loadTracks;
$('btrc').onclick=()=>{if(confirm('Effacer toutes les traces ?'))post('/api/tracks/clear').then(()=>{TR=[];renderMap(DR);toast('Traces effacées')})};

function match(q,arr){q=q.trim().toLowerCase();return !q||arr.some(v=>String(v||'').toLowerCase().indexOf(q)>=0)}
function renderWifi(){const q=$('wq').value;const l=WF.filter(n=>match(q,[n.ssid,n.bssid,n.enc_txt,n.ch])).sort((a,b)=>b.rssi-a.rssi);const tb=$('wtb');tb.textContent='';
  l.forEach(n=>{const tr=document.createElement('tr');td(tr,n.ssid||'(réseau masqué)',n.ssid?'':'mut');td(tr,n.bssid,'mono');td(tr,n.ch||'—');td(tr,n.enc_txt);tdBar(tr,n.rssi);td(tr,n.age+' s');td(tr,n.frames);tb.appendChild(tr)});
  $('wempty').style.display=l.length?'none':'block';$('wcnt').textContent=plural(l.length,'réseau','réseaux');
  const cnt=Array(13).fill(0),mxr=Array(13).fill(-100);WF.forEach(n=>{if(n.ch>=1&&n.ch<=13){cnt[n.ch-1]++;mxr[n.ch-1]=Math.max(mxr[n.ch-1],n.rssi)}});
  const lb=cnt.map((_,i)=>String(i+1));
  barChart($('wch1'),cnt,lb,'Réseaux par canal');
  barChart($('wch2'),mxr.map(v=>v+100),lb,'Signal le plus fort par canal','#f0883e',v=>(v-100)+' dBm');
  const empty=cnt.map((c,i)=>c?'':String(i+1)).filter(x=>x);
  $('wchh').textContent=WF.length?('Canaux sans aucun réseau relevé : '+(empty.length?empty.join(', '):'aucun')+'. Un canal libre est un bon choix pour fixer le canal du scanner (Réglages → Scanner) ; le NAN (Remote ID) émet sur le canal 6. Le scanner ne voit un réseau que sur les canaux qu\'il visite.'):'Aucun réseau relevé pour le moment.'}
function renderBt(){const q=$('bq').value;const l=BT.filter(d=>(!$('btrk').checked||d.tag)&&match(q,[d.name,d.mac,d.mf_name,d.addr_type,d.tag])).sort((a,b)=>b.rssi-a.rssi);const tb=$('btb');tb.textContent='';
  l.forEach(d=>{const tr=document.createElement('tr');td(tr,d.name||'(sans nom)',d.name?'':'mut');td(tr,d.mac,'mono');td(tr,d.addr_type);td(tr,d.tag?('⚠ '+d.tag+(d.mf_name?' · '+d.mf_name:'')):(d.mf!==undefined?(d.mf_name||('0x'+d.mf.toString(16).toUpperCase().padStart(4,'0'))):'—'));tdBar(tr,d.rssi);td(tr,d.age+' s');td(tr,d.frames);tb.appendChild(tr)});
  $('bempty').style.display=l.length?'none':'block';$('bcnt').textContent=plural(l.length,'appareil','appareils')}
$('wq').oninput=renderWifi;$('bq').oninput=renderBt;$('btrk').onchange=renderBt;
function sensText(d){const p=[];
  if(d.encrypted&&Object.keys(d).length<12)return 'Annonces chiffrées (non lisibles)';
  if(d.temperature!==undefined)p.push('🌡 '+d.temperature.toFixed(1)+' °C');if(d.humidity!==undefined)p.push('💧 '+d.humidity.toFixed(0)+' %');
  if(d.moisture!==undefined)p.push('🌱 sol '+d.moisture+' %');if(d.conductivity!==undefined)p.push('fertilité '+d.conductivity+' µS/cm');
  if(d.illuminance!==undefined)p.push('☀ '+d.illuminance+' lx');if(d.pressure!==undefined)p.push('pression '+d.pressure.toFixed(1)+' hPa');
  if(d.window)p.push('fenêtre '+(d.window==='open'?'OUVERTE':'fermée'));if(d.door)p.push('porte '+(d.door==='open'?'OUVERTE':'fermée'));
  if(d.motion!==undefined)p.push(d.motion?'mouvement':'pas de mouvement');if(d.button!==undefined)p.push('bouton '+d.button);
  if(d.rotation!==undefined)p.push('rotation '+d.rotation+'°');return p.join(' · ')||'—'}
function renderSens(){const tb=$('stb');tb.textContent='';
  SN.slice().sort((a,b)=>a.age-b.age).forEach(d=>{const tr=document.createElement('tr');td(tr,d.name+' ('+d.type+')');td(tr,d.mac,'mono');td(tr,sensText(d));td(tr,d.battery!==undefined?d.battery+' %'+(d.battery_voltage!==undefined?' · '+(d.battery_voltage/1000).toFixed(2)+' V':''):(d.battery_voltage!==undefined?(d.battery_voltage/1000).toFixed(2)+' V':'—'));tdBar(tr,d.rssi);td(tr,dur(d.age));td(tr,d.frames);tb.appendChild(tr)});
  $('sempty').style.display=SN.length?'none':'block';$('scnt').textContent=plural(SN.length,'capteur','capteurs')}
async function loadSens(){try{SN=await api('/api/sensors');renderSens()}catch(e){}}
const ASRC={wifi:['/api/wifi',n=>n.bssid,n=>(n.ssid||'(réseau masqué)')+' · canal '+(n.ch||'?')],ble:['/api/ble',d=>d.mac,d=>(d.name||d.mac)],sens:['/api/sensors',d=>d.mac,d=>d.name+' ('+d.type+')'],drone:['/api/drones',d=>d.mac,d=>d.id||d.mac]};
async function loadTargets(){const c=ASRC[$('asrc').value],sel=$('atg');try{const l=(await api(c[0])).sort((a,b)=>b.rssi-a.rssi);const cur=sel.value;sel.textContent='';
  l.forEach(x=>{const o=document.createElement('option');o.value=c[1](x);o.textContent=c[2](x)+' · '+x.rssi+' dBm';sel.appendChild(o)});if(cur)sel.value=cur;
  if(!l.length){const o=document.createElement('option');o.textContent='(aucune source reçue pour le moment)';o.value='';sel.appendChild(o)}}catch(e){}}
function aStats(r){const p=r.pts;if(!p.length)return null;const v=p.map(x=>x[1]);let gap=0;for(let i=1;i<p.length;i++)gap=Math.max(gap,p[i][0]-p[i-1][0]);
  const T=Math.max(1,r.dur||0);return{n:p.length,min:Math.min(...v),max:Math.max(...v),avg:v.reduce((a,b)=>a+b,0)/v.length,gap,rate:(r.frames||0)*60/T,frames:r.frames||0}}
function drawAnt(){const host=$('achart');host.textContent='';const W=640,H=240,L=38,R=8,T=10,B=22;const runs=ARUN.concat(AR&&AR.pts.length?[AR]:[]);
  let tm=60;runs.forEach(r=>{if(r.pts.length)tm=Math.max(tm,r.pts[r.pts.length-1][0])});if(AR)tm=Math.max(tm,AR.dur||0);
  const X=t=>L+(W-L-R)*t/tm,Y=v=>T+(H-T-B)*(-30-v)/70;
  const s=sv('svg',{viewBox:'0 0 '+W+' '+H,width:'100%',role:'img'});
  for(let v=-100;v<=-30;v+=10){s.appendChild(sv('line',{x1:L,x2:W-R,y1:Y(v),y2:Y(v),stroke:cv('--line')}));s.appendChild(sv('text',{x:L-4,y:Y(v)+3,fill:cv('--mut'),'font-size':9,'text-anchor':'end'},String(v)))}
  for(let t=0;t<=tm;t+=tm>300?60:tm>120?30:10){s.appendChild(sv('text',{x:X(t),y:H-8,fill:cv('--mut'),'font-size':9,'text-anchor':'middle'},t+' s'))}
  runs.forEach((r,i)=>{const col=r===AR?cv('--fg'):ACOL[i%4];if(r.pts.length>1)s.appendChild(sv('polyline',{points:r.pts.map(x=>X(x[0]).toFixed(1)+','+Y(x[1]).toFixed(1)).join(' '),fill:'none',stroke:col,'stroke-width':1.6}));
    r.pts.forEach(x=>s.appendChild(sv('circle',{cx:X(x[0]),cy:Y(x[1]),r:2,fill:col})))});
  host.appendChild(s);
  const lg=[];ARUN.forEach((r,i)=>{const a=aStats(r);lg.push('■ '+r.label+' : '+(a?a.n+' pts · moyenne '+a.avg.toFixed(0)+' dBm · min '+a.min+' · max '+a.max+' · '+a.rate.toFixed(1)+' trames/min · plus long silence '+a.gap.toFixed(0)+' s':'—'))});
  if(AR){const a=aStats(AR);lg.push('▶ En cours : '+(a?a.n+' pts · moyenne '+a.avg.toFixed(0)+' dBm · min '+a.min+' · max '+a.max+' · '+a.rate.toFixed(1)+' trames/min · plus long silence '+a.gap.toFixed(0)+' s':'en attente de réception…')+' · '+Math.round(AR.dur||0)+' s')}
  $('astat').textContent=lg.join('\n')}
async function aTick(){if(!AR)return;const c=ASRC[AR.src];try{const l=await api(c[0]);const it=l.find(x=>c[1](x)===AR.key);AR.dur=(Date.now()-AR.t0)/1000;
  if(it){if(AR.fl===null){AR.fl=it.frames;AR.f0=it.frames}else if(it.frames!==AR.fl){AR.pts.push([AR.dur,it.rssi]);AR.fl=it.frames}AR.frames=it.frames-AR.f0}
  drawAnt()}catch(e){}}
$('ago').onclick=()=>{if(AR){clearInterval(AT);AR.dur=(Date.now()-AR.t0)/1000;$('ago').textContent='Démarrer';AR.stopped=true;AT=null;drawAnt();return}
  const k=$('atg').value;if(!k){toast('Choisissez une source');return}AR={key:k,src:$('asrc').value,t0:Date.now(),pts:[],fl:null,f0:0,frames:0,dur:0};$('ago').textContent='Arrêter';AT=setInterval(aTick,2000);aTick()};
$('akeep').onclick=()=>{if(!AR||!AR.pts.length){toast('Aucune mesure à garder');return}const a=$('antScanner')?$('antScanner').value:'';
  const lab=$('alab').value.trim()||('Courbe '+(ARUN.length+1)+(a==='1'?' (antenne externe)':a==='0'?' (antenne céramique)':''));
  ARUN.push({label:lab,pts:AR.pts.slice(),dur:AR.dur,frames:AR.frames});if(ARUN.length>4)ARUN.shift();clearInterval(AT);AT=null;AR=null;$('ago').textContent='Démarrer';$('alab').value='';drawAnt()};
$('aclr').onclick=()=>{ARUN=[];drawAnt()};$('aref').onclick=loadTargets;$('asrc').onchange=loadTargets;
const ACOL=['#3ea6ff','#f0883e','#3fb950','#bc8cff'];
async function loadWifi(){try{WF=await api('/api/wifi');renderWifi()}catch(e){}}
async function loadBt(){try{BT=await api('/api/ble');renderBt()}catch(e){}}
const NS='http://www.w3.org/2000/svg';
function cv(n){return getComputedStyle(document.documentElement).getPropertyValue(n).trim()}
function sv(t,a,txt){const e=document.createElementNS(NS,t);for(const k in a)e.setAttribute(k,a[k]);if(txt!==undefined)e.textContent=txt;return e}
function barChart(host,vals,labels,title,color,fmt){fmt=fmt||(v=>String(v));host.textContent='';const W=480,H=140,L=26,B=18,T=16,n=vals.length,mx=Math.max(1,...vals),bw=(W-L)/n;
  const s=sv('svg',{viewBox:'0 0 '+W+' '+H,width:'100%',role:'img'});
  s.appendChild(sv('text',{x:0,y:11,fill:cv('--mut'),'font-size':11},title));
  vals.forEach((v,i)=>{const h=(H-B-T)*v/mx;s.appendChild(sv('rect',{x:L+i*bw+1,y:H-B-h,width:Math.max(1,bw-2),height:h,fill:color||cv('--acc'),rx:2}));
    if(labels[i]!=='')s.appendChild(sv('text',{x:L+i*bw+bw/2,y:H-4,fill:cv('--mut'),'font-size':9,'text-anchor':'middle'},labels[i]))});
  s.appendChild(sv('text',{x:L-4,y:T+8,fill:cv('--mut'),'font-size':9,'text-anchor':'end'},fmt(mx)));
  s.appendChild(sv('text',{x:L-4,y:H-B,fill:cv('--mut'),'font-size':9,'text-anchor':'end'},fmt(0)));host.appendChild(s)}
function renderStats(j){const v=j.filter(x=>x.first>1.7e9);const now=new Date();
  const days=[],dl=[];for(let k=13;k>=0;k--){const d=new Date(now.getTime()-k*86400000);days.push(d.toDateString());dl.push(k%2?'':String(d.getDate()).padStart(2,'0')+'/'+String(d.getMonth()+1).padStart(2,'0'))}
  const dv=days.map(d=>v.filter(x=>new Date(x.first*1000).toDateString()===d).length);
  const hv=Array(24).fill(0);v.forEach(x=>hv[new Date(x.first*1000).getHours()]++);
  const ids=new Set(v.map(x=>x.id||x.mac||'?'));
  $('jsum').textContent=v.length?(v.length+' détection(s) datée(s) · '+ids.size+' drone(s) distinct(s) · aujourd\'hui : '+dv[13]):'Aucune détection datée pour le moment (l\'heure doit être connue).';
  barChart($('jday'),dv,dl,'Détections par jour (14 derniers jours)');
  barChart($('jhour'),hv,hv.map((_,i)=>i%3?'':i+'h'),'Détections par heure de la journée','#3fb950')}
async function loadJournal(){try{const j=await api('/api/journal');renderStats(j);$('jcnt').textContent=j.length;$('n-jnl').textContent=j.length;const tb=$('jtb');tb.textContent='';
  j.forEach(x=>{const tr=document.createElement('tr');td(tr,fdate(x.first));td(tr,fdate(x.last));td(tr,dur(x.dur));td(tr,(x.id||'(sans id)')+(x.reg?' · '+x.reg:''),'mono');td(tr,x.src);
    td(tr,x.lat!==undefined?x.lat.toFixed(5)+', '+x.lon.toFixed(5):'—','mono');td(tr,x.alt_geo!==undefined?x.alt_geo+' m':'—');td(tr,x.frames);td(tr,x.rssi_best+' dBm');tb.appendChild(tr)});
  $('jempty').style.display=j.length?'none':'block'}catch(e){toast('Journal : '+e.message)}}
async function loadLog(){try{const l=await api('/api/log');const p=$('log');p.textContent=l.join('\n');p.scrollTop=p.scrollHeight}catch(e){}}

function renderStatus(s){S=s;const sc=s.scanner,w=s.wifiInfo,m=s.mqttInfo;
  setChip($('csc'),sc.alive?'ESP 1 reçoit':'ESP 1 absent',sc.alive?'ok':'err');
  setChip($('cwf'),w.sta?('Wi-Fi '+w.ssid):(w.apOn?'Point d\'accès '+w.ap:'Wi-Fi…'),w.sta?'ok':(w.apOn?'warn':''));
  setChip($('cmq'),(!m.enabled||!m.host)?'MQTT off':(m.connected?'MQTT connecté':'MQTT non connecté'),(!m.enabled||!m.host)?'':(m.connected?'ok':'err'));
  $('n-live').textContent=s.drones;$('n-wifi').textContent=s.nets;$('n-ble').textContent=s.bts;$('n-sens').textContent=s.sens;$('n-jnl').textContent=s.journal;
  $('stats').textContent='· Bluetooth '+sc.ble+' · Wi-Fi '+sc.wifi+' trames de drones · canal '+(sc.ch||'—')+' · MQTT '+m.published+' msg';
  const off=$('off');const msgs=[];
  if(!sc.alive)msgs.push('L\'ESP 1 ne répond pas sur la liaison série : vérifiez les fils TX/RX croisés (D6↔D7) et la masse commune.');
  if(w.apOn&&!w.sta)msgs.push('Réseau « '+w.ssid+' » inaccessible : point d\'accès « '+w.ap+' » ouvert ('+w.apIp+').');
  if(m.enabled&&!m.connected&&m.error)msgs.push('MQTT : '+m.error);
  off.style.display=msgs.length?'block':'none';off.textContent=msgs.join('  ·  ');
  $('info').textContent=['ESP 2 — RAM libre '+s.heap+' o · démarrage n°'+s.boot+' · actif '+dur(s.uptime)+' · heure '+(s.timeOk?'connue':'inconnue')+' · flash '+(s.fs?'OK':'ERREUR'),
   'ESP 1 — '+(sc.alive?'actif':'absent')+' · actif '+dur(sc.up)+' · RAM libre '+sc.heap+' o · BLE '+(sc.bleOn?'on':'off')+' · Wi-Fi '+(sc.wifiOn?'on':'off')+' (canal '+sc.ch+') · perdus '+sc.drop+' · drones '+sc.tracked+' · réseaux '+sc.nets+' · appareils BT '+sc.bts,
   'Firmware — ESP 2 compilé le '+(s.fw||'?')+' · ESP 1 compilé le '+(s.scannerFw||'inconnu (ancienne version sans OTA)'),
   'Liaison — '+sc.lines+' ligne(s) valides · '+sc.bad+' rejetée(s) (bruit/CRC)',
   'Wi-Fi — '+w.state+(w.sta?' · '+w.ip+' · '+w.rssi+' dBm':'')+(w.apOn?' · point d\'accès '+w.ap+' ('+w.apClients+' client(s))':''),
   'VPN — '+(!s.vpn.on?'désactivé':['','en attente','démarrage','poignée de main','connecté ('+s.vpn.ip+')','erreur'][s.vpn.state]+(s.vpn.msg?' · '+s.vpn.msg:''))+'',
   'Syslog — '+(s.syslog.enabled?s.syslog.host+' · '+s.syslog.sent+' message(s) envoyé(s)'+(s.syslog.error?' · '+s.syslog.error:''):'désactivé'),
   'MQTT — '+(m.enabled?m.host:'désactivé')+' · '+(m.connected?'connecté':'non connecté')+(m.error?' · '+m.error:'')].join('\n');
  if(!s.timeOk){post('/api/time',{t:Math.floor(Date.now()/1000)}).catch(()=>{})}
  if(tab==='jnl'&&s.journalRev!==lastRev){lastRev=s.journalRev;loadJournal()}}

const FIELDS=['wifiSsid','hostname','staGiveupSec','apMode','apName','mode','wifiSurvey','btSurvey','chan','bleDuty','rssiMin','antScanner','antGateway','mqttEnabled','mqttHost','mqttPort','mqttUser','mqttBase','pubList','pubDrones','pubWifi','pubBt','pubSens','mqttRetain','mqttListSec','mqttDroneSec','syslogOn','syslogHost','syslogPort','syslogFmt','syslogBody','syslogFac','slDrones','slDroneSec','slWifi','slBt','slSystem','alertOn','alertMqtt','alertSyslog','haOn','haPrefix','trackOn','trackMinM','trackKeepMin','updRepo','vpnOn','vpnHost','vpnPort','vpnPeerPub','vpnIp','vpnMask','vpnFull','vpnPskClear','updTokenClear','expireMin','expireNetMin','gapMin','saveSec','webUser'];
const PASSES=['wifiPass','apPass','mqttPass','webPass','updToken','vpnPriv','vpnPsk'];
async function loadCfg(){const c=await api('/api/config');FIELDS.forEach(k=>{let v=c[k];if(typeof v==='boolean')v=v?'1':'0';if(v!==undefined)$(k).value=v});
  PASSES.forEach(k=>{$(k).placeholder=c[k+'Set']?'(inchangé)':'(aucun)'});if(!c.apName)$('apName').placeholder=c.apNameNow||'espscanner';cfgLoaded=true}
async function tick(){try{const[s,d]=await Promise.all([api('/api/status'),api('/api/drones')]);offSince=0;DR=d;renderStatus(s);renderDrones();if(!cfgLoaded)loadCfg();if(++trN%3===0)loadTracks();
    if(tab==='tech')loadLog();if(tab==='wifi')loadWifi();if(tab==='ble')loadBt();if(tab==='sens')loadSens()}
  catch(e){if(!offSince)offSince=Date.now();$('off').style.display='block';$('off').textContent='Page injoignable depuis '+Math.round((Date.now()-offSince)/1000)+' s…'}
  setTimeout(tick,2000)}
$('f').onsubmit=async e=>{e.preventDefault();const b={};FIELDS.forEach(k=>b[k]=$(k).value);PASSES.forEach(k=>{if($(k).value)b[k]=$(k).value});
  try{await post('/api/config',b);PASSES.forEach(k=>$(k).value='');$('vpnPskClear').value='0';$('updTokenClear').value='0';toast('Enregistré');loadCfg()}catch(x){toast(x.message)}};
$('bmq').onclick=async()=>{const r=$('mqres');r.textContent='Test en cours…';try{const j=await post('/api/mqtt/test');r.textContent='';j.steps.forEach(s=>{const d=document.createElement('div');d.className='step '+(s.ok?'ok':'ko');d.textContent=s.msg;r.appendChild(d)})}catch(x){r.textContent=x.message}};
$('bsl').onclick=async()=>{const r=$('slres');r.textContent='Envoi…';try{const j=await post('/api/syslog/test');r.textContent=(j.ok?'✔ ':'✖ ')+j.msg}catch(x){r.textContent=x.message}};
$('bpub').onclick=()=>post('/api/publish').then(()=>toast('Publication demandée')).catch(x=>toast(x.message));
$('bsim').onclick=()=>post('/api/test/drone').then(()=>toast('Drone de test ajouté')).catch(x=>toast(x.message));
$('bclr').onclick=()=>{if(confirm('Vider la liste des drones ?'))post('/api/clear',{what:'drones'}).then(()=>toast('Liste vidée'))};
$('bwclr').onclick=()=>{if(confirm('Vider la liste des réseaux Wi-Fi ?'))post('/api/clear',{what:'wifi'}).then(()=>{toast('Liste vidée');loadWifi()})};
$('bsclr').onclick=()=>{if(confirm('Vider la liste des capteurs ?'))post('/api/clear',{what:'sens'}).then(()=>{toast('Liste vidée');loadSens()})};
$('bbclr').onclick=()=>{if(confirm('Vider la liste des appareils Bluetooth ?'))post('/api/clear',{what:'bt'}).then(()=>{toast('Liste vidée');loadBt()})};
$('bjclr').onclick=()=>{if(confirm('Vider le journal ?'))post('/api/journal/clear').then(()=>{toast('Journal vidé');loadJournal()})};
function otaSend(target,inp){const f=inp.files[0];const m=$('fwm'),p=$('fwp');
  if(!f){m.textContent='Choisissez d\'abord un fichier firmware.bin';return}
  if(f.size<100000||f.size>1703936){m.textContent='Ce fichier ne ressemble pas à un firmware.bin ('+f.size+' octets).';return}
  if(!confirm('Envoyer '+f.name+' ('+Math.round(f.size/1024)+' ko) vers l\'ESP '+(target==='scanner'?'1':'2')+' ? La carte va redémarrer.'))return;
  const fd=new FormData();fd.append('fw',f,f.name);const x=new XMLHttpRequest();
  x.open('POST','/api/ota/'+target+'?size='+f.size);p.style.display='block';p.value=0;m.textContent='Envoi en cours… ne coupez pas l\'alimentation.';
  x.upload.onprogress=e=>{if(e.lengthComputable)p.value=Math.round(e.loaded*100/e.total)};
  x.onload=()=>{let j=null;try{j=JSON.parse(x.responseText)}catch(e){}
    if(j&&j.ok){p.value=100;m.textContent='✔ Mise à jour terminée, la carte redémarre (patientez 20 s puis rechargez la page).';if(target==='gateway')setTimeout(()=>location.reload(),20000)}
    else m.textContent='✖ '+((j&&j.error)||('Échec (HTTP '+x.status+')'))};
  x.onerror=()=>{m.textContent=target==='gateway'?'Connexion coupée : la carte redémarre peut-être. Rechargez la page dans 20 s.':'✖ Connexion perdue pendant l\'envoi.'};
  x.send(fd)}
async function updRun(path,msg,body){const m=$('upm');m.textContent=msg;try{return await post(path,body)}catch(x){m.textContent='✖ '+x.message;return null}}
$('bupc').onclick=async()=>{const j=await updRun('/api/update/check','Recherche en cours…');if(!j)return;
  $('bupg').style.display=$('bups').style.display='none';
  if(!j.ok){$('upm').textContent='✖ '+j.error;return}
  const g=j.newer,sc=j.newerSc,st='ESP 2 : '+j.current+(g?' → '+j.latest:' (à jour)')+'\nESP 1 : '+(j.scCurrent||'inconnue (ESP 1 absent ou ancienne version)')+(sc?' → '+j.latest:(j.scCurrent?' (à jour)':''));
  if(!g&&!sc){$('upm').textContent='✔ Tout est à jour (dernière publication '+j.latest+').\n'+st;return}
  $('upm').textContent='Publication '+j.latest+' disponible.\n'+st+(j.notes?'\n'+j.notes:'')+(g&&!j.hasGw?'\n⚠ gateway.bin absent de la publication':'')+(sc&&!j.hasSc?'\n⚠ scanner.bin absent de la publication':'');
  $('bupg').style.display=g&&j.hasGw?'':'none';$('bups').style.display=sc&&j.hasSc?'':'none'};
async function updGo(t,name){if(!confirm('Télécharger et installer la mise à jour sur '+name+' ? La carte va redémarrer.'))return;
  const j=await updRun('/api/update/install','Téléchargement et installation… ne coupez pas l\'alimentation (jusqu\'à 2 minutes).',{target:t});if(!j)return;
  $('upm').textContent=j.ok?'✔ Installée, '+name+' redémarre (patientez 20 s puis rechargez la page).':'✖ '+j.error;if(j.ok&&t==='gateway')setTimeout(()=>location.reload(),20000)}
$('bupg').onclick=()=>updGo('gateway','l\'ESP 2');$('bups').onclick=()=>updGo('scanner','l\'ESP 1');
$('bfw2').onclick=()=>otaSend('gateway',$('fw2'));$('bfw1').onclick=()=>otaSend('scanner',$('fw1'));
$('brs').onclick=()=>{if(confirm('Redémarrer l\'ESP 2 ?'))post('/api/restart').then(()=>toast('Redémarrage…'))};
loadMap();tick();wxInit();
</script></body></html>)rawliteral";
