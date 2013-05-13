IF(ZEROMQ)
    ADD_LIBRARY(zeromq MODULE
        stream
        driver
        module)

    TARGET_LINK_LIBRARIES(zeromq
        cocaine-core)

    SET_TARGET_PROPERTIES(zeromq PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x"
        VERSION 1)

    INSTALL(
        TARGETS zeromq
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(ZEROMQ)
