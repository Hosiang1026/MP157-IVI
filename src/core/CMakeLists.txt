add_library(ivi-core STATIC
    AppListModel.cpp
    AppCatalog.cpp
    AudioFocus.cpp
    SystemState.cpp
    VehicleState.cpp
    CameraService.cpp
    MediaSession.cpp
    CallSession.cpp
    NavSession.cpp
    MapTiles.cpp
    WallpaperStore.cpp
    WeatherService.cpp
    DlnaRenderer.cpp
    FfmpegUrlPlayer.cpp
    RadioSession.cpp
    FileBrowser.cpp
    GpsSource.cpp
    UpdateService.cpp
)

target_include_directories(ivi-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ivi-core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent)
if(WIN32)
    target_link_libraries(ivi-core PUBLIC comdlg32 winmm wlanapi)
elseif(UNIX)
    target_link_libraries(ivi-core PUBLIC ${CMAKE_DL_LIBS})
    find_package(ALSA)
    if(ALSA_FOUND)
        target_compile_definitions(ivi-core PUBLIC IVI_HAVE_ALSA=1)
        target_include_directories(ivi-core PUBLIC ${ALSA_INCLUDE_DIRS})
        target_link_libraries(ivi-core PUBLIC ${ALSA_LIBRARIES})
    endif()
endif()
target_compile_features(ivi-core PUBLIC cxx_std_17)
