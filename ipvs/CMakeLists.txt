IF(APPLE)
    MESSAGE(WARNING "  IPVS plugin is not supported under OS X, skipping")
ELSE()
    ADD_SUBDIRECTORY(libipvs-1.26)

    ADD_LIBRARY(ipvs-gateway MODULE
        gateway
        module)

    TARGET_LINK_LIBRARIES(ipvs-gateway
        cocaine-core
        cocaine-io-util
        ipvs)

    SET_TARGET_PROPERTIES(ipvs-gateway PROPERTIES
        PREFIX ""
        SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
        COMPILE_FLAGS "-std=c++0x")

    INSTALL(TARGETS ipvs-gateway
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF()
