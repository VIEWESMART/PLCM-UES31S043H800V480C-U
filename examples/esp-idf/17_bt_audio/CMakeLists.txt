# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "IDF_PATH not defined")
endif()

# Hardware is initialized by BSP. Ignore leftover / auto-generated board-manager
# components so this example does not pull esp_board_manager.
set(EXCLUDE_COMPONENTS gen_bmgr_codes gen_bmgr_codes.off s31_es8389 s31_es8389.off)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# IDF 6.2 + GCC 16 treats Bluedroid strncpy as -Werror=stringop-truncation.
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=stringop-truncation" APPEND)

project(17_bt_audio)
