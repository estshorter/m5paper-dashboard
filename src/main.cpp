#include <M5EPD.h>
#include <ArduinoOTA.h>
#define FASTLED_INTERNAL //suppress pragma message
#include <FastLED.h>
#include "SHT3X.h"
#include "WiFiInfo.h"

#define LGFX_M5PAPER
#include <LovyanGFX.hpp>
#include "myFont.h"
#include "misc.h"

constexpr float FONT_SIZE_LARGE = 3.0;
constexpr float FONT_SIZE_SMALL = 1.0;
constexpr uint_fast16_t M5PAPER_SIZE_LONG_SIDE = 960;
constexpr uint_fast16_t M5PAPER_SIZE_SHORT_SIDE = 540;

rtc_time_t time_ntp;
rtc_date_t date_ntp{4, 1, 1, 1970};

TwoWire &wire_portA = Wire1;
SemaphoreHandle_t xMutex = nullptr;
SHT3X::SHT3X sht30(wire_portA);
LGFX gfx;
CRGB leds[3];

inline int syncNTPTimeJP(void)
{
  constexpr auto NTP_SERVER1 = "ntp.nict.jp";
  constexpr auto NTP_SERVER2 = "time.cloudflare.com";
  constexpr auto NTP_SERVER3 = "time.google.com";
  constexpr auto TIME_ZONE = "JST-9";

  auto datetime_setter = [](const tm &datetime) {
    rtc_time_t time{
        static_cast<int8_t>(datetime.tm_hour),
        static_cast<int8_t>(datetime.tm_min),
        static_cast<int8_t>(datetime.tm_sec)};
    rtc_date_t date{
        static_cast<int8_t>(datetime.tm_wday),
        static_cast<int8_t>(datetime.tm_mon + 1),
        static_cast<int8_t>(datetime.tm_mday),
        static_cast<int16_t>(datetime.tm_year + 1900)};

    M5.RTC.setDateTime(date, time);
    date_ntp = date;
    time_ntp = time;
  };

  return syncNTPTime(datetime_setter, TIME_ZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
}

void handleBtnPPress(void)
{
  xSemaphoreTake(xMutex, portMAX_DELAY);
  prettyEpdRefresh(gfx);
  gfx.setTextSize(FONT_SIZE_SMALL);

  gfx.startWrite();
  gfx.setCursor(0, 0);
  if (!syncNTPTimeJP())
  {
    gfx.println("Succeeded to sync time");
    struct tm timeInfo;
    if (getLocalTime(&timeInfo))
    {
      gfx.print("getLocalTime:");
      gfx.println(&timeInfo, "%Y/%m/%d %H:%M:%S");
    }
  }
  else
  {
    gfx.println("Failed to sync time");
  }

  rtc_date_t date;
  rtc_time_t time;

  // Get RTC
  M5.RTC.getDateTime(date, time);
  gfx.print("RTC         :");
  gfx.printf("%04d/%02d/%02d ", date.year, date.mon, date.day);
  gfx.printf("%02d:%02d:%02d", time.hour, time.min, time.sec);
  gfx.endWrite();

  delay(1000);

  gfx.setTextSize(FONT_SIZE_LARGE);
  xSemaphoreGive(xMutex);
}

inline void handleBtnRPress(void)
{
  xSemaphoreTake(xMutex, portMAX_DELAY);
  prettyEpdRefresh(gfx);
  xSemaphoreGive(xMutex);
}

void handleBtnLPress(void)
{
  xSemaphoreTake(xMutex, portMAX_DELAY);
  prettyEpdRefresh(gfx);
  gfx.setCursor(0, 0);
  gfx.setTextSize(FONT_SIZE_SMALL);
  gfx.print("Good bye..");
  gfx.waitDisplay();
  M5.disableEPDPower();
  M5.disableEXTPower();
  M5.disableMainPower();
  esp_deep_sleep_start();
  while (true)
    ;
  xSemaphoreGive(xMutex);
}

void handleButton(void *pvParameters)
{
  while (true)
  {
    delay(500);
    M5.update();
    if (M5.BtnP.isPressed())
    {
      handleBtnPPress();
    }
    else if (M5.BtnR.isPressed())
    {
      handleBtnRPress();
    }
    else if (M5.BtnL.isPressed())
    {
      handleBtnLPress();
    }
  }
}

void setup(void)
{
  constexpr uint_fast16_t WIFI_CONNECT_RETRY_MAX = 60; // 10 = 5s
  constexpr uint_fast16_t WAIT_ON_FAILURE = 2000;

  M5.begin(true, false, true, true, true, true);
  WiFi.begin(WiFiInfo::SSID, WiFiInfo::PASS);

  FastLED.addLeds<WS2811, 26, GRB>(leds, 3).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(5);

  gfx.init();
  gfx.setEpdMode(epd_mode_t::epd_fast);
  gfx.setRotation(1);
  // gfx.setFont(&fonts::lgfxJapanGothic_40);
  gfx.setFont(&myFont::myFont);
  gfx.setTextSize(FONT_SIZE_SMALL);

  gfx.print("Connecting to Wi-Fi network");
  for (int cnt_retry = 0;
       cnt_retry < WIFI_CONNECT_RETRY_MAX && !WiFi.isConnected();
       cnt_retry++)
  {
    delay(500);
    gfx.print(".");
  }
  gfx.println("");
  if (WiFi.isConnected())
  {
    gfx.print("Local IP: ");
    gfx.println(WiFi.localIP());
  }
  else
  {
    gfx.println("Failed to connect to a Wi-Fi network");
    delay(WAIT_ON_FAILURE);
  }

  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });

  ArduinoOTA.begin();

  // env2 unit
  if (!sht30.begin(25, 32, 400000))
  {
    gfx.println("Failed to initialize external I2C");
  }

  xMutex = xSemaphoreCreateMutex();
  if (xMutex != nullptr)
  {
    xSemaphoreGive(xMutex);
    xTaskCreatePinnedToCore(handleButton, "handleButton", 4096, nullptr, 1, nullptr, 1);
  }
  else
  {
    gfx.println("Failed to create a task for buttons");
  }
  gfx.println("Init done");
  delay(1000);
  gfx.setTextSize(FONT_SIZE_LARGE);
  prettyEpdRefresh(gfx);
  gfx.setCursor(0, 0);
}

