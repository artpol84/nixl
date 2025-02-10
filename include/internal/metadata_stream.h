#ifndef __METADATA_STREAM_H
#define __METADATA_STREAM_H

#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <thread>
#include <mutex>
#include <string>
#include <queue>
#include <vector>
#include <netinet/in.h>

#define RECV_BUFFER_SIZE 16384

class nixlMetadataStream {
    protected:
        int                 port;
        int                 socketFd;
        std::string         listenerAddress;
        struct sockaddr_in  listenerAddr;

        bool setupStream();
        void closeStream();

    public:
        nixlMetadataStream(int port);
        ~nixlMetadataStream();
};


class nixlMDStreamListener: public nixlMetadataStream {
    private:
        std::thread listenerThread;

        void setupListener();
        void acceptClients();
        void recvFromClient(int clientSocket);

    public:
        nixlMDStreamListener(int port);
        ~nixlMDStreamListener();
        void startListener();
};

class nixlMDStreamClient: public nixlMetadataStream {
    private:
        std::string listenerAddress;
        bool setupClient();

    public:
        nixlMDStreamClient(const std::string& listenerAddress, int port);
        ~nixlMDStreamClient();

        bool connectListener();
        void sendData(const std::string& data);
        std::string recvData();
};
#endif
