#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <regex>
#include "settings.h"
#include "util.h"
#include "hexutil.h"
#include "stringutil.h"
#include "cli.h"
#include "automation.h"
#include "vpw.h"
#include "rtc.h"

#ifdef USE_SD
#include "sdlog.h"
#endif

#ifndef defaultBaudRate
#define defaultBaudRate 115200
#endif

#ifndef DEVICE_DESCRIPTION
#define DEVICE_DESCRIPTION (std::string("OBD2-Pico-VPW/") + BOARD_NAME)
#endif

#define TOGGLE_FN_BASE(var, normal) {    \
            if (data == "1")             \
                var = normal;            \
            else if (data == "0")        \
                var = !normal;           \
            else if (data == "?")        \
                response =               \
                    (var == normal)      \
                        ? "1" : "0";     \
            else                         \
                response = "?";          \
        }

#define TOGGLE_FN(var) TOGGLE_FN_BASE(var, true)
#define INV_TOGGLE_FN(var) TOGGLE_FN_BASE(var, false)

#define SET_FN(var, value) {            \
            if (data.size() == 0)       \
                var = value;            \
            else                        \
                response = "?";         \
        }

#define BYTE_FN(var) {                                      \
            if (data == "?")                                \
                response = HexUtil.hex(var);                \
            else if (data.size() == 0 || data.size() > 2)   \
                response = "?";                             \
            else                                            \
                var = HexUtil.getByte(data);                \
        }
        
#define MONITOR_FN(type, var) {                             \
            if (data.size() !=                              \
                ((type == 'A' || type == 'B') ? 0 : 2))     \
                    response = "?";                         \
            else {                                          \
                monitor = type;                             \
                monitorCount = 0;                           \
                var = HexUtil.getByte(data);                \
                response = "SEARCHING...";                  \
            }                                               \
        }

#define CONST_FN(value) { response = value; }

#define NOARGS(code) {          \
    if (data.size() != 0) {     \
        response = "?";         \
    } else {                    \
        code;                   \
    }                           \
}

#define CMDCASE(value, code) {                              \
            if (cmd.rfind(value, 0) == 0) {                 \
                data = cmd.substr(strlen(value));           \
                code;                                       \
                return true;                                \
            }                                               \
        }

class ELM {
private:
    uint baudRate = defaultBaudRate;
    const std::string Y = "Y";
    const std::string N = "N";
    
public:
    const char* defaultHeader = "686ATT";

    const char* CR = "\r";
    const char* LF = "\n";
    const char* CRLF = "\r\n";

    const char* newline () const {
        return linefeed ? CRLF : CR;
    }

    const char* version() const {
        return "ELM327 V2.3";
    };

    const std::string replaceTT(std::string hexString) {
        std::string ta = HexUtil.hex(testerAddress);
        for (size_t i = 0; i < hexString.length(); i += 2) {
            if (i + 1 < hexString.length() && hexString.substr(i, 2) == "TT") {
                hexString.replace(i, 2, ta);
            }
        }
        return hexString;
    }

    // STANDARD ELM STUFF
    std::string lastCommand;
    std::string header;
    bool echo;
    bool allowLong;
    bool linefeed;
    bool autoReceive;
    bool responses;
    bool spaces;
    bool customHeader;
    bool headers;
    char monitor;
    ulong monitorCount;
    byte monitorTransmit;
    byte monitorReceive;
    byte monitorTimeout;
    byte adaptiveTiming;
    byte testerAddress;
    
    // CUSTOM STUFF
    bool notifications;
    bool allowInvalid;
    bool showTimestamp;
    bool showVpwMode;
    bool autoCRC;
    char vpwSpeed;
    byte inactiveTime;
    byte responseCount;
    bool waitSend;
    
    timeval timestampOffset = { 0, 0 };
    void zeroTimestamp() {
        gettimeofday(&timestampOffset, NULL);
    }
    void restoreTimestamp() {
        timestampOffset = {0, 0};
    }

    bool send4X() {
        if (vpwSpeed == 'A')
            return VPW::SEND_4X;
        else if (vpwSpeed = '4')
            return true;
        else
            return false;
    }
    
