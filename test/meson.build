nixl_dep = declare_dependency(link_with: nixl_lib_rb, include_directories: inc_dir)
ucx_backend_dep = declare_dependency(link_with: ucx_backend_lib, include_directories: [inc_dir, '../src/nixl_nw_backends'])

ucx_backend_example = executable('ucx_backend_example',
           'ucx_backend_example.cpp',
           dependencies: [nixl_dep, ucx_backend_dep],
           include_directories: [inc_dir, '../src/nixl_nw_backends'],
           install: true)

ucx_backend_multi = executable('ucx_backend_multi',
           'ucx_backend_multi.cpp',
           dependencies: [nixl_dep, ucx_backend_dep],
           include_directories: [inc_dir, '../src/nixl_nw_backends'],
           install: true)

desc_example = executable('desc_example',
           'desc_example.cpp',
           dependencies: [nixl_dep],
           include_directories: [inc_dir, '../src/utils/serdes'],
           link_with: [serdes_lib],
           install: true)

md_streamer = executable('md_streamer',
            'metadata_streamer.cpp',
            dependencies: [nixl_dep],
            include_directories: [inc_dir],
            install: true)

agent_example = executable('agent_example',
           'agent_example.cpp',
           dependencies: [nixl_dep, ucx_backend_dep],
           include_directories: [inc_dir, '../src/utils/serdes', '../src/nixl_nw_backends'],
           link_with: [serdes_lib],
           install: true)

p2p_socket = executable('p2p_test',
            'p2p_socket_test.cpp',
            dependencies: [nixl_dep],
            include_directories: [inc_dir],
            install: true)

nixl_app  = executable('nixl_test',
           'nixl_test.cpp',
           dependencies: [nixl_dep],
           include_directories: [inc_dir, '../src/utils/serdes'],
           link_with: [serdes_lib],
           install: true)
