IF(CACHE)
    INCLUDE_DIRECTORIES(
        ${PROJECT_SOURCE_DIR}/cache/include)

    LINK_DIRECTORIES(
        )

    ADD_LIBRARY(cache MODULE
        src/cache
        src/module)

    TARGET_LINK_LIBRARIES(cache
        cocaine-core)

    SET_TARGET_PROPERTIES(cache PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS cache
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)

    INSTALL(
        DIRECTORY
            include/
        DESTINATION include
        COMPONENT development)
ENDIF(CACHE)
