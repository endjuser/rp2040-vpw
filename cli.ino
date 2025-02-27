#include <algorithm>
#include <functional>
#include <regex>
#include "cli.h"
#include "pins.h"
#include "elm.h"
#include "vpw.h"
#include "j1850.h"
#include "message.h"

void CLI::prompt(bool force) {
    if ((!atPrompt || force)) {
        port.print('>');
        port.flush();
        atPrompt = true;
    }
    
    if (!messages.available() && !notifications.available())
        dsr(false);
}

void CLI::printNotifications() {
    if (!active || !notifications.available())
        return;
    bool needsPrompt = (elm.monitor == 0x00 || elm.monitor == 'B');
    bool blank = atPrompt;
    while (notifications.available()) {
        std::shared_ptr<std::string> n = notifications.pull();
        if (!elm.notifications)
            continue;

        const size_t tsZeroes = 4;
        std::string line;
        line.reserve(n->size() + 24);
        if (elm.showTimestamp) {
            struct timeval tv = vpw.getTimestamp();
            tv.tv_sec -= elm.timestampOffset.tv_sec;
            if (elm.timestampOffset.tv_usec != 0)
                tv.tv_usec = (1000000 + tv.tv_usec - elm.timestampOffset.tv_usec) % 1000000;
            line += Util.dec((ulong)tv.tv_sec, tsZeroes) + '.' + Util.dec((ulong)tv.tv_usec, 6);
            line += '\t';
        }
        line += '#';
        line += *n;
        line += elm.newline();
        
        atPrompt = false;
        if (blank) {
            blank = false;
            port.print(elm.newline());
        }
        port.print(line.c_str());
    }
    if (needsPrompt)
        prompt();
}

bool CLI::ready() const {
    return dtr();
}

void CLI::push(const std::shared_ptr<Message>& message) {
    while (messages.size() > queueSize)
        messages.pop_front();
    messages.push_back(message);
    dsr(true);
}

void CLI::notify(const std::shared_ptr<std::string>& notification) {
    notifications.push_back(notification);
    dsr(true);
}

bool CLI::begin(bool showPrompt = true) {
    port.print(elm.version());
    port.print(elm.newline());
    printNotifications();
    if (showPrompt)
        prompt();
    initialized = true;
    return true;
}

void CLI::flush() {
    port.print(""); // theoretically this should not be necessary, but without it sometimes we encounter a hang when flushing in multiple cores
    port.flush();
}

