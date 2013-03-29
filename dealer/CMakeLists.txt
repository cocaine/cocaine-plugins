IF(DEALER)

    ADD_LIBRARY(dealer MODULE
        stream
        driver
        io
        module)

    TARGET_LINK_LIBRARIES(dealer
        cocaine-core)

    SET_TARGET_PROPERTIES(dealer PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(
        TARGETS dealer
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(DEALER)
