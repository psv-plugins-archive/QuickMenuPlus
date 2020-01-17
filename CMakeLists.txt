cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(quick_power LANGUAGES C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib -Wall -Wextra -O3 -std=c99")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")

add_subdirectory(libpaf)

set(ELF "${PROJECT_NAME}.elf")
set(SELF "${PROJECT_NAME}.suprx")

add_executable("${ELF}"
  main.c
)

target_link_libraries("${ELF}"
  SceKernelModulemgr_stub
  ScePafWidget_stub_weak
  SceVshBridge_stub
  taihen_stub
)

vita_create_self("${SELF}"
  "${ELF}"
  CONFIG exports.yml
  UNSAFE
)
