INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/unicorn/include)

# Headers are not required but can be usefull in some cases,
# for example in some IDEs it's required to add files to project.
SET(HEADERS
    include/cocaine/cluster/unicorn.hpp
    include/cocaine/idl/unicorn.hpp
    include/cocaine/unicorn/api.hpp
    include/cocaine/unicorn/api/zookeeper.hpp
    include/cocaine/unicorn/handlers.hpp
    include/cocaine/unicorn/path.hpp
    include/cocaine/unicorn/value.hpp
    include/cocaine/zookeeper/connection.hpp
    include/cocaine/zookeeper/exception.hpp
    include/cocaine/zookeeper/handler.hpp
    include/cocaine/zookeeper/session.hpp
    include/cocaine/zookeeper/zookeeper.hpp
    include/cocaine/unicorn.hpp
)

ADD_LIBRARY(unicorn MODULE
    ${HEADERS}
    src/module.cpp
    src/cluster/unicorn.cpp
    src/unicorn/api/zookeeper.cpp
    src/unicorn.cpp
    src/unicorn/value.cpp
    src/zookeeper/zookeeper.cpp
    src/zookeeper/exception.cpp
    src/zookeeper/session.cpp
    src/zookeeper/handler.cpp
    src/zookeeper/connection.cpp
)

TARGET_LINK_LIBRARIES(unicorn
    msgpack
    cocaine-core
    zookeeper_mt
    ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(unicorn PROPERTIES
    PREFIX ""
    SUFFIX ".cocaine-plugin"
    COMPILE_FLAGS "-std=c++0x -Wall -Werror -Wextra -pedantic -Winit-self -Wold-style-cast -Woverloaded-virtual -Wshadow -Wctor-dtor-privacy -Wnon-virtual-dtor")

INSTALL(TARGETS unicorn
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)