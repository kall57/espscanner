# Compresse la page web (src/gateway/web_ui.h) en gzip -> src/gateway/web_ui_gz.h
# Lancé automatiquement avant chaque compilation par PlatformIO (extra_scripts), ou à la main : python tools/gzip_ui.py
import gzip, os, re, sys

def build(root):
    src = os.path.join(root, "src", "gateway", "web_ui.h")
    dst = os.path.join(root, "src", "gateway", "web_ui_gz.h")
    if os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(src):
        return
    txt = open(src, encoding="utf-8").read()
    m = re.search(r'R"rawliteral\((.*)\)rawliteral"', txt, re.S)
    if not m:
        raise SystemExit("gzip_ui: page introuvable dans web_ui.h")
    raw = m.group(1).encode("utf-8")
    gz = gzip.compress(raw, 9, mtime=0)
    out = ["#pragma once", "#include <pgmspace.h>",
           "// FICHIER GÉNÉRÉ par tools/gzip_ui.py à partir de web_ui.h — ne pas modifier.",
           "static const size_t INDEX_GZ_LEN = %d;" % len(gz),
           "static const uint8_t INDEX_GZ[] PROGMEM = {"]
    for i in range(0, len(gz), 24):
        out.append(",".join(str(b) for b in gz[i:i + 24]) + ",")
    out.append("};")
    open(dst, "w", newline="\n").write("\n".join(out) + "\n")
    print("gzip_ui: %d -> %d octets" % (len(raw), len(gz)))

try:
    Import("env")  # noqa: F821  (PlatformIO)
    build(env.subst("$PROJECT_DIR"))  # noqa: F821
except NameError:
    build(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
