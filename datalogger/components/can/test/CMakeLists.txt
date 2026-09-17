set(COMPONENT_NAME "can")
set(COMPONENT_REQUIRES unity driver can test_helper)
idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES ${COMPONENT_REQUIRES}
)
