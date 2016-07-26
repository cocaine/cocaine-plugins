include_directories(
    ${PROJECT_SOURCE_DIR}/foreign/blackhole/src
    ${PROJECT_SOURCE_DIR}/elliptics/include
    )

ADD_LIBRARY(elliptics-extensions MODULE
    src/service
    src/storage
    src/module)

TARGET_LINK_LIBRARIES(elliptics-extensions
    cocaine-core
    cocaine-io-util
    blackhole
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(elliptics-extensions PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(TARGETS elliptics-extensions
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/
    DESTINATION include/cocaine
    COMPONENT development)