bool CLI::loop() {
    if (!initialized || !ready())
        return false;        

    if (!inhibitOutput)
        printNotifications();
        
    // MONITOR
    while (!inhibitOutput && messages.available()) {
        std::shared_ptr<Message> m = messages.pull();
            
        if (elm.monitor == 'S') {
            if (lastSent == NULL || *lastSent == *m)
                continue;
                
            if (elm.autoReceive) {
                bool relevant = false;
                if ( (lastSent->target() == 0xFE && m->target() == 0xFE) || lastSent->source() == m->target() ) {
                    if ( m->secondaryAddress() == 0x7F || m->secondaryAddress() == lastSent->secondaryAddress() + 0x40 ) {
                        if (m->isPhysical() == lastSent->isPhysical()) {
                            relevant = true;
                        }
                    }
                }
                if (!relevant)
                    continue;
            } else {
                //
                // TO-DO
                //
            }
        } else if (elm.monitor == 'R') {
            if (elm.monitorReceive != m->target())
                continue;
        } else if (elm.monitor == 'T') {
            if (elm.monitorTransmit != m->source())
                continue;
        } else if (elm.monitor == 'A') {
            // no filter
        } else if (elm.monitor == 'B') {
            // TO-DO: bi-directional, but we should still honour ATRS and ATSR
        } else {
            continue;
        }
        elm.monitorCount++;
        messageCount++;
        if (m->mode != lastMode && !elm.showVpwMode) {
            lastMode = m->mode;
            port.print(m->mode == 1 ? "[MODE: 1X]" : "[MODE: 4X]");
            port.print(elm.newline());
        }
        port.print(m->tostring(elm.timestampOffset, elm.showTimestamp, elm.headers, elm.spaces, elm.allowLong, 4, elm.showVpwMode).c_str());
        port.print(elm.newline());
        atPrompt = false;
        if (elm.monitor == 'B')
            prompt();
        lastMessageTime = millis();
        //return;
    }
    if (elm.monitor == 'S') {
        if (elm.responseCount > 0 && elm.responseCount == elm.monitorCount) {
            elm.monitor = 0x00;
            prompt();
        } else if (millis() - lastMessageTime > (4 * elm.monitorTimeout)) {
            elm.monitor = 0x00;
            if (elm.monitorCount == 0) {
                port.print("NO DATA");
                port.print(elm.newline());
            }
            prompt();
        }
    }

    if (messageCount > 0 && elm.inactiveTime > 0 && millis() - lastMessageTime > (1000 * elm.inactiveTime)) {
        static const std::shared_ptr<std::string> inactiveMessage = std::make_shared<std::string>(std::string("[INACTIVE]"));
        messageCount = 0;
        notify(inactiveMessage);
    }

    if (!port.available()) {
        if (millis() - lastInputTime >= (20 * 1000) && cmd.size() > 0) {
            Serial.print("?");
            Serial.print(elm.newline());
            prompt(true);
            cmd.clear();
        }
        return true;
    }
        
    char c = (char)port.read();
    
    lastInputTime = millis();
    if (c == '\n' && lastC == '\r')
        return true;
    active = true;
    atPrompt = false;
    
    if (elm.monitor && elm.monitor != 'B') {
        while (port.available() > 0) {
            c = (char) port.peek();
            if (c == '\r' || c == '\n') {
                monitorEnter = monitorText ? true : false;
            } else {
                if (monitorEnter)
                    break;
                monitorText = true;
            }
            port.read();
        }
        elm.monitor = 0x00;
        port.print(elm.newline());
        port.print("STOPPED");
        port.print(elm.newline());
        prompt();
        return true;
    }
    
    if (c == '\r' || c == '\n') {
        if (c == '\r' || lastC != '\r') {
            inhibitOutput = false;
            monitorEnter = false;
            monitorText = false;
            port.print(elm.newline());
            this->process(cmd);
            cmd.clear();
            lastMessageTime = millis();            
        }
    } else if (c == 0x08) {
        if (cmd.size() > 0) {
            if (elm.echo) {
                port.print(c);
            }
            cmd.pop_back();
        }
        inhibitOutput = (cmd.size() > 0);
    } else {
        if (c < 0x20 || c > 0x7E) {
            //Uncomment for debugging non-printable character inputs
            //port.print('{');
            //port.print(HexUtil.hex(c).c_str());
            //port.print('}');
        }
        if (elm.echo) {
            port.print(c);
        }
        cmd += c;
        inhibitOutput = true;
    }
    
    lastC = c;
    return true;
}

