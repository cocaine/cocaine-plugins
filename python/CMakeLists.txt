EXECUTE_PROCESS(
    COMMAND python-config --includes
    OUTPUT_VARIABLE PYTHON_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

EXECUTE_PROCESS(
    COMMAND python-config --libs
    OUTPUT_VARIABLE PYTHON_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

IF(NOT PYTHON_CFLAGS OR NOT PYTHON_LIBS)
    MESSAGE(FATAL_ERROR "Python development files are required to build.")
    RETURN()
ENDIF()

ADD_LIBRARY(python MODULE
    log
    event_source
    dispatch
    readable_stream
    writable_stream
    sandbox
    module)

TARGET_LINK_LIBRARIES(python
    cocaine-core
    cocaine-io-util
    ${PYTHON_LIBS})

SET_TARGET_PROPERTIES(python PROPERTIES
    COMPILE_FLAGS "${PYTHON_CFLAGS} -fno-strict-aliasing -std=c++0x"
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
)

INSTALL(TARGETS python
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
