ADD_LIBRARY(server MODULE
    service
    module)

TARGET_LINK_LIBRARIES(server
    cocaine-common)

SET_TARGET_PROPERTIES(server PROPERTIES
    PREFIX ""
    SUFFIX ".cocaine-plugin"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(
    TARGETS server
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
