#pragma once
#include "pixel.h"
#include "vpw_led.h"

//
// BLINKENLIGHTS
//

#ifndef BLIP_DELAY
#define BLIP_DELAY 5000
#endif

static uint fadeSend = 0;
static uint fadeReceive = 0;
static uint fadeCongestion = 0;
static uint fadeSave = 0;
static short ledCongestion = 0;
static short ledSend = 0;
static short ledReceive = 0;
static short ledSave = 0;

static bool ledError = 0;

void fadePixels(bool decrement);

void ledHandler(bool led, ledHandlerState state) {
  static uint now;
  now = millis();
  
  if (state == LED_HANDLER_RECEIVE)
    return;

  if (state & LED_HANDLER_RECEIVE) {
    if (state == LED_HANDLER_SOF) {
      ledReceive = MAX_INTENSITY;
    } else if (state == LED_HANDLER_EOF) {
      ledReceive = 0;      
    } else if (state == LED_HANDLER_EOT) {
      ledReceive = 0;      
      fadeReceive = now;
    }    
  } else if (state & LED_HANDLER_SEND) {
      ledSend = (led ? MAX_INTENSITY : 0);
      if (state == LED_HANDLER_CONGESTION) {
        ledCongestion = MAX_INTENSITY;
        fadeCongestion = now;
      }
      if (led)
        fadeSend = now;
  }
  fadePixels(false);  
}

void fadePixels(bool decrement) {
  static byte targetR = 0;
  static byte targetG = 0;
  static byte targetB = 0;
  static uint lastBlip = millis();
  static bool blip = false;
  static uint now;
  now = millis();

  if (!blip && now - lastBlip >= (BLIP_DELAY - 10)) {
    blip = true;
  } else {
    if (blip && now - lastBlip >= BLIP_DELAY) {
      blip = false; 
      lastBlip = now;
    }
  }
  
  if (decrement) {
    if (now - fadeReceive > 25) {
      ledReceive = max(ledReceive - 1, 0);
      fadeReceive = now;
    }
    if (now - fadeSend > 25) {
      ledSend = max(ledSend - 1, 0);
      fadeSend = now;
    }
    if (now - fadeCongestion > 25) {
      ledCongestion = max(ledCongestion - 1, 0);
      fadeCongestion = now;
    }
    if (now - fadeSave > 25) {
      ledSave = max(ledSave - 1, 0);
      fadeSave = now;
    }
  }

  targetR = ledError ? MAX_INTENSITY : 0;
  if (ledSave > 0)
    targetR = ledSave;
    
  if (blip)
    targetR = max(ledSave, 2);
  
  if (ledCongestion > 0) {
    targetR = ledSend;
    targetB = 0;      
  } else {
    targetB = ledSend;
  }

  targetG = ledReceive;

  targetR *= 2; // harder to see red

  if (PIXEL_R != targetR || PIXEL_G != targetG || PIXEL_B != targetB)
    setPixel(targetR, targetG, targetB);
}
