#pragma once
#ifdef USE_SD

#include "pins.h"
#include "pixel.h"
#include "blinkenlights.h"
#include "message.h"
#include "rtc.h"
#include <SPI.h>
#include <SD.h>
#include <string>

#define APPEND    (O_CREAT | O_WRITE | O_APPEND)
#define OVERWRITE (O_CREAT | O_WRITE | O_TRUNC)

extern uint fadeSave;
extern short ledSave;

void fatDateTime(uint16_t* date, uint16_t* time) {
 DateTime now = rtc.now();
 *date = FAT_DATE(now.year(), now.month(), now.day());
 *time = FAT_TIME(now.hour(), now.minute(), now.second());
}

class SDLog {
private:
  static inline bool mutexInitialized = false;
  static inline recursive_mutex_t mutex;
  static inline std::deque<char> buffer;
  static inline int initialCHUNK_SIZE = 0;
  static inline HardwareSerial* debug = nullptr;
  
public:

  static inline MessageQueue queue;
  static inline ulong messageCount = 0;
  
  static std::string cardInfo();

  static inline bool ready = false;
  static inline int index = -1;
  static inline fs::File file = fs::File();
  static inline bool empty = true;
  
  static bool begin(HardwareSerial* port);
  static void increment();
  static void open(bool reopen = false);
  static void flush();
  static void close();
  static bool write(const char* data, bool flush = false);
  static bool write(const std::string& data, bool flush = false);
  static void print(size_t index, Stream& stream, const char* newline);
  static ulong bytesFree();
  
  static std::string info();
  static const char* cardType();

  static bool dirty() {
    return buffer.size() > 0;
  }

  static void clearBuffer() {
    recursive_lock_guard lock(mutex);
    buffer.clear();
  }
  
  SDLog& operator << (const std::string& text) {
    write(text, false);
    return *this;
  }
  
  SDLog() {
    if (!mutexInitialized) {
      recursive_mutex_init(&mutex);
      mutexInitialized = true;
    }
  }

  static recursive_mutex_t getMutex() {
    return mutex;        
  }

};

bool SDLog::begin(HardwareSerial* port) {
  recursive_lock_guard lock(mutex);
  ready = false;
  index = -1;
  empty = true;
  buffer.clear();
  
  debug = port;
  
  SPI.setRX(PIN_SD_MISO);
  SPI.setTX(PIN_SD_MOSI);
  SPI.setCS(PIN_SD_CS);
  SPI.setSCK(PIN_SD_SCK);

  SdFile::dateTimeCallback(fatDateTime);
  
  if (debug) debug->print("Initializing SD");

  if (!SD.begin(PIN_SD_CS)) {
    if (debug) debug->println("... FAIL");
    return false;
  }
  if (debug) debug->print("...");

  fs::File dir = SD.open("/");
  while(true) {
    fs::File entry = dir.openNextFile();
    if (!entry) {
        dir.rewindDirectory();
        break;
    }
    const char* filename = entry.name();
    const char* ext = strrchr(filename, '.');
    if (ext && strcmp(ext, ".log") == 0) {
        int number = 0;
        const char* ptr = filename;
        while (ptr < ext) {
            if (!isdigit(*ptr)) {
                number = 0;
                break;
            }
            number = number * 10 + (*ptr - '0');
            ptr++;
        }
        if (number > index)
            index = number;
    }
    entry.close();
  }
  index++;
  
  open(true);
  if (file && file.size() > 0)
    increment();
  close();
  if (debug) debug->println((std::string(" OK (LOG #") + std::to_string(index) + ")").c_str());
  
  ready = true;
  return true;
}

const char* SDLog::cardType() {
  recursive_lock_guard lock(mutex);
  if (!ready)
    return "N/A";
  switch (SD.type()) 
  {
    case SD_CARD_TYPE_SD1:
      return "SD1";
    case SD_CARD_TYPE_SD2:
      return "SD2";
    case SD_CARD_TYPE_SDHC:
      return "SDHC";
    default:
      return "Unknown";
  }    
}

void SDLog::print(size_t index, Stream& stream, const char* newline) {
  recursive_lock_guard lock(mutex);
  close();
  std::string filename = std::to_string(index) + ".log";
  fs::File fileIndex = SD.open(filename.c_str(), FILE_READ);
  if (fileIndex) {
    stream.print("--BEGIN ");
    stream.print(filename.c_str());
    stream.print("--");
    stream.print(newline);
    while (fileIndex.available()) 
    {
      String line = fileIndex.readStringUntil('\n');
      stream.print(line);
      stream.print(newline);
    }
    fileIndex.close();
    stream.print("--END ");
    stream.print(filename.c_str());
    stream.print("--");
    stream.print(newline);
  } else {
    stream.print("!ERROR OPENING ");
    stream.print(filename.c_str());
    stream.print(newline);
  }
  open(true);
}

std::string SDLog::cardInfo() {
    recursive_lock_guard lock(mutex);
    if (!ready)
        return "N/A";
    std::string ret;
    ret.reserve(64);
    ret += cardType();
    ret += "/FAT";
    ret += std::to_string(SD.fatType());
    ret += "@";
    ret += Util.dec(((float)SD.size()/1024.0/1024.0/1024.0), 0, 2);
    ret += "GB";

    FSInfo fs_info;
    SDFS.info(fs_info);
    float pctFree = fs_info.usedBytes / fs_info.totalBytes * 100;
    ret += "[";
    ret += Util.dec(pctFree, 0, 2);
    ret += "%]";

    return ret;
}

void SDLog::increment() {
  recursive_lock_guard lock(mutex);
  close();

  index++;
  messageCount = 0;
}

void SDLog::open(bool reopen) {
  recursive_lock_guard lock(mutex);

  if (!reopen && file && (!empty || file.size() > 0))
    increment();

  setTimeFromRTC();

  std::string filename = std::to_string(index) + ".log";
  file = SD.open(filename.c_str(), (reopen ? APPEND : OVERWRITE));
  if (!file) {
    ready = false;
    if (debug) debug->println("[OPEN FAIL]");
    return;    
  }
  empty = true;
}

void SDLog::flush() {
  recursive_lock_guard lock(mutex);
  write("", true);
  file.flush();
}

void SDLog::close() {
  recursive_lock_guard lock(mutex);
  if (file) {
    flush();
    file.close();
    file = fs::File();
  }
}

bool SDLog::write(const char* data, bool flush) {
  return write(std::string(data), flush);
}

bool SDLog::write(const std::string& data, bool flush) {
  recursive_lock_guard lock(mutex);
  
  if (!file) {
    //if (debug) debug->println("[OPEN]");
    open();
    if (!file)
      return false;
  }

  if (data.size() > 0)
    empty = false;

  for (char c : data)
    buffer.push_back(c);

  size_t bufferSize = buffer.size();
  size_t chunkSize = 4096;
  
  if (flush == true)
    chunkSize = bufferSize;
  
  if (chunkSize > 0 && (bufferSize >= chunkSize || flush == true)) {
    ledSave = MAX_INTENSITY;
    fadeSave = millis();
    char toWrite[chunkSize];
    std::copy(buffer.begin(), buffer.begin() + chunkSize, toWrite);
    buffer.erase(buffer.begin(), buffer.begin() + chunkSize);
    size_t written = file.write(toWrite, chunkSize);
    if (flush)
        file.flush();
    if (written != chunkSize) {
        ready = false;
        if (debug) debug->println("[SD WRITE FAIL]");
        return false;
    }
  }
  
  return true;
}

SDLog sdlog;

#endif
