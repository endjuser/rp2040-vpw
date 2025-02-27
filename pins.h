#pragma once

#if ARDUINO_ADAFRUIT_FEATHER_RP2040

    #define PIN_DSR         24  // OUTPUT; RP2040 sets low when data available for consumption
    #define PIN_DTR         25  // INPUT_PULLUP; connected device pulls low to enable RP2040 HW serial data flow

    #define PIN_UART0_TX    28
    #define PIN_UART0_RX    29

    #define PIN_UART1_TX    8
    #define PIN_UART1_RX    9
    
    #define PIN_SD_SCK      18
    #define PIN_SD_MOSI     19
    #define PIN_SD_MISO     20
    #define PIN_SD_CS       1
    
    #define PIN_VPW_OUTPUT  12
    #define PIN_VPW_INPUT   11
    #define PIN_VPW_ENABLE  10
    #define PIN_VPW_MODE    13
    
    #define PIN_ACCESSORY   6   // OUTPUT; RP2040 sets low when ACCESSORY power should be applied

    #define PIN_VOLTAGE_DIVIDER 26
    #define VOLTAGE_CALIBRATION (14.07 / 2525.0)

#elif defined(ARDUINO_WAVESHARE_RP2040_ZERO)

    #define PIN_UART0_TX    0   // Consider using pin 12 (rear)
    #define PIN_UART0_RX    1   // Consider using pin 13 (rear)
    
    #define PIN_SD_SCK      2
    #define PIN_SD_MOSI     3
    #define PIN_SD_MISO     4
    #define PIN_SD_CS       5
    
    #define PIN_DSR         6   // OUTPUT; RP2040 sets low when data available for consumption
    #define PIN_DTR         7   // INPUT_PULLUP; connected device pulls low to enable RP2040 HW serial data flow
    
    #define PIN_UART1_TX    8
    #define PIN_UART1_RX    9
    
    #define PIN_ACCESSORY   15  // OUTPUT; RP2040 sets low when ACCESSORY power should be applied
    
    #define PIN_VPW_OUTPUT  26
    #define PIN_VPW_INPUT   27
    #define PIN_VPW_ENABLE  28
    #define PIN_VPW_MODE    29

#else

    static_assert(false, "UNSUPPORTED RP2040 BOARD; PLEASE UPDATE pins.h");
    
#endif

#ifndef PIN_NEOPIXEL
    #define PIN_NEOPIXEL    PICO_DEFAULT_WS2812_PIN /* 16 */
#endif
