/*
This file is part of Quick Menu Plus
Copyright © 2020 浅倉麗子
Copyright © 2020 Princess-of-Sleeping

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

#ifndef SCEPAF_H
#define SCEPAF_H

#include <psp2/paf.h>

typedef struct ScePafStyleVTable ScePafStyleVTable;

typedef struct ScePafStyle {
	ScePafStyleVTable *vptr;
	// size is unknown
} ScePafStyle;

typedef struct ScePafStyleVTable {
	char unk_0x0[0x30];
	void (*set_colour)(ScePafStyle*, float*);
	// size is unknown
} ScePafStyleVTable;

typedef struct ScePafWidgetVTable ScePafWidgetVTable;

typedef struct ScePafWidget {
	ScePafWidgetVTable *vptr;
	char unk_0x4[0x148];
	SceUInt32 id;
	char unk_0x150[0x47];
	char flags;
	// size is unknown
} ScePafWidget;

typedef struct ScePafWidgetVTable {
	char unk_0x0[0xF8];
	ScePafStyle *(*get_style_obj)(ScePafWidget*, int);
	char unk_0xFC[0x20];
	int (*set_label)(ScePafWidget*, ScePafWString*);
	// size is unknown
} ScePafWidgetVTable;

void ScePafWidget_16479BA7(ScePafWidget*, int, int);

#endif
