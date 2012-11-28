ADD_LIBRARY(logging MODULE
    service
    module)

TARGET_LINK_LIBRARIES(logging
    cocaine-common)

SET_TARGET_PROPERTIES(logging PROPERTIES
    PREFIX ""
    SUFFIX ".cocaine-plugin"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(
    TARGETS logging
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
