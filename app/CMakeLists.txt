set(QT_MINIMUM_VERSION "5.15.0")

find_package(Qt5Widgets ${QT_MINIMUM_VERSION} REQUIRED)
find_package(libzip REQUIRED)
find_package(QTermWidget5 REQUIRED)


set(CMAKE_AUTOMOC ON)

set(VS_SOURCES
    src/main.cpp
    src/Application.cpp
    src/Application.hpp
    src/PresentationWindow.cpp
    src/PresentationWindow.hpp
    src/Presentation.cpp
    src/Presentation.hpp
)

add_executable(virtual-slides ${VS_SOURCES})

target_link_libraries(virtual-slides PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    qtermwidget5
    libzip::zip
)