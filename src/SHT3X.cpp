#include "SHT3X.h"
/*
Software License Agreement (BSD License)

Copyright (c) 2012, Adafruit Industries
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3. Neither the name of the copyright holders nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
namespace SHT3X
{
  int SHT3X::read()
  {
    uint8_t data[6];
    constexpr uint8_t COMMAND_MEASURE[2] = {0x2C, 0x06};

    // Start I2C Transmission
    _wire.beginTransmission(Addr);
    // Send measurement command
    _wire.write(COMMAND_MEASURE, 2);
    // Stop I2C transmission
    if (_wire.endTransmission() != 0)
      return 1;

    delay(20);

    // Request 6 bytes of data
    _wire.requestFrom(Addr, static_cast<size_t>(6));

    // Read 6 bytes of data
    // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
    for (int i = 0; i < 6; i++)
      data[i] = _wire.read();

    if (data[2] != crc8(data, 2) || data[5] != crc8(data + 3, 2))
      return 1;

    int32_t sTemp = (static_cast<uint32_t>(data[0]) << 8) | data[1];
    // simplified (65536 instead of 65535) integer version of:
    // temp = (sTemp * 175.0f) / 65535.0f - 45.0f;
    sTemp = ((4375 * sTemp) >> 14) - 4500;
    _cTemp = static_cast<float>(sTemp) / 100.0f;

    uint32_t sHum = (static_cast<uint32_t>(data[3]) << 8) | data[4];
    // simplified (65536 instead of 65535) integer version of:
    // humidity = (shum * 100.0f) / 65535.0f;
    sHum = (625 * sHum) >> 12;
    _humidity = static_cast<uint_fast8_t>(sHum / 100);

    return 0;
  }

  /**
 * Performs a CRC8 calculation on the supplied values.
 *
 * @param data  Pointer to the data to use when calculating the CRC8.
 * @param len   The number of bytes in 'data'.
 *
 * @return The computed CRC8 value.
 */
  uint8_t SHT3X::crc8(const uint8_t *data, const int len)
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
} // namespace SHT3X