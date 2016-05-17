INCLUDE_DIRECTORIES(
${PROJECT_SOURCE_DIR}/echo/include)

ADD_LIBRARY(echo MODULE
    src/echo
    src/module)

TARGET_LINK_LIBRARIES(echo
    cocaine-core
    cocaine-io-util
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(echo PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(TARGETS echo
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)
