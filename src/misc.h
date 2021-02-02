#pragma once

#include <functional>
#include <HTTPClient.h>
#include <ArduinoJson.hpp>
#include <FastLED.h>

#include <LovyanGFX.hpp>

inline String WiFiConnectedToString(void)
{
    return WiFi.isConnected() ? String("OK") : String("NG");
}

String weekdayToString(const int8_t weekDay)
{
    switch (weekDay)
    {
    case 0:
        return String("日");
    case 1:
        return String("月");
    case 2:
        return String("火");
    case 3:
        return String("水");
    case 4:
        return String("木");
    case 5:
        return String("金");
    case 6:
        return String("土");
    }
    return String("");
}

uint_fast16_t getCo2Data(void)
{
    using namespace ArduinoJson;
    constexpr auto CO2_DATA_URL = "http://192.168.10.105/api/data";
    constexpr uint16_t HTTP_TIMEOUT = 3000;

    if (!WiFi.isConnected())
        return 0;

    WiFiClient client;
    HTTPClient http;
    if (!http.begin(client, CO2_DATA_URL))
    {
        Serial.printf("[HTTP] Failed to parse url\n");
        return 0;
    }
    http.setTimeout(HTTP_TIMEOUT);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n",
                      http.errorToString(httpCode).c_str());
        http.end();
        return 0;
    }
    StaticJsonDocument<64> filter;
    filter["co2"]["value"] = true;

    StaticJsonDocument<64> doc;
    auto err = deserializeJson(doc, client, DeserializationOption::Filter(filter));
    http.end();
    if (err)
    {
        Serial.printf("[JSON] DeserializationError, error: %s\n", err.c_str());
        return 0;
    }

    return doc["co2"]["value"];
}

void setLEDColor(CRGB *leds, const uint_fast16_t co2)
{
    constexpr uint_fast8_t ID_LED_USE = 1;
    leds[0] = CRGB::Black;
    leds[2] = CRGB::Black;
    if (co2 < 600)
    {
        leds[ID_LED_USE] = CRGB::White;
    }
    else if (co2 < 1200)
    {
        leds[ID_LED_USE] = CRGB::Green;
    }
    else if (co2 < 1500)
    {
        leds[ID_LED_USE] = CRGB::Yellow;
    }
    else if (co2 < 2000)
    {
        leds[ID_LED_USE] = CRGB::Red;
    }
    else
    {
        leds[ID_LED_USE] = CRGB::Red;
        leds[0] = CRGB::Red;
        leds[2] = CRGB::Red;
    }
    FastLED.show();
}

inline void prettyEpdRefresh(LGFX &gfx)
{
    gfx.setEpdMode(epd_mode_t::epd_quality);
    gfx.fillScreen(TFT_WHITE);
    gfx.setEpdMode(epd_mode_t::epd_fast);
}

int syncNTPTime(std::function<void(const tm &)> datetimeSetter, const char *tz,
                const char *server1, const char *server2 = nullptr, const char *server3 = nullptr)
{
    if (!WiFi.isConnected())
    {
        return 1;
    }

    configTzTime(tz, server1, server2, server3);

    // https://github.com/espressif/esp-idf/blob/master/examples/protocols/sntp/main/sntp_example_main.c
    int retry = 0;
    constexpr int retry_count = 50;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        delay(100);
    }
    if (retry == retry_count)
    {
        return 1;
    }

    struct tm datetime;
    if (!getLocalTime(&datetime))
        return 1;

    datetimeSetter(datetime);

    return 0;
}