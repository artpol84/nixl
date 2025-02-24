subdir('utils')
nixl_lib = library('nixl_build',
           'nixl_descriptors.cpp',
           'nixl_memory_section.cpp',
           'nixl_metadata_stream.cpp',
            include_directories: ['../include', '../include/internal', 'nixl_nw_backends', './utils/serdes', './utils/data_structures'],
            dependencies: [serdes_interface],
            install: true)

subdir('nixl_nw_backends')
nixl_lib_rb = library('nixl',
              'nixl_descriptors.cpp',
              'nixl_memory_section.cpp',
              'nixl_agent.cpp',
              'nixl_metadata_stream.cpp',
              include_directories: ['../include', '../include/internal', 'nixl_nw_backends','./utils/serdes', './utils/data_structures'],
              dependencies: [ucx_backend_interface, serdes_interface],
              install: true)

subdir('adapter')
subdir('pybind')
