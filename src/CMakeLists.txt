# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS ${app_sources}
    REQUIRES esp_adc esp_driver_gpio esp_driver_ledc esp_event esp_netif esp_wifi nvs_flash
)
