nixl_lib = library('nixl',
           'nixl_descriptors.cpp',
           'nixl_memory_section.cpp',
           dependencies: dependency('uuid', required : true),
           include_directories: ['../include', '../include/internal'],
           install: true)

subdir('utils')
subdir('nixl_nw_backends')
