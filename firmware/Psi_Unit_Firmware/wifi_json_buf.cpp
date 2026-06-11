/*
 * wifi_json_buf.cpp — helpers for fixed-buffer JSON (web API, sensor log)
 */

#include "wifi_json_buf.h"
#include "system_info.h"
#include <stdio.h>
#include <stdarg.h>

static const char kOverflowJson[] = "{\"ok\":false,\"error\":\"json_overflow\"}";

int wifi_json_off(char *buf, size_t cap, int off, const char *fmt, ...) {
  if (buf == nullptr || cap == 0 || off < 0) {
    return -1;
  }
  if ((size_t)off >= cap) {
    return -1;
  }
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(buf + off, cap - (size_t)off, fmt, ap);
  va_end(ap);
  if (n < 0) {
    return -1;
  }
  const int next = off + n;
  if ((size_t)next >= cap) {
    return -1;
  }
  return next;
}

int wifi_json_append_cstr(char *buf, size_t cap, int off, const char *src) {
  if (off < 0) {
    return -1;
  }
  if (src == nullptr) {
    return off;
  }
  while (*src) {
    if ((size_t)off + 1 >= cap) {
      return -1;
    }
    buf[off++] = *src++;
  }
  buf[off] = '\0';
  return off;
}

int wifi_json_append_escape(char *buf, size_t cap, int off, const char *src) {
  if (off < 0) {
    return -1;
  }
  if (src == nullptr) {
    return off;
  }
  while (*src) {
    const char c = *src++;
    if (c == '\\' || c == '"') {
      if ((size_t)off + 2 >= cap) {
        return -1;
      }
      buf[off++] = '\\';
      buf[off++] = c;
    } else if (c == '\n' || c == '\r') {
      if ((size_t)off + 1 >= cap) {
        return -1;
      }
      buf[off++] = ' ';
    } else {
      if ((size_t)off + 1 >= cap) {
        return -1;
      }
      buf[off++] = c;
    }
    buf[off] = '\0';
  }
  return off;
}

int wifi_json_append_uptime(char *buf, size_t cap, int off, uint32_t uptimeSec) {
  char uptimeFmt[SYSINFO_VALUE_CHARS + 1];
  system_info_formatUptimeSec(uptimeSec, uptimeFmt, sizeof(uptimeFmt));
  off = wifi_json_off(buf, cap, off, ",\"uptime\":%lu,\"uptime_fmt\":\"", (unsigned long)uptimeSec);
  if (off < 0) {
    return -1;
  }
  off = wifi_json_append_escape(buf, cap, off, uptimeFmt);
  if (off < 0) {
    return -1;
  }
  return wifi_json_append_cstr(buf, cap, off, "\"");
}

bool wifi_json_valid(int off, size_t cap) {
  return off >= 0 && cap > 0 && (size_t)off < cap;
}

const char *wifi_json_overflow_response() {
  return kOverflowJson;
}
