import nixl_bindings as nixl

def test_desc():

    test_desc1 = nixl.nixlBasicDesc(1000, 105, 0)
    test_desc2 = nixl.nixlBasicDesc(1010, 30, 0)
    test_desc3 = nixl.nixlBasicDesc(990, 20, 0)

    print(test_desc1)

    test_desc1.print("desc1")
    test_desc2.print("desc2")
    test_desc3.print("desc3")

    assert test_desc1.covers(test_desc2)
    assert test_desc1.overlaps(test_desc3)

def test_list():

    test_desc1 = nixl.nixlBasicDesc(1000, 105, 0)
    test_desc2 = nixl.nixlBasicDesc(2000, 30, 0)
    test_desc3 = nixl.nixlBasicDesc(3000, 20, 0)

    test_list = nixl.nixlDescList(nixl.DRAM_SEG, True, False)

    test_list.addDesc(test_desc1)
    test_list.addDesc(test_desc2)
    test_list.addDesc(test_desc3)

    test_list.print()

    assert test_list.getType() == nixl.DRAM_SEG
    assert test_list.isUnifiedAddr()
    
    print(test_list.descCount())
    assert test_list.descCount() == 3
    
    test_list.clear()

    assert test_list.isEmpty()

test_desc()
test_list()
