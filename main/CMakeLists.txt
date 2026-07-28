idf_component_register(SRCS "main.c"
                            "shell/shell.c"
                            "shell/cmd_ac.c"
                            "driver/nvs/nvs_driver.c"
                            "driver/ir/ir_driver.c"
                            "driver/i2c/i2c_driver.c"
                            "hardware/aht20/aht20.c"
                            "hardware/ir_remote/ir_learn.c"
                            "hardware/ir_remote/ir_remote.c"
                            "protocol/ir_protocol.c"
                            "protocol/haier_protocol.c"
                            "protocol/gree_protocol.c"
                            "config/config.c"
                            "ac/ac_state.c"
                            "wifi/wifi_sta.c"
                            "ble/ble_server.c"
                            "task/task_manager.c"
                            "task/net/net_task.c"
                            "task/control/control_task.c"
                            "task/sensor/sensor_task.c"
                       REQUIRES esp_driver_gpio esp_driver_rmt esp_driver_i2c
                       PRIV_REQUIRES console esp_driver_uart fatfs esp_driver_usb_serial_jtag
                                     esp_usb_cdc_rom_console nvs_flash cmd_system cmd_nvs
                                     esp_wifi esp_event esp_netif bt cmd_task
                       INCLUDE_DIRS "." "shell" "driver/nvs" "driver/ir" "driver/i2c"
                                    "hardware/aht20" "hardware/ir_remote" "protocol"
                                    "config" "ac" "wifi" "ble" "task" "task/net" "task/control" "task/sensor")
