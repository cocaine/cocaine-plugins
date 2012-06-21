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
        elliptics)

    TARGET_LINK_LIBRARIES(elliptics
        cocaine-common
        elliptics_cpp)

    SET_TARGET_PROPERTIES(elliptics PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin")

    INSTALL(TARGETS elliptics
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(ELLIPTICS)
