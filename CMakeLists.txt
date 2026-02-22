cmake_minimum_required(VERSION 3.12)

project(VirtualSlides LANGUAGES C CXX)

option(BUILD_QEMU "Build custom QEMU with virtio-vsock-device" ON)

if(BUILD_QEMU)
    add_subdirectory(qemu)
endif()

add_subdirectory(app)