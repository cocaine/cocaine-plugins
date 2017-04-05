find_package(Boost 1.46 COMPONENTS filesystem system thread REQUIRED)
INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/logging/include
    ${PROJECT_SOURCE_DIR}/unicorn/include)

ADD_LIBRARY(logging MODULE
        src/logging_v2.cpp
    src/module.cpp
    src/logging/filter.cpp
    src/logging/metafilter.cpp
)

TARGET_LINK_LIBRARIES(logging
    ${Boost_LIBRARIES}
    blackhole
    cocaine-core
    cocaine-io-util
)

SET_TARGET_PROPERTIES(logging PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x -Wall -Wextra -pedantic -Winit-self -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor")

INSTALL(TARGETS logging
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

INSTALL(
    DIRECTORY
        include/cocaine/idl
        include/cocaine/logging
        include/cocaine/traits
    DESTINATION include/cocaine
    COMPONENT development)

