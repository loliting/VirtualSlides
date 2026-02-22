include(ExternalProject)
include(FetchContent)

set(QEMU_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/qemu/)
set(QEMU_BUILD_DIR ${QEMU_SRC_DIR}/build/)
set(QEMU_INSTALL_DIR ${QEMU_BUILD_DIR}/out/ CACHE INTERNAL "")
set(QEMU_CONFIGURE_ARGS
    --target-list=x86_64-softmmu
    --enable-slirp
    --enable-kvm
    --prefix=${QEMU_INSTALL_DIR}
    --with-devices-x86_64=virtual-slides-devices
)

make_directory(${QEMU_SRC_DIR}/)
make_directory(${QEMU_BUILD_DIR}/)
make_directory(${QEMU_INSTALL_DIR})
Set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(qemu
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}
    SOURCE_DIR ${QEMU_SRC_DIR}
    STAMP_DIR stamp
    TMP_DIR tmp
    DOWNLOAD_DIR tmp
    BINARY_DIR ${QEMU_SRC_DIR}
    GIT_REPOSITORY https://gitlab.com/qemu-project/qemu.git
    GIT_TAG "v10.2.0"
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE FALSE
    GIT_PROGRESS TRUE
    USES_TERMINAL_DOWNLOAD TRUE
    USES_TERMINAL_UPDATE TRUE
)
FetchContent_MakeAvailable(qemu)

make_directory(${QEMU_BUILD_DIR}/)

if(NOT EXISTS ${QEMU_BUILD_DIR}/.patch_done)
    execute_process(
        COMMAND sed -i "\
1 a system_virtio_ss.add(when: 'CONFIG_VIRTIO_VSOCK', if_true: files('virtio-vsock.c'))\
" hw/virtio/meson.build 
        WORKING_DIRECTORY ${QEMU_SRC_DIR}
    )
file(APPEND ${QEMU_SRC_DIR}/hw/virtio/Kconfig "\
\n\
config VIRTIO_VSOCK\n\
    bool\n\
    default y\n\
    depends on VIRTIO\n\
")
    file(CREATE_LINK
        ${CMAKE_CURRENT_SOURCE_DIR}/virtio-vsock.c
        ${QEMU_SRC_DIR}/hw/virtio/virtio-vsock.c
        SYMBOLIC
    )
    file(CREATE_LINK
        ${CMAKE_CURRENT_SOURCE_DIR}/virtio-vsock.h
        ${QEMU_SRC_DIR}/include/hw/virtio/virtio-vsock.h
        SYMBOLIC
    )
    file(CREATE_LINK
        ${CMAKE_CURRENT_SOURCE_DIR}/virtual-slides-config-devices.mak
        ${QEMU_SRC_DIR}/configs/devices/x86_64-softmmu/virtual-slides-devices.mak
        SYMBOLIC
    )
    file(TOUCH ${QEMU_BUILD_DIR}/.patch_done)
endif()
if(NOT EXISTS ${QEMU_BUILD_DIR}/.configure_done)
    execute_process(
        COMMAND ${QEMU_SRC_DIR}//configure ${QEMU_CONFIGURE_ARGS}
        WORKING_DIRECTORY ${QEMU_BUILD_DIR}/
    )
    file(TOUCH ${QEMU_BUILD_DIR}/.configure_done)
endif()

make_directory(${QEMU_INSTALL_DIR})

add_custom_target(qemu ALL
    COMMAND ninja
    WORKING_DIRECTORY ${QEMU_BUILD_DIR}
    USES_TERMINAL
)

add_custom_target(qemu-install
    COMMAND ninja install 
    WORKING_DIRECTORY ${QEMU_BUILD_DIR}
    USES_TERMINAL
)
