IF(ELLIPTICS)
    LOCATE_LIBRARY(LIBELLIPTICS "cppdef.h" "elliptics_cpp" "elliptics")
    LOCATE_LIBRARY(LIBEBLOB "eblob.hpp" "eblob_cpp" "eblob")

    INCLUDE_DIRECTORIES(
        ${LIBELLIPTICS_INCLUDE_DIRS}
        ${LIBEBLOB_INCLUDE_DIRS})

    LINK_DIRECTORIES(
        ${LIBELLIPTICS_LIBRARY_DIRS}
        ${LIBEBLOB_LIBRARY_DIRS})

    ADD_LIBRARY(elliptics MODULE
        storage
        module)

    TARGET_LINK_LIBRARIES(elliptics
        cocaine-core
        elliptics_cpp)

    SET_TARGET_PROPERTIES(elliptics PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS elliptics
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(ELLIPTICS)
