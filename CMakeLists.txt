CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(COCAINE-PLUGINS)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

OPTION(CACHE "Build LRU Cache plugin" OFF)
OPTION(CHRONO "Build Chrono Service plugin" OFF)
OPTION(DOCKER "Build Docker Isolation plugin" OFF)
OPTION(ELASTICSEARCH "Build ElasticSearch Service plugin" OFF)
OPTION(IPVS "Build IPVS Gateway Service plugin" OFF)
OPTION(MONGO "Build MongoDB Storage plugin" OFF)
OPTION(URLFETCH "Build URLFetch Service plugin" OFF)
OPTION(GRAPHITE "Build Graphite Service plugin" ON)
OPTION(UNICORN "Build Unified configuration service plugin" ON)

SET(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

INCLUDE(cmake/locate_library.cmake)

FIND_PACKAGE(Boost 1.46.0
    REQUIRED COMPONENTS
        system
        filesystem
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
INCLUDE_DIRECTORIES(
    /usr/local/include
    )

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
        graphite
        unicorn)

FOREACH(PLUGIN ${PLUGINS})
    STRING(TOUPPER ${PLUGIN} OPTION_NAME)
    IF(${${OPTION_NAME}})
        ADD_SUBDIRECTORY(${PLUGIN})
    ENDIF()
ENDFOREACH(PLUGIN)
