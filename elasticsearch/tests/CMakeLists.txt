IF(ELASTICSEARCH_PLUGIN_TESTING)
    INCLUDE_DIRECTORIES(.)

    LINK_DIRECTORIES()

    ADD_EXECUTABLE(tests
        ../src/rest/get.hpp
        main.cpp
        test_ResponseHandling.cpp)

    TARGET_LINK_LIBRARIES(tests
        gtest
        gmock
        swarm
        cocaine-core)

    SET_TARGET_PROPERTIES(tests PROPERTIES
        COMPILE_FLAGS "-std=c++0x")

ENDIF(ELASTICSEARCH_PLUGIN_TESTING)
