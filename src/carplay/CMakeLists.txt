set(CARPLAY_SOURCES
    LocalMfiAuth.cpp
    Iap2Wire.cpp
    Iap2Body.cpp
    Iap2Protocol.cpp
    Iap2LinkEngine.cpp
    Iap2LinkSession.cpp
    Iap2WirelessBootstrap.cpp
    BluetoothRfcomm.cpp
    BluetoothDevices.cpp
    AirPlayCrypto.cpp
    AirPlayIdentity.cpp
    AirPlayControlCipher.cpp
    AirPlayAuthSetup.cpp
    AirPlayBplist.cpp
    AirPlayScreenStream.cpp
    AirPlayAudioStream.cpp
    AirPlayBufferedAudio.cpp
    AirPlayPcmPlayer.cpp
    AirPlayH264Decoder.cpp
    AirPlayServer.cpp
    AirPlayMirrorSession.cpp
    BonjourAdvertiser.cpp
    ExistingWifi.cpp
    CarPlaySession.cpp
    third_party/monocypher.c
)

add_library(ivi-carplay STATIC ${CARPLAY_SOURCES})

target_include_directories(ivi-carplay PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(ivi-carplay PUBLIC Qt6::Core Qt6::Network Qt6::Gui Qt6::Concurrent)
if(WIN32)
    target_link_libraries(ivi-carplay PUBLIC bcrypt ncrypt crypt32 ws2_32 Bthprops wlanapi mfplat mf mfuuid wmcodecdspuuid ole32 winmm)
elseif(UNIX)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(ivi-carplay PUBLIC OpenSSL::Crypto OpenSSL::SSL ${CMAKE_DL_LIBS})
    find_package(ALSA)
    if(ALSA_FOUND)
        target_compile_definitions(ivi-carplay PUBLIC IVI_HAVE_ALSA=1)
        target_include_directories(ivi-carplay PUBLIC ${ALSA_INCLUDE_DIRS})
        target_link_libraries(ivi-carplay PUBLIC ${ALSA_LIBRARIES})
    endif()
endif()
set_source_files_properties(third_party/monocypher.c PROPERTIES LANGUAGE C)
target_compile_features(ivi-carplay PUBLIC cxx_std_17)
