#pragma once

#include <string>
#include <map>
#include "util.h"

class Automation {
private:
    recursive_mutex_t mutex;
public:
    Automation() {
        recursive_mutex_init(&mutex);        
    }

    recursive_mutex_t getMutex() const {
        return mutex; 
    }

    byte powerMode = 0x00;
    byte keyPosition = 0x00;

    bool sendPowerMode = false;
    bool sendTesterPresent = false;

    bool programmaticResponsesEnabled = false;
    std::map<std::string, std::string> programmaticResponses;

    bool sendVIN= false;
    std::string vin;
};

Automation Automation;
