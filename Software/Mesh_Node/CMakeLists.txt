cmake_minimum_required(VERSION 3.16.0)

# see https://docs.platformio.org/en/latest/frameworks/espidf.html#esp-idf-components
list(APPEND EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/peripherals/rmt/led_strip/components/led_strip)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_mdf_new)
