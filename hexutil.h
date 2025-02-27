#pragma once

#include <vector>
#include <span>

struct HexUtil {

    const char* hexDigits = "0123456789ABCDEF";

    template <typename I>
    /* specify hexLen = -1 for auto (based on variable type), or 0 to not pad */
    std::string hex(const I& w, size_t hexLen = -1) {
        bool trim = (hexLen == 0);
        if (hexLen == 0 || hexLen == -1)
            hexLen = sizeof(I) << 1;
        std::string ret(hexLen, '0');
        for (size_t i = 0, j = (hexLen - 1) * 4; i < hexLen; ++i, j -= 4)
            ret[i] = hexDigits[(w >> j) & 0x0F];
        if (trim)
            ret.erase(0, std::min(ret.find_first_not_of('0'), ret.size() -1 ));
        return ret;
    }

    byte getByte(const std::string_view& hex, uint index = 0, byte defaultValue = 0x00) {
        auto data = bytes(hex, 1);
        return (data.size() <= index) ? defaultValue : data[index];
    }
    
    std::vector<byte> bytes(const std::string_view& hex, uint byteCount = 0, uint offset = 0) {
        // NOTE: THIS FUNCTION ASSUMES hex HAS NO SPACES
        
        std::vector<byte> ret;
        
        uint hLen = hex.size();
        if (offset < 0 || offset >= hLen)
            return ret;

        bool odd = (hLen % 2 != 0);
                    
        if (byteCount <= 0)
            byteCount = hLen / 2 + (odd ? 1 : 0);

        ret.reserve(byteCount);
        
        char buffer[3];
        buffer[2] = 0;
        for (int i = offset; i < byteCount * 2; i += 2) {
            if (odd && i == offset) {
                buffer[0] = '0';
                buffer[1] = hex[i];
                i--;
            } else if (i >= hLen) {
                break;
            } else {
                buffer[0] = hex[i];
                buffer[1] = hex[i + 1];
            }
            byte b = (byte) strtol(buffer, NULL, 16);
            if (b == 0x00 && (buffer[0] != '0' || buffer[1] != '0')) {
                // Invalid input
                ret.clear();
                break;
            }
            ret.push_back(b);
        }

        return ret;
    }

    std::string tostring(const byte* input, size_t size, bool spaces = false) {
        uint inputSize = size;
        uint length = inputSize * 2;
        if (spaces)
            length += (inputSize - 1);
        std::string ret;
        //if (inputSize > 1024)
        //    return ret;
        ret.resize(length);
        byte b;
        uint buf = 0;
        for (uint c = 0; c < inputSize; c++) {
            b = input[c];
            ret[buf++] = hexDigits[b >> 4];
            ret[buf++] = hexDigits[b & 0x0F];
            if (spaces && c < inputSize - 1) {
                ret[buf++] = ' ';
            }
        }
        return ret;
    }
    
    std::string tostring(const std::vector<byte>& input, int offset, int length = INT_MAX, bool spaces = false) {
        if (length <= 0 || offset >= input.size())
            return "";
        return tostring(input.data() + offset, min(length, input.size() - offset), spaces);
    }

    std::string tostring(const std::vector<byte>& input, bool spaces = false) {
        return tostring(input, 0, input.size(), spaces);
    }
};

HexUtil HexUtil;
