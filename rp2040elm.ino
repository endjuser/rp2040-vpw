#include <vector>

#if ARDUINO_ADAFRUIT_FEATHER_RP2040
#define USE_SD
#endif

#include "cli.h"
#include "blinkenlights.h"
#include "vpw.h"
#include "rtc.h"
#include "sdlog.h"
#include "util.h"
#include "stringutil.h"
#include "automation.h"

// CONFIGURATION:
// Board: "Waveshare RP2040 Zero" or "Adafruit Feather RP2040"
// CPI Speed: "133 MHz" (although we should test if we can run it slower)
// Flash Size: "2MB (Sketch: 1984KB, FS: 64KB)" (for settings storage)

// LIBRARIES:
// NeoPixelConnect
// RTCLib (for RTC_PCF8523)
// LittleFS
// SD, SDFS
// SPI


MessageQueue class2;

HostCLI cliHost(Serial1, 65536, PIN_DTR, PIN_DSR);
AltCLI  cliBT  (Serial2, 32);
AltCLI  cliUSB (Serial,  1024);

volatile bool setupComplete = false;

void setup() {
    Terminals.add(cliHost, cliBT, cliUSB);

    setPixel(4, 4, 4);

    Serial1.setTX(PIN_UART0_TX);
    Serial1.setRX(PIN_UART0_RX);
    Serial1.begin(115200);

    Serial2.setTX(PIN_UART1_TX);
    Serial2.setRX(PIN_UART1_RX);
    Serial2.begin(115200);

    Serial.begin(115200 * 4);

    for (int i = 0; i < 40; i++) {
        if (Serial)
            break;
        setPixel(0, 0, 0);
        delay(50);
        setPixel(4, 4, 4);
        delay(50);
    }
    setPixel(0, 0, 0);
    
    cliUSB.active = true;
    Terminals.begin(false);

    if (!ELM::loadTZ())
        setenv("TZ", "EST+5EDT,M3.2.0/2,M11.1.0/2", 1);
        
    if (rtc.begin()) {
        rtc.start();
        if (rtc.lostPower())
            Terminals.notify("[RTC lost power]");

        if (setTimeFromRTC())
            Terminals.notify("[RTC INITIALIZED]");
        else
            Terminals.notify("[FAILED TO SET TIME FROM RTC]");        
    } else {
        Terminals.notify("[NO RTC]");
    }
    
    #ifdef USE_SD
        bool sdOK = sdlog.begin(&Serial2);
        if (!sdOK) {
            Terminals.notify("[SD FAIL]");
        }
    #endif
    
    bool vpwOK = vpw.begin();
    if (vpwOK)
        vpw.setReceiveLedHandler(ledHandler);
    else
        Terminals.notify("[VPW FAIL]");

    if (watchdog_enable_caused_reboot())
        Terminals.notify("[WATCHDOG REBOOT]");
    watchdog_enable(5000, 1);

    Terminals.prompt();
    
    setupComplete = true;
}

void loop() {
    watchdog_update(); // consider having a volatile uint set by loop1() and check it here to see if it's run within the last 5 seconds
    
    fadePixels(true);

    vpw.receiveLoop();
    VPWMessageQueue.process();

    while (VPWMessageQueue.available()) {
        const std::shared_ptr<Message> ptr = VPWMessageQueue.pull();
        Terminals.push(ptr);
        class2.push(*ptr); // dereference to make copy for thread safety
    }

    Terminals.loop();
}

void setup1() {
    pinMode(PIN_ACCESSORY, OUTPUT);
    digitalWriteFast(PIN_ACCESSORY, false);
}

