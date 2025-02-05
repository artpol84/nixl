ucx_utils_dep = declare_dependency(link_with: ucx_utils_lib, include_directories: inc_dir)
nixl_dep = declare_dependency(link_with: nixl_lib, include_directories: inc_dir)

ucx_backend_lib = library('ucx_backend',
           'ucx_backend.cpp', 'ucx_backend.h',
           dependencies: [ucx_utils_dep, nixl_dep],
           include_directories: inc_dir,
           install: true)
