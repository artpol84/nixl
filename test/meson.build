nixl_dep = declare_dependency(link_with: nixl_lib, include_directories: inc_dir)
ucx_backend_dep = declare_dependency(link_with: ucx_backend_lib, include_directories: [inc_dir, '../src/nixl_nw_backends'])

ucx_backend_example = executable('ucx_backend_example',
           'ucx_backend_example.cpp',
           dependencies: [dependency('uuid', required : true), nixl_dep, ucx_backend_dep],
           include_directories: [inc_dir, '../src/nixl_nw_backends'],
           install: true)

