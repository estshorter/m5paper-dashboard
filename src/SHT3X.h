#pragma once

#include "Arduino.h"
#include "Wire.h"

namespace SHT3X
{

  class SHT3X
  {
  public:
    SHT3X(TwoWire &wire = Wire) : _wire(wire) {}

    int read(void);

    // assume _wire.begin() is already called
    // void begin(void)
    // {
    //   _wire.begin();
    // };

    float getTemperature(void) const
    {
      return _cTemp;
    }

    uint_fast8_t getHumidity(void) const
    {
      return _humidity;
    }

  private:
    TwoWire &_wire;
    float _cTemp = 0;
    uint_fast8_t _humidity = 0;
    static constexpr int Addr = 0x44;

    static uint8_t crc8(const uint8_t *data, const int len);
  };
}; // namespace SHT3X