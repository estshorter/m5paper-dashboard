#include "SHT3X.h"

namespace SHT3X
{
  int SHT3X::read()
  {
    uint8_t data[6];
    constexpr uint8_t COMMAND_MEASURE[2] = {0x2C, 0x06};

    // Start I2C Transmission
    _wire->beginTransmission(_address);
    // Send measurement command
    _wire->write(COMMAND_MEASURE, 2);
    // Stop I2C transmission
    if (_wire->endTransmission() != 0)
      return 1;

    delay(20);

    // Request 6 bytes of data
    _wire->requestFrom(_address, static_cast<size_t>(6));

    // Read 6 bytes of data
    // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
    for (int i = 0; i < 6; i++)
      data[i] = _wire->read();

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
} // namespace SHT3X