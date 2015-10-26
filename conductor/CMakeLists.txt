
INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/node/include
    ${CMAKE_SOURCE_DIR}/conductor/include)

ADD_LIBRARY(conductor MODULE
#    service/include/cocaine/conductor
    include/cocaine/idl/conductor
    src/client
    src/module)


TARGET_LINK_LIBRARIES(conductor
    cocaine-core
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(conductor PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++11")

INSTALL(TARGETS conductor
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

