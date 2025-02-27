#pragma once

#define MAX_INTENSITY 8
#define PHOSPHORESCENCE 250

byte PIXEL_R = 0;
byte PIXEL_G = 0;
byte PIXEL_B = 0;

void setPixel();
void setPixel(byte r, byte g, byte b);
void setPixelR(byte r);
void setPixelG(byte g);
void setPixelB(byte b);