void CLI::process(std::string input) {
    std::string cmd(input);

    // Convert to uppercase and remove spaces
    transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    cmd.erase(std::remove(cmd.begin(), cmd.end(), ' '), cmd.end());
    
    std::string response = "OK";

    bool ok = false;
    if (cmd.size() == 0) {
        cmd = elm.lastCommand; // ELM standard treats blank input as repeat last command
        if (cmd.size() == 0)
            goto skip;
    }
    elm.lastCommand = std::string(cmd);

    if (cmd.rfind("NOTIFY", 0) == 0) {
        // send notification to all terminals
        Terminals.notify(input.substr(6));
        ok = true;
    } else if (cmd.rfind("SIM", 0) == 0) {
        // simulate raw VPW message from bus
        Message msg(cmd.substr(3), false);
        VPWMessageQueue.push(msg);
        ok = true;
        
    } else if (cmd.size() == 3 && cmd[0] == '\x25' && cmd[1] == '\x00' && cmd[2] == '\xDA') {

        // special case for DVI "reset back to boot" command
        elm.ATZ(port);
        while (port.available())
            port.read();
        ok = false;
                    
    } else if (cmd.size() > 0) {

        // special case for DXSD
        bool isDXSD = false;
        bool dxsd4X = VPW::SEND_4X;
        if (cmd.rfind("DXSD", 0) == 0 && cmd.size() > 7 && cmd.size() % 2 == 0) {
            cmd.erase(0, 4);
            if (cmd.rfind("4X", 0) == 0) {
                dxsd4X = true;
                cmd.erase(0, 2);
            } else if (cmd.rfind("1X", 0) == 0) {
                dxsd4X = false;
                cmd.erase(0, 2);
            }
            isDXSD = true;
        }

        static const std::regex patternHexT("^(?:[0-9A-F][0-9A-F]|TT)+$");
        if (isDXSD || (std::regex_match(cmd.begin(), cmd.end(), patternHexT))) {
            // Try parsing cmd as J1850 data
            J1850 message(elm.replaceTT(isDXSD ? cmd : (elm.header + cmd)));
            ok = message.isValid();        
            if (ok) {
                response.clear();
                sendVPW_status_t status;
                while (true) {
                    status = vpw.send(message, elm.allowInvalid, isDXSD ? dxsd4X : elm.send4X());
                    if (!elm.waitSend || !(status == SEND_VPW_STATUS_CONGESTION || status == SEND_VPW_STATUS_STILL_SENDING))
                        break;
                    watchdog_update();
                }
                printSendError(status);
                if (status == SEND_VPW_STATUS_OK) {
                    lastSent = std::make_shared<J1850>(message);
                    if (message.isPhysical() && message.target() == 0xFE) {
                        // Special case for command to enter 4X mode
                        if (message.secondaryAddress() == 0xA1)
                            VPW::SEND_4X = true;
                        // Special case for command to return to normal
                        if (message.secondaryAddress() == 0x20)
                            VPW::SEND_4X = false;
                    }
                    if (!elm.monitor) {
                        if (elm.responses) {
                            elm.monitor = 'S';
                            elm.monitorCount = 0;
                            elm.monitorReceive = message.source();
                            elm.monitorTransmit = message.target();
                        } else {
                            port.print(elm.newline());
                        }
                    }
                    this->waitMonitor = false;                    
                }
            }
        }
        
        if (!ok) {

            // Special pre-reset handling for ATZ
            if (cmd == "ATZ") {
                #ifdef USE_SD
                    sdlog.close();
                #endif
            }

            // Not valid J1850 data... perhaps it's a command?
            ok = elm.process(response, cmd, input, port);
            if (elm.monitor)
                this->waitMonitor = false;

            // Special post-reset handling for ATZ
            if (cmd == "ATZ") {
                #ifdef USE_SD
                    bool sdOK = sdlog.begin(&Serial);
                    if (!sdOK) {
                        std::string fail("[SD FAIL]");
                        notify(std::make_shared<std::string>(fail));
                    }                
                #endif
            }
        }

    }

    if (!ok) {
        response = "?";
    }
    
    if (response.size() > 0) {
        port.print(response.c_str());
        port.print(elm.newline());
    }

skip:
    printNotifications();
    
    if (!elm.monitor || elm.monitor == 'B')
        prompt();
}

void CLI::printSendError(sendVPW_status_t status) const {
    switch(status) {
        case SEND_VPW_STATUS_OK:
            // NOTHING TO SEE HERE, FOLKS!
            return;
        case SEND_VPW_STATUS_CONGESTION:
            port.print("!CONGESTION");
            break;
        case SEND_VPW_STATUS_INVALID_CRC:
            port.print("!INVALID CRC");
            break;
        case SEND_VPW_STATUS_TOO_SHORT:
            port.print("!TOO SHORT");
            break;
        case SEND_VPW_STATUS_TOO_LONG:
            port.print("!TOO LONG");
            break;
        case SEND_VPW_STATUS_NO_ECHO:
            port.print("!NO ECHO");
            break;
        case SEND_VPW_STATUS_STILL_SENDING:
            port.print("!STILL SENDING");
            break;
        default:
            port.print("!UNKNOWN STATUS");
            break;
    }
    port.print(elm.newline());
}
