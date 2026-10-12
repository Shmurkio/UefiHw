# UefiHw

A modern C++ hardware abstraction and utility library for UEFI applications and DXE drivers, built on top of UefiCpp.

UefiHw provides reusable components for low-level UEFI development, including:

- Memory management
- Event and protocol handling
- Driver infrastructure
- PCI device access
- Memory-mapped and port-mapped I/O (MMIO/PIO)
- Direct Memory Access (DMA)

The library also provides abstractions for interacting with UEFI firmware services within UefiCpp's preemptively scheduled, multithreaded runtime.

## Development

UefiHw is developed for the proprietary UefiCpp SDK, which is not publicly distributed or included in this repository.

The library targets x64 UEFI environments and is built using the MSVC v145 toolchain.