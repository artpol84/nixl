#include <iostream>
#include <string>
#include <algorithm>
#include <nixl_descriptors.h>
#include <nixl_params.h>
#include <nixl.h>
#include <cassert>
#include "internal/metadata_stream.h"
#define NUM_TRANSFERS 1
#define SIZE 1024


/**
 * This test does p2p from using PUT.
 * intitator -> target so the metadata and
 * desc list needs to move from
 * target to initiator
 */

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    size_t start = 0, end = 0;

    while ((end = str.find(delimiter, start)) != std::string::npos) {
        token = str.substr(start, end - start);
        tokens.push_back(token);
        start = end + 1;
    }
    // Add the last token
    token = str.substr(start);
    tokens.push_back(token);
    return tokens;
}

bool allBytesAre(void* buffer, size_t size, uint8_t value) {
    uint8_t* byte_buffer = static_cast<uint8_t*>(buffer); // Cast void* to uint8_t*
    // Iterate over each byte in the buffer
    for (size_t i = 0; i < size; ++i) {
        if (byte_buffer[i] != value) {
            return false; // Return false if any byte doesn't match the value
        }
    }
    return true; // All bytes match the value
}

std::string recvFromTarget(int port) {
    nixlMDStreamListener listener(port);
    listener.startListenerForClient();
    return listener.recvFromClient();
}

void sendToInitiator(const char *ip, int port, std::string data) {
    nixlMDStreamClient client(ip, port);
    client.connectListener();
    client.sendData(data);
}

int main(int argc, char *argv[]) {
    int                     initiator_port;
    nixl_status_t           ret = NIXL_SUCCESS;
    void                    *addr[NUM_TRANSFERS];
    std::string             role;
    const char              *initiator_ip;
    std::string             str_desc;
    std::string             remote_desc;
    std::string             tgt_metadata;
    std::string             tgt_md_init;
    int                     status = 0;
    bool                    rc = false;

    /** NIXL declarations */
    /** Agent and backend creation parameters */
    nixlAgentConfig             cfg(true);
    nixlUcxInitParams           params;
    nixlBasicDesc               buf[NUM_TRANSFERS];
    nixlBackendEngine           *ucx;

    /** Serialization/Deserialization object to create a blob */
    nixlSerDes                  *serdes;
    nixlSerDes                  *remote_serdes;

    /** Descriptors and Transfer Request */
    nixlDescList<nixlBasicDesc> dram_for_ucx(DRAM_SEG);
    nixlXferReqH                *treq;

    /** Argument Parsing */
    if (argc < 4) {
        std::cout <<"Enter the required arguments\n" << std::endl;
        std::cout <<"<Role> " <<"Initiator IP> <Initiator Port>"
                  << std::endl;
        exit(-1);
    }

    role = std::string(argv[1]);
    initiator_ip   = argv[2];
    initiator_port = std::stoi(argv[3]);
    std::transform(role.begin(), role.end(), role.begin(), ::tolower);

    if (!role.compare("initiator") && !role.compare("target")) {
            std::cerr << "Invalid role. Use 'initiator' or 'target'."
                      << "Currently "<< role <<std::endl;
            return 1;
    }
    /*** End - Argument Parsing */

    /** Common to both Initiator and Target */
    std::cout << "Starting Agent for "<< role << "\n";
    nixlAgent     agent(role, cfg);
    ucx         = agent.createBackend(&params);
    serdes      = new nixlSerDes();

    for (int i = 0; i < NUM_TRANSFERS; i++) {
        addr[i] = calloc(1, SIZE);
        if (role != "target") {
            memset(addr[i], 0xbb, SIZE);
            std::cout << "Allocating for initiator : "
                      << addr[i] << ", "
                      << "Setting to 0xbb "
                      << std::endl;
        } else {
            memset(addr[i], 0, SIZE);
            std::cout << "Allocating for target : "
                      << addr[i] << ", "
                      << "Setting to 0 " << std::endl;
        }
        buf[i].addr  = (uintptr_t)(addr[i]);
        buf[i].len   = SIZE;
        buf[i].devId = 0;
        dram_for_ucx.addDesc(buf[i]);
    }

    /** Register memory in both initiator and target */
    agent.registerMem(dram_for_ucx, ucx);
    if (role == "target") {
        tgt_metadata = agent.getLocalMD();
    }

    std::cout << " Start Control Path metadata exchanges \n";
    if (role == "target") {
        std::cout << " Desc List from Target to Initiator\n";
        dram_for_ucx.print();

        /** Serialize for MD transfer */
        assert(dram_for_ucx.serialize(serdes) == NIXL_SUCCESS);

        /** Sending both metadata strings together */
        str_desc                    = serdes->exportStr();
        std::string sstr            = tgt_metadata + ";" + str_desc;

        std::cout << " Serialize Metadata to string and Send to Initiator\n";
        std::cout << " \t -- To be handled by runtime - currently sent via a TCP Stream\n";
        sendToInitiator(initiator_ip, initiator_port, sstr);
        std::cout << " End Control Path metadata exchanges \n";

        std::cout << " Start Data Path Exchanges \n";
        std::cout << " Waiting to receive Data from Initiator\n";

        while (!rc) {
            ucx->progress();
            /** Sanity Check */
            for (int i = 0; i < NUM_TRANSFERS; i++) {
                rc = allBytesAre(addr[i], SIZE, 0xbb);
                if (!rc)
                    break;
            }
        }
        if (!rc)
            std::cerr << " UCX Transfer failed, buffers are different\n";
        else
            std::cout << " Transfer completed and Buffers match with Initiator\n"
                      <<"  UCX Transfer Success!!!\n";

    } else {

        std::cout << " Recieve metadaat from Target \n";
        std::cout << " \t -- To be handled by runtime - currently received via a TCP Stream\n";
        std::string rrstr = recvFromTarget(initiator_port);

        std::vector<std::string> tokens = split(rrstr, ';');
        tgt_md_init = tokens[0];
        remote_desc = tokens[1];

        std::cout << " Verfiy Deserialized Target's Desc List at Initiator\n";
        remote_serdes = new nixlSerDes();
        remote_serdes->importStr(remote_desc);
        nixlDescList<nixlBasicDesc> dram_target_ucx(remote_serdes);
        dram_target_ucx.print();
        agent.loadRemoteMD(tgt_md_init);

        std::cout << " End Control Path metadata exchanges \n";
        std::cout << " Start Data Path Exchanges \n\n";
        std::cout << " Create transfer request with UCX backend\n ";

        ret = agent.createXferReq(dram_for_ucx, dram_target_ucx,
                                  "target", "", NIXL_WRITE, treq);
        if (ret != NIXL_SUCCESS) {
            std::cerr << "Error creating transfer request\n";
            exit(-1);
        }

        std::cout << " Post the request with UCX backend\n ";
        status = agent.postXferReq(treq);
        std::cout << " Initiator posted Data Path transfer\n";
        std::cout << " Waiting for completion\n";

        while (status != NIXL_XFER_DONE) {
            status = agent.getXferStatus(treq);
            assert(status != NIXL_XFER_ERR);
        }
        std::cout << " Completed Sending Data using UCX backend\n";
        agent.invalidateXferReq(treq);
    }

    std::cout <<"Cleanup.. \n";
    agent.deregisterMem(dram_for_ucx, ucx);
    for (int i = 0; i < NUM_TRANSFERS; i++) {
        free(addr[i]);
    }
    return 0;
}
