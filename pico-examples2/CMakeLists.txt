cmake_minimum_required(VERSION 3.13)

# 1) Choose whether we're building for Pico (ARM) or host only
option(BUILD_PICO "Build the RP2040 firmware target" ON)

# 2) If Pico build, import SDK *before* project() so the toolchain can register ASM etc.
if(BUILD_PICO)
    include(pico_sdk_import.cmake)
endif()

# 3) Project declaration with the proper languages
#    - If Pico, we need ASM; if host-only, we skip ASM to stay native.
if(BUILD_PICO)
    project(forth_project LANGUAGES C CXX ASM)
else()
    project(forth_project LANGUAGES C CXX)
endif()

# 4) If Pico build, now initialize the SDK (sets up arm-none-eabi compilers)
if(BUILD_PICO)
    pico_sdk_init()
endif()

# 5) Common settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ----- Core library (always built with whichever compiler is active) -----
add_library(forth_core STATIC
    src/forth_core.c
)
target_include_directories(forth_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ----- Host executable (built with host toolchain) -----
add_executable(forth_host
    src/forth_host.c
)
target_link_libraries(forth_host PRIVATE
    forth_core
)

# ----- Pico executable (only if BUILD_PICO=ON) -----
if(BUILD_PICO)
    add_executable(forth_pico
        src/pico_main.c
    )
    target_link_libraries(forth_pico PRIVATE
        pico_stdlib
        forth_core
    )
    pico_add_extra_outputs(forth_pico)
endif()

# ----- Python black-box tests -----
enable_testing()
add_test(NAME forth-blackbox
         COMMAND pytest --maxfail=1 --disable-warnings -q
                 ${CMAKE_SOURCE_DIR}/tests
)

