idf_component_register(SRCS "cmd_task.c"
                       INCLUDE_DIRS "include" "../../main/task"
                       PRIV_REQUIRES console esp_driver_gpio)
