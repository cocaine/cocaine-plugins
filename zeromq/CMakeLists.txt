IF(ZEROMQ)
    ADD_LIBRARY(zeromq MODULE
        zmqsocket
        utils
        stream
        driver
        module)

    TARGET_LINK_LIBRARIES(zeromq
        cocaine-core
        ev
        zmq)

    SET_TARGET_PROPERTIES(zeromq PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(
        TARGETS zeromq
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(ZEROMQ)
