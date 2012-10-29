IF(CHRONO)
    ADD_LIBRARY(chrono MODULE
        recurring_timer
        drifting_timer
        module)

    TARGET_LINK_LIBRARIES(chrono
        cocaine-core)

    SET_TARGET_PROPERTIES(chrono PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS chrono
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(CHRONO)
