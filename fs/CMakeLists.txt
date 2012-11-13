IF(FS)
    ADD_LIBRARY(fs MODULE
        driver
        module)

    TARGET_LINK_LIBRARIES(fs
        cocaine-core)

    SET_TARGET_PROPERTIES(fs PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS fs
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(FS)
