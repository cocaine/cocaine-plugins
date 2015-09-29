
INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/node/include
    ${CMAKE_SOURCE_DIR}/conductor/service/include)

ADD_LIBRARY(conductor MODULE
    service/include/cocaine/conductor
    service/include/cocaine/idl/conductor
    service/src/conductor
    isolate/module
    isolate/isolate)

TARGET_LINK_LIBRARIES(conductor
    cocaine-core
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(conductor PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x -Wall -Werror -Wextra -pedantic -Winit-self -Wold-style-cast -Woverloaded-virtual -Wshadow -Wctor-dtor-privacy -Wnon-virtual-dtor")

INSTALL(TARGETS conductor
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        service/include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)
