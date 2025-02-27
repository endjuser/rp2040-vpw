#pragma once

enum ledHandlerState : byte {
  LED_HANDLER_RECEIVE = 0x10,  
  LED_HANDLER_SOF = 0x11,
  LED_HANDLER_EOF = 0x12,
  LED_HANDLER_EOT = 0x13,
  LED_HANDLER_SEND = 0x20,
  LED_HANDLER_CONGESTION = 0x21
};

typedef void (*led_handler_t)(bool led, ledHandlerState state);
