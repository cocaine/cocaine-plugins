CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(COCAINE-PLUGINS)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

OPTION(BLASTBEAT "Build Blastbeat Driver plugin" ON)
OPTION(CACHE "Build LRU Cache plugin" ON)
OPTION(CGROUPS "Build CGroups Isolation plugin" ON)
OPTION(IPVS "Build IPVS Gateway Service plugin" ON)
OPTION(LOGSTASH "Build Logstash Logger plugin" ON)
OPTION(MONGO "Build MongoDB Storage plugin" OFF)
OPTION(PYTHON "Build Python Sandbox plugin" OFF)
OPTION(CHRONO "Build Chrono Service plugin" ON)
OPTION(URLFETCH "Build URLFetch Service plugin" ON)
OPTION(ZEROMQ "Build ZeroMQ Driver plugin" ON)
OPTION(ELASTICSEARCH "Build ElasticSearch Service plugin" ON)

SET(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

FIND_PACKAGE(Boost 1.40.0
    REQUIRED COMPONENTS
        system
        filesystem)

INCLUDE(cmake/locate_library.cmake)

LOCATE_LIBRARY(LIBCOCAINE "cocaine/context.hpp" "cocaine-core")
LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBZMQ "zmq.h" "zmq")

INCLUDE_DIRECTORIES(BEFORE
    ${PROJECT_SOURCE_DIR}/foreign/cppzmq)

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${LIBCOCAINE_INCLUDE_DIRS}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBZMQ_INCLUDE_DIRS})

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBCOCAINE_LIBRARY_DIRS}
    ${LIBEV_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBZMQ_LIBRARY_DIRS})

SET(PLUGINS
        blastbeat
        cache
        cgroups
        chrono
        ipvs
        logstash
        mongo
        python
        urlfetch
        zeromq
        elasticsearch)

FOREACH(PLUGIN ${PLUGINS})
    ADD_SUBDIRECTORY(${PLUGIN})
ENDFOREACH(PLUGIN)
