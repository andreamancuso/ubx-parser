cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0042 NEW)
set (CMAKE_CXX_STANDARD 17)

project (ubx-parser)
set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/deps)

# cmake-js v8: generate node.lib from .def file on Windows
if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
    execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()

include_directories(${CMAKE_JS_INC})
file(GLOB SOURCE_FILES "ubx.cc" "parser.cpp" "schema.cpp")

if(MSVC)
    add_compile_options(/bigobj)
    string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()
if(NOT MSVC)
    add_link_options("$<$<CONFIG:Release>:-s>")
endif()
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX "" SUFFIX ".node"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(${PROJECT_NAME} PRIVATE "$<$<PLATFORM_ID:Linux>:-Wl,--gc-sections>")
endif()
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

# Include Node-API wrappers
execute_process(COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
        )
string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

target_include_directories(${PROJECT_NAME} PRIVATE
        ${NODE_ADDON_API_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-api
        ${DEPS}/comms/include
        ${DEPS}/cc.ublox.generated/include
)

# define NAPI_VERSION
add_definitions(-DNAPI_VERSION=9)

# Precompiled headers for heavy comms/ublox headers (disable on memory-constrained CI)
option(UBX_USE_PCH "Use precompiled headers" ON)
if(UBX_USE_PCH AND NOT MSVC)
    target_precompile_headers(${PROJECT_NAME} PRIVATE
        <napi.h>
        <vector>
        <cstdint>
        <type_traits>
        <string>
        <cc_ublox/Message.h>
        <cc_ublox/input/AllMessages.h>
        <cc_ublox/frame/UbloxFrame.h>
        <comms/process.h>
        <comms/util/Tuple.h>
        <comms/field/tag.h>
    )
endif()
