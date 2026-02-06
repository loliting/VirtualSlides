find_package(Qt6Core ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt6Test ${QT_MINIMUM_VERSION} REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)

enable_testing(true)


add_executable(tst_vsock
    tst_vsock.cpp
)

add_executable(tst_unixsock
    tst_unixsock.cpp
)

target_link_libraries(tst_vsock PRIVATE Qt6::Test Qt6::Core vs-common-sockets)
target_link_libraries(tst_unixsock PRIVATE Qt6::Test Qt6::Core vs-common-sockets)

add_test(NAME tst_vsock COMMAND tst_vsock)

add_test(NAME tst_unixsock COMMAND tst_unixsock)