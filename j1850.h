#pragma once

#include <vector>
#include <deque>
#include "hexutil.h"

class J1850 {
private:
    bool valid = false;

    static inline byte crcTable[0x100] = {
        //
        // for (i = 0; i < 8; i++)
        //     crc = crc & 0x80 ? (crc << 1) ^ 0x1d : crc << 1;
        //
        0x00, 0x1d, 0x3a, 0x27, 0x74, 0x69, 0x4e, 0x53,
        0xe8, 0xf5, 0xd2, 0xcf, 0x9c, 0x81, 0xa6, 0xbb,
        0xcd, 0xd0, 0xf7, 0xea, 0xb9, 0xa4, 0x83, 0x9e,
        0x25, 0x38, 0x1f, 0x02, 0x51, 0x4c, 0x6b, 0x76,
        0x87, 0x9a, 0xbd, 0xa0, 0xf3, 0xee, 0xc9, 0xd4,
        0x6f, 0x72, 0x55, 0x48, 0x1b, 0x06, 0x21, 0x3c,
        0x4a, 0x57, 0x70, 0x6d, 0x3e, 0x23, 0x04, 0x19,
        0xa2, 0xbf, 0x98, 0x85, 0xd6, 0xcb, 0xec, 0xf1,
        0x13, 0x0e, 0x29, 0x34, 0x67, 0x7a, 0x5d, 0x40,
        0xfb, 0xe6, 0xc1, 0xdc, 0x8f, 0x92, 0xb5, 0xa8,
        0xde, 0xc3, 0xe4, 0xf9, 0xaa, 0xb7, 0x90, 0x8d,
        0x36, 0x2b, 0x0c, 0x11, 0x42, 0x5f, 0x78, 0x65,
        0x94, 0x89, 0xae, 0xb3, 0xe0, 0xfd, 0xda, 0xc7,
        0x7c, 0x61, 0x46, 0x5b, 0x08, 0x15, 0x32, 0x2f,
        0x59, 0x44, 0x63, 0x7e, 0x2d, 0x30, 0x17, 0x0a,
        0xb1, 0xac, 0x8b, 0x96, 0xc5, 0xd8, 0xff, 0xe2,
        0x26, 0x3b, 0x1c, 0x01, 0x52, 0x4f, 0x68, 0x75,
        0xce, 0xd3, 0xf4, 0xe9, 0xba, 0xa7, 0x80, 0x9d,
        0xeb, 0xf6, 0xd1, 0xcc, 0x9f, 0x82, 0xa5, 0xb8,
        0x03, 0x1e, 0x39, 0x24, 0x77, 0x6a, 0x4d, 0x50,
        0xa1, 0xbc, 0x9b, 0x86, 0xd5, 0xc8, 0xef, 0xf2,
        0x49, 0x54, 0x73, 0x6e, 0x3d, 0x20, 0x07, 0x1a,
        0x6c, 0x71, 0x56, 0x4b, 0x18, 0x05, 0x22, 0x3f,
        0x84, 0x99, 0xbe, 0xa3, 0xf0, 0xed, 0xca, 0xd7,
        0x35, 0x28, 0x0f, 0x12, 0x41, 0x5c, 0x7b, 0x66,
        0xdd, 0xc0, 0xe7, 0xfa, 0xa9, 0xb4, 0x93, 0x8e,
        0xf8, 0xe5, 0xc2, 0xdf, 0x8c, 0x91, 0xb6, 0xab,
        0x10, 0x0d, 0x2a, 0x37, 0x64, 0x79, 0x5e, 0x43,
        0xb2, 0xaf, 0x88, 0x95, 0xc6, 0xdb, 0xfc, 0xe1,
        0x5a, 0x47, 0x60, 0x7d, 0x2e, 0x33, 0x14, 0x09,
        0x7f, 0x62, 0x45, 0x58, 0x0b, 0x16, 0x31, 0x2c,
        0x97, 0x8a, 0xad, 0xb0, 0xe3, 0xfe, 0xd9, 0xc4
    };

    static byte CRC(const byte* data, int length) {
        byte crc = 0xFF;
        for (int i = 0; i < length; i++)
            crc = crcTable[crc ^ data[i]];
        crc ^= 0xff;
        return crc;
    }

    static std::vector<byte> addCRC(std::vector<byte>&& bytes, bool autoCRC) {
        std::size_t size = bytes.size();
        if (autoCRC && size > 0) {
            byte crc = CRC(bytes.data(), size);
            bytes.push_back(crc);
        }
        return bytes;
    }

protected:
    const std::vector<byte> raw;
    
    virtual bool validate(bool checkCRC) /*const*/ {
        if (this->raw.size() < 5)
            return false;
        if(checkCRC && this->raw.back() != CRC((byte*)this->raw.data(), this->raw.size() - 1))
            return false;
        return true;
    }
    
public:

    J1850(const std::vector<byte>& raw) : raw(raw) {
        this->valid = this->validate(true);
    }

    J1850(const std::string& hex, bool autoCRC = true) : raw(addCRC(HexUtil.bytes(hex), autoCRC)) {
        this->valid = this->validate(!autoCRC);
    }
    
    ~J1850() {
        // free memory here as needed
    }

    bool operator == (const J1850& other) const {
        return raw == other.raw;
    }

    bool operator != (const J1850& other) const {
        return raw != other.raw;
    }

    bool isValid() const {
        return valid;
    }
    
    const std::vector<byte>& rawBytes() const {
        return raw;
    }

    const byte* rawByteArray() const {
        return raw.data();
    }

    std::vector<byte> dataBytes() const {
        int prefixSize = headerLength() + 1 /*primary address byte*/ + (isExtended() ? 1 : 0);
        int dataSize = raw.size() - prefixSize - 1 /*CRC byte*/;
        if (dataSize <= 0)
            return {};
        return {raw.begin() + prefixSize, raw.end() - 1};        
    }

    const size_t size() const {
        return raw.size();
    }

    std::string tostring(bool includeHeader, bool spaces = false, bool crc = true) const {
        int offset = includeHeader ? 0 : headerLength();
        return HexUtil.tostring(raw, offset, raw.size() - offset - (crc ? 0 : 1), spaces);
    }

    bool operator () () const {
        return valid;
    }

    const byte operator[](size_t index) const {
        if (index >= raw.size())
            return (const byte)0x00;
        return raw[index];
    }

    const size_t headerLength() const {
        return (raw[0] & 0x10) ? 1 : 3;
    }

    const byte hdr() const {
        return raw[0];
    }

    const byte target() const {
        return raw[1];
    }

    const byte source() const {
        return raw[2];
    }

    const byte secondaryAddress() const {
        return raw[3];
    }

    const byte extendedAddress() const {
        return isExtended() ? raw[4] : 0x00;
    }
    
    const uint8_t priority() const {
        return raw[0] >> 5;
    }

    const bool ifr() const {
        return (raw[0] & 8) == 0;
    }
    
    const bool isFunctional() const {
        return (raw[0] & 4) == 0;
    }

    const bool isPhysical() const {
        return (raw[0] & 4) != 0;
    }

    const bool isExtended() const {
        return ((raw[0] >> 1) & 5) == 5; // functional message, type 2 or 3, with no IFR
    }
    
    const uint8_t type() const {
        return (raw[0] & 3);
    }
};
