idf_component_register(SRCS "geolocation.c" "main.c" 
                        "wifi.c" 
                        "self_test.c" 
                        "sensors.c" 
                        "io_pins.c" 
                        "http.c"
                        "display.c"
                       INCLUDE_DIRS "." "../include")
set(EXTRA_COMPONENT_DIRS ../components)
target_compile_definitions(${COMPONENT_TARGET} PRIVATE USE_PRIVATE_CONFIG)
