/*
 * Arduino.h — stub for desktop simulator (do not use on ESP32)
 */

#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef PGM_P
#define PGM_P const char *
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif

#ifndef pgm_read_ptr
#define pgm_read_ptr(addr) (*(const void *const *)(addr))
#endif

#ifndef F
#define F(x) (x)
#endif

#ifndef PSTR
#define PSTR(s) (s)
#endif

#ifndef constrain
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif

#ifndef digitalPinToGPIONumber
#define digitalPinToGPIONumber(p) (p)
#endif

typedef const char *__FlashStringHelper;

unsigned long millis();
void delay(unsigned long ms);
void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, int value);

#define INPUT 0
#define INPUT_PULLUP 1
#define OUTPUT 2
#define LOW 0
#define HIGH 1

#define LED_BUILTIN 13

// Analog pins (in config.h: A4/A5 I2C, A6 DS18)
#ifndef A0
#define A0 0
#define A1 1
#define A2 2
#define A3 3
#define A4 4
#define A5 5
#define A6 6
#define A7 7
#endif

class String {
public:
  String() {}
  String(const char *) {}
  String(const __FlashStringHelper *) {}
  String(int) {}
  String(unsigned int) {}
  String(long) {}
  String(unsigned long) {}
  void reserve(unsigned) {}
  String &operator=(const char *) { return *this; }
  String &operator+=(const char *) { return *this; }
  String &operator+=(char) { return *this; }
  String &operator+=(int) { return *this; }
  String &operator+=(unsigned int) { return *this; }
  String &operator+=(long) { return *this; }
  String &operator+=(unsigned long) { return *this; }
  String &operator+=(const __FlashStringHelper *) { return *this; }
  const char *c_str() const { return ""; }
};

class Print {
public:
  void print(const char *) {}
  void print(char) {}
  void print(int, int = 10) {}
  void print(unsigned int, int = 10) {}
  void print(long, int = 10) {}
  void print(unsigned long, int = 10) {}
  void println(const char *) {}
  void println(const __FlashStringHelper *) {}
  void println() {}
};

class HardwareSerial : public Print {};

extern HardwareSerial Serial;

inline void yield() {}

#ifndef strcpy_P
#define strcpy_P(dest, src) strcpy((dest), (src))
#endif
#ifndef strncpy_P
#define strncpy_P(dest, src, n) strncpy((dest), (src), (n))
#endif
#ifndef memcpy_P
#define memcpy_P(dest, src, n) memcpy((dest), (src), (n))
#endif

#endif // ARDUINO_H
