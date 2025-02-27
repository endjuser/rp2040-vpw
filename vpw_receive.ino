#include <deque>
#include <cstring>
#include "vpw.h"
#include "pins.h"

byte vpwByteBuffer = 0;
byte vpwBitCount = 0;
uint vpwFrameBits = 0;
bool vpwInFrame = false;

ulong vpwBitsReceived = 0;
ulong vpwMessagesReceived = 0;

ulong VPW::getBitsReceived() { return vpwBitsReceived; }
ulong VPW::getMessagesReceived() { return vpwMessagesReceived; }

std::deque<uint> rawQueue;
std::deque<byte> encodedQueue;

#define vpw_receive_wrap_target 2
#define vpw_receive_wrap 23

//
// WITH CLOCK DIVIDER SET TO CPU HZ / 5000000, EACH INSTRUCTION TAKES 200ns
//
static const uint16_t vpw_receive_program_instructions[] = {
    0xa04b, //  0: mov    y, !null                   
    0x2020, //  1: wait   0 pin, 0                   
            //     .wrap_target
    0xa0c3, //  2: mov    isr, null                  ; count LOW duration
    0x4001, //  3: in     pins, 1                    
    0xa026, //  4: mov    x, isr                     
    0x0048, //  5: jmp    x--, 8                     
    0x0082, //  6: jmp    y--, 2                     
    0x20a0, //  7: wait   1 pin, 0                   
    0xa0ca, //  8: mov    isr, !y                    ; got HIGH edge
    0xa04b, //  9: mov    y, !null                   
    0x4061, // 10: in     null, 1                    ; shift out MSB of counter, make LSB of result 0 for pin low
    0x8000, // 11: push   noblock                    
    0xc001, // 12: irq    nowait 1                   
    0xa0c3, // 13: mov    isr, null                  ; count HIGH duration
    0x4001, // 14: in     pins, 1                    
    0xa026, // 15: mov    x, isr                     
    0x0033, // 16: jmp    !x, 19                     ; sure would be nice if we had a JMP !PINS instruction...
    0x008d, // 17: jmp    y--, 13                    
    0x2020, // 18: wait   0 pin, 0                   
    0xa0ca, // 19: mov    isr, !y                    ; got LOW edge
    0xa04b, // 20: mov    y, !null                   
    0x4041, // 21: in     y, 1                       ; shift out MSB of counter, make LSB of result 1 for pin high
    0x8000, // 22: push   noblock                    
    0xc001, // 23: irq    nowait 1                   
            //     .wrap
};

static const struct pio_program vpw_receive_program = {
    .instructions = vpw_receive_program_instructions,
    .length = 24,
    .origin = -1,
};

static inline pio_sm_config vpw_receive_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + vpw_receive_wrap_target, offset + vpw_receive_wrap);
    return c;
}

uint VPW::beginReceive() {
    PIO pio = _pioReceive;

    // sanity check and initialize state machine
    if (!pio_can_add_program(pio, &vpw_receive_program))
        return -1;
    uint sm = pio_claim_unused_sm(pio, true);
    if (sm == -1)
        return -1;

    // load program
    uint offset = pio_add_program(pio, &vpw_receive_program);
    pio_sm_config c = vpw_receive_program_get_default_config(offset);

    // set PIO clock
    const float pioFreq = 5000000;
    float cpuHz = clock_get_hz(clk_sys);
    float clkDiv = cpuHz / pioFreq;
    sm_config_set_clkdiv(&c, clkDiv);

    // join FIFOs for larger buffer
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    // configure ISR shift left without auto-pull
    sm_config_set_in_shift(&c, false, false, 32);

    // configure instruction-specific pins
    sm_config_set_jmp_pin(&c, PIN_VPW_INPUT);
    sm_config_set_in_pins(&c, PIN_VPW_INPUT);

    // set pin directions
    pio_sm_set_consecutive_pindirs(pio, sm, PIN_VPW_INPUT, 1, false);

    // connect GPIOs to this PIO block
    pio_gpio_init(pio, PIN_VPW_INPUT);

    // prepare interrupt
    pio_set_irq1_source_enabled(pio, pis_interrupt1, true);
    uint irq = (pio == pio0) ? PIO0_IRQ_1 : PIO1_IRQ_1;
    pio_interrupt_clear(pio, 1);

    // start state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    // start irq
    irq_set_exclusive_handler(irq, receiveHandler);
    irq_set_enabled(irq, true);

    return sm;
}

void VPW::reset() {
    rawQueue.clear();
    encodedQueue.clear();
}

inline void VPW::receiveHandler() {
  static uint value;
  do {
    value = pio_sm_get_blocking(_pioReceive, _smReceive);
    rawQueue.push_back(value);
    if (vpwBitsReceived++ == 0)
        vpwBitsReceived++;
  } while (!pio_sm_is_rx_fifo_empty(_pioReceive, _smReceive));
  pio_interrupt_clear(_pioReceive, 1);  
}

