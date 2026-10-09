add_library(ivi-core STATIC
    AppListModel.cpp
    AppCatalog.cpp
    AudioFocus.cpp
    SystemState.cpp
    VehicleState.cpp
    MediaSession.cpp
    CallSession.cpp
    NavSession.cpp
    WallpaperStore.cpp
    WeatherService.cpp
)

target_include_directories(ivi-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ivi-core PUBLIC Qt6::Core Qt6::Network Qt6::Concurrent)
if(WIN32)
    target_link_libraries(ivi-core PUBLIC comdlg32 winmm wlanapi)
endif()
target_compile_features(ivi-core PUBLIC cxx_std_17)
