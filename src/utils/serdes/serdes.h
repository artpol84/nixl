#ifndef __SERDES_H
#define __SERDES_H

#include <cstring>
#include <string>
#include <cstdint>

class nixlSerDes {
private:
    typedef enum { SERIALIZE, DESERIALIZE } ser_mode_t;

    std::string workingStr;
    ssize_t des_offset;
    ser_mode_t mode;

public:
    nixlSerDes();
    
    /* Ser/Des for Strings */
    int addStr(std::string tag, std::string str);
    std::string getStr(std::string tag);

    /* Ser/Des for Byte buffers */
    int addBuf(std::string tag, void* buf, ssize_t len);
    ssize_t getBufLen(std::string tag);
    int getBuf(std::string tag, void *buf, ssize_t len);

    /* Ser/Des buffer management */
    std::string exportStr();
    int importStr(std::string sdbuf);

    static std::string _bytesToString(void *buf, ssize_t size);
    static void _stringToBytes(void* fill_buf, std::string s, ssize_t size);
};

#endif
