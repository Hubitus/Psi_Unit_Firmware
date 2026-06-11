$root = Split-Path -Parent $PSScriptRoot
$svgPath = Join-Path $root "assets\logo.svg"
$hPath = Join-Path $root "firmware\Psi_Unit_Firmware\web_logo.h"
$svg = [System.IO.File]::ReadAllText($svgPath)
if ($svg.Contains(')svg"')) {
    Write-Error "SVG contains raw-string terminator"
    exit 1
}
$header = @"
/*
 * web_logo.h — SVG logo for web UI (PROGMEM)
 *
 * Source: assets/logo.svg
 */

#ifndef WEB_LOGO_H
#define WEB_LOGO_H

#include <Arduino.h>

static const char WEB_LOGO_SVG[] PROGMEM = R"svg(
"@
$footer = @"
)svg";

#endif // WEB_LOGO_H
"@
[System.IO.File]::WriteAllText($hPath, $header + $svg + $footer)
Write-Host "Wrote $hPath ($((Get-Item $hPath).Length) bytes)"
