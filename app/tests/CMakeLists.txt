find_package(Qt6Core ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt6Test ${QT_MINIMUM_VERSION} REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)

enable_testing(true)


add_executable(tst_vsock
    tst_vsock.cpp
    ../src/VSockServer.hpp
    ../src/VSockServer.cpp
    ../src/VSock.hpp
    ../src/VSock.cpp
)

target_link_libraries(tst_vsock PRIVATE Qt6::Test Qt6::Core)

add_test(NAME tst_vsock COMMAND tst_vsock)