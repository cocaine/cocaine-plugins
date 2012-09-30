ADD_LIBRARY(blastbeat MODULE
    module
    driver
    job)

TARGET_LINK_LIBRARIES(blastbeat
    cocaine-core)

SET_TARGET_PROPERTIES(blastbeat PROPERTIES
    PREFIX ""
    SUFFIX ".cocaine-plugin")

INSTALL(
    TARGETS blastbeat
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
