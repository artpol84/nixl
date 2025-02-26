#!/usr/bin/env python3

from nixl_wrapper import nixl_wrapper
import nixl_utils
import time

if __name__ == "__main__":
    desc_count = 24*64*1024
    nixl_agent = nixl_wrapper("test", None)
    addr = nixl_utils.malloc_passthru(256)
    start_time = time.perf_counter()
    descs = nixl_agent.get_descs(("DRAM", [(addr, 256, 0)]*desc_count))
    end_time = time.perf_counter()
    print ("Time per desc add in us:", (1000000.0*(end_time - start_time))/desc_count)
    nixl_utils.free_passthru(addr)


