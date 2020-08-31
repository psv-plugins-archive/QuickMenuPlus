# This file is part of Quick Volume
# Copyright © 2020 浅倉麗子

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(quick_volume LANGUAGES C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib -Wall -Wextra -O3 -std=c99")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")

set(ELF "${PROJECT_NAME}.elf")
set(SELF "${PROJECT_NAME}.suprx")

add_executable("${ELF}"
  main.c
)

target_link_libraries("${ELF}"
  SceAVConfig_stub
  SceKernelModulemgr_stub
  taihen_stub
)

vita_create_self("${SELF}"
  "${ELF}"
  CONFIG exports.yml
  UNSAFE
)
