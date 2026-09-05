cmake_minimum_required(VERSION 3.24)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
    message(FATAL_ERROR "Use the WASI SDK pthread toolchain for DSP; em++ builds the GUI separately.")
endif()
include(${CMAKE_CURRENT_LIST_DIR}/web-dependencies.cmake)
find_program(EMXX_EXECUTABLE em++ REQUIRED)

set(bundle "${CMAKE_BINARY_DIR}/artifacts/clap-saw-demo-imgui.wclap")
set(imgui "${CMAKE_SOURCE_DIR}/libs/imgui")
set(ui_sources
    web/ui/Main.cpp src/clap-saw-demo-editor.cpp
    libs/imgui/imgui.cpp libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp libs/imgui/imgui_widgets.cpp
    libs/imgui/backends/imgui_impl_glfw.cpp libs/imgui/backends/imgui_impl_opengl3.cpp)
list(TRANSFORM ui_sources PREPEND "${CMAKE_SOURCE_DIR}/")
file(GLOB ui_headers CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.h" "${imgui}/*.h" "${imgui}/backends/*.h")
add_custom_command(
    OUTPUT "${bundle}/ui/clap-saw-ui.js" "${bundle}/ui/clap-saw-ui.wasm"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${bundle}/ui"
    COMMAND "${EMXX_EXECUTABLE}" -std=c++17 -O2 -DCLAP_SAW_WEB=1
        ${ui_sources}
        "-I${CMAKE_SOURCE_DIR}/src" "-I${imgui}"
        "-I${CMAKE_SOURCE_DIR}/libs/readerwriterqueue"
        "-I${web_clap_SOURCE_DIR}/include" "-I${web_clap_helpers_SOURCE_DIR}/include"
        -sUSE_GLFW=3 -sFULL_ES3=1 -sALLOW_MEMORY_GROWTH=1
        -sMODULARIZE=1 -sEXPORT_NAME=createClapSawUi -sENVIRONMENT=web
        -o "${bundle}/ui/clap-saw-ui.js"
    DEPENDS ${ui_sources} ${ui_headers}
    VERBATIM)
add_custom_target(clap-saw-ui DEPENDS
    "${bundle}/ui/clap-saw-ui.js" "${bundle}/ui/clap-saw-ui.wasm")

add_executable(clap-saw-wclap
    src/clap-saw-demo.cpp src/saw-voice.cpp web/WebClapSawDemo.cpp web/Entry.cpp)
target_link_libraries(clap-saw-wclap PRIVATE clap clap-helpers char-clap-utils::char-clap-utils)
target_include_directories(clap-saw-wclap PRIVATE src libs/readerwriterqueue)
target_compile_definitions(clap-saw-wclap PRIVATE CLAP_SAW_WEB=1)
target_compile_options(clap-saw-wclap PRIVATE -msimd128 -fno-exceptions
    -include "${CMAKE_SOURCE_DIR}/web/WasiReaderWriterQueue.h")
target_link_options(clap-saw-wclap PRIVATE -mexec-model=reactor
    -Wl,--max-memory=2147483648,--export-table,--growable-table,--export=malloc,--export=clap_entry)
set_target_properties(clap-saw-wclap PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${bundle}" OUTPUT_NAME module SUFFIX .wasm)

add_custom_target(wclap ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/web/resources" "${bundle}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/LICENSE.md" "${bundle}/LICENSE.md"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${bundle}/THIRD_PARTY_LICENSES"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${imgui}/LICENSE.txt" "${bundle}/THIRD_PARTY_LICENSES/imgui.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/libs/readerwriterqueue/LICENSE.md"
        "${bundle}/THIRD_PARTY_LICENSES/readerwriterqueue.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${web_clap_SOURCE_DIR}/LICENSE" "${bundle}/THIRD_PARTY_LICENSES/clap.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${web_clap_helpers_SOURCE_DIR}/LICENSE" "${bundle}/THIRD_PARTY_LICENSES/clap-helpers.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${char_clap_utils_SOURCE_DIR}/LICENSE" "${bundle}/THIRD_PARTY_LICENSES/char-clap-utils.txt"
    COMMAND ${CMAKE_COMMAND}
        "-DOUTPUT=${bundle}.tar.gz" -P "${CMAKE_SOURCE_DIR}/cmake/package-wclap.cmake"
    WORKING_DIRECTORY "${bundle}"
    DEPENDS clap-saw-wclap clap-saw-ui
    VERBATIM)