    std::string serialize(const char& delim) {
        std::string dat;
        dat.reserve(512);
                
        dat += "SH="  + header                          + delim;
        dat += "E="   + (echo                  ? Y : N) + delim;
        dat += "AL="  + (allowLong             ? Y : N) + delim;
        dat += "L="   + (linefeed              ? Y : N) + delim;
        dat += "AR="  + (autoReceive           ? Y : N) + delim;
        dat += "R="   + (responses             ? Y : N) + delim;
        dat += "S="   + (spaces                ? Y : N) + delim;
        dat += "CH="  + (customHeader          ? Y : N) + delim;
        dat += "H="   + (headers               ? Y : N) + delim;
        dat += "MT="  + HexUtil.hex(monitorTransmit)    + delim;
        dat += "MR="  + HexUtil.hex(monitorReceive)     + delim;
        dat += "ST="  + HexUtil.hex(monitorTimeout)     + delim;
        dat += "AT="  + HexUtil.hex(adaptiveTiming)     + delim;
        dat += "TA="  + HexUtil.hex(testerAddress)      + delim;

        dat += "N="   + (notifications         ? Y : N) + delim;
        dat += "AI="  + (allowInvalid          ? Y : N) + delim;
        dat += "TS="  + (showTimestamp         ? Y : N) + delim;
        dat += "VM="  + (showVpwMode           ? Y : N) + delim;
        dat += "CRC=" + (autoCRC               ? Y : N) + delim;
        dat += "W="   + (waitSend              ? Y : N) + delim;
        dat += "RC="  + HexUtil.hex(responseCount)      + delim;
        dat += "VPW=" + std::string(1, vpwSpeed);       /* LAST PARAM HAS NO DELIM */       
        return dat;
    }

    bool save(byte index) {
        std::string dat = serialize(*LF);
        dat.append(LF);
        std::string filename = "elm-" + HexUtil.hex(index);
        return SettingsRepository.write(filename.c_str(), dat) > 0;
    }
    
    bool load(byte index) {
        ATD(); // set everything to default values first
        
        std::string filename = "elm-" + HexUtil.hex(index);
        std::string dat;
        if (SettingsRepository.read(filename.c_str(), dat) <= 0)
            return false;
        return load(dat, *LF);
    }

    bool load(std::string dat, const char& delim) {
        std::istringstream iss(dat);
        std::string line;
        size_t found = 0;
        
        while (std::getline(iss, line, delim)) {
            std::istringstream lineStream(line);
            std::string key, value;
            
            if (std::getline(lineStream, key, '=')) {
                std::getline(lineStream, value);
                
                found++;
                
                if (key == "SH")
                    header = value;
                else if (key == "E")
                    echo = (value == Y);
                else if (key == "AL")
                    allowLong = (value == Y);
                else if (key == "L")
                    linefeed = (value == Y);
                else if (key == "AR")
                    autoReceive = (value == Y);
                else if (key == "R")
                    responses = (value == Y);
                else if (key == "S")
                    spaces = (value == Y);
                else if (key == "CH")
                    customHeader = (value == Y);
                else if (key == "H")
                    headers = (value == Y);
                else if (key == "MT")
                    monitorTransmit = HexUtil.getByte(value);
                else if (key == "MR")
                    monitorReceive = HexUtil.getByte(value);
                else if (key == "ST")
                    monitorTimeout = HexUtil.getByte(value);
                else if (key == "AT")
                    adaptiveTiming = HexUtil.getByte(value);
                else if (key == "TA")
                    testerAddress = HexUtil.getByte(value);

                else if (key == "N")
                    notifications = (value == Y);
                else if (key == "AI")
                    allowInvalid = (value == Y);
                else if (key == "TS")
                    showTimestamp = (value == Y);
                else if (key == "VM")
                    showVpwMode = (value == Y);
                else if (key == "CRC")
                    autoCRC = (value == Y);
                else if (key == "VPW")
                    vpwSpeed = (value.size() > 0 ? value[0] : 'A');
                else if (key == "RC")
                    responseCount = HexUtil.getByte(value);
                else if (key == "W")
                    waitSend = (value == Y);

                else
                    found--;
            }
        }

        return found > 0;
    }

    static bool saveTZ() {
        return (SettingsRepository.write("elm-tz", getenv("TZ")) > 0);
    }

    static bool loadTZ() {
        std::string tz;
        if (SettingsRepository.read("elm-tz", tz) <= 0)
            return false;
        setenv("TZ", tz.c_str(), 1);
        tzset();
        return true;
    }

