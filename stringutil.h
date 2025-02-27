#pragma once

#include <list>
#include <sstream>
#include <string>

struct StringUtil {
    
    std::list<std::string> split(const std::string& input, const char delimiter) {
        std::list<std::string> result;
        std::stringstream ss(input);
        std::string token;    
        while (std::getline(ss, token, delimiter))
            result.push_back(token);
        return result;
    }

    std::string join(const std::list<std::string>& input, const char delimiter) {
        std::stringstream ss;
        bool first = true;
        for (const std::string& element : input) {
            if (!first)
                ss << ",";
            else
                first = false;
            ss << element;
        }    
        return ss.str();    
    }
};

StringUtil StringUtil;
