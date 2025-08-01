# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(efuse)

idf_component_get_property(esptool_py_dir esptool_py COMPONENT_DIR)
idf_component_get_property(espefuse_py_cmd esptool_py ESPEFUSEPY_CMD)
set(efuse_names "MAC" "WR_DIS")
add_custom_target(efuse-filter
    COMMAND ${CMAKE_COMMAND}
    -D "IDF_PATH=${IDF_PATH}"
    -D "esptool_py_dir=${esptool_py_dir}"
    -D "ESPEFUSEPY=${espefuse_py_cmd}"
    -D "ESPEFUSEPY_OFFLINE=${CONFIG_IDF_CI_BUILD}" # Only for CI tests. Do not establish a connection with the chip
    -D "IDF_TARGET=${IDF_TARGET}"
    -D "efuse_names=${efuse_names}"
    -P get_efuse_summary.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    USES_TERMINAL
    VERBATIM
    )