    void ATD() {
        lastCommand = "";
        header = defaultHeader;
        
        echo         = true;
        allowLong    = false;
        linefeed     = true;
        autoReceive  = true;
        responses    = true;
        spaces       = true;
        customHeader = false;
        headers      = false;
        monitor      = 0x00;
        monitorCount = 0;
        monitorTransmit = 0x00;
        monitorReceive  = 0x00;
        monitorTimeout  = 0x32;
        adaptiveTiming  = 0x01;
        testerAddress   = 0xF1;
        
        notifications = true;
        allowInvalid  = false;
        showTimestamp = false;
        showVpwMode   = false;
        autoCRC       = true;
        waitSend      = false;
        vpwSpeed      = 'A';
        inactiveTime  = 0;
        responseCount = 0;
        
        restoreTimestamp();
    }

    void ATWS() {
        ATD();

        Automation.powerMode = 0x00;
        Automation.keyPosition = 0x00;
        Automation.sendPowerMode = false;
        Automation.sendTesterPresent = false;

#ifdef USE_SD
        sdlog.clearBuffer();
#endif

        // If we have any other ELM-related tasks going on, stop them here.
    }
    
    void ATZ(HardwareSerial& port) {
        // According to the ELM documentation, ATZ is also supposed to reset the baud rate
        baudRate = defaultBaudRate;
        port.flush();
        port.begin(baudRate);
        
        // Warm start
        ATWS();
    }

    ELM() {
        ATWS();
    }

