IF(URLFETCH)
    ADD_LIBRARY(urlfetch MODULE
        urlfetch
        module)

    INCLUDE_DIRECTORIES(
    "/usr/include/swarm")

    TARGET_LINK_LIBRARIES(urlfetch
        cocaine-core
        swarm)

    SET_TARGET_PROPERTIES(urlfetch PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS urlfetch
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(URLFETCH)
