#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

#include "ucx_backend.h"

volatile bool ready[2]  = {false, false};
volatile bool done[2]  = {false, false};
volatile bool disconnect[2]  = {false, false};
std::string conn_info[2];

void test_thread(int id)
{
    nixlUcxInitParams init_params;
    nixlBackendEngine* ucxw;
    nixlBackendTester* ucx_tester;
    nixl_status_t ret;
    
    std::string my_name("Agent1");
    std::string other("Agent2");

    if(id){ 
        my_name = "Agent2";
        other = "Agent1";
    }

    init_params.localAgent = my_name;
    init_params.enableProgTh = true;

    std::cout << my_name << " Started\n";

    ucxw = (nixlBackendEngine*) new nixlUcxEngine (&init_params);
    ucx_tester = new nixlBackendTester(ucxw);

    conn_info[id] = ucx_tester->getConnInfo();
    
    ready[id] = true;
    //wait for other
    while(!ready[!id]);

    ret = ucx_tester->loadRemoteConnInfo(other, conn_info[!id]);
    assert(ret == NIXL_SUCCESS);

    //one-sided connect
    if(!id) 
        ret = ucx_tester->connect(other);

    assert(ret == NIXL_SUCCESS);

    done[id] = true;
    while(!done[!id]);

    std::cout << "Thread passed with id " << id << "\n";

    //test one-sided disconnect
    if(!id)
        ucx_tester->disconnect(other);

    disconnect[id] = true;
    //wait for other
    while(!disconnect[!id]);

    std::cout << "Thread disconnected with id " << id << "\n";

    std::this_thread::sleep_for(std::chrono::seconds(1));

    delete ucx_tester;
    delete ucxw;
}

int main()
{
    std::thread th1(test_thread, 0);
    std::thread th2(test_thread, 1);

    th1.join();
    th2.join();
}
