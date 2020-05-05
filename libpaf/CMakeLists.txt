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

set(LIB "ScePafWidget_stub_weak")
set(LIB_FILE "lib${LIB}.a")
set(LIB_YAML "${CMAKE_CURRENT_SOURCE_DIR}/ScePafWidget.yml")
set(LIB_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/stubs")

add_custom_target("${LIB}.target"
  DEPENDS "${LIB_FILE}"
)

add_custom_command(
  OUTPUT "${LIB_FILE}"
  DEPENDS "${LIB_YAML}"
  COMMAND vita-libs-gen "${LIB_YAML}" "${LIB_BUILD_DIR}"
  COMMAND make -C "${LIB_BUILD_DIR}"
  COMMAND mv "${LIB_BUILD_DIR}/libScePaf_stub_weak.a" "${LIB_FILE}"
  COMMENT "Building ScePafWidget stubs"
)

add_library("${LIB}" STATIC IMPORTED GLOBAL)

add_dependencies("${LIB}" "${LIB}.target")

set_target_properties("${LIB}" PROPERTIES
  IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${LIB_FILE}"
)
