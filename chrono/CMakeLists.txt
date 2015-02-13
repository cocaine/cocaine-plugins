INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/chrono/include)

ADD_LIBRARY(chrono MODULE
    src/chrono
    src/module)

TARGET_LINK_LIBRARIES(chrono
    cocaine-core
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(chrono PROPERTIES
    PREFIX ""
    SUFFIX ".cocaine-plugin"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(TARGETS chrono
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/framework
        include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)
