#include <iostream>
#include <string>
#include <cassert>
#include "nixl.h"

int main()
{
    // nixlBasicDesc functionality
    nixlBasicDesc buff1;
    buff1.addr   = (uintptr_t) 1000;
    buff1.len    = 105;
    buff1.devId  = 0;

    nixlBasicDesc buff2 (2000,23,3);
    nixlBasicDesc buff3 (buff2);
    nixlBasicDesc buff4;
    buff4 = buff1;
    buff4.copyMeta (buff2);
    nixlBasicDesc buff5 (1980,21,3);
    nixlBasicDesc buff6 (1010,30,4);
    nixlBasicDesc buff7 (1010,30,0);
    nixlBasicDesc buff8 (1010,31,0);

    nixlBasicDesc importDesc(buff2.serialize());
    assert(buff2 == importDesc);

    assert (buff3==buff2);
    assert (buff4==buff1);
    assert (buff3!=buff1);
    assert (buff8!=buff7);

    assert (buff2.covers(buff3));
    assert (buff4.overlaps(buff1));
    assert (!buff1.covers(buff2));
    assert (!buff1.overlaps(buff2));
    assert (!buff2.covers(buff1));
    assert (!buff2.overlaps(buff1));
    assert (buff2.overlaps(buff5));
    assert (buff5.overlaps(buff2));
    assert (!buff2.covers(buff5));
    assert (!buff5.covers(buff2));
    assert (!buff1.covers(buff6));
    assert (!buff6.covers(buff1));
    assert (buff1.covers(buff7));
    assert (!buff7.covers(buff1));

    nixlStringDesc stringd1;
    stringd1.addr   = 2392382;
    stringd1.len    = 23;
    stringd1.devId  = 4;
    stringd1.metadata = std::string("567");

    nixlStringDesc importStringD(stringd1.serialize());
    assert(stringd1 == importStringD);

    std::cout << "\nSerDes Desc tests:\n";
    buff2.print("");
    std::cout << "this should be a copy:\n";
    importDesc.print("");
    std::cout << "\n";
    stringd1.print("");
    std::cout << "this should be a copy:\n";
    importStringD.print("");
    std::cout << "\n";

    nixlStringDesc stringd2;
    stringd2.addr   = 1010;
    stringd2.len    = 31;
    stringd2.devId  = 0;
    stringd2.metadata = std::string("567f");

    nixlMetaDesc meta1;
    meta1.addr     = 56;
    meta1.len      = 1294;
    meta1.devId    = 0;
    meta1.metadata = nullptr;

    nixlMetaDesc meta2;
    meta2.addr     = 70;
    meta2.len      = 43;
    meta2.devId    = 0;
    meta2.metadata = nullptr;

    assert (stringd1!=buff1);
    assert (stringd2==buff8);
    nixlBasicDesc buff9 (stringd1);

    buff1.print("");
    buff2.print("");
    buff9.print("");
    stringd1.print("");
    stringd2.print("");


    // DescList functionality
    std::cout << "\n\n";
    nixlMetaDesc meta3 (10070, 43, 0);
    nixlMetaDesc meta4 (10070, 42, 0);
    meta3.metadata = nullptr;
    meta4.metadata = nullptr;

    nixlDescList<nixlMetaDesc> dlist1 (DRAM_SEG);
    dlist1.addDesc(meta1);
    assert (dlist1.addDesc(meta2)==-1);
    dlist1.addDesc(meta3);

    nixlDescList<nixlMetaDesc> dlist2 (VRAM_SEG, false, false);
    dlist2.addDesc(meta3);
    dlist2.addDesc(meta2);
    assert (dlist2.addDesc(meta1)==-1);

    nixlDescList<nixlMetaDesc> dlist3 (VRAM_SEG, false, true);
    dlist3.addDesc(meta3);
    dlist3.addDesc(meta2);
    assert (dlist3.addDesc(meta1)==-1);

    nixlDescList<nixlMetaDesc> dlist4 (dlist1);
    nixlDescList<nixlMetaDesc> dlist5 (VRAM_SEG);
    dlist5 = dlist3;

    dlist1.print();
    dlist2.print();
    dlist3.print();
    dlist4.print();
    dlist5.print();

    assert(dlist1.remDesc(2)==-1);
    std::cout << dlist1.getIndex(meta3) << "\n";
    dlist1.remDesc(0);
    std::cout << dlist1.getIndex(meta3) << "\n";
    assert(dlist2.remDesc(meta1)==-1);
    dlist2.remDesc(meta3);
    assert(dlist2.getIndex(meta3)==-1);
    assert(dlist3.getIndex(meta1)==-1);
    assert (dlist3.remDesc(meta4)==-1);

    dlist1.print();
    dlist2.print();
    dlist3.print();

    // Populate and unifiedAddr test
    std::cout << "\n\n";
    nixlStringDesc s1 (10070, 43, 0);
    s1.metadata = "s1";
    nixlStringDesc s2 (900, 43, 2);
    s2.metadata = "s2";
    nixlStringDesc s3 (500, 43, 1);
    s3.metadata = "s3";
    nixlStringDesc s4 (100, 43, 3);
    s4.metadata = "s4";

    nixlBasicDesc b1 (10075, 30, 0);
    nixlBasicDesc b2 (905, 30, 2);
    nixlBasicDesc b3 (505, 30, 1);
    nixlBasicDesc b4 (105, 30, 3);
    nixlBasicDesc b5 (305, 30, 4);
    nixlBasicDesc b6 (100, 30, 3);

    nixlDescList<nixlBasicDesc> dlist10 (DRAM_SEG, false, false);
    nixlDescList<nixlBasicDesc> dlist11 (DRAM_SEG, false, true);
    nixlDescList<nixlBasicDesc> dlist12 (DRAM_SEG, true,  true);
    nixlDescList<nixlBasicDesc> dlist13 (DRAM_SEG, true,  true);
    nixlDescList<nixlBasicDesc> dlist14 (DRAM_SEG, true,  true);

    nixlDescList<nixlStringDesc> dlist20 (DRAM_SEG, false,  true);

    dlist10.addDesc(b1);
    dlist10.addDesc(b2);
    dlist10.addDesc(b3);
    dlist10.addDesc(b4);

    dlist11.addDesc(b1);
    dlist11.addDesc(b2);
    dlist11.addDesc(b3);
    dlist11.addDesc(b4);

    dlist12.addDesc(b1);
    dlist12.addDesc(b2);
    dlist12.addDesc(b3);
    dlist12.addDesc(b4);

    dlist13.addDesc(b1);
    dlist13.addDesc(b2);
    dlist13.addDesc(b3);
    dlist13.addDesc(b5);

    dlist14.addDesc(b1);
    dlist14.addDesc(b2);
    dlist14.addDesc(b3);
    dlist14.addDesc(b6);

    dlist20.addDesc(s1);
    dlist20.addDesc(s2);
    dlist20.addDesc(s3);
    dlist20.addDesc(s4);

    dlist11.print();
    dlist12.print();
    dlist13.print();
    dlist14.print();

    std::cout << "\nSerDes DescList tests:\n";
    nixlSerDes* ser_des = new nixlSerDes();
    nixlSerDes* ser_des2 = new nixlSerDes();

    assert(dlist10.serialize(ser_des) == 0);
    nixlDescList<nixlBasicDesc> importList (ser_des);;
    assert(importList == dlist10);

    assert(dlist20.serialize(ser_des2) == 0);
    nixlDescList<nixlStringDesc> importSList (ser_des2);
    assert(importSList == dlist20);

    dlist10.print();
    std::cout << "this should be a copy:\n";
    importList.print();
    std::cout << "\n";
    dlist20.print();
    std::cout << "this should be a copy:\n";
    importSList.print();
    std::cout << "\n";

    nixlDescList<nixlStringDesc> dlist21 (DRAM_SEG, false,  false);
    nixlDescList<nixlStringDesc> dlist22 (DRAM_SEG, false,  false);
    nixlDescList<nixlStringDesc> dlist23 (DRAM_SEG, true,  false);
    nixlDescList<nixlStringDesc> dlist24 (DRAM_SEG, true,  false);
    nixlDescList<nixlStringDesc> dlist25 (DRAM_SEG, true,  false);

    dlist20.populate (dlist10, dlist21);
    dlist20.populate (dlist11, dlist22);
    dlist20.populate (dlist12, dlist23);
    dlist20.populate (dlist13, dlist24);
    dlist20.populate (dlist14, dlist25);

    std::cout << "\n";
    dlist21.print();
    dlist22.print();
    dlist23.print();
    dlist24.print();
    dlist25.print();

    return 0;
}
