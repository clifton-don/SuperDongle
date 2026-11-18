# Main component configuration for SuperDongle
# Auto-generated based on header file analysis

# Auto-discover source files in main directory
FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)

idf_component_register(
    # Source files (auto-discovered from main directory)
    SRCS 
        ${app_sources}

    # Include directories
    INCLUDE_DIRS 
        "."
        "entry"
        "system"
        "wifi"
        "storage"
        "web"
        "web/web_handlers"
        "web/web_handlers/ota_web_handler"
        "web/web_handlers/storage_web_handler"
        "web/web_handlers/system_web_handler"
        "web/web_handlers/usb_web_handler"
        "web/web_handlers/wifi_web_handler"
        "usb_func"

    # 强制链接所有 .o 文件，确保构造函数自注册不被链接器丢弃
    WHOLE_ARCHIVE

    # Required components grouped by functionality
    REQUIRES 
        # Custom components
        event_bus         # Event bus system
        esp_tinyusb       # Custom USB MSC implementation
        ota_manager       # OTA update manager
        led_strip         # LED strip driver

        # Network components
        lwip              # lwIP protocol stack (UDP for HID receiver)
        mdns              # mDNS service discovery
        esp_wifi          # WiFi driver
        esp_netif         # Network interface
        esp_http_server   # HTTP server

        # Storage components
        nvs_flash         # Non-volatile storage
        sdmmc             # SD card driver
        fatfs             # FAT file system
        vfs               # Virtual file system
        spiffs            # SPIFFS file system

        # System components
        app_update        # OTA update support
        esp_system        # System API
        esp_timer         # High-resolution timer
        log               # Logging utilities
        freertos          # RTOS kernel
        json              # cJSON library
)

spiffs_create_partition_image(spiffs_0 ../webui FLASH_IN_PROJECT)
spiffs_create_partition_image(spiffs_1 ../webui FLASH_IN_PROJECT)