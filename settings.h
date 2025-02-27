#pragma once

#include <LittleFS.h>

#include <string>
#include <functional>
#include <cstdarg>
#include <map>

#ifndef SETTINGS_PATH
#define SETTINGS_PATH "/settings"
#endif

const char* SETTINGS_PATH_STR = SETTINGS_PATH;

class ISetting {
private:
    const char* name;

protected:
    ISetting(const char* name) : name(name) {}
    
public:    
    virtual bool load() = 0;
    virtual bool save() const = 0;

    const char* getName() const { return name; }
};

class SettingsRepository {
private:
    static inline bool ready;

protected:
    static void initialize() {
        if (!ready)
            ready = LittleFS.begin();
        if (ready) {
            LittleFS.mkdir(SETTINGS_PATH_STR);
        }
    }

    int fileCallback(const char* mode, const char* name, std::function<int(File)> callback) const {
        if (!ready)
            initialize();
        if (ready) {
            File f = open(name, mode);
            if (f) {
                int ret = callback(f);
                f.close();
                return ret;
            }
        }
        return -1;
    }

    int readCallback(const char* name, std::function<int(File)> callback) const {
        return fileCallback("r", name, callback);
    }

    int writeCallback(const char* name, std::function<int(File)> callback) const {
        return fileCallback("w", name, callback);
    }

public:
    bool begin() const {
        initialize();
        return ready;    
    }
    
    template <typename T>
    int write(const char* name, const T& value) const {
        return writeCallback(name, [&](File f) {
            return f.print(value);
        });
    }

    File open(const char* name, const char *mode) const {
        if (!ready)
            initialize();
        if (!ready)
            return File();
        std::string fileName(SETTINGS_PATH_STR);
        fileName.append("/").append(name);
        return LittleFS.open(fileName.c_str(), mode);
    }
   
    int read(const char* name, std::string& buffer) const {
        return readCallback(name, [&](File f) {
            int length = f.size();
            char data[length + 1];
            f.readBytes(data, length);
            data[length] = 0;
            buffer.append(data);
            f.close();
            return length;
        });
    }

    int read(const char* name, String& buffer) const {
        return readCallback(name, [&](File f) {
            String value = f.readString();
            buffer += value;
            return value.length();
        });
    }

    int read(const char* name, byte& value) const {
        return readCallback(name, [&](File f) {
            value = (byte)f.read();
            return sizeof(value);
        });
    }

    int read(const char* name, char& value) const {
        return read(name, (byte&)value);
    }

    int read(const char* name, bool& value) const {
        byte b = 0;
        int ret = read(name, b);
        value = (b == '1');
        return ret;
    }

    int read(const char* name, int& value) const {
        return readCallback(name, [&](File f) {
            f.setTimeout(10);
            value = f.parseInt();
            return sizeof(value);
        });
    }

    int read(const char* name, float& value) const {
        return readCallback(name, [&](File f) {
            f.setTimeout(10);
            value = f.parseFloat();
            return sizeof(value);
        });
    }

    template <typename T>
    T read(const char* name) const {
        T value;
        read(name, value);
        return value;
    }


    template <typename... Args>
    bool load(Args&&... args) {
        bool ok = true;
        (void)std::initializer_list<int>{(ok == ok && args.load(), 0)...};
        return ok;
    }

    template <typename... Args>
    bool save(Args&&... args) {
        bool ok = true;
        (void)std::initializer_list<int>{(ok == ok && args.load(), 0)...};
        return ok;
    }
    
};

template <>
int SettingsRepository::write<std::string>(const char* name, const std::string& value) const {
    return writeCallback(name, [&](File f) {
        return f.print(value.c_str());
    });
}

SettingsRepository SettingsRepository;


template <typename T>
class Setting : public ISetting {
    T value;

private:    
    Setting() {}
    
public:
    Setting(const char* name, const T& value) : ISetting(name), value(value) {}
    
    Setting(const char* name) : ISetting(name), value(load()) {}

    operator T () const { 
        return value;
    }
    Setting& operator = (const T& value) {
        this->value = value;
        return *this;
    }
    Setting& operator += (const T& value) {
        this->value += value;
        return *this;
    }
    Setting& operator -= (const T& value) {
        this->value += value;
        return *this;
    }
    Setting& operator *= (const T& value) {
        this->value *= value;
        return *this;
    }
    Setting& operator /= (const T& value) {
        this->value /= value;
        return *this;
    }
    Setting& operator %= (const T& value) {
        this->value %= value;
        return *this;
    }

    Setting& operator ++ () {
        this->value ++;
        return *this;
    }
    Setting operator ++ (T) {
        Setting copy = *this;
        this->value++;
        return copy;
    }
    Setting& operator -- () {
        this->value --;
        return *this;
    }
    Setting operator -- (T) {
        Setting copy = *this;
        this->value--;
        return copy;
    }

    int write() const {
        return SettingsRepository.write(getName(), value);
    }
    
    int read() {
        return SettingsRepository.read(getName(), value);
    }

    bool load() {
        return (read() != -1);
    }

    bool save() const {
        return (write() != -1);
    }

    bool save(const T& value) {
        this->value = value;
        return save();
    }
};

#define SETTING(type, name, value) Setting<type> name = Setting<type>(#name, value)

/*
class CatSettings : public SettingsRepository {
public:
    Setting<int> meow = Setting<int>("meow", 0);
    SETTING(int, purr, 0);

    bool begin(bool loadAll) {
        SettingsRepository::begin();
        return loadAll ? SettingsRepository::load(meow, purr) : true;
    }
    
    bool save() {
        return SettingsRepository::save(meow, purr);
    }
};

CatSettings CatSettings;
*/
