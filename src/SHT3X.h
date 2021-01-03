#pragma once

#include "Arduino.h"
#include "Wire.h"

namespace SHT3X
{

  class SHT3X
  {
  public:
    SHT3X(TwoWire *theWire = &Wire, const uint8_t address = 0x44)
    {
      _wire = theWire;
      _address = address;
    };

    int read(void);
    void begin(void)
    {
      _wire->begin();
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
    uint8_t _address;
    TwoWire *_wire;
    float _cTemp = 0;
    uint_fast8_t _humidity = 0;

    /**
 * Performs a CRC8 calculation on the supplied values.
 *
 * @param data  Pointer to the data to use when calculating the CRC8.
 * @param len   The number of bytes in 'data'.
 *
 * @return The computed CRC8 value.
 */
    static uint8_t crc8(const uint8_t *data, const int len)
    {
      /*
   *
   * CRC-8 formula from page 14 of SHT spec pdf
   *
   * Test data 0xBE, 0xEF should yield 0x92
   *
   * Initialization data 0xFF
   * Polynomial 0x31 (x8 + x5 +x4 +1)
   * Final XOR 0x00
   */

      constexpr uint8_t POLYNOMIAL(0x31);
      uint8_t crc(0xFF);

      for (int j = len; j; --j)
      {
        crc ^= *data++;

        for (int i = 8; i; --i)
        {
          crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
        }
      }
      return crc;
    }
  };
} // namespace SHT3X