cmake_minimum_required(VERSION 3.21)

project(mp157-ivi VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

option(IVI_BUILD_BOARD_PROBE "Build /dev/fb0 board probe for STM32MP157" ${UNIX})

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network Gui Qml Quick QuickControls2 Concurrent)
qt_standard_project_setup()

add_subdirectory(src/core)
add_subdirectory(src/carplay)
add_subdirectory(src/androidauto)
add_subdirectory(src/shell)

if(IVI_BUILD_BOARD_PROBE)
    add_subdirectory(tools/board-probe)
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/wallpapers/
        DESTINATION share/mp157-ivi/wallpapers/builtin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/media/
        DESTINATION share/mp157-ivi/media)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/maps/
        DESTINATION share/mp157-ivi/maps)
install(PROGRAMS
        ${CMAKE_SOURCE_DIR}/scripts/run-ivi-shell.sh
        ${CMAKE_SOURCE_DIR}/scripts/mp157-board-check.sh
        DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/deploy/ivi-shell.service
        DESTINATION share/mp157-ivi)
