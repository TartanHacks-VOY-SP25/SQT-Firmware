idf_component_register(SRCS "main.c" 
                        "wifi.c" 
                        "self_test.c" 
                        "sensors.c" 
                        "io_pins.c" 
                        "http.c"
                        "display.c"
                       INCLUDE_DIRS "." "../include")
set(EXTRA_COMPONENT_DIRS ../components)