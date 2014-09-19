IF(MONGO)
    FIND_PACKAGE(MongoDB)

    IF(MongoDB_FOUND)
        LOCATE_LIBRARY(LIBMONGOCLIENT "dbclient.h" "mongoclient" "mongo" "mongo/client")

        INCLUDE_DIRECTORIES(
            ${LIBMONGOCLIENT_INCLUDE_DIRS})

        LINK_DIRECTORIES(
            ${LIBMONGOCLIENT_LIBRARY_DIRS})
        
        ADD_LIBRARY(mongodb MODULE
            storage
            module)

        TARGET_LINK_LIBRARIES(mongodb
            cocaine-core
            ${Boost_LIBRARIES}
            mongoclient)

        SET_TARGET_PROPERTIES(mongodb PROPERTIES
            PREFIX ""
            SUFFIX ".cocaine-plugin"
            COMPILE_FLAGS "-std=c++0x")

        INSTALL(TARGETS mongodb
            LIBRARY DESTINATION lib/cocaine
            COMPONENT runtime)
    ENDIF(MongoDB_FOUND)
ENDIF(MONGO)
