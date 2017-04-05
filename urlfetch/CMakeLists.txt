LOCATE_LIBRARY(LIBSWARM "swarm/http_request.hpp" "swarm")
LOCATE_LIBRARY(LIBSWARM_URLFETCHER "swarm/urlfetcher/url_fetcher.hpp" "swarm_urlfetcher")

find_package(Boost COMPONENTS system thread REQUIRED)

INCLUDE_DIRECTORIES(
    ${LIBSWARM_INCLUDE_DIRS}
    ${LIBSWARM_URLFETCHER_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/urlfetch/include)

LINK_DIRECTORIES(
    ${LIBSWARM_LIBRARY_DIRS})

ADD_LIBRARY(urlfetch MODULE
    src/urlfetch
    src/module)

TARGET_LINK_LIBRARIES(urlfetch
    cocaine-core
    cocaine-io-util
    blackhole
    swarm
    swarm_urlfetcher
    ${Boost_LIBRARIES}
)

SET_TARGET_PROPERTIES(urlfetch PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(TARGETS urlfetch
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/framework
        include/cocaine/idl
    DESTINATION include/cocaine
    COMPONENT development)
