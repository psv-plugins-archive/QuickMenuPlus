# This file is part of Quick Power
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

cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{DOLCESDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DOLCESDK}/share/dolce.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define DOLCESDK to point to your SDK path!")
  endif()
endif()

include("$ENV{DOLCESDK}/share/dolce.cmake" REQUIRED)

project(quick_power LANGUAGES C)

dolce_gen_libs(ScePaf_stubs
  ScePaf.yml
  LIB ScePaf2_stub_weak
)

set(ELF "${PROJECT_NAME}.elf")
set(SELF "${PROJECT_NAME}.suprx")

add_executable("${ELF}"
  main.c
)

target_compile_options("${ELF}"
  PRIVATE -O3 -std=c99
  PRIVATE -Wall -Wextra -Wdouble-promotion -Wshadow -Wundef
  PRIVATE -fsingle-precision-constant -fno-common
)

target_link_options("${ELF}"
  PRIVATE -nostdlib
)

target_link_libraries("${ELF}"
  SceKernelModulemgr_stub
  ScePaf2_stub_weak
  SceShellSvc_stub_weak
  taihen_stub
)

dolce_create_self("${SELF}"
  "${ELF}"
  CONFIG exports.yml
)
