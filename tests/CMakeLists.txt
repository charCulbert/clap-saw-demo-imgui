cmake_minimum_required(VERSION 3.24)
project(clap-saw-smoke LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
include(../cmake/web-dependencies.cmake)
enable_testing()

add_library(web-plugin MODULE
    ../src/clap-saw-demo.cpp ../src/saw-voice.cpp
    ../web/WebClapSawDemo.cpp ../web/Entry.cpp)
target_link_libraries(web-plugin PRIVATE clap clap-helpers)
target_include_directories(web-plugin PRIVATE ../src ../libs/readerwriterqueue)
target_compile_definitions(web-plugin PRIVATE CLAP_SAW_WEB=1)

add_executable(smoke smoke.cpp)
target_link_libraries(smoke PRIVATE clap ${CMAKE_DL_LIBS})
if(APPLE)
    enable_language(OBJCXX)
    set_source_files_properties(smoke.cpp PROPERTIES LANGUAGE OBJCXX)
    target_link_libraries(smoke PRIVATE "-framework AppKit")
endif()
add_test(NAME web-adapter COMMAND smoke $<TARGET_FILE:web-plugin>
    "${CMAKE_CURRENT_SOURCE_DIR}/../web/resources")
set(NATIVE_PLUGIN "" CACHE FILEPATH "Native plugin binary to load and exercise")
if(NATIVE_PLUGIN)
    add_test(NAME native-plugin COMMAND smoke "${NATIVE_PLUGIN}" "${NATIVE_PLUGIN}")
endif()