bool VPW::receiveLoop() {
  static bool proceed;
  static bool active;
  static ulong diff;
  static ulong lastActivity = 0;
  static bool activityThisLoop;
  static bool receive4x = false;

  activityThisLoop = false;
  
  while (!VPW::idle()) {
    lastActivity = micros();
    activityThisLoop = true;
    
    diff = rawQueue.front();
    rawQueue.pop_front();
    active = (diff & 1);
    diff >>= 1;

    //push2encoded(W_DEBUG);
    //push2((byte)min(diff, 255));
    
    if (!vpwInFrame && !vpwFrameBits) {
      if (diff > 163 && diff <= 239) {
        if (receive4x)
          push2encoded(W_MODE_1X);
        receive4x = false; // 1X SOF
      } else if (diff > 163 / 4 /*40.75*/ && diff <= 239 / 4 /*59.75*/) {
        if (!receive4x)
          push2encoded(W_MODE_4X);
        receive4x = true;  // 4X SOF
      }
    }
    if (receive4x) {
      diff *= 4;
      if (diff > 240 && diff <= 1000 && active) {
        diff /= 4;
        receive4x = false;
        push2encoded(W_MODE_1X);
      }    
    }
    VPW::SEND_4X = receive4x;
    
    if (diff > 240) {
      if (diff <= (receive4x ? 4000 : 1000) && active) {
        debug("BRK");
        vpw_break();
      } else if (!active) {
        debug("EOF");
        vpw_eof();
        if (_ledHandler)
          _ledHandler(false, LED_HANDLER_EOF);
      } else {
        debug("HIGH");
        push2encoded(W_HIGH);
        // BUS SHORTED HIGH?
      }
    } else if (diff > 160) {
      if (active) {
        debug("SOF");
        vpw_sof();
        if (_ledHandler)
          _ledHandler(true, LED_HANDLER_SOF);
      } else {
        debug("EOD");
        vpw_eod();
      }
    } else if (diff > 96) {
      //debug(!active ? '1' : '0');
      vpw_bit(!active);
    } else if (diff > 32) {
      //debug(active ? '1' : '0');
      vpw_bit(active);
    } else {
      debug("RUNT");
      push2encoded(W_RUNT);
    }
  }
  
  if (vpwInFrame && VPW::idle()) {
    diff = (micros() - lastActivity) & 32767;
    if (diff > 240 && active) {
      vpw_eof();
      push2encoded(W_EOT);
      if (_ledHandler)
        _ledHandler(false, LED_HANDLER_EOT);
    }
  }

  if (_ledHandler)
    _ledHandler(activityThisLoop, LED_HANDLER_RECEIVE);
    
  return activityThisLoop;
}

void debug(char text) {
  //
  return;
  //
  push2encoded(W_DEBUG_STRING);
  push2(1);
  push2(text);
}

void debug(const char* text) {
  //
  return;
  //
  push2encoded(W_DEBUG_STRING);
  size_t length = std::strlen(text);
  push2(length);
  for (int i = 0; i < length; i++)
    push2(text[i]);
}

bool VPW::idle() {
  return rawQueue.size() == 0;
}

bool VPW::available() {
  return encodedQueue.size() != 0;
}

byte VPW::pop() {
  byte b = encodedQueue.front();
  encodedQueue.pop_front();
  return b;
}

void push2byte(byte b) {
  if (b == W_WILDCARD)
    push2encoded(b);
  else
    push2(b);  
}

void push2(byte b) {
    encodedQueue.push_back(b);
}

void push2encoded(byte b) {
  push2(W_WILDCARD);
  push2(b);  
}

void push2timestamp() {
  static struct timeval tv;
  if (VPW::USE_TIMESTAMP) {
    // token
    push2encoded(W_TIMESTAMP);
    // get time
    tv = VPW::getTimestamp();
    // push time
    static int tvLength = sizeof(tv);
    static byte *p;
    p = (byte*) &tv;
    for (int i = 0; i < tvLength; i++) {
      push2(*p++);
    }
  }
}

struct timeval VPW::getTimestamp() {
    static struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv;    
}

struct timeval VPW::popTimestamp() {
  static struct timeval tv;
  static int tvLength = sizeof(tv);
  static byte *p;
  p = (byte*) &tv;
  for (int i = 0; i < tvLength; i++) {
    *(p + i) = VPW::pop();
  }
  return tv;
}

void vpw_reset() {
  vpwInFrame = false;
  vpwBitCount = 0;
  vpwByteBuffer = 0;
  vpwFrameBits = 0;
}

void vpw_sof() {
  if (vpwInFrame || vpwFrameBits > 0)
    push2encoded(W_ERROR_UNEXPECTED_SOF);
  push2timestamp();
  push2encoded(W_SOF);
  vpwInFrame = true;  
}

void vpw_eod() {
  push2encoded(W_EOD);
}

void vpw_eof() {
  if (vpwBitCount > 0)
    push2encoded(W_ERROR_UNEXPECTED_EOF);
  if (vpwInFrame) {
    push2encoded(W_EOF);
    if (vpwMessagesReceived++ == 0)
        vpwMessagesReceived++;
  }
  vpw_reset();
}

void vpw_break() {
  push2encoded(W_BRK);
  vpw_reset();
}

void vpw_bit(bool b) {
  vpwFrameBits++;
  vpwByteBuffer = (vpwByteBuffer << 1) | (b ? 1 : 0);
  if (++vpwBitCount == 8) {
    vpwBitCount = 0;
    push2byte(vpwByteBuffer);
  }
}
