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
    int addStr(const std::string &tag, const std::string &str);
    std::string getStr(const std::string &tag);

    /* Ser/Des for Byte buffers */
    int addBuf(const std::string &tag, const void* buf, ssize_t len);
    ssize_t getBufLen(const std::string &tag) const;
    int getBuf(const std::string &tag, void *buf, ssize_t len);

    /* Ser/Des buffer management */
    std::string exportStr() const;
    int importStr(const std::string &sdbuf);

    static std::string _bytesToString(const void *buf, ssize_t size);
    static void _stringToBytes(void* fill_buf, const std::string &s, ssize_t size);
};

#endif
