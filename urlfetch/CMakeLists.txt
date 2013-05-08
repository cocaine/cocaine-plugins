IF(URLFETCH)
    LOCATE_LIBRARY(LIBSWARM "swarm/networkmanager.h" "swarm")
    
    INCLUDE_DIRECTORIES(
        ${LIBSWARM_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/urlfetch/include)

    LINK_DIRECTORIES(
        ${LIBSWARM_LIBRARY_DIRS})

    ADD_LIBRARY(urlfetch MODULE
        src/urlfetch
        src/module)

    TARGET_LINK_LIBRARIES(urlfetch
        cocaine-core
        swarm)

    SET_TARGET_PROPERTIES(urlfetch PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS urlfetch
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
        
    INSTALL(
        DIRECTORY
            include/
        DESTINATION include
        COMPONENT development)
ENDIF(URLFETCH)
