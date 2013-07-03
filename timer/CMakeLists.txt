IF(TIMER)
    INCLUDE_DIRECTORIES(
        ${PROJECT_SOURCE_DIR}/timer/include)

    LINK_DIRECTORIES(
        )

    ADD_LIBRARY(timer MODULE
        src/timer
        src/module)

    TARGET_LINK_LIBRARIES(timer
        cocaine-core)

    SET_TARGET_PROPERTIES(timer PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS timer
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)

    INSTALL(
        DIRECTORY
            include/
        DESTINATION include
        COMPONENT development)
ENDIF(TIMER)
