set(SRC_FILES "isotp.c" "isotp_user.c" "test.c")
set(PRIV_REQUIRES "esp_timer" "can_backend")

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${PRIV_REQUIRES}
)
