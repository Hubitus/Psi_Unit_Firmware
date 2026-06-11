#ifndef DALLAS_TEMPERATURE_H
#define DALLAS_TEMPERATURE_H

class OneWire;

class DallasTemperature {
public:
  explicit DallasTemperature(OneWire *) {}
};

#endif
