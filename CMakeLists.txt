cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
project(BetterCppShot VERSION 1.2.0)
add_definitions(-DUNICODE -D_UNICODE)

IF(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO /SUBSYSTEM:WINDOWS,4.00")
ENDIF()

# 0x0400 = Windows 95/NT4, IE 0x0300 = IE 3.0 (lowest available)
add_definitions(-D_WIN32_WINNT=0x0501 -D_WIN32_IE=0x0300 -DWINVER=0x0501)
# Note: UNICODE is intentionally omitted — Win95 has very poor Unicode support.
# Use ANSI API calls (e.g. MessageBoxA) where possible for 95 compat.

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/res/resources.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/res/resources.rc
    @ONLY)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/version.h
    @ONLY)

    
file(GLOB_RECURSE SOURCES
    src/*.cpp
    res/*.rc
)

set(CMAKE_RC_FLAGS "--include-dir=${CMAKE_SOURCE_DIR}/res")

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/res/resources.rc)

# target_* commands must come after add_executable
IF(MSVC)
    # /Zc:threadSafeInit- disables thread-safe statics which require Vista+
    target_compile_options(${PROJECT_NAME} PRIVATE /Zc:threadSafeInit-)
ENDIF()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    IF(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /O2 /GL)
        target_link_options(${PROJECT_NAME} PRIVATE /OPT:REF /OPT:ICF /LTCG /INCREMENTAL:NO)
    ELSE()
        target_compile_options(${PROJECT_NAME} PRIVATE -O2 -s)
        target_link_options(${PROJECT_NAME} PRIVATE -mwindows)
    ENDIF()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src)

IF(DEFINED LINK_UNICOWS)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/lib/libunicows.a)
ENDIF()

target_link_libraries(${PROJECT_NAME} gdiplus advapi32 gdi32 user32 kernel32 comctl32 shell32 ole32)

set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

IF(NOT MSVC)
    target_link_libraries(${PROJECT_NAME} -static-libgcc -static-libstdc++)
ENDIF()