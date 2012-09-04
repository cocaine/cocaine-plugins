CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(COCAINE-PLUGINS)

SET(COCAINE_VERSION 0.8)

SET(CMAKE_MODULE_PATH 
    ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

OPTION(CHRONO "Build Chrono Driver plugin" ON)
OPTION(FS "Build Filesystem Driver plugin" ON)
OPTION(DEALER "Build Dealer Driver plugin" ON)
OPTION(PYTHON "Build Python Sandbox plugin" ON)
OPTION(PERL "Build Perl Sandbox plugin" ON)
OPTION(BINARY "Build Binary Sandbox plugin" ON)
OPTION(JAVASCRIPT "Build Javascript Sandbox plugin" ON)
OPTION(MONGO "Build MongoDB Storage plugin" ON)

IF(NOT APPLE)
    OPTION(ELLIPTICS "Build Elliptics Storage plugin" ON)
ELSE()
    OPTION(ELLIPTICS "Build Elliptics Storage plugin" OFF)
ENDIF()

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

FIND_PACKAGE(Boost 1.40.0 REQUIRED
    COMPONENTS
        filesystem
        thread
        system)

INCLUDE(cmake/locate_library.cmake)

LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBZMQ "zmq.hpp" "zmq")
LOCATE_LIBRARY(LIBCOCAINE "cocaine/context.hpp" "cocaine-common")

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBZMQ_INCLUDE_DIRS}
    ${LIBCOCAINE_INCLUDE_DIRS})

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBEV_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBZMQ_LIBRARY_DIRS}
    ${LIBCOCAINE_LIBRARY_DIRS})


SET(PLUGINS
        python
        javascript
        #mongo
        elliptics
        binary
        chrono
        fs
        dealer)

IF(NOT APPLE)
    SET(PLUGINS ${PLUGINS} perl)
ENDIF()

FOREACH(PLUGIN ${PLUGINS})
    ADD_SUBDIRECTORY(${PLUGIN})
ENDFOREACH(PLUGIN)
