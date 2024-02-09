set(QT_MINIMUM_VERSION "5.15.0")

find_package(Qt5Core ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt5Widgets ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt5Network ${QT_MINIMUM_VERSION} REQUIRED)

find_package(QTermWidget5 REQUIRED)

find_package(libzip REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)

set(VS_SOURCES
    src/main.cpp
    src/Application.cpp
    src/Application.hpp
    src/PresentationWindow.cpp
    src/PresentationWindow.hpp
    src/Presentation.cpp
    src/Presentation.hpp
    src/VirtualMachineManager.cpp
    src/VirtualMachineManager.hpp
    src/VirtualMachine.cpp
    src/VirtualMachine.hpp
    src/Network.cpp
    src/Network.hpp
    src/NetworkManager.cpp
    src/NetworkManager.hpp
    src/DiskImageManager.hpp
    src/DiskImageManager.cpp
    src/VirtualMachineWidget.cpp
    src/VirtualMachineWidget.hpp
)

qt5_add_resources(VS_SOURCES Resources/res.qrc)

add_executable(virtual-slides ${VS_SOURCES} Resources/res.qrc)

target_link_libraries(virtual-slides PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    qtermwidget5
    libzip::zip
)

add_executable(vs-sock-stdio-connector src/SockStdioConnector.cpp src/SockStdioConnector.hpp)

target_link_libraries(vs-sock-stdio-connector PRIVATE
    Qt5::Core
    Qt5::Network
)