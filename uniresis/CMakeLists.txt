include_directories(${PROJECT_SOURCE_DIR}/uniresis/include)

set(MOD uniresis)

add_library(uniresis MODULE
    src/module.cpp
    src/uniresis.cpp
    src/uniresis/error.cpp
)

target_link_libraries(${MOD}
    blackhole
    cocaine-core
    cocaine-io-util
    ${Boost_LIBRARIES})

set_target_properties(${MOD} PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++11")

install(TARGETS ${MOD}
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

install(
    DIRECTORY
        include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)
