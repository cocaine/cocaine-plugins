ADD_LIBRARY(ipvs STATIC
    libipvs
    ip_vs_nl_policy)

SET_TARGET_PROPERTIES(ipvs PROPERTIES
    COMPILE_FLAGS "-fpic")

