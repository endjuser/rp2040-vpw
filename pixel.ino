#include <NeoPixelConnect.h>
#include "pixel.h"
#include "pins.h"

NeoPixelConnect neoPixel(PIN_NEOPIXEL, 1, pio0, 1);

void PIXEL_SHOW() {
#ifdef ARDUINO_ADAFRUIT_FEATHER_RP2040
  neoPixel.neoPixelSetValue(0, PIXEL_R, PIXEL_G, PIXEL_B, true);
#else
  neoPixel.neoPixelSetValue(0, PIXEL_G, PIXEL_R, PIXEL_B, true);
#endif
}

void setPixel() {
  PIXEL_SHOW();
}

void setPixel(byte r, byte g, byte b) {
  PIXEL_R = r;
  PIXEL_G = g;
  PIXEL_B = b;
  PIXEL_SHOW();
}

void setPixelR(byte r) {
  PIXEL_R = r;
  PIXEL_SHOW();
}

void setPixelG(byte g) {
  PIXEL_G = g;
  PIXEL_SHOW();
}

void setPixelB(byte b) {
  PIXEL_B = b;
  PIXEL_SHOW();
}
