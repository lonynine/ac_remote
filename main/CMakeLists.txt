idf_component_register(SRCS "main.c"
                            "shell/shell.c"
                            "driver/nvs/nvs_driver.c"
                            "config/config.c"
                            "wifi/wifi_sta.c"
                            "ble/ble_server.c"
                            "task/task_manager.c"
                            "task/net/net_task.c"
                       PRIV_REQUIRES console esp_driver_uart fatfs esp_driver_usb_serial_jtag
                                     esp_usb_cdc_rom_console nvs_flash cmd_system cmd_wifi cmd_nvs
                                     esp_wifi esp_event esp_netif bt cmd_task
                       INCLUDE_DIRS "." "shell" "driver/nvs" "config" "wifi" "ble" "task" "task/net")
