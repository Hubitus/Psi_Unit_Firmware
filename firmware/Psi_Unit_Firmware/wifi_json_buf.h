/*
 * wifi_json_buf.h — JSON assembly into static buffers without Arduino String
 */

#ifndef WIFI_JSON_BUF_H
#define WIFI_JSON_BUF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Append formatted text; returns new offset, or -1 on overflow / vsnprintf error.
int wifi_json_off(char *buf, size_t cap, int off, const char *fmt, ...);

int wifi_json_append_cstr(char *buf, size_t cap, int off, const char *src);

int wifi_json_append_escape(char *buf, size_t cap, int off, const char *src);

int wifi_json_append_uptime(char *buf, size_t cap, int off, uint32_t uptimeSec);

bool wifi_json_valid(int off, size_t cap);

const char *wifi_json_overflow_response();

#endif // WIFI_JSON_BUF_H
