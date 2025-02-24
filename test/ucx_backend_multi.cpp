#include <iostream>
#include <cassert>
#include <thread>

#include "ucx_backend.h"

volatile bool ready[2]  = {false, false};
volatile bool done[2]  = {false, false};
volatile bool disconnect[2]  = {false, false};
std::string conn_info[2];

void test_thread(int id)
{
    nixlUcxInitParams init_params;
    nixlBackendEngine* ucxw;
    nixl_status_t ret;
    
    std::string my_name("Agent1");
    std::string other("Agent2");

    if(id){ 
        my_name = "Agent2";
        other = "Agent1";
    }

    init_params.localAgent = my_name;

    std::cout << my_name << " Started\n";

    ucxw = (nixlBackendEngine*) new nixlUcxEngine (&init_params);

    conn_info[id] = ucxw->getConnInfo();
    
    ucxw->progress();

    ready[id] = true;
    //wait for other
    while(!ready[!id]);

    ret = ucxw->loadRemoteConnInfo(other, conn_info[!id]);
    assert(ret == NIXL_SUCCESS);

    //one-sided connect
    if(!id) 
        ret = ucxw->connect(other);

    assert(ret == NIXL_SUCCESS);

    done[id] = true;
    while(!done[!id])
        if(id) ucxw->progress();

    std::cout << "Thread passed with id " << id << "\n";

    //test one-sided disconnect
    if(!id)
        ucxw->disconnect(other);

    disconnect[id] = true;
    //wait for other
    while(!disconnect[!id]);

    //let disconnect process
    ucxw->progress();

    std::cout << "Thread disconnected with id " << id << "\n";
}

int main()
{
    std::thread th1(test_thread, 0);
    std::thread th2(test_thread, 1);

    th1.join();
    th2.join();
}