void loop(void)
{
  constexpr uint_fast16_t SLEEP_SEC = 5;
  constexpr uint_fast32_t TIME_SYNC_CYCLE = 7 * 3600 * 24 / SLEEP_SEC;

  static uint32_t cnt = 0;

  xSemaphoreTake(xMutex, portMAX_DELAY);
  ArduinoOTA.handle();

  float tmp = 0.0;
  uint_fast8_t hum = 0;

  if (!sht30.read())
  {
    tmp = sht30.getTemperature();
    hum = sht30.getHumidity();
  }
  auto co2 = getCo2Data();
  setLEDColor(leds, co2);

  rtc_date_t date;
  rtc_time_t time;

  M5.RTC.getDateTime(date, time);

  gfx.startWrite();
  gfx.fillScreen(TFT_WHITE);
  gfx.fillRect(0.57 * M5PAPER_SIZE_LONG_SIDE, 0, 3, M5PAPER_SIZE_SHORT_SIDE, TFT_BLACK);

  constexpr uint_fast16_t offset_y = 30;
  constexpr uint_fast16_t offset_x = 45;

  gfx.setCursor(0, offset_y);
  gfx.setClipRect(offset_x, offset_y, M5PAPER_SIZE_LONG_SIDE - offset_x, M5PAPER_SIZE_SHORT_SIDE - offset_y);
  gfx.printf("%02d:%02d:%02d\r\n", time.hour, time.min, time.sec);
  gfx.printf("%04dppm\r\n", co2);
  gfx.printf("%02.1f℃\r\n", tmp);
  gfx.printf("%0d%%", hum);
  gfx.clearClipRect();

  constexpr float x = 0.61 * M5PAPER_SIZE_LONG_SIDE;
  gfx.setCursor(0, offset_y);
  gfx.setClipRect(x, offset_y, M5PAPER_SIZE_LONG_SIDE - offset_x - x, M5PAPER_SIZE_SHORT_SIDE - offset_y);
  gfx.printf("%04d\r\n", date.year);
  gfx.printf("%02d/%02d\r\n", date.mon, date.day);
  gfx.println(weekdayToString(date.week));
  gfx.clearClipRect();

  constexpr float offset_y_info = 0.75 * M5PAPER_SIZE_SHORT_SIDE;
  gfx.setCursor(0, offset_y_info);
  gfx.setTextSize(FONT_SIZE_SMALL);
  gfx.setClipRect(x, offset_y_info, M5PAPER_SIZE_LONG_SIDE - x, gfx.height() - offset_y_info);
  gfx.print("WiFi: ");
  gfx.println(WiFiConnectedToString());

  constexpr uint32_t low = 3300;
  constexpr uint32_t high = 4350;

  auto vol = std::min(std::max(M5.getBatteryVoltage(), low), high);
  gfx.printf("BAT : %04dmv\r\n", vol);
  gfx.print("NTP : ");
  if (date_ntp.year == 1970)
  {
    gfx.print("YET"); // not initialized
  }
  else
  {
    gfx.printf("%02d/%02d %02d:%02d",
               date_ntp.mon, date_ntp.day,
               time_ntp.hour, time_ntp.min);
  }

  gfx.clearClipRect();
  gfx.setTextSize(FONT_SIZE_LARGE);
  gfx.endWrite();

  cnt++;
  if (cnt == TIME_SYNC_CYCLE)
  {
    syncNTPTimeJP();
    cnt = 0;
  }
  xSemaphoreGive(xMutex);
  delay(SLEEP_SEC * 1000);
}
