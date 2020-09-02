# This file is part of Quick Menu Plus
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

set(CMAKE_TOOLCHAIN_FILE "$ENV{DOLCESDK}/share/dolce.toolchain.cmake" CACHE PATH "DolceSDK toolchain file")
set(DOLCESDK_CMAKE_FILE "$ENV{DOLCESDK}/share/dolce.cmake" CACHE PATH "DolceSDK CMake functions file")
include("${DOLCESDK_CMAKE_FILE}" REQUIRED)

project(quickmenuplus LANGUAGES C)

add_compile_options(
  -Os -std=c11
  -Wall -Wextra -Wdouble-promotion -Wshadow -Wundef
  -fsingle-precision-constant -fno-common -flto
)

add_link_options(
  -nostdlib
  --entry=module_start
)

# Build ScePaf stubs

dolce_gen_libs(ScePaf_stubs
  ScePaf.yml
  LIB ScePaf2_stub_weak
)

# Build self

set(ELF "quickmenuplus.elf")
set(SELF "quickmenuplus.suprx")
set(MODULE_INFO "quickmenuplus.yml")

add_executable("${ELF}"
  quickmenuplus.c
  config.c
  opcode.c
)

target_link_libraries("${ELF}"
  taihen_stub

  SceAVConfig_stub
  SceKernelModulemgr_stub
  SceLibKernel_stub
  ScePaf_stub_weak
  ScePaf2_stub_weak
  SceRegistryMgr_stub
  SceShellSvc_stub_weak
  SceVshBridge_stub
)

dolce_create_self("${SELF}"
  "${ELF}"
  CONFIG "${MODULE_INFO}"
)
