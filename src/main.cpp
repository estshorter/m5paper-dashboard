#include <M5EPD.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.hpp>
#include "SHT3X.h"
#include "wifiinfo.h"

#define LGFX_M5PAPER
#include <LovyanGFX.hpp>
#include "myFont.h"

constexpr uint_fast16_t SLEEP_SEC = 5;
constexpr uint_fast32_t TIME_SYNC_CYCLE = 7 * 3600 * 24 / SLEEP_SEC;
constexpr auto NTP_SERVER1 = "ntp.nict.jp";
constexpr auto NTP_SERVER2 = "time.cloudflare.com";
constexpr auto NTP_SERVER3 = "time.google.com";
constexpr auto TIME_ZONE = "JST-9";
constexpr auto CO2_DATA_URL = "http://192.168.10.105/api/data";
constexpr uint_fast16_t WIFI_CONNECT_RETRY_MAX = 60; // 10 = 5s
constexpr float FONT_SIZE_LARGE = 3.0;
constexpr float FONT_SIZE_SMALL = 1.0;
constexpr uint_fast16_t WAIT_ON_NOTIFY = 2000;
constexpr uint_fast16_t M5PAPER_SIZE_LONG_SIDE = 960;
constexpr uint_fast16_t M5PAPER_SIZE_SHORT_SIDE = 540;

rtc_time_t time_ntp;
rtc_date_t date_ntp{4, 1, 1, 1970};

TwoWire *wire_portA = &Wire1;
SemaphoreHandle_t xMutex = nullptr;
SHT3X::SHT3X sht30(wire_portA);
LGFX gfx;

String WiFiConnectedToString()
{
  return WiFi.isConnected() ? String("OK") : String("NG");
}

void prettyEpdRefresh(void)
{
  gfx.setEpdMode(epd_mode_t::epd_quality);
  gfx.fillScreen(TFT_WHITE);
  gfx.setEpdMode(epd_mode_t::epd_fast);
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

template <class...>
struct conjunction : std::true_type
{
};
template <class B1_>
struct conjunction<B1_> : B1_
{
};
template <class B1_, class... Bn>
struct conjunction<B1_, Bn...>
    : std::conditional<bool(B1_::value), conjunction<Bn...>, B1_>::type
{
};

template <typename... Ts>
using AreAllPtrToConstChar = typename conjunction<std::is_same<Ts, std::add_pointer<const char>::type>...>::type;

template <class... NtpServers, typename std::enable_if<AreAllPtrToConstChar<NtpServers...>::value, std::nullptr_t>::type = nullptr>
int syncNTPTime(const char *tz, NtpServers... ntps)
{
  static_assert(sizeof...(ntps) <= 3 && sizeof...(ntps) >= 1, "NTP servers must be one at least and three at most");
  if (!WiFi.isConnected())
  {
    return 1;
  }

  configTzTime(tz, std::forward<NtpServers>(ntps)...);

  struct tm datetime;
  if (!getLocalTime(&datetime))
    return 1;

  rtc_time_t time{
      static_cast<int8_t>(datetime.tm_hour),
      static_cast<int8_t>(datetime.tm_min),
      static_cast<int8_t>(datetime.tm_sec)};
  rtc_date_t date{
      static_cast<int8_t>(datetime.tm_wday),
      static_cast<int8_t>(datetime.tm_mon + 1),
      static_cast<int8_t>(datetime.tm_mday),
      static_cast<int16_t>(datetime.tm_year + 1900)};

  M5.RTC.setTime(&time);
  M5.RTC.setDate(&date);
  date_ntp = date;
  time_ntp = time;
  return 0;
}

uint_fast16_t getCo2Data(void)
{
  using namespace ArduinoJson;
  if (!WiFi.isConnected())
    return 0;

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, CO2_DATA_URL))
  {
    Serial.printf("[HTTP] Failed to parse url\n");
    return 0;
  }

  // start connection and send HTTP header
  int httpCode = http.GET();
  // httpCode will be negative on error
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n",
                  http.errorToString(httpCode).c_str());
    http.end();
    return 0;
  }
  // String payload = http.getString();

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

  uint_fast16_t co2 = doc["co2"]["value"];
  return co2;
}

void handleBtnPPress(void)
{
  xSemaphoreTake(xMutex, portMAX_DELAY);
  prettyEpdRefresh();
  gfx.setTextSize(FONT_SIZE_SMALL);

  gfx.startWrite();
  gfx.setCursor(0, 0);
  if (!syncNTPTime(TIME_ZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3))
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
  M5.RTC.getDate(&date);
  M5.RTC.getTime(&time);
  gfx.print("RTC         :");
  gfx.printf("%04d/%02d/%02d ", date.year, date.mon, date.day);
  gfx.printf("%02d:%02d:%02d", time.hour, time.min, time.sec);
  gfx.endWrite();

  delay(1000);

  gfx.setTextSize(FONT_SIZE_LARGE);
  xSemaphoreGive(xMutex);
}

void handleBtnRPress(void)
{
  xSemaphoreTake(xMutex, portMAX_DELAY);
  prettyEpdRefresh();
  xSemaphoreGive(xMutex);
}

void handleBtnLLongPress(void)
{
  xSemaphoreTake(xMutex, portMAX_DELAY);
  prettyEpdRefresh();
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
    if (M5.BtnP.wasPressed())
    {
      handleBtnPPress();
    }
    else if (M5.BtnR.wasPressed())
    {
      handleBtnRPress();
    }
    else if (M5.BtnL.pressedFor(2000))
    {
      handleBtnLLongPress();
    }
  }
}

void setup(void)
{
  M5.begin(false, false, true, true);
  WiFi.begin(WiFiInfo::SSID, WiFiInfo::PASS);

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
    delay(WAIT_ON_NOTIFY);
  }

  M5.RTC.begin();

  // env2 unit
  if (wire_portA->begin(25, 32))
  {
    sht30.begin();
  }
  else
  {
    gfx.println("Failed to initialize external I2C");
  }

  xMutex = xSemaphoreCreateMutex();
  if (xMutex != nullptr)
  {
    xSemaphoreGive(xMutex);
    xTaskCreatePinnedToCore(handleButton, "handleButton", 4096, nullptr, 1, nullptr, 1);
    gfx.println("Succeeded to create a task for buttons");
  }
  else
  {
    gfx.println("Failed to create a task for buttons");
  }
  gfx.println("Init done");
  delay(1000);
  gfx.setTextSize(FONT_SIZE_LARGE);
  prettyEpdRefresh();
  gfx.setCursor(0, 0);
}

void loop(void)
{
  static uint32_t cnt = 0;

  xSemaphoreTake(xMutex, portMAX_DELAY);
  float tmp = 0.0;
  uint_fast8_t hum = 0;

  if (!sht30.read())
  {
    tmp = sht30.getTemperature();
    hum = sht30.getHumidity();
  }
  auto co2 = getCo2Data();

  rtc_date_t date;
  rtc_time_t time;

  M5.RTC.getDate(&date);
  M5.RTC.getTime(&time);

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
    syncNTPTime(TIME_ZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
    cnt = 0;
  }
  xSemaphoreGive(xMutex);
  delay(SLEEP_SEC * 1000);
}
