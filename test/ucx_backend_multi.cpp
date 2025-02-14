#include <iostream>
#include <cassert>
#include <thread>

#include "ucx_backend.h"

volatile bool ready[2]  = {false, false};
std::string conn_info[2];

void test_thread(int id)
{
    nixlUcxInitParams init_params;
    nixlBackendEngine* ucxw;
    int ret;
    
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
    assert(ret == 0);

    if(id){
        ret = ucxw->makeConnection(other);
    } else {
        ret = ucxw->listenForConnection(other);
    }
    assert(ret == 0);

    std::cout << "Thread passed with id " << id << "\n";
}

int main()
{
    std::thread th1(test_thread, 0);
    std::thread th2(test_thread, 1);

    th1.join();
    th2.join();
}
