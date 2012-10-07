IF(CGROUPS)
    LOCATE_LIBRARY(LIBCGROUP "libcgroup.h" "cgroup")

    ADD_LIBRARY(cgroups MODULE
        module
        isolate)

    TARGET_LINK_LIBRARIES(cgroups
        cocaine-common
        cgroup)

    SET_TARGET_PROPERTIES(cgroups PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin")

    INSTALL(
        TARGETS cgroups
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(CGROUPS)
