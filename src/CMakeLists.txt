# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(web_assets_dir "${CMAKE_SOURCE_DIR}/.pio/build/generated")
else()
    set(web_assets_dir "${CMAKE_BINARY_DIR}/generated")
endif()
set(web_assets_header "${web_assets_dir}/web_assets.h")
file(MAKE_DIRECTORY "${web_assets_dir}")
file(WRITE "${web_assets_header}" "")

function(embed_web_asset name path)
    file(READ "${path}" asset_bytes HEX)
    string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," asset_bytes "${asset_bytes}")
    file(APPEND "${web_assets_header}" "static const unsigned char ${name}[] = {${asset_bytes}0x00};\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
endfunction()

embed_web_asset(web_index "${CMAKE_SOURCE_DIR}/include/index.html")
embed_web_asset(web_style "${CMAKE_SOURCE_DIR}/include/style.css")
embed_web_asset(web_script "${CMAKE_SOURCE_DIR}/include/script.js")
embed_web_asset(web_icon "${CMAKE_SOURCE_DIR}/include/images/iconeW.png")
embed_web_asset(web_house "${CMAKE_SOURCE_DIR}/include/images/casa-card.png")

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "." "${web_assets_dir}"
    REQUIRES esp_adc esp_driver_gpio esp_driver_ledc esp_event esp_http_server esp_netif esp_wifi nvs_flash
)
