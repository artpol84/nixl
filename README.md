# NVIDIA Inference Xfer Library (NIXL)

NIXL is an abstraction library to abstract memory of heterogeneous devices, i.e., CPU, GPU, storage, and enable most efficient and low-latency communication among them, integrated with distributed inference servers such as Triton. This library will target distributed inference communication patterns to effectively transfer the KV cache in disaggregated LLM serving platform.

# Prerequisites
Ubuntu: sudo apt install build-essential meson ninja-build cmake uuid-dev pkg-config
Fedora: sudo dnf install gcc-c++ meson ninja-build cmake uuid-dev pkg-config

## Getting started
To compile and use this library use:
meson setup <name_of_build_dir>
cd <build_dir>
ninja

## Contributing

## License
TBD

