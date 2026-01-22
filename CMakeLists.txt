cmake_minimum_required(VERSION 3.16)

project(sunSensorAnalyze LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Добавляем QuickWidgets сюда
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick Charts QuickWidgets)

qt_standard_project_setup()

set(SOURCES
    main.cpp
    sensormodel.cpp
    sensormodel.h
)

add_executable(SolarSensors ${SOURCES})

# 2. И добавляем Qt6::QuickWidgets сюда
target_link_libraries(SolarSensors PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::Charts
    Qt6::QuickWidgets
)

# Копирование ресурсов
file(COPY Main.qml results.txt DESTINATION ${CMAKE_BINARY_DIR})
