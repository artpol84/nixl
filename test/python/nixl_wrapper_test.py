#!/usr/bin/env python3

from nixl_wrapper import nixl_wrapper
import nixl_utils

if __name__ == "__main__":

    buf_size = 256
    # Allocate memory and register with NIXL
    nixl_agent1 = nixl_wrapper("target", None)
    addr1 = nixl_utils.malloc_passthru(buf_size)
    addr2 = nixl_utils.malloc_passthru(buf_size)
    agent1_descs = nixl_agent1.get_descs(("DRAM",
                               [(addr1, buf_size, 0), (addr2, buf_size, 0)]))
    assert (nixl_agent1.register_memory(agent1_descs) != None)

    nixl_agent2 = nixl_wrapper("initiator", None)
    addr3 = nixl_utils.malloc_passthru(buf_size)
    addr4 = nixl_utils.malloc_passthru(buf_size)
    agent2_descs = nixl_agent2.register_memory(("DRAM",
                               [(addr3, buf_size, 0), (addr4, buf_size, 0)]))
    assert (agent2_descs != None)

    # Exchange metadata
    meta = nixl_agent1.get_agent_metadata()
    remote_name = nixl_agent2.add_remote_agent(meta)
    print ("Loaded name from metadata:", remote_name)

    ser = nixl_agent1.get_serialized_descs(agent1_descs)
    src_descs_recvd = nixl_agent2.deserialize_descs(ser)
    assert (src_descs_recvd == agent1_descs)

    xfer_handle = nixl_agent2.initialize_xfer(agent2_descs, agent1_descs,
                                              remote_name, "UUID", "READ")
    if not xfer_handle:
        print("Creating transfer failed.")
        exit()

    xfer_handle = nixl_agent2.transfer(xfer_handle)
    assert (xfer_handle != None)

    target_done = False
    init_done = False

    while (not init_done) or (not target_done):
        if not init_done:
            state = nixl_agent2.check_xfer_state(xfer_handle)
            if (state == "ERR"):
                print("Transfer got to Error state.")
                exit()
            elif (state == "DONE"):
                init_done = True
                print ("Initiator done")

        if not target_done:
            if nixl_agent1.check_remote_xfer_done("initiator", "UUID"):
                target_done = True
                print ("Target done")
            if nixl_agent2.check_remote_xfer_done("initiator", "UUID"):
                target_done = True
                print ("Target done")

    nixl_agent2.remove_remote_agent("target")
    nixl_agent2.abort_xfer(xfer_handle)
    nixl_agent1.deregister_memory(agent1_descs)
    nixl_agent2.deregister_memory(agent2_descs)

    nixl_utils.free_passthru(addr1)
    nixl_utils.free_passthru(addr2)
    nixl_utils.free_passthru(addr3)
    nixl_utils.free_passthru(addr4)

    print ("Test Complete.")
