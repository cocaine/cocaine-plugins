IF(DEALER)
    ADD_LIBRARY(dealer MODULE
        dealer_job
        dealer_server)

    TARGET_LINK_LIBRARIES(dealer
        cocaine-core)

    SET_TARGET_PROPERTIES(dealer PROPERTIES
        PREFIX ""
        SUFFIX ".cocaine-plugin")

    INSTALL(
        TARGETS dealer
        LIBRARY DESTINATION lib/cocaine
        COMPONENT runtime)
ENDIF(DEALER)
