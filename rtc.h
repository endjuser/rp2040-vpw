#pragma once

#include "RTClib.h"
RTC_PCF8523 rtc;

bool setTimeFromRTC() {
    DateTime dt = rtc.now();
    if (!dt.isValid())
        return false;
    uint unix = dt.unixtime();
    struct timeval tv = { .tv_sec = unix, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    return true;
}
