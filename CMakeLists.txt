# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_definitions(
    "LV_CALENDAR_DEFAULT_MONTH_NAMES={\"\\xE4\\xB8\\x80\\xE6\\x9C\\x88\", \"\\xE4\\xBA\\x8C\\xE6\\x9C\\x88\", \"\\xE4\\xB8\\x89\\xE6\\x9C\\x88\", \"\\xE5\\x9B\\x9B\\xE6\\x9C\\x88\", \"\\xE4\\xBA\\x94\\xE6\\x9C\\x88\", \"\\xE5\\x85\\xAD\\xE6\\x9C\\x88\", \"\\xE4\\xB8\\x83\\xE6\\x9C\\x88\", \"\\xE5\\x85\\xAB\\xE6\\x9C\\x88\", \"\\xE4\\xB9\\x9D\\xE6\\x9C\\x88\", \"\\xE5\\x8D\\x81\\xE6\\x9C\\x88\", \"\\xE5\\x8D\\x81\\xE4\\xB8\\x80\\xE6\\x9C\\x88\", \"\\xE5\\x8D\\x81\\xE4\\xBA\\x8C\\xE6\\x9C\\x88\"}"
)

set(EXTRA_COMPONENT_DIRS ./common)

project(xiaocai_esp32_project)