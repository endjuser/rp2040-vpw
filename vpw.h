#pragma once

#include "hardware/pio.h"
#include "pins.h"
#include "vpw_led.h"
#include "j1850.h"

enum wildcardEncoding : byte {
  W_SOF = 0x01,
  W_EOD = 0x02,
  W_EOF = 0x03,
  W_BRK = 0x04,
  W_EOT = 0x05, // assumed end-of-transmission

  W_MODE_1X = 0x10,
  W_MODE_4X = 0x11,
  
  W_ERROR_UNEXPECTED_EOF = 0x80,
  W_ERROR_UNEXPECTED_SOF = 0x81,

  W_HIGH = 0x90,
  W_RUNT = 0x91,

  W_WILDCARD = 0xEE, // least-used byte from logging in a real vehicle

  W_DEBUG_STRING = 0xFD,
  W_DEBUG = 0xFE,
  W_TIMESTAMP = 0xFF
};

enum sendVPW_status_t : byte {
  SEND_VPW_STATUS_CONGESTION = 0,
  SEND_VPW_STATUS_OK = 1,
  SEND_VPW_STATUS_INVALID_CRC = 2,
  SEND_VPW_STATUS_TOO_SHORT = 3,
  SEND_VPW_STATUS_TOO_LONG = 4,
  SEND_VPW_STATUS_NO_ECHO = 5,
  SEND_VPW_STATUS_STILL_SENDING = 6
};

class VPW {
  private:
    static inline VPW *_instance = nullptr;
    static inline bool ok = false;
    
    static inline PIO _pioSend = pio1;
    static inline uint _smSend = -1;
    static uint beginSend();

    static inline PIO _pioReceive = pio0;
    static inline uint _smReceive = -1;
    static inline void receiveHandler();
    static uint beginReceive();

    static inline void (*_ledHandler)(bool led, ledHandlerState state) = (NULL);
    
  public:
    bool begin();
    bool ready();
    
    static inline bool USE_TIMESTAMP = true;
    static inline bool SEND_4X = false;

    static void reset();
    
    static bool sendRaw(const byte* data, uint16_t bytes, bool send4X = VPW::SEND_4X);
    static sendVPW_status_t send(const J1850& message, bool allowInvalid = false, bool send4X = VPW::SEND_4X);
    
    static bool receiveLoop();
    static byte pop();
    static struct timeval popTimestamp();

    static bool available();
    static bool idle();
        
    static void setReceiveLedHandler(led_handler_t handler);

    static ulong getBitsReceived();
    static ulong getMessagesReceived();

    static struct timeval getTimestamp();
};

bool VPW::begin() {
  if (_instance != nullptr)
    return false;

  pinMode(PIN_VPW_ENABLE, OUTPUT);
  pinMode(PIN_VPW_MODE, OUTPUT);
  digitalWriteFast(PIN_VPW_ENABLE, LOW);
  digitalWriteFast(PIN_VPW_MODE, LOW);

  _instance = this;  
  _pioSend = pio1;
  _pioReceive = pio0;
  
  _smSend = beginSend();
  _smReceive = beginReceive();

  ok = (_smSend != -1 && _smReceive != -1);
  return ok;
}

bool VPW::ready() {
  return VPW::ok;
}

void VPW::setReceiveLedHandler(led_handler_t handler) {
  _ledHandler = handler;
}

VPW vpw;
