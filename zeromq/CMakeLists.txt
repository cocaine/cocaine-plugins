IF(ZEROMQ)
    LOCATE_LIBRARY(LIBZMQ "zmq.h" "zmq")

    INCLUDE_DIRECTORIES(${LIBZMQ_INCLUDE_DIRS})

    INCLUDE_DIRECTORIES(BEFORE
        ${PROJECT_SOURCE_DIR}/foreign/cppzmq)

    LINK_DIRECTORIES(${LIBZMQ_LIBRARY_DIRS})

    ADD_LIBRARY(zeromq MODULE
        stream
        driver
        module)

    TARGET_LINK_LIBRARIES(zeromq
        cocaine-core)

    SET_TARGET_PROPERTIES(zeromq PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(
        TARGETS zeromq
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(ZEROMQ)
