IF(TIME)
    INCLUDE_DIRECTORIES(
        ${PROJECT_SOURCE_DIR}/time/include)

    ADD_LIBRARY(time MODULE
        src/time
        src/module)

    TARGET_LINK_LIBRARIES(time
        cocaine-core)

    SET_TARGET_PROPERTIES(time PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS time
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)

    INSTALL(
        DIRECTORY
            include/
        DESTINATION include
        COMPONENT development)
ENDIF(TIME)
