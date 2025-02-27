#include "pins.h"
#include "vpw.h"

#define vpw_send_wrap_target 0
#define vpw_send_wrap 30

//
// WITH CLOCK DIVIDER SET TO CPU HZ /  250000, EACH INSTRUCTION TAKES 4us (1X MODE)
// WITH CLOCK DIVIDER SET TO CPU HZ / 1000000, EACH INSTRUCTION TAKES 1us (4X MODE)
//

static const uint16_t vpw_send_program_instructions[] = {
    //      .wrap_target
    //      start:
    0xe000, //  0: set    pins, 0       ; passive
    0x6048, //  1: out    y, 8          ; read # of bit pairs in message (1 byte = 4)
    0x0060, //  2: jmp    !y, 0         ; if # bytes is zero, ignore
    //      multiply_by_4:
    0x4048, //  3: in     y, 8          ; if successful, isr return value will be > 0
    0x4062, //  4: in     null, 2
    0xa046, //  5: mov    y, isr
    0x0087, //  6: jmp    y--, 7        ; subtract 1 from byte count since the loop runs before decrementing
    //      subtracted:
    0xe036, //  7: set    x, 22         ; wait for 280us of passive bus
    //      wait_280:
    0x01da, //  8: jmp    pin, 26  [1]  ; check for congestion
    0x0048, //  9: jmp    x--, 8
    0x00da, // 10: jmp    pin, 26       ; double-check for congestion
    //      sof:
    0xe001, // 11: set    pins, 1       ; active for 200us (SOF)
    0xe136, // 12: set    x, 22    [1]
    //      wait_200:
    0x014d, // 13: jmp    x--, 13  [1]
    //      loop_data:
    0x6021, // 14: out    x, 1          ; read next bit (passive)
    0xec00, // 15: set    pins, 0  [12] ; passive for 64us
    0x0032, // 16: jmp    !x, 18
    0xaf42, // 17: nop             [15] ; wait another ~64us if bit is 1
    //      passive_short:
    0x00db, // 18: jmp    pin, 27       ; check for congestion
    0x6021, // 19: out    x, 1          ; read next bit (active)
    0xec01, // 20: set    pins, 1  [12] ; active for 64us
    0x0057, // 21: jmp    x--, 23
    0xaf42, // 22: nop             [15] ; wait another 64us if bit is 0
    //      active_short:
    0x008e, // 23: jmp    y--, 14
    //      complete:
    0xe000, // 24: set    pins, 0       ; passive
    0x001e, // 25: jmp    30
    //      congestion:
    0x6021, // 26: out    x, 1          ; discard unused passive bit
    //      congestion_in_loop:
    0x6021, // 27: out    x, 1          ; discard unused active bit
    0x009a, // 28: jmp    y--, 26
    0xa0c3, // 29: mov    isr, null     ; clear isr so return value will == 0
    //      finish:
    0x8020, // 30: push   block         ; send return value in isr to cpu
    //      .wrap
};

static const struct pio_program vpw_send_program = {
    .instructions = vpw_send_program_instructions,
    .length = 31,
    .origin = -1,
};

static inline pio_sm_config vpw_send_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + vpw_send_wrap_target, offset + vpw_send_wrap);
    return c;
}

static inline float getPioClockDivider(bool send4X = VPW::SEND_4X) {
    static float cpuHz;
    cpuHz = clock_get_hz(clk_sys);
    return cpuHz / (250000 * (send4X ? 4 : 1));
}

uint VPW::beginSend() {
    PIO pio = _pioSend;
    
    // initialize state machine
    if (!pio_can_add_program(pio, &vpw_send_program))
        return -1;
    uint sm = pio_claim_unused_sm(pio, true);
    if (sm == -1)
        return -1;

    // load program
    uint offset = pio_add_program(pio, &vpw_send_program);
    pio_sm_config c = vpw_send_program_get_default_config(offset);

    // set PIO clock
    sm_config_set_clkdiv(&c, getPioClockDivider(false));

    // configure ISR shift left without auto-pull
    sm_config_set_in_shift(&c, false, false, 32);

    // configure OSR shift left with auto-pull
    sm_config_set_out_shift(&c, false, true, 32);

    // configure instruction-specific pins
    sm_config_set_jmp_pin(&c, PIN_VPW_INPUT);
    sm_config_set_set_pins(&c, PIN_VPW_OUTPUT, 1);

    // set pin directions
    pio_sm_set_consecutive_pindirs(pio, sm, PIN_VPW_INPUT, 1, false);
    pio_sm_set_consecutive_pindirs(pio, sm, PIN_VPW_OUTPUT, 1, true);

    // connect GPIOs to this PIO block
    pio_gpio_init(pio, PIN_VPW_INPUT);
    pio_gpio_init(pio, PIN_VPW_OUTPUT);

    // start state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    return sm;
}

