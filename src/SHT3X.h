#pragma once

#include "Arduino.h"
#include "Wire.h"

namespace SHT3X
{

  class SHT3X
  {
  public:
    SHT3X(TwoWire &wire = Wire, const uint8_t address = 0x44) : _wire(wire), _address(address) {}

    int read(void);
    void begin(void)
    {
      _wire.begin();
    };

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
    uint8_t _address;
    float _cTemp = 0;
    uint_fast8_t _humidity = 0;

    static uint8_t crc8(const uint8_t *data, const int len);
  };
}; // namespace SHT3X