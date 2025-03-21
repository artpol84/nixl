# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

subdir('utils')
nixl_infra = library('nixl_build',
           'nixl_descriptors.cpp',
           'nixl_memory_section.cpp',
           'nixl_metadata_stream.cpp',
            include_directories: ['../include', '../include/internal', 'nixl_nw_backends',
                                  'nixl_storage_backends',
                                  './utils/serdes', './utils/data_structures',
                                   './utils/sys/'],
            dependencies: [serdes_interface],
            install: true)

subdir('nixl_nw_backends')
nixl_lib_deps = [ucx_backend_interface, serdes_interface]
nixl_inc_dirs = ['../include', '../include/internal', 'nixl_nw_backends',
                 './utils/serdes', './utils/data_structures', './utils/sys/',
                 './utils/ucx/']

if cuda_dep.found()
    nixl_lib_deps += [ cuda_dep ]
endif

disable_gds_backend = get_option('disable_gds_backend')
if not disable_gds_backend and cuda_dep.found()
    subdir('nixl_storage_backends')
    nixl_lib_deps += [ gds_backend_interface ]
    nixl_inc_dirs += [ 'nixl_storage_backends/gds/']
endif

nixl_lib   = library('nixl',
                     'nixl_descriptors.cpp',
                     'nixl_memory_section.cpp',
                     'nixl_agent.cpp',
                     'nixl_metadata_stream.cpp',
                     'nixl_plugin_manager.cpp',
                      include_directories: [ nixl_inc_dirs ],
                      dependencies: [ nixl_lib_deps, ucx_dep ], install: true)
subdir('pybind')