static volatile bool sending = false;

bool VPW::sendRaw(const byte* data, uint16_t bytes, bool send4X) {
  if (_smSend == -1 || data == NULL || bytes == 0)
    return false;
  sending = true;
  uint timeout = millis();
  bool congestion = false;

  // set PIO clock speed for 1x or 4x
  pio_sm_set_clkdiv(_pioSend, _smSend, getPioClockDivider(send4X));
  if (send4X) {
    digitalWriteFast(PIN_VPW_MODE, HIGH);
  } else {
    digitalWriteFast(PIN_VPW_MODE, LOW);
  }
  VPW::SEND_4X = send4X;
  
  // enable + wait 80us according to MC33390 datasheet
  digitalWriteFast(PIN_VPW_ENABLE, HIGH);
  delayMicroseconds(80);

  while (millis() - timeout < 1000 /* ONE SECOND TIMEOUT */) {
    byte padding = (3 - (bytes % 4)) % 4;
    uint w = bytes;
    byte bits = 8;
    for (uint16_t i = 0; i < bytes; i++) {
      w = (w << 8) | data[i]; // shift data in
      bits += 8;
      if (bits == 32) {
        pio_sm_put_blocking(_pioSend, _smSend, w);
        w = 0;
        bits = 0;      
      }
    }
    if (padding > 0) {
      for (short i = 0; i < padding; i++) {
        w <<= 8;
        bits += 8;
      }
      pio_sm_put_blocking(_pioSend, _smSend, w);
    }
    
    // PIO sends 1 for successful send, 0 for congestion
    if (pio_sm_get_blocking(_pioSend, _smSend) > 0) {
      digitalWriteFast(PIN_VPW_ENABLE, LOW); // disable transceiver
      digitalWriteFast(PIN_VPW_MODE, LOW); // 1X
      if (_ledHandler)
        _ledHandler(true, congestion ? LED_HANDLER_CONGESTION : LED_HANDLER_SEND);
      sending = false;
      return true;
    }
    congestion = true;
    sending = false;
  }
  
  digitalWriteFast(PIN_VPW_ENABLE, LOW); // disable transceiver
  digitalWriteFast(PIN_VPW_MODE, LOW); // 1X
  
  // timed out attempting to send
  if (_ledHandler)
    _ledHandler(false, LED_HANDLER_CONGESTION);
  return false;  
}

sendVPW_status_t VPW::send(const J1850& message, bool allowInvalid, bool send4X) {
    if (sending)
      return SEND_VPW_STATUS_STILL_SENDING;

    int messageLength = message.size();
    if (messageLength == 0)
        return SEND_VPW_STATUS_OK;
    
    if (!allowInvalid) {
      if (messageLength < 4)
        return SEND_VPW_STATUS_TOO_SHORT;
      if (messageLength > 0xFF)
        return SEND_VPW_STATUS_TOO_LONG;
      if (!message.isValid())
        return SEND_VPW_STATUS_INVALID_CRC;
    } else {
      if (messageLength == 0)
        return SEND_VPW_STATUS_TOO_SHORT;        
    }

    ulong bitsReceived = VPW::getBitsReceived();
    bool sendOk = vpw.sendRaw(message.rawByteArray(), messageLength, send4X);
    
    if (!sendOk)
      return SEND_VPW_STATUS_CONGESTION;

    if (bitsReceived == vpw.getBitsReceived())
      return SEND_VPW_STATUS_NO_ECHO;
      
    return SEND_VPW_STATUS_OK;
}
