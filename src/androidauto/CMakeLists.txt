set(ANDROIDAUTO_SOURCES
    AoapTransport.cpp
    AndroidAutoSession.cpp
)

add_library(ivi-androidauto STATIC ${ANDROIDAUTO_SOURCES})

target_include_directories(ivi-androidauto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ivi-androidauto PUBLIC Qt6::Core ivi-core)
if(WIN32)
    target_link_libraries(ivi-androidauto PUBLIC setupapi winusb)
endif()
target_compile_features(ivi-androidauto PUBLIC cxx_std_17)
