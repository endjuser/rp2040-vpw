#pragma once

#include "pico/unique_id.h"
#include "hardware/adc.h"
#include <regex>

const std::regex patternNumeric("^[0-9]+$");
const std::regex patternDecimal("^[0-9]+(\\.[0-9]*)?$");
const std::regex patternHex("^[0-9A-Fa-f]+$");

struct Util {
    
    bool isNumeric(const std::string_view& text) {
        return std::regex_match(text.begin(), text.end(), patternNumeric);
    }

    bool isDecimal(const std::string_view& text) {
        return std::regex_match(text.begin(), text.end(), patternDecimal);
    }

    bool isHex(const std::string_view& text) {
        return std::regex_match(text.begin(), text.end(), patternHex);
    }

    //
    // hex string from number
    //
    std::string hex(uint x, byte digits) {
      char hex[9];
      sprintf(hex, "%0*X", min(digits, 8), x);
      return std::string(hex);  
    }
    std::string hex(uint x) { return hex(x, 8); }
    std::string hex(byte x) { return hex(x, 2); }
    
    //
    // binary string from number
    //
    std::string bin(uint x, byte bits) {
      char bin[bits + 1];
      bin[bits] = 0;
      for (int i = 0; i < bits; i++) {
        bin[bits - i - 1] = (x & 1) ? '1' : '0';
        x >>= 1;
      }
      return std::string(bin);  
    }
    std::string bin(uint x) { return bin(x, 32); }
    std::string bin(byte x) { return bin(x, 8); }
    
    //
    // string from number
    //
    std::string dec(byte x, byte digits) {
      char dec[4];
      std::snprintf(dec, sizeof(dec), "%0*d", min(digits, 3), x);
      return std::string(dec);
    }
    std::string dec(uint x, byte digits) {
      char dec[11];
      std::snprintf(dec, sizeof(dec), "%0*d", min(digits, 10), x);
      return std::string(dec);
    }
    std::string dec(ulong x, byte digits) {
      char dec[32];
      snprintf(dec, sizeof(dec), "%0*d", min(digits, 31), x);
      return std::string(dec);
    }
    std::string dec(float x, byte leading, byte decimals) {
      char dec[32];
      std::snprintf(dec, sizeof(dec), "%0*.*f", min(10, leading + decimals + 1), min(decimals, 10), x);
      return std::string(dec);
    }    
    
    //
    // get available memory on heap
    //
    uint getFreeMemory(void) {
        struct mallinfo m = mallinfo();
        extern char __StackLimit, __bss_end__;   
        return (&__StackLimit - &__bss_end__) - m.uordblks;
    }
    
    //
    // get unique Raspberry Pi Pico ID
    //
    std::string getUniqueBoardId(bool includeDashes = true) {
      size_t each = includeDashes ? 3 : 2;
      char ret[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 3 + 1];
      pico_unique_board_id_t board_id;
      pico_get_unique_board_id(&board_id);
      for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        sprintf(&ret[each * i], "%02X", board_id.id[i]);
        if (includeDashes && i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 1)
          ret[each * i + 2] = '-';
      }
      ret[sizeof(ret) - 1] = 0;
      return std::string(ret);
    }
    
    //
    // get CPU temperature
    //
    float getCpuTemperature() {
      adc_init();
      adc_set_temp_sensor_enabled(true);
      // Select ADC input 4 for internal temperature sensor
      adc_select_input(4);
      uint16_t adc = adc_read();
      float ADC_Voltage = float(adc) * (3.3f / (1 << 12));  // 12-bit conversion, assume max value == ADC_VREF == 3.3 V
      float ret = 27 - (ADC_Voltage - 0.706) / 0.001721;  // formula found on page 71 (section 4.1.1. hardware_adc) of the Raspberry Pi Pico C/C++ SDK documentation
      adc_set_temp_sensor_enabled(false);
      return ret; 
    }

    //
    // get current time string
    //
    std::string timevalToString(const struct timeval& tv) {
        char buffer[32];
        time_t rawtime = tv.tv_sec;
        //struct tm* timeinfo = gmtime(&rawtime);
        struct tm timeinfo;
        localtime_r(&rawtime, &timeinfo);        
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return std::string(buffer);
    }

    void(*reset) (void) = 0; // Arduino-style reset
    
    void reboot() {
        (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C))) = 0x5FA0004; // Modify AIRCR register
    }
};

Util Util;

class recursive_lock_guard {
private:
    recursive_mutex_t* mutex;
public:
    explicit recursive_lock_guard(recursive_mutex_t& m) : mutex(&m) {
        recursive_mutex_enter_blocking(mutex);
    }
    ~recursive_lock_guard() {
        recursive_mutex_exit(mutex);
    }
    recursive_lock_guard(const recursive_lock_guard&) = delete;
    recursive_lock_guard& operator=(const recursive_lock_guard&) = delete;
};
