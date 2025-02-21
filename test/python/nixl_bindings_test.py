import nixl_bindings as nixl
import nixl_utils
import pickle

def test_desc():

    test_desc1 = nixl.nixlBasicDesc(1000, 105, 0)
    test_desc1b = nixl.nixlBasicDesc(1000, 105, 0)
    test_desc2 = nixl.nixlBasicDesc(1010, 30, 0)
    test_desc3 = nixl.nixlBasicDesc(990, 20, 0)

    print(test_desc1)

    test_desc1.print("desc1")
    test_desc2.print("desc2")
    test_desc3.print("desc3")

    assert test_desc1.covers(test_desc2)
    assert test_desc1.overlaps(test_desc3)

    assert test_desc1 == test_desc1b
    assert test_desc1 != test_desc2

def test_list():

    test_desc1 = nixl.nixlBasicDesc(1000, 105, 0)
    test_desc2 = nixl.nixlBasicDesc(2000, 30, 0)
    test_desc3 = nixl.nixlBasicDesc(3000, 20, 0)

    test_list = nixl.nixlDescList(nixl.DRAM_SEG, True, False)

    test_list.addDesc(test_desc1)
    test_list.addDesc(test_desc2)
    test_list.addDesc(test_desc3)

    test_list.print()

    pickled_list = pickle.dumps(test_list)

    print(pickled_list)

    unpickled_list = pickle.loads(pickled_list)

    assert unpickled_list == test_list

    assert test_list.getType() == nixl.DRAM_SEG
    assert test_list.isUnifiedAddr()
    
    print(test_list.descCount())
    assert test_list.descCount() == 3
   
    test_list.remDesc(1)
    assert test_list.descCount() == 2

    assert test_list[0] == test_desc1

    test_list.clear()

    assert test_list.isEmpty()

def test_agent():

    name1 = "Agent1"
    name2 = "Agent2"

    devices = nixl.nixlDeviceMD()
    init1 = nixl.nixlUcxInitParams()
    init2 = nixl.nixlUcxInitParams()

    agent1 = nixl.nixlAgent(name1, devices)
    agent2 = nixl.nixlAgent(name2, devices)

    ucx1 = agent1.createBackend(init1)
    ucx2 = agent2.createBackend(init2)

    size = 256
    addr1 = nixl_utils.malloc_passthru(size)
    addr2 = nixl_utils.malloc_passthru(size)

    buff1 = nixl.nixlBasicDesc(addr1, size, 0)
    buff2 = nixl.nixlBasicDesc(addr2, size, 0)

    print(buff1.m_addr)
    print(buff2.m_addr)

    nixl_utils.ba_buf(addr1, size)

    reg_list1 = nixl.nixlDescList(nixl.DRAM_SEG, True, False)
    reg_list1.addDesc(buff1)

    reg_list2 = nixl.nixlDescList(nixl.DRAM_SEG, True, False)
    reg_list2.addDesc(buff2)

    ret = agent1.registerMem(reg_list1, ucx1)
    assert ret == 0

    ret = agent2.registerMem(reg_list2, ucx2)
    assert ret == 0

    meta1 = agent1.getLocalMD()
    meta2 = agent2.getLocalMD()

    print("Agent1 MD: ")
    print(meta1)
    print("Agent2 MD: ")
    print(meta2)

    ret_name = agent1.loadRemoteMD(meta2)
    assert ret_name == name2
    ret_name = agent2.loadRemoteMD(meta1)
    assert ret_name == name1

    offset = 8
    req_size = 8

    src_buff = nixl.nixlBasicDesc(addr1 + offset, req_size, 0)
    dst_buff = nixl.nixlBasicDesc(addr2 + offset, req_size, 0)

    src_list = nixl.nixlDescList(nixl.DRAM_SEG, True, False)
    src_list.addDesc(src_buff)

    dst_list = nixl.nixlDescList(nixl.DRAM_SEG, True, False)
    dst_list.addDesc(dst_buff)

    print("Transfer from " + str(addr1 + offset) + " to " + str(addr2 + offset))

    noti_str = "n\0tification"
    print(noti_str)

    handle = agent1.createXferReq(src_list, dst_list, name2, noti_str, nixl.NIXL_WR_NOTIF);
    assert handle != None

    ret = agent1.postXferReq(handle)
    assert ret != nixl.NIXL_XFER_ERR

    print("Transfer posted")

    status = 0
    notifMap = {}

    while status != nixl.NIXL_XFER_DONE or len(notifMap) == 0:
        if status != nixl.NIXL_XFER_DONE:
            status = agent1.getXferStatus(handle)

        if len(notifMap) == 0:
            notifMap = agent2.getNotifs(notifMap)

        assert status != nixl.NIXL_XFER_ERR
    
    nixl_utils.verify_transfer(addr1 + offset, addr2 + offset, req_size)
    assert len(notifMap[name1]) == 1
    print(notifMap[name1][0])
    assert notifMap[name1][0] == noti_str

    print("Transfer verified")

    agent1.invalidateXferReq(handle)

    ret = agent1.deregisterMem(reg_list1, ucx1)
    assert ret == 0

    ret = agent2.deregisterMem(reg_list2, ucx2)
    assert ret == 0

    agent1.invalidateRemoteMD(name2)
    agent2.invalidateRemoteMD(name1)

    nixl_utils.free_passthru(addr1)
    nixl_utils.free_passthru(addr2)

test_desc()
test_list()
test_agent()
