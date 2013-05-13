IF(DEALER)

    ADD_LIBRARY(dealer MODULE
        stream
        driver
        io
        module)

    TARGET_LINK_LIBRARIES(dealer
        ev
        msgpack
        zmq
        cocaine-core)

    SET_TARGET_PROPERTIES(dealer PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x"
        VERSION 2)

    INSTALL(
        TARGETS dealer
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(DEALER)
