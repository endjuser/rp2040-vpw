#pragma once

#include "pico/lock_core.h"
#include "elm.h"
#include "j1850.h"
#include "message.h"

#ifdef USE_SD
#include "sdlog.h"
#endif

class CLI {
private:
    HardwareSerial& port;
    ELM elm;
    MessageQueue messages;
    StringQueue notifications;
    uint queueSize = 0;
    
protected:
    CLI(HardwareSerial& port, uint queueSize) : port(port), queueSize(queueSize) { }
    
    virtual bool dtr() const = 0;
    virtual void dsr(bool value) = 0;
    bool inhibitOutput = false; // pause output during input
    bool waitMonitor = true;    // buffer messages until monitor command
    void printNotifications();
    std::shared_ptr<J1850> lastSent = NULL;
    uint lastMessageTime = 0;
    uint messageCount = 0;    
    bool atPrompt = false;
    bool initialized = false;

    uint lastInputTime = 0;
    byte lastMode = 0x01;
    char lastC = 0x00;    
    
    // for discarding remaining line, if applicable
    bool monitorEnter = false;
    bool monitorText = false;

    std::string cmd = "";

        
public:
    bool ready() const;
    virtual void push(const std::shared_ptr<Message>& message);
    virtual void notify(const std::shared_ptr<std::string>& notification);
    bool begin(bool showPrompt);
    bool loop();
    void flush();
    void prompt(bool force = false);
    void process(std::string cmd);
    void printSendError(sendVPW_status_t status) const;
    bool active = false; // gets set to true after input has been received
    ELM& getElm() {
        return elm;
    }
};


// HostCLI uses hardware DTR/DSR pins
class HostCLI : public CLI {
protected:
    uint pinDTR;
    uint pinDSR;

    virtual bool dtr() const {
        return digitalReadFast(pinDTR);
    }
    virtual void dsr(bool value) {
        digitalWriteFast(pinDSR, value);
    }
public:
    HostCLI(HardwareSerial& port, uint queueSize, uint pinDTR, uint pinDSR) : CLI(port, queueSize), pinDTR(pinDTR), pinDSR(pinDSR) {
        pinMode(pinDTR, INPUT_PULLUP);
        pinMode(pinDSR, OUTPUT);
        dsr(false);
    }

#ifdef USE_SD
    /*
    virtual void push(const std::shared_ptr<Message>& message) {
        CLI::push(message);
        sdlog.queue.push(*message); //dereference to make copy for thread safety
    }
    */  
    virtual void notify(const std::shared_ptr<std::string>& notification) {
        CLI::notify(notification);
        Message m(0, VPW::getTimestamp(), {}, *notification);
        sdlog.queue.push(m);
    }
#endif
};

class AltCLI : public CLI {
protected:
    virtual bool dtr() const { return true; }    
    virtual void dsr(bool value) { }
public:
    AltCLI(HardwareSerial& port, uint queueSize) : CLI(port, queueSize) { }
};

class Terminals {
protected:
    static inline std::vector<std::reference_wrapper<CLI>> all;
public:
    template <typename... Args>
    static void add(Args&&... cliList) {
        (all.push_back(std::forward<Args>(cliList)), ...);
    }
    static void begin(bool showPrompt) {
        for (CLI& cli : all) cli.begin(showPrompt);
    }
    static void loop() {
        for (CLI& cli : all) cli.loop();
    }
    static void flush() {
        for (CLI& cli : all) cli.flush();
    }
    static void push(const std::shared_ptr<Message>& message) {
        for (CLI& cli : all) cli.push(message);
    }
    static void prompt() {
        for (CLI& cli : all) cli.prompt();
    }
    static void push(const Message& message) {
        std::shared_ptr<Message> ptr = std::make_shared<Message>(message);
        push(message);
    }
    static void notify(std::string notification) {
        std::shared_ptr<std::string> ptr = std::make_shared<std::string>(notification);
        for (CLI& cli : all) cli.notify(ptr);
    }
};
Terminals Terminals;
