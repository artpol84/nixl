ucx_utils_dep = declare_dependency(link_with: ucx_utils_lib, include_directories: inc_dir)
nixl_dep = declare_dependency(link_with: nixl_infra, include_directories: inc_dir)

compile_flags = []
if cuda_dep.found()
    compile_flags = [ '-DHAVE_CUDA' ]
endif

ucx_backend_lib = library('ucx_backend',
           'ucx_backend.cpp', 'ucx_backend.h',
           dependencies: [ucx_utils_dep, nixl_dep, serdes_interface, cuda_dep],
           include_directories: inc_dir,
           install: true, 
           cpp_args : compile_flags)

ucx_backend_interface = declare_dependency(link_with: ucx_backend_lib)

ucx_mo_backend_lib = library('ucx_mo_backend',
           'ucx_mo_backend.cpp', 'ucx_mo_backend.h',
           dependencies: [ucx_utils_dep, nixl_dep, serdes_interface, cuda_dep, ucx_backend_interface ],
           include_directories: inc_dir,
           install: true, 
           cpp_args : compile_flags)

ucx_mo_backend_interface = declare_dependency(link_with: ucx_mo_backend_lib)
