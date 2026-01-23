cmake_minimum_required(VERSION 3.16)

project(SolarSensors LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QuickDialogs2 нужен для FileDialog
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick Charts QuickWidgets QuickControls2)

qt_standard_project_setup()

set(SOURCES
    main.cpp
    sensormodel.cpp
    sensormodel.h
)

add_executable(SolarSensors ${SOURCES})

target_link_libraries(SolarSensors PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::Charts
    Qt6::QuickWidgets
    Qt6::QuickControls2
)

# Копируем Main.qml и results.txt в папку сборки
file(COPY Main.qml results.txt DESTINATION ${CMAKE_BINARY_DIR})
