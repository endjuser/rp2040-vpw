#pragma once

#include "j1850.h"
#include "vpw.h"
#include "util.h"

#define MessagePtr std::shared_ptr<Message>

class Message : public J1850 {
public:
    struct timeval timestamp;
    byte mode = 0; // 0 = unspecified; 1 = 1X, 4 = 4X
    std::string information = "";
    
    Message(struct timeval timestamp, const std::vector<byte>& raw, const std::string& information = "") : J1850(raw), timestamp(timestamp), information(information) { }

    Message(const std::string& hex) : J1850(hex, false), timestamp(VPW::getTimestamp()) { }

    Message(const std::string& hex, bool autoCRC, const std::string& information = "") : J1850(hex, autoCRC), timestamp(VPW::getTimestamp()), information(information) { }
    
    Message(byte mode, struct timeval timestamp, const std::vector<byte>& raw, const std::string& information = "") : Message(timestamp, raw, information) {
        this->mode = mode;
    }
    
    Message() : J1850("") { }
    
    std::string tostring(
        struct timeval timestampOffset,
        bool showTimestamp = true,
        bool headers = true,
        bool spaces = false,
        bool allowLong = true,
        size_t tsZeroes = 4,
        bool showVpwMode = false
    ) const {
        std::string ret;
        if (showTimestamp) {
            struct timeval tv = timestamp;
            tv.tv_sec -= timestampOffset.tv_sec;
            if (timestampOffset.tv_usec != 0)
                tv.tv_usec = (1000000 + tv.tv_usec - timestampOffset.tv_usec) % 1000000;
            ret += Util.dec((ulong)tv.tv_sec, tsZeroes) + '.' + Util.dec((ulong)tv.tv_usec, 6);
            ret += '\t';
        }
        
        std::string data = J1850::tostring(headers, spaces);
        if (!allowLong) {
            size_t maxSize = (headers ? 12 : 8) * (spaces ? 3 : 2) - (spaces ? 1 : 0);
            if (data.size() > maxSize)
                data = data.substr(0, maxSize) + " <DATA ERROR";
        }                
        if (showVpwMode && data.size() > 0)
            ret += (mode == 4 ? "[4X] " : mode == 1 ? "[1X] " : "[--] ");
        ret += data;
        
        if (this->information.size() > 0) {
            if (this->size() > 0)
                ret += '\t';
            ret += this->information;
        }
        return ret;
    }

    std::string tostring(bool showTimestamp = true, bool headers = true, bool spaces = false, bool allowLong = true) const {
        return tostring({0, 0}, showTimestamp, headers, spaces, allowLong);
    }

};

template <class T>
class QueueOf : public std::deque<std::shared_ptr<T>> {
private:
    recursive_mutex_t mutex;
public:
    static inline std::shared_ptr<T> empty = std::make_shared<T>(T());

    QueueOf() {
        recursive_mutex_init(&mutex);        
    }

    recursive_mutex_t getMutex() {
        return mutex;        
    }
    
    void push(const T& value) {
        recursive_lock_guard lock(mutex);
        this->push_back(std::make_shared<T>(value));
    }
    void push(const std::shared_ptr<T>& value) {
        recursive_lock_guard lock(mutex);
        this->push_back(value);
    }
    std::shared_ptr<T> pull() {
        recursive_lock_guard lock(mutex);
        if (this->size() == 0) {
            return empty;
        }
        std::shared_ptr<T> ptr = this->front();
        this->pop_front();
        return ptr;
    }
    bool available() {
        recursive_lock_guard lock(mutex);
        return this->size() > 0;
    }
};

class StringQueue : public QueueOf<std::string> { };

class MessageQueue : public QueueOf<Message> { };

class VPWMessageQueue : public MessageQueue {
private:
    std::vector<byte> buffer;
    byte mode = 1;
    timeval tv;
    
public:

    VPWMessageQueue() {
        buffer.reserve(0x100);
    }
    
    void process() {
        bool proceed = true;
        byte b;
                
        while (vpw.available() && proceed) {
            b = vpw.pop();
            
            if (b == W_WILDCARD) {
                b = vpw.pop();
            
                switch (b) {
                case W_TIMESTAMP:
                    tv = vpw.popTimestamp();
                    break;
                case W_WILDCARD:
                    buffer.emplace_back((byte)W_WILDCARD);
                    break;
                case W_SOF:
                    buffer.clear();
                    break;
                case W_EOD:
                    // This only applies to J1850 PWM, which has IFR
                    break;
                case W_EOF:
                    {
                        Message message(mode, tv, buffer);
                        this->push(message);
                        if (message.isPhysical() && message.target() == 0xFE) {
                            // Special case for command to enter 4X mode
                            if (message.secondaryAddress() == 0xA1)
                                VPW::SEND_4X = true;
                            // Special case for command to return to normal
                            if (message.secondaryAddress() == 0x20)
                                VPW::SEND_4X = false;
                        }
                        break;
                    }
                case W_EOT:
                    proceed = vpw.idle(); // keep going if there are no unprocessed raw inputs
                    break;
                case W_BRK:
                    {
                        Message m(mode, VPW::getTimestamp(), {}, "[BREAK]");
                        this->push(m);
                        buffer.clear();
                        VPW::SEND_4X = false;
                        break;
                    }
                case W_ERROR_UNEXPECTED_SOF:
                    break;
                case W_ERROR_UNEXPECTED_EOF:
                    break;
                case W_HIGH:
                    {
                        Message m(mode, VPW::getTimestamp(), {}, "[BUS ERROR]");
                        this->push(m);
                        break;
                    }
                case W_RUNT:
                    break;
                case W_MODE_1X:
                    mode = 1;
                    break;
                case W_MODE_4X:
                    mode = 4;
                    break;
                case W_DEBUG_STRING:
                    {
                        b = vpw.pop();
                        std::string s;
                        s.reserve(b + 2);
                        s += "{";
                        for (int i = 0; i < b; i++)
                            s += (char) vpw.pop();
                        s += "}";
                        Message m(mode, VPW::getTimestamp(), {}, s);
                        this->push(m);
                        break;
                    }
                case W_DEBUG:
                    {
                        b = vpw.pop();
                        std::string s;
                        s.reserve(5);
                        s += "{";
                        s += Util.dec(b, 3);
                        s += "}";
                        Message m(mode, VPW::getTimestamp(), {}, s);
                        this->push(m);
                        break;
                    }
                default:
                    // UNKNOWN ENCODED BYTE ???
                    break;
                }
            } else {
                buffer.emplace_back(b);
            }
        }        
    }
};

VPWMessageQueue VPWMessageQueue;
