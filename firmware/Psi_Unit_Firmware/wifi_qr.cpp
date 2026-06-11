/*
 * wifi_qr.cpp — Wi-Fi join QR code generation
 *
 * Payload format: WIFI:T:WPA;S:<ssid>;P:<pass>;;
 * Generator: qrcode.c / qrcode.h (Richard Moore, MIT) — included in the project.
 */

#include "wifi_qr.h"
#include "config.h"
#include "display_backend.h"

#if WIFI_ENABLE && defined(ARDUINO_ARCH_ESP32) && !defined(SIM_BUILD)

#include "qrcode.h"
#include "wifi_json_buf.h"

static bool wifi_qr_probeVersion(const char *payload, uint8_t version, uint8_t *outDim) {
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(version)];
  if (qrcode_initText(&qrcode, qrcodeData, version, ECC_LOW, payload) != 0) {
    return false;
  }
  *outDim = qrcode.size;
  return true;
}

static bool wifi_qr_drawVersion(uint8_t x, uint8_t y, uint8_t pixelSize, const char *payload, uint8_t version) {
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(version)];
  if (qrcode_initText(&qrcode, qrcodeData, version, ECC_LOW, payload) != 0) {
    return false;
  }

  const uint8_t dim = qrcode.size;
  const uint16_t needW = (uint16_t)dim * pixelSize;
  const uint16_t needH = needW;
  if ((uint16_t)x + needW > DISP_WIDTH || (uint16_t)y + needH > DISP_HEIGHT) {
    return false;
  }

  disp_setDrawColor(1);
  for (uint8_t row = 0; row < dim; row++) {
    for (uint8_t col = 0; col < dim; col++) {
      if (qrcode_getModule(&qrcode, col, row)) {
        disp_drawBox(x + (uint8_t)(col * pixelSize), y + (uint8_t)(row * pixelSize), pixelSize, pixelSize);
      }
    }
  }
  return true;
}

static bool wifi_qr_pickVersion(const char *payload, uint8_t *outVersion, uint8_t *outDim) {
  static const uint8_t kTryVersions[] = {3, 4, 5, 6};
  for (uint8_t i = 0; i < sizeof(kTryVersions); i++) {
    const uint8_t version = kTryVersions[i];
    if (wifi_qr_probeVersion(payload, version, outDim)) {
      *outVersion = version;
      return true;
    }
  }
  return false;
}

bool wifi_qr_draw(uint8_t x, uint8_t y, uint8_t pixelSize, const char *payload) {
  if (payload == nullptr || pixelSize == 0) {
    return false;
  }
  uint8_t version = 0;
  uint8_t dim = 0;
  if (!wifi_qr_pickVersion(payload, &version, &dim)) {
    return false;
  }
  return wifi_qr_drawVersion(x, y, pixelSize, payload, version);
}

bool wifi_qr_drawFullscreen(const char *payload, uint8_t *outPixelSize) {
  if (payload == nullptr) {
    return false;
  }

  uint8_t dim = 0;
  uint8_t version = 0;
  if (!wifi_qr_pickVersion(payload, &version, &dim)) {
    return false;
  }

  const uint8_t pixW = (uint8_t)(DISP_WIDTH / dim);
  const uint8_t pixH = (uint8_t)(DISP_HEIGHT / dim);
  uint8_t pixelSize = pixW < pixH ? pixW : pixH;
  if (pixelSize < 1) {
    pixelSize = 1;
  }
  if (pixelSize > 4) {
    pixelSize = 4;
  }

  const uint8_t qrW = (uint8_t)(dim * pixelSize);
  const uint8_t qrH = qrW;
  const uint8_t x = (uint8_t)((DISP_WIDTH - qrW) / 2);
  const uint8_t y = (uint8_t)((DISP_HEIGHT - qrH) / 2);

  if (!wifi_qr_drawVersion(x, y, pixelSize, payload, version)) {
    return false;
  }
  if (outPixelSize != nullptr) {
    *outPixelSize = pixelSize;
  }
  return true;
}

bool wifi_qr_buildSvg(const char *payload, char *out, size_t cap, uint8_t modulePx, int *outLen) {
  if (payload == nullptr || out == nullptr || cap == 0) {
    return false;
  }
  if (modulePx == 0) {
    modulePx = 4;
  }

  uint8_t version = 0;
  uint8_t dim = 0;
  if (!wifi_qr_pickVersion(payload, &version, &dim)) {
    return false;
  }

  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(version)];
  if (qrcode_initText(&qrcode, qrcodeData, version, ECC_LOW, payload) != 0) {
    return false;
  }

  const uint16_t px = (uint16_t)dim * modulePx;
  int off = wifi_json_off(out, cap, 0,
                          "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%u\" height=\"%u\" "
                          "viewBox=\"0 0 %u %u\" shape-rendering=\"crispEdges\">"
                          "<rect width=\"%u\" height=\"%u\" fill=\"#fff\"/><path fill=\"#000\" d=\"",
                          (unsigned)px, (unsigned)px, (unsigned)dim, (unsigned)dim, (unsigned)dim,
                          (unsigned)dim);
  if (off < 0) {
    return false;
  }

  for (uint8_t row = 0; row < dim; row++) {
    uint8_t col = 0;
    while (col < dim) {
      while (col < dim && !qrcode_getModule(&qrcode, col, row)) {
        col++;
      }
      if (col >= dim) {
        break;
      }
      const uint8_t runStart = col;
      while (col < dim && qrcode_getModule(&qrcode, col, row)) {
        col++;
      }
      const uint8_t runLen = (uint8_t)(col - runStart);
      // Closed 1-unit-tall bar; plain "M x,y h w" is a zero-area stroke and does not fill.
      off = wifi_json_off(out, cap, off, "M%u,%uh%uv1h-%uz", (unsigned)runStart, (unsigned)row,
                          (unsigned)runLen, (unsigned)runLen);
      if (off < 0) {
        return false;
      }
    }
  }

  off = wifi_json_append_cstr(out, cap, off, "\"/></svg>");
  if (off < 0) {
    return false;
  }
  if (outLen != nullptr) {
    *outLen = off;
  }
  return true;
}

#else

bool wifi_qr_draw(uint8_t x, uint8_t y, uint8_t pixelSize, const char *payload) {
  (void)x;
  (void)y;
  (void)pixelSize;
  (void)payload;
  return false;
}

bool wifi_qr_drawFullscreen(const char *payload, uint8_t *outPixelSize) {
  (void)payload;
  if (outPixelSize != nullptr) {
    *outPixelSize = 0;
  }
  return false;
}

bool wifi_qr_buildSvg(const char *payload, char *out, size_t cap, uint8_t modulePx, int *outLen) {
  (void)payload;
  (void)out;
  (void)cap;
  (void)modulePx;
  if (outLen != nullptr) {
    *outLen = 0;
  }
  return false;
}

#endif
