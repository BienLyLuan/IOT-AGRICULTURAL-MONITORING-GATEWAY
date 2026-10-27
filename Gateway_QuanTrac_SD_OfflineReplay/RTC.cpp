#include "RTC.h"
#include "Config.h"
#include <Arduino.h>
#include <Wire.h>
#include <stdlib.h>

namespace {
bool validDate(int year, int month, int day) {
  
  if (year < 2024 || year > 2099 || month < 1 || month > 12 || day < 1) return false;
  const int daysPerMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int maxDay = daysPerMonth[month - 1];
  const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  if (month == 2 && leap) maxDay = 29;
  return day <= maxDay;
}
bool validNetworkDate(int year2, int month, int day) {
  return year2 >= 24 && year2 <= 99 && validDate(2000 + year2, month, day);
}
long daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const long era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = (unsigned)(year - era * 400);
  const unsigned adjustedMonth = (unsigned)((int)month + (month > 2 ? -3 : 9));
  const unsigned doy = (153 * adjustedMonth + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + (long)doe;
}
bool validRtcDateTime(const ThoiGian &t) {
  return validDate(t.Nam, t.Thang, t.Ngay) && t.Gio >= 0 && t.Gio <= 23 &&
         t.Phut >= 0 && t.Phut <= 59 && t.Giay >= 0 && t.Giay <= 59;
}
int64_t epochLikeSeconds(const ThoiGian &t) {
  return (int64_t)daysFromCivil(t.Nam, (unsigned)t.Thang, (unsigned)t.Ngay) * 86400LL +
         (int64_t)t.Gio * 3600LL + (int64_t)t.Phut * 60LL + t.Giay;
}
}

// This project is 4G-only. Keep the old method names for source compatibility,
// but do not pull Wi-Fi/NTP/HTTP libraries into the gateway firmware.
bool RTC::LayRTCTuServerNTP(void) { return false; }
bool RTC::LayRTCTuServerIoTVision(void) { return false; }

void RTC::beginRTC() {
  // Pins verified against Gateway.kicad_pcb: SDA=GPIO8, SCL=GPIO9.
  Wire.begin(MY_SDA, MY_SCL);
  Wire.setClock(100000);
  ds.setClockMode(false);
}

void RTC::LayRTCTuDS3231(void) {
  bool century = false;
  bool h12Flag = false;
  bool pmFlag = false;
  dt.Thu = ds.getDoW();
  dt.Ngay = ds.getDate();
  dt.Thang = ds.getMonth(century);
  dt.Nam = ds.getYear() + 2000;
  dt.Gio = ds.getHour(h12Flag, pmFlag);
  dt.Phut = ds.getMinute();
  dt.Giay = ds.getSecond();
}

void RTC::CaiDatRTCChoDS3231(void) {
  // dt.Nam is a full year in this project; DS3231 library expects year 00..99.
  ds.setClockMode(false);
  ds.setYear(dt.Nam % 100);
  ds.setMonth(dt.Thang);
  ds.setDate(dt.Ngay);
  ds.setDoW(dt.Thu);
  ds.setHour(dt.Gio);
  ds.setMinute(dt.Phut);
  ds.setSecond(dt.Giay);
}

void RTC::SoSanhSaiLechRTCDeCapNhatDS3231(void) {
  // No Wi-Fi time source exists on this 4G-only gateway.
}

void RTC::SoSanhSaiLechRTCDeCapNhatDS3231Tu4G(int thu, int ngay, int thang, int nam,
                                              int gio, int phut, int giay) {
  // SIM7600 AT+CCLK? returns a two-digit year (e.g. 26 for 2026).
  if (!validNetworkDate(nam, thang, ngay) || thu < 1 || thu > 7 ||
      gio < 0 || gio > 23 || phut < 0 || phut > 59 || giay < 0 || giay > 59) {
    MAIN_PRINTLN("[RTC] Tu choi thoi gian 4G khong hop le.");
    return;
  }

  LayRTCTuDS3231();
  const int fullYear = 2000 + nam;
  ThoiGian networkTime{thu, ngay, thang, fullYear, gio, phut, giay};
  const bool rtcValid = validRtcDateTime(dt);
  bool shouldSet = !rtcValid;
  if (rtcValid) {
    int64_t difference = llabs(epochLikeSeconds(networkTime) - epochLikeSeconds(dt));
    // CCLK timezone/configuration may be wrong. Never overwrite a valid RTC
    // when the discrepancy is implausibly large; log it for hardware diagnosis.
    if (difference <= 12L * 3600L && difference > 10L) shouldSet = true;
    else if (difference > 12L * 3600L) {
      MAIN_PRINTLN("[RTC] Khong ghi de RTC hop le: gio SIM lech >12h; kiem tra timezone/CTZU.");
      return;
    }
  }
  if (shouldSet) {
    dt = networkTime;
    CaiDatRTCChoDS3231();
    MAIN_PRINTLN(rtcValid ? "[RTC] Da hieu chinh DS3231 tu SIM7600 (chenh lech hop ly)."
                            : "[RTC] DS3231 khong hop le; da dong bo tu SIM7600.");
  }
}

String RTC::ChuanHoaChuoiRTCDeGuiVeServer(void) {
  char value[24];
  snprintf(value, sizeof(value), "%02d:%02d;%02d/%02d/%02d",
           dt.Gio, dt.Phut, dt.Ngay, dt.Thang, dt.Nam % 100);
  return String(value);
}

String RTC::ChuanHoaHienThiHMI(void) {
  const char *day = "---";
  switch (dt.Thu) {
    case 1: day = " CN"; break;
    case 2: day = "Th2"; break;
    case 3: day = "Th3"; break;
    case 4: day = "Th4"; break;
    case 5: day = "Th5"; break;
    case 6: day = "Th6"; break;
    case 7: day = "Th7"; break;
  }
  char value[32];
  snprintf(value, sizeof(value), "%s %02d/%02d %02d:%02d:%02d",
           day, dt.Ngay, dt.Thang, dt.Gio, dt.Phut, dt.Giay);
  return String(value);
}
