set(QT_MINIMUM_VERSION "6.5.0")

find_package(Qt6Core ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt6Widgets ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt6Network ${QT_MINIMUM_VERSION} REQUIRED)

find_package(qtermwidget6 2.0 REQUIRED)
find_package(libzip REQUIRED)

include(CheckIncludeFile)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)

check_include_file(termios.h HAVE_TERMIOS_H)


add_library(vs-common-sockets SHARED
    src/UnixSocket.cpp
    src/UnixSocket.hpp
    src/UnixSocketServer.cpp
    src/UnixSocketServer.hpp
    src/VSock.cpp
    src/VSock.hpp
    src/VSockServer.cpp
    src/VSockServer.hpp
    src/VSockUser.cpp
    src/VSockUser.hpp
    src/VSockUserServer.cpp
    src/VSockUserServer.hpp
)

set(VS_SOURCES
    src/main.cpp
    src/Application.cpp
    src/Application.hpp
    src/PresentationWindow.cpp
    src/PresentationWindow.hpp
    src/Presentation.cpp
    src/Presentation.hpp
    src/VirtualMachine.cpp
    src/VirtualMachine.hpp
    src/Network.cpp
    src/Network.hpp
    src/Config.hpp
    src/Config.cpp
    src/GuestBridge.hpp
    src/GuestBridge.cpp
    src/VirtualMachineWidget.cpp
    src/VirtualMachineWidget.hpp
    src/VmTaskList.cpp
    src/VmTaskList.hpp
)

qt6_add_resources(VS_SOURCES Resources/res.qrc)

add_executable(virtual-slides ${VS_SOURCES} Resources/res.qrc)
target_compile_definitions(virtual-slides PRIVATE $<$<CONFIG:Debug>:DEBUG_BUILD=1>)
target_link_libraries(virtual-slides PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    qtermwidget6
    libzip::zip
    vs-common-sockets
)

add_executable(vs-sock-stdio-connector src/SockStdioConnector.cpp src/SockStdioConnector.hpp)
target_compile_definitions(vs-sock-stdio-connector PRIVATE $<${HAVE_TERMIOS_H}:HAVE_TERMIOS_H>)
target_link_libraries(vs-sock-stdio-connector PRIVATE
    Qt6::Core
    vs-common-sockets
)

target_link_libraries(vs-common-sockets PRIVATE
    Qt6::Core
)

add_subdirectory(tests)