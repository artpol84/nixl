#
# Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
#
# This software product is a proprietary product of NVIDIA CORPORATION &
# AFFILIATES (the "Company") and all right, title, and interest in and to the
# software product, including all associated intellectual property rights, are
# and shall remain exclusively with the Company.
#
# This software product is governed by the End User License Agreement
# provided with the software product.
#

project('nixl', 'CPP',
    default_options: ['buildtype=debug',
                'werror=true',
                'cpp_std=c++11',
                'prefix=/opt/nvidia/nvda_nixl'],
    meson_version: '>= 0.57.0'
)

# set up some global vars for compiler, platform, configuration, etc.
cpp = meson.get_compiler('cpp')

ucx_dep = dependency('ucx')
cuda_dep = dependency('cuda', required : false)

inc_dir = include_directories('include', 'include/internal', 'src', 'src/utils/serdes')

subdir('include')
subdir('include/internal')
subdir('src')
subdir('test')

#install_headers('include/nixl.h', subdir: 'nixl')
#install_headers('include/nixl_metadata.h', subdir: 'nixl')
#install_headers('include/nixl_descriptors.h', subdir: 'nixl')

