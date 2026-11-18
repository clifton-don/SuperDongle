idf_component_register(SRCS "src/http_ota_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_client app_update esp_partition nvs_flash json event_bus)