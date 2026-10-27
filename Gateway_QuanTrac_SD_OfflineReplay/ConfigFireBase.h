#ifndef CONFIGFIREBASE_H
#define CONFIGFIREBASE_H

// TEST MODE ONLY: Firebase Rules must permit unauthenticated writes to this
// branch. Anyone who knows this path can read/write it. Do not use this rule
// configuration for production or sensitive data.
#define BASE_URL "https://lora-ad871-default-rtdb.asia-southeast1.firebasedatabase.app/Devices_7/GW001"

// SIM7600 HTTPACTION supports POST. Firebase REST supports method override
// through the X-HTTP-Method-Override request header, set in SIM7600E.cpp.
#define URL_SENSORS_PUT BASE_URL "/Sensors.json"
#define URL_STATUS_PUT  BASE_URL "/Status.json"
// History URLs are assembled with deterministic record keys in ChuongTrinhChinh.cpp.
#define HTTP_STATUS_OK 200
#define MAX_NETWORK_ERRORS 3
#define CMD_POST 1
#define CMD_GET 0

#endif