    //
    // Programmed response: i.e. if XXXXXX is received then send YYYY,ZZZZZZ
    //
    void ATPR(std::string& response, std::string_view data) {
        recursive_mutex_t mutex = Automation.getMutex();
        recursive_lock_guard lock(mutex);
        if (data == "1") {
            Automation.programmaticResponsesEnabled = true;
        } else if (data == "0") {
            Automation.programmaticResponsesEnabled = false;
        } else if (data == "?") {
            response = Automation.programmaticResponsesEnabled ? "1" : "0";
        } else if (data == "??" || data == "???") {
            if (Automation.programmaticResponses.size() == 0) {
                response = "[]";
            } else {
                bool prettyPrint = (data == "???");
                response = "[";
                if (prettyPrint)
                    response += newline();
                for (auto it = Automation.programmaticResponses.begin(); it != Automation.programmaticResponses.end(); ++it) {
                    response += it->first + "=" + it->second;
                    if (std::next(it) != Automation.programmaticResponses.end())
                        response += (prettyPrint ? newline() : ";");
                }                
                if (prettyPrint)
                    response += newline();
                response += "]";
            }
        } else {
            //static const std::regex rxATPR("^([0-9A-F]+)([\\=\\+\\-\\?])([^,][0-9A-F,]+)?$"); // *** This regex crashes rp2040 if input is too long
            //std::smatch sm;
            //std::string str(data);
            //std::regex_match (str, sm, rxATPR);
            //if (sm.size() == 4) {
            //    std::string key   = sm[1];
            //    std::string op    = sm[2];
            //    std::string value = sm[3];
            
            int group = 1;
            std::string key;
            std::string op;
            std::string value;
            key.reserve(data.size());
            value.reserve(data.size());
            for (char c : data) {
                switch (group) {
                    case 1: {
                        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
                            key += c;
                        } else if (c == '=' || c == '+' || c == '-' || c == '?') {
                            op = c;
                            group = 3;                            
                        } else {
                            group = -1;
                            break;
                        }
                        break;                   
                    }
                    case 3: {
                        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
                            value += c;
                        } else if (c == ',') {
                            if (value.size() == 0) {
                                group = -2;
                                break;
                            }
                            value += ",";
                        } else {
                            group = -3;
                            break;
                        }
                        break;
                    }
                    default: {
                        group = -4;
                        break;
                    }
                }
            }
            if (group > 0) {
                value.erase(std::find_if(value.rbegin(), value.rend(), [](int ch) { return ch != ','; }).base(), value.end());
                size_t valueSize = value.size();
                
                // verify content is valid hex length
                std::string check;
                check.reserve(valueSize);
                for (size_t i = 0; i <= valueSize; i++) {
                    char c = (i == valueSize ? ',' : value[i]);
                    if (c == ';') {
                        size_t checkSize = check.size();
                        check.clear();
                        if (checkSize > 0 && checkSize % 2 != 0) {
                            op = "X"; // invalid operator so that the switch below produces "?"
                            break;
                        }
                    } else {
                        check += c;
                    }
                }

                bool keyExists = false;
                if (key.size() > 0) {
                    auto match = Automation.programmaticResponses.find(key);
                    if (match != Automation.programmaticResponses.end())
                        keyExists = true;
                }
                
                switch (op[0]) {
                    case '=': {
                        if (valueSize == 0)
                            Automation.programmaticResponses.erase(key);
                        else
                            Automation.programmaticResponses[key] = value;
                        break;
                    }
                    case '+': {
                        if (valueSize == 0) {
                            response = "?";
                        } else {
                            if (keyExists)
                                Automation.programmaticResponses[key] += ",";
                            Automation.programmaticResponses[key] += value;
                        }
                        break;   
                    }
                    case '-': {
                        if (valueSize == 0) {
                            response = "?";
                        } else if (keyExists) {
                            std::list values = StringUtil.split(Automation.programmaticResponses[key], ',');
                            auto it = std::find(values.begin(), values.end(), value);
                            if (it != values.end())
                                values.erase(it);
                            if (values.size() == 0)
                                Automation.programmaticResponses.erase(key);
                            else
                                Automation.programmaticResponses[key] = StringUtil.join(values, ',');
                        }
                        break;   
                    }
                    case '?': {
                        response = key;
                        response += "=";
                        if (keyExists)
                            response += Automation.programmaticResponses[key];
                        break;
                    }
                    default: {
                        response = "?";
                        break;   
                    }      
                }
            } else {
                response = "?";
            }
        }
    }
    
    bool process(std::string& response, std::string_view cmd, std::string_view input, HardwareSerial& port) {
        std::string_view data;
        
        // NOTE: Commands that start with the same letters must be ordered with the longest command first
        //       e.g. ATSH must be checked before ATS
        
        CMDCASE("AT@1",  CONST_FN(DEVICE_DESCRIPTION));
        CMDCASE("ATAI",  TOGGLE_FN(allowInvalid));
        CMDCASE("ATAL",  SET_FN(allowLong, true));
        CMDCASE("ATAR",  SET_FN(autoReceive, true));
        CMDCASE("ATCH",  {
            TOGGLE_FN(customHeader));
            if (data == "1") {
                header = "";
            } else if (data == "0") {
                auto bytes = HexUtil.bytes(data);
                if (bytes.size() != 3)
                    header = defaultHeader;
            }
        }
        CMDCASE("ATCFG", {
            if (data == "?") {
                response = serialize(',');
            } else if (data.size() > 0) {
                if (!load(std::string(data), ','))
                    response = "?";                
            } else {
                response = "?";
            }
        });
        CMDCASE("ATCRC", TOGGLE_FN(autoCRC));
        CMDCASE("ATCT",  NOARGS(CONST_FN(std::to_string(Util.getCpuTemperature()))));
        CMDCASE("ATDPN", CONST_FN("2"));
        CMDCASE("ATDP",  CONST_FN("SAE J1850 VPW"));
        CMDCASE("ATD",   NOARGS(ATD()));
        CMDCASE("ATE",   TOGGLE_FN(echo));
        CMDCASE("ATH",   TOGGLE_FN(headers));
        CMDCASE("ATIA",  BYTE_FN(inactiveTime));
        CMDCASE("ATID",  NOARGS(CONST_FN(Util.getUniqueBoardId())));
        CMDCASE("ATI",   CONST_FN(version()));
        CMDCASE("ATLOAD",{
            if (data.size() == 1) {
                byte index = HexUtil.getByte(data);
                if (!load(index))
                    response = "!ERROR";
            } else {
                response = "?";
            }
        });
#ifdef USE_SD
        CMDCASE("ATLOG", {
            if (data == "") {
                response = std::to_string(sdlog.index);
                response += ":";
                response += std::to_string(sdlog.messageCount);
            } else if (data == "?") {
                response = sdlog.cardInfo();
                response += " LOG #";
                response += std::to_string(sdlog.index);
            } else if (data == "+") {
                sdlog.open();
            } else if (Util.isNumeric(data)) {
                sdlog.print(atoi(data.data()), port, newline());
            } else if (data.size() > 0 && data[0] == '#') {
                std::string line(input);
                line.erase(0, 5);
                line.append(newline());
                sdlog.write(line);
            } else {
                response = "?";
            }
        });
#endif
        CMDCASE("ATL",   TOGGLE_FN(linefeed));
        CMDCASE("ATMA",  MONITOR_FN('A', monitorReceive));
        CMDCASE("ATMB",  {
            if (monitor == 'B') {
                response = std::string("STOPPED") + newline();
                monitor = 0x00;
            } else {
                MONITOR_FN('B', monitorReceive);
                monitorTransmit = 0x00;
            }
        });
        CMDCASE("ATMEM", {
            response = std::to_string(Util.getFreeMemory()); 
        });
        CMDCASE("ATMR",  MONITOR_FN('R', monitorReceive));
        CMDCASE("ATMT",  MONITOR_FN('T', monitorTransmit));
        CMDCASE("ATNL",  SET_FN(allowLong, false));
        CMDCASE("ATN",   TOGGLE_FN(notifications));
        CMDCASE("ATPR",  ATPR(response, data));
        CMDCASE("ATRA",  {
            BYTE_FN(monitorReceive);
            autoReceive = false;
        });
        CMDCASE("ATRC",  BYTE_FN(responseCount));
        CMDCASE("ATRTC", {
            if (data == "B") {
                if (rtc.begin())
                    response = "OK";
                else
                    response = "FAIL";
            } else if (data == "S") {
                rtc.start();
                response = "OK";
            } else if (data == "?") {
                std::string temp = "";
                temp += "[CHECKING RTC]";
                temp += newline();
                if (rtc.lostPower()) {
                    temp += "[RTC LOST POWER]";
                    temp += newline();
                }
                if (setTimeFromRTC()) {
                    temp += "[RTC INITIALIZED]";
                    temp += newline();
                    temp += "OK";
                } else {
                    temp += "[FAILED TO SET TIME FROM RTC]";
                    temp += newline();
                    temp += "FAIL";
                }
                response = temp;
            } else {
                response = "ERROR";
            }
        });
        CMDCASE("ATR",   TOGGLE_FN(responses));
        CMDCASE("ATSAVE",{
            if (data.size() == 1) {
                byte index = HexUtil.getByte(data);
                if (!save(index))
                    response = "!ERROR";
            } else {
                response = "?";
            }
        });
        CMDCASE("ATSH",  {
            auto bytes = HexUtil.bytes(replaceTT(std::string(data)));
            if (data == "?") {
                response = header;
            } else if (!customHeader && bytes.size() != 3) {
                response = "?";
            } else {
                header = data;
            }
        });
        CMDCASE("ATSP",  {
            if (data != "2" && data != "0")
                response = "?";
        });
        CMDCASE("ATSR",  {
            autoReceive = false;
            BYTE_FN(monitorReceive);
        });
        CMDCASE("ATST",  {
            if (data.size() == 2) {
                monitorTimeout = HexUtil.getByte(data);
                if (monitorTimeout == 0x00)
                    monitorTimeout = 0x32;
                if (monitorTimeout < 0x08)
                    monitorTimeout = 0x08;
            } else if (data == "?") {
                response = HexUtil.hex(monitorTimeout);
            } else {
                response = "?";
            }
        });
        CMDCASE("ATS",   TOGGLE_FN(spaces));
        CMDCASE("ATTA",  BYTE_FN(testerAddress));
        CMDCASE("ATTIME", {
            if (data.size() != 0 && data != "?") {
                response = "?";
            } else {
                struct timeval tv;
                if (gettimeofday(&tv, nullptr) != 0) {
                    response = "ERROR";
                } else {
                    response = Util.timevalToString(tv);
                }
            }
        });
        CMDCASE("ATTP",  {
            if (data != "2" && data != "A")
                response = "?";
        });
        CMDCASE("ATTS", {
            if (data == "Z")
                zeroTimestamp();
            else if (data == "R")
                restoreTimestamp();
            else if (data == "0")
                showTimestamp = false;
            else if (data == "1")
                showTimestamp = true;
            else if (data == "Z?") {
                response = std::to_string((uint)timestampOffset.tv_sec);
                response += ".";
                response += std::to_string((uint)timestampOffset.tv_usec);
            } else if (data == "?")
                response = showTimestamp ? "1" : "0";
            else
                response = "?";                
        });
        CMDCASE("ATTZ", {
            if (data == "?") {
                response = getenv("TZ");
            } else if (data.size() == 0) {
                response = "?";
            } else if (data == "S") {
                if (!saveTZ())
                    response = "!ERROR";
            } else if (data == "L") {
                if (!loadTZ())
                    response = "!ERROR";
            } else {
                std::string tz(input);
                tz.erase(std::remove(tz.begin(), tz.end(), ' '), tz.end());
                tz.erase(0, 4);
                setenv("TZ", tz.c_str(), 1);
                tzset();
            }
        });
        CMDCASE("ATUT", {
            if (data == "?") {
                struct timeval tv;
                if (gettimeofday(&tv, nullptr) != 0) {
                    response = "ERROR";
                } else {
                    response = std::to_string(tv.tv_sec) + "." + std::to_string(tv.tv_usec);
                }
            } else if (Util.isNumeric(data)) {
                struct timeval tv;
                tv.tv_sec = atol(data.data());
                tv.tv_usec = 0;
                if (settimeofday(&tv, nullptr) != 0) {
                    response = "ERROR";
                } else {
                    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
                }
            } else {
                response = "?";
            } 
        });
        CMDCASE("ATVM",   TOGGLE_FN(showVpwMode));
        CMDCASE("ATVPW",  {
            if (data == "?") {
                response = vpwSpeed; 
            } else if (data == "A" || data == "1" || data == "4") {
                vpwSpeed = data[0];
            } else {
                response = "?";
            }
        });
        CMDCASE("ATWS",   NOARGS(ATWS()));
        CMDCASE("ATW",    TOGGLE_FN(waitSend));
        CMDCASE("ATZ",    NOARGS(ATZ(port)));


        /*
         * ADDITIONAL OBDX PRO COMMANDS
         */
         
        CMDCASE("DXI",    NOARGS(CONST_FN(DEVICE_DESCRIPTION)));
        CMDCASE("DXPT",   {
            if (data == "0") {
                monitor = 0x00;
                monitorReceive = 0x00;
                monitorTransmit = 0x00;
            } else if (data == "1") {
                // same as ATMB, except response is "OK" instead of "SEARCHING..."
                monitor = 'B';
                monitorCount = 0;
                monitorTransmit = 0x00;
                monitorReceive  = 0x00;
            } else if (data == "?") {
                response = (monitor == 'B' ? "1" : "0");
            } else {
                response = "?";
            }
        });
        // NOTE: DXSD is handled in cli
        CMDCASE("DXSM",  {});
        CMDCASE("DXUS",  {
            if (data.size() == 0) {
                response = Util.getUniqueBoardId(false);
                response += std::string(response.rbegin(), response.rend()); // append reversed to get 16 bytes total (also a palendrome, just for fun)
            } else {
                response = "?";
            }
        });
        CMDCASE("DXVS",  {
            if (data == "1") {
                vpwSpeed = '1';
            } else if (data == "4") {
                vpwSpeed = '4';
            } else if (data == "?") {
                response = vpwSpeed;
            } else {
                response = "?";
            }
        });

        /*
         * ADDITIONAL GM COMMANDS
         */
         
        CMDCASE("GMTP",  TOGGLE_FN(Automation.sendTesterPresent));
        CMDCASE("GMPM",  {
            if (data == "?") {
                response = std::string(Automation.sendPowerMode ? "1:" : "0:") + HexUtil.hex(Automation.powerMode) + HexUtil.hex(Automation.keyPosition);
            } else if (data == "1") {
                Automation.sendPowerMode = true;
            } else if (data == "0") {
                Automation.sendPowerMode = false;
            } else {
                std::vector<byte> bytes = HexUtil.bytes(data);
                if (bytes.size() == 2) {
                    Automation.powerMode = bytes[0];
                    Automation.keyPosition = bytes[1];
                    Automation.sendPowerMode = (Automation.powerMode > 0 || Automation.keyPosition > 0);
                } else {
                    response = "?";
                }
            }
        });
        CMDCASE("GMVIN", {
            if (data == "?") {
                response = std::string(Automation.sendVIN ? "1:" : "0:") + Automation.vin;
            } else if (data == "1") {
                Automation.sendVIN = true;
            } else if (data == "0") {
                Automation.sendVIN = false;
            } else if (data.size() == 17) {
                std::string vin = std::string(data);
                std::transform(vin.begin(), vin.end(), vin.begin(), ::toupper);
                vin.erase(std::remove(vin.begin(), vin.end(), ' '), vin.end());
                Automation.vin = vin;
                Automation.sendVIN = true;
            } else {
                response = "?";
            }
        });
        return false;
    }
};
