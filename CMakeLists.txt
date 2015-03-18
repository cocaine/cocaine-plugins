CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(COCAINE-PLUGINS)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

OPTION(CACHE "Build LRU Cache plugin" ON)
OPTION(CHRONO "Build Chrono Service plugin" ON)
OPTION(DOCKER "Build Docker Isolation plugin" ON)
OPTION(ELASTICSEARCH "Build ElasticSearch Service plugin" ON)
OPTION(IPVS "Build IPVS Gateway Service plugin" ON)
OPTION(MONGO "Build MongoDB Storage plugin" OFF)
OPTION(URLFETCH "Build URLFetch Service plugin" ON)
OPTION(GRAPHITE "Build Graphite Service plugin" ON)

ADD_DEFINITIONS(-DBLACKHOLE_HEADER_ONLY)

SET(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

INCLUDE(cmake/locate_library.cmake)

FIND_PACKAGE(Boost 1.46.0
    REQUIRED COMPONENTS
        system
        filesystem
        regex
        thread)

LOCATE_LIBRARY(LIBCOCAINE "cocaine/context.hpp" "cocaine-core")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${LIBCOCAINE_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS})

INCLUDE_DIRECTORIES(BEFORE
    ${PROJECT_SOURCE_DIR}/foreign/asio/asio/include
    ${PROJECT_SOURCE_DIR}/foreign/blackhole/src
    ${PROJECT_SOURCE_DIR}/foreign/rapidjson/include)

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBCOCAINE_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS})

SET(PLUGINS
        cache
        chrono
        docker
        elasticsearch
        ipvs
        mongo
        urlfetch
        graphite)

FOREACH(PLUGIN ${PLUGINS})
    STRING(TOUPPER ${PLUGIN} OPTION_NAME)
    IF(${${OPTION_NAME}})
        ADD_SUBDIRECTORY(${PLUGIN})
    ENDIF()
ENDFOREACH(PLUGIN)
