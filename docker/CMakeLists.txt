IF(DOCKER)
    LOCATE_LIBRARY(LIBCURL "curl/curl.h" "curl")

    INCLUDE_DIRECTORIES(
        ${LIBCURL_INCLUDE_DIRS})

    LINK_DIRECTORIES(
        ${LIBCURL_LIBRARY_DIRS})

    ADD_LIBRARY(docker MODULE
        http
        docker_client
        module
        isolate)

    TARGET_LINK_LIBRARIES(docker
        cocaine-core
        curl
        boost_system)

    SET_TARGET_PROPERTIES(docker PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(
        TARGETS docker
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(DOCKER)