void loop1() {
    static uint now;

    static bool bus4X = false;
    
    static uint lastPowerMode = 0;
    static byte powerMode = 0x00;
    static byte keyPosition = 0x00;
    
    static uint  lastMessageTime = 0;
    static ulong messageCount = 0;
    static uint  logRotateGraceTime = 0;

    if (!setupComplete)
        return;

    now = millis();

    if (class2.available()) {
        MessagePtr m = class2.pull();
        
        lastMessageTime = now;
        if (messageCount++ == 0)
            messageCount++;
        
        #ifdef USE_SD
            sdlog.queue.push(m);
            if (logRotateGraceTime > 0)
                logRotateGraceTime = now;
        #endif

        if (m->isValid()) {            

            std::vector<byte> data = m->dataBytes();
            
            if (m->mode == 4)
                bus4X = true;
            else if (m->mode == 1)
                bus4X = false;
            
            if (m->isFunctional()) {

                // FUNCTIONAL MESSAGES
                
                switch (m->secondaryAddress()) {
                    case 0x06: {
                        if (data.size() == 3) { // sanity check
                            if (powerMode != data[0] || keyPosition != data[1]) {
                                powerMode = data[0];
                                keyPosition = data[1];
                                lastPowerMode = now;
                                Terminals.notify(std::string("[POWER MODE: ") + HexUtil.hex(powerMode) + " " + HexUtil.hex(keyPosition) + "]");
                                if (powerMode >= 0x01 && powerMode <= 0x03 && keyPosition == 0x00) {
                                    Terminals.notify("[POWER OFF]");
                                    logRotateGraceTime = now;
                                }
                            }
                            // Enable accessory power pin if power mode is RAP Unlock (05), Accessory (06), Run (07), Crank (08), or RAP (09)
                            digitalWriteFast(PIN_ACCESSORY, powerMode >= 0x05 && powerMode <= 0x09);
                        }
                        break;
                    }
                }

                if (Automation.sendVIN && m->target() == 0xFA && Automation.vin.size() == 17) {
                    byte sa = m->secondaryAddress();
                    const static std::string vinPrefix("88FB40");
                    switch(sa) {
                        case 0x01:
                            vpw.send(vinPrefix + HexUtil.hex(sa) + "000000" + HexUtil.hex(Automation.vin.at(0)) , false);
                            break;
                        case 0x02:
                        case 0x03:
                        case 0x04:
                        case 0x05:
                            size_t idx = 1 + 4 * (sa - 2);
                            vpw.send(vinPrefix + HexUtil.hex(sa) + HexUtil.hex(Automation.vin.at(idx + 0)) + HexUtil.hex(Automation.vin.at(idx + 1)) + HexUtil.hex(Automation.vin.at(idx + 2)) + HexUtil.hex(Automation.vin.at(idx + 3)) , false);
                            break;
                    }
                }
            
            } else if (m->isPhysical()) {
                
                // PHYSICAL MESSAGES
                if (m->target() == 0xFE && m->secondaryAddress() == 0xA1) {
                    if (!bus4X)
                        Terminals.notify("[Entering 4X Mode]");
                    bus4X = true;
                } else if (m->target() == 0xFE && m->secondaryAddress() == 0x20 && bus4X == true) {
                    bus4X = false;
                }

                if (Automation.sendVIN && m->target() == 0x10 && m->secondaryAddress() == 0x3C && Automation.vin.size() == 17) {
                    const byte* raw = m->rawByteArray();
                    byte block = raw[4];
                    std::string res("6C");
                    res += HexUtil.hex(m->source());
                    res += "107C";
                    res += HexUtil.hex(block);
                    switch (block) {
                        case 0x01:
                            res += "00";
                            res += HexUtil.hex(Automation.vin.at(0x00)) + HexUtil.hex(Automation.vin.at(0x01)) + HexUtil.hex(Automation.vin.at(0x02)) + HexUtil.hex(Automation.vin.at(0x03)) + HexUtil.hex(Automation.vin.at(0x04));
                            break;
                        case 0x02:
                            res += HexUtil.hex(Automation.vin.at(0x05)) + HexUtil.hex(Automation.vin.at(0x06)) + HexUtil.hex(Automation.vin.at(0x07)) + HexUtil.hex(Automation.vin.at(0x08)) + HexUtil.hex(Automation.vin.at(0x09)) + HexUtil.hex(Automation.vin.at(0x0A));
                        case 0x03:
                            res += HexUtil.hex(Automation.vin.at(0x0B)) + HexUtil.hex(Automation.vin.at(0x0C)) + HexUtil.hex(Automation.vin.at(0x0D)) + HexUtil.hex(Automation.vin.at(0x0E)) + HexUtil.hex(Automation.vin.at(0x0F)) + HexUtil.hex(Automation.vin.at(0x10));
                            break;
                        default:
                            res.clear();
                            break;
                    }
                    if (res.size())
                        vpw.send(res, false);
                }
            }

            // Automated Responses
            if (Automation.programmaticResponsesEnabled) {
                std::string sansCrc = static_cast<J1850>(*m).tostring(true, false, false);
                std::list<std::string> responses;
                {
                    recursive_mutex_t mutex = Automation.getMutex();
                    recursive_lock_guard lock(mutex);
                    auto match = Automation.programmaticResponses.find(sansCrc);
                    if (match != Automation.programmaticResponses.end()) {
                        responses = StringUtil.split(match->second, ',');
                    }
                }
                for (const std::string& entry : responses) {
                    J1850 reply(entry, true);
                    sendVPW_status_t status = vpw.send(reply, false);
                }
            }
            
        }
    }

    if (messageCount > 0 && now - lastMessageTime > (10 * 1000)) {
        digitalWriteFast(PIN_ACCESSORY, false);
        messageCount = 0;
    }

    #ifdef USE_SD
        static uint lastLog = 0;
        static uint sdFailures = 0;
        static uint sdWait = 0;
        
        if (sdlog.queue.available() && !sdlog.ready && (sdWait == 0 || now - sdWait > (1000 * sdFailures))) {
            bool sdOK = sdlog.begin(&Serial); // retry;
            if (sdOK)
                sdFailures = 0;
            else
                sdFailures++;
        }
        if (sdlog.ready) {
            if (sdlog.queue.available()) {
                if (!sdlog.file) {
                    sdlog.open(true);
                }
                if (sdlog.file) {
                    MessagePtr m = sdlog.queue.pull();
                    sdlog.messageCount++;
                    std::string s = m->tostring(cliHost.getElm().timestampOffset, true, true, true, true, 10, true);
                    s.append("\r\n");
                    if (!sdlog.write(s)) {
                        // TO-DO: if write fails, retry next loop
                    }
                    lastLog = now;
                }
            } else {
                if (sdlog.dirty && now - lastLog >= 1000) {
                    sdlog.flush();
                }
            }
            // Auto-close log after 10 seconds of inactivity
            if (lastLog > 0 && now - lastLog >= 10000) {
                sdlog.flush();
                sdlog.write("[INACTIVE]\r\n\r\n");
                sdlog.flush();
                //sdlog.close();
                lastLog = 0;
            }
            // Auto-rotate log after 1 minute of inactivity after vehicle powered off
            if (logRotateGraceTime > 0 && now - logRotateGraceTime >= 60000) {
                sdlog.increment();
                logRotateGraceTime = 0;
            }
        }
    #endif

    // AUTOMATION

    static uint lastTesterPresent = 0;
    static const J1850 mTesterPresent("8CFEF03F", true);
    
    if (Automation.sendTesterPresent) {
        if (now - lastTesterPresent >= 2000) {
            sendVPW_status_t status = vpw.send(mTesterPresent, false);
            switch (status) {
                case SEND_VPW_STATUS_OK:
                    lastTesterPresent = now;
                    break;
                case SEND_VPW_STATUS_STILL_SENDING:
                    break;
                case SEND_VPW_STATUS_CONGESTION:
                    lastTesterPresent += 100;
                    break;
                default:
                    lastTesterPresent += 1000;
                    break;
            }
        }
    }

    if (Automation.sendPowerMode) {
        if (now - lastPowerMode >= 2000) {
            J1850 mPowerMode(std::string("28FF4006") + HexUtil.hex(Automation.powerMode) + HexUtil.hex(Automation.keyPosition) + std::string("0B"), true);
            sendVPW_status_t status = vpw.send(mPowerMode, false);
            switch (status) {
                case SEND_VPW_STATUS_OK:
                    lastPowerMode = now;
                    break;
                case SEND_VPW_STATUS_STILL_SENDING:
                    break;
                case SEND_VPW_STATUS_CONGESTION:
                    lastPowerMode += 100;
                    break;
                default:
                    lastPowerMode += 1000;
                    break;
            }
        }
    }

}
