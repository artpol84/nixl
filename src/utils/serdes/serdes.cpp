#include "serdes.h"

nixlSerDes::nixlSerDes() {
    workingStr = "nixlSerDes|";
    des_offset = 11;

    mode = SERIALIZE;
}

std::string nixlSerDes::_bytesToString(void *buf, ssize_t size) {
    std::string ret_str = std::string(reinterpret_cast<const char*>(buf), size);
    return ret_str;
}

void nixlSerDes::_stringToBytes(void* fill_buf, std::string s, ssize_t size){
    s.copy(reinterpret_cast<char*>(fill_buf), size); 
}

/* Ser/Des for Strings */
int nixlSerDes::addStr(std::string tag, std::string str){

    size_t len = str.size();

    workingStr.append(tag);
    workingStr.append(_bytesToString(&len, sizeof(size_t)));
    workingStr.append(str);
    workingStr.append("|");

    return 0;
}

std::string nixlSerDes::getStr(std::string tag){

    if(workingStr.compare(des_offset, tag.size(), tag) != 0){
       //incorrect tag
       return "";
    } 
    ssize_t len;

    //skip tag
    des_offset += tag.size();
    
    //get len
    //_stringToBytes(&len, workingStr.data() + des_offset, sizeof(ssize_t));
    _stringToBytes(&len, workingStr.substr(des_offset, sizeof(ssize_t)), sizeof(ssize_t));
    des_offset += sizeof(ssize_t);

    //get string
    std::string ret = workingStr.substr(des_offset, len);
    
    //move past string plus | delimiter
    des_offset += len + 1;

    return ret;
}

/* Ser/Des for Byte buffers */
int nixlSerDes::addBuf(std::string tag, void* buf, ssize_t len){

    workingStr.append(tag);
    workingStr.append(_bytesToString(&len, sizeof(ssize_t)));
    workingStr.append(_bytesToString(buf, len));
    workingStr.append("|");

    return 0;
}

ssize_t nixlSerDes::getBufLen(std::string tag){
    if(workingStr.compare(des_offset, tag.size(), tag) != 0){
       //incorrect tag
       return -1;
    } 

    ssize_t len;

    //get len
    //_stringToBytes(&len, workingStr.data() + des_offset + tag.size(), sizeof(ssize_t));
    _stringToBytes(&len, workingStr.substr(des_offset + tag.size(), sizeof(ssize_t)), sizeof(ssize_t));

    return len;
}

int nixlSerDes::getBuf(std::string tag, void *buf, ssize_t len){
    if(workingStr.compare(des_offset, tag.size(), tag) != 0){
       //incorrect tag
       return -1;
    }
    
    //skip over tag and size, which we assume has been read previously
    des_offset += tag.size() + sizeof(ssize_t);

    //_stringToBytes(buf, workingStr.data() + des_offset, len);
    _stringToBytes(buf, workingStr.substr(des_offset, len), len);

    //bytes in string form are twice as long, skip those plus | delimiter
    des_offset += len + 1;

    return 0;
}

/* Ser/Des buffer management */
std::string nixlSerDes::exportStr() {
	std::string ret_str = workingStr;
    return ret_str;
}

int nixlSerDes::importStr(std::string sdbuf) {
 
    if(sdbuf.compare(0, 11, "nixlSerDes|") != 0){
       //incorrect tag
       return -1;
    }    

    workingStr = sdbuf;
    mode = DESERIALIZE;
    des_offset = 11;

    return 0;
}
