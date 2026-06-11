$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$url = 'https://raw.githubusercontent.com/olikraus/u8g2/master/csrc/u8g2_fonts.c'
$tmp = Join-Path $root 'u8g2_fonts.c'
$dst = Join-Path $root 'u8g2_font_6x10_tr.cpp'
Write-Host "Downloading..."
Invoke-WebRequest -Uri $url -OutFile $tmp -UseBasicParsing
$c = Get-Content $tmp -Raw -Encoding UTF8
$marker = 'const uint8_t u8g2_font_6x10_tr'
$start = $c.IndexOf($marker)
if ($start -lt 0) {
  Select-String -Path $tmp -Pattern '6x10' | Select-Object -First 20
  throw "u8g2_font_6x10_tr not found"
}
$end = $c.IndexOf('const uint8_t u8g2_font_', $start + $marker.Length)
if ($end -lt 0) { $end = $c.Length }
$chunk = $c.Substring($start, $end - $start).Trim().TrimEnd(',')
$out = @(
  '// Auto-generated from u8g2_fonts.c'
  '#include <stdint.h>'
  ''
  $chunk
  ';'
)
[System.IO.File]::WriteAllText($dst, ($out -join "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "OK" $dst
