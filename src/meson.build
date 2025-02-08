subdir('utils')

nixl_lib = library('nixl',
           'nixl_descriptors.cpp',
           'nixl_memory_section.cpp',
           dependencies: [],
           include_directories: ['../include', '../include/internal', './utils/serdes'],
           link_with: serdes_lib,
           install: true)

subdir('nixl_nw_backends')
