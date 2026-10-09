find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 OLD)

qt_add_executable(ivi-shell
    main.cpp
    VideoScreen.cpp
    CarPlayVideoItem.cpp
)

qt_add_qml_module(ivi-shell
    URI IviShell
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/StatusBar.qml
        qml/HomeScreen.qml
        qml/AppIcon.qml
        qml/AppGlyph.qml
        qml/AppStage.qml
        qml/WeatherFx.qml
    RESOURCES
        weather.frag.qsb
)

target_link_libraries(ivi-shell PRIVATE Qt6::Quick Qt6::QuickControls2 ivi-core ivi-carplay)

add_custom_command(TARGET ivi-shell POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:ivi-shell>/apps"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:ivi-shell>/feed"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/apps" "$<TARGET_FILE_DIR:ivi-shell>/apps"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/feed" "$<TARGET_FILE_DIR:ivi-shell>/feed"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:ivi-shell>/wallpapers/builtin"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets/wallpapers" "$<TARGET_FILE_DIR:ivi-shell>/wallpapers/builtin"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets/media" "$<TARGET_FILE_DIR:ivi-shell>/media"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:ivi-shell>/offline-mfi"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets/carplay/offline-mfi" "$<TARGET_FILE_DIR:ivi-shell>/offline-mfi"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/carplay/skoda-oem-icon.png" "$<TARGET_FILE_DIR:ivi-shell>/skoda-oem-icon.png"
)

if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/win64")
    add_custom_command(TARGET ivi-shell POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/win64"
            "$<TARGET_FILE_DIR:ivi-shell>"
    )
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ivi-shell POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml" "$<TARGET_FILE:ivi-shell>"
        )
    endif()
endif()

install(TARGETS ivi-shell RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/apps DESTINATION share/mp157-ivi)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/feed DESTINATION share/mp157-ivi)
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/carplay/offline-mfi")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/carplay/offline-mfi
            DESTINATION share/mp157-ivi)
endif()
