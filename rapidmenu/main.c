/*
Rapidmenu
Copyright (C) 2020 浅倉麗子

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// Author: 浅倉麗子

#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/power.h>

int _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize argc, const void *argv) { (void)argc; (void)argv;
	kscePowerSetPsButtonPushTime(200 * 1000);
	return SCE_KERNEL_START_NO_RESIDENT;
}

int module_stop(SceSize argc, const void *argv) { (void)argc; (void)argv;
	return SCE_KERNEL_STOP_SUCCESS;
}
