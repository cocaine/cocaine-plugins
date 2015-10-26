set(MODULE metrics)

find_package(Boost 1.46 COMPONENTS REQUIRED
    system
)

include_directories(
    ${PROJECT_SOURCE_DIR}/metrics/include
)

add_library(${MODULE} MODULE
    src/module
)

target_link_libraries(${MODULE}
    ${Boost_LIBRARIES}
    cocaine-core
)

set_target_properties(${MODULE} PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x -Wall -Wextra -Winit-self -Wconversion -Wno-sign-conversion -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor -Wunreachable-code -pedantic -pedantic-errors"
)

install(TARGETS ${MODULE}
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)

install(
    DIRECTORY
        include/cocaine/idl
        include/cocaine/service
    DESTINATION include/cocaine
    COMPONENT development)
