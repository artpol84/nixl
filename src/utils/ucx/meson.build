ucx_utils_lib = library('ucx_utils',
           'ucx_utils.cpp', 'ucx_utils.h',
           dependencies: [ dependency('uuid', required : true),
	      ucx_dep ],
           include_directories: inc_dir,
           install: true)

ucx_worker_bin = executable('ucx_worker_test',
           'ucx_worker_test.cpp',
           dependencies: [ dependency('uuid', required : true),
	      ucx_dep, cuda_dep ],
           include_directories: inc_dir,
	   link_with: ucx_utils_lib,
           install: true)
