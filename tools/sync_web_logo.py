#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SVG_PATH = ROOT / "assets" / "logo.svg"
OUT_PATH = ROOT / "firmware" / "Psi_Unit_Firmware" / "web_logo.h"

svg = SVG_PATH.read_text(encoding="utf-8")
if ")svg\"" in svg:
    raise SystemExit("SVG contains raw-string terminator )svg\"")

header = """\
/*
 * web_logo.h — SVG logo for web UI (PROGMEM)
 *
 * Source: assets/logo.svg
 */

#ifndef WEB_LOGO_H
#define WEB_LOGO_H

#include <Arduino.h>

static const char WEB_LOGO_SVG[] PROGMEM = R"svg("""
footer = """\
)svg";

#endif // WEB_LOGO_H
"""

OUT_PATH.write_text(header + svg + footer, encoding="utf-8", newline="\n")
print(f"Wrote {OUT_PATH} ({OUT_PATH.stat().st_size} bytes)")
