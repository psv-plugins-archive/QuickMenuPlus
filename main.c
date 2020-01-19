/*
Quick Volume
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

#include <string.h>
#include <psp2/avconfig.h>
#include <psp2/kernel/modulemgr.h>
#include <taihen.h>

#define AVLS_MAX 0x15
extern int sceAVConfigGetVolCtrlEnable(int *vol_ctrl, int *muted, int *avls);
extern int sceAVConfigWriteRegSystemVol(int vol);

#define GLZ(x) do {\
	if ((x) < 0) { goto fail; }\
} while (0)

#define RLZ(x) do {\
	if ((x) < 0) { return (x); }\
} while(0)

#define INJECT_DATA(idx, modid, segidx, offset, data, size)\
	(inject_id[idx] = taiInjectData(modid, segidx, offset, data, size))

#define HOOK_IMPORT(idx, mod, libnid, funcnid, func)\
	(hook_id[idx] = taiHookFunctionImport(hook_ref+idx, mod, libnid, funcnid, func##_hook))

#define HOOK_OFFSET(idx, modid, offset, func)\
	(hook_id[idx] = taiHookFunctionOffset(hook_ref+idx, modid, 0, offset, 1, func##_hook))

#define N_INJECT 1
static SceUID inject_id[N_INJECT];

#define N_HOOK 4
static SceUID hook_id[N_HOOK];
static tai_hook_ref_t hook_ref[N_HOOK];

static int (*master_volume_widget_init)(int r0, int r1);

typedef int (*set_slidebar_ptr)(int, int, int);

// cannot TAI_CONTINUE due to lack of relocation by taiHEN
static int slidebar_callback_hook(int a) {
	int obj = *(int*)(a + 0x8);
	int vptr = *(int*)obj;
	int pos = *(int*)(obj + 0x294);

	int r0, r1, avls;
	RLZ(sceAVConfigGetVolCtrlEnable(&r0, &r1, &avls));
	if (avls && pos > AVLS_MAX) { pos = AVLS_MAX; }
	RLZ(sceAVConfigSetSystemVol(pos));
	(*(set_slidebar_ptr*)(vptr + 0x188))(obj, pos, 0);

	return 0;
}

static int sceAVConfigGetMasterVol_hook(int *v) {
	TAI_CONTINUE(int, hook_ref[1], v);
	return sceAVConfigGetSystemVol(v);
}

static int sceAVConfigWriteMasterVol_hook(void) {
	TAI_CONTINUE(int, hook_ref[2]);

	int vol;
	RLZ(sceAVConfigGetSystemVol(&vol));
	return sceAVConfigWriteRegSystemVol(vol);
}

static int music_widget_init_hook(int r0, int r1) {
	master_volume_widget_init(r0 + 0x40, r1);
	return TAI_CONTINUE(int, hook_ref[3], r0, r1);
}

static void startup(void) {
	memset(inject_id, 0xFF, sizeof(inject_id));
	memset(hook_id, 0xFF, sizeof(hook_id));
	memset(hook_ref, 0xFF, sizeof(hook_ref));
}

static void cleanup(void) {
	for (int i = 0; i < N_INJECT; i++) {
		if (inject_id[i] >= 0) { taiInjectRelease(inject_id[i]); }
	}
	for (int i = 0; i < N_HOOK; i++) {
		if (hook_id[i] >= 0) { taiHookRelease(hook_id[i], hook_ref[i]); }
	}
}

int _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize argc, const void *argv) { (void)argc; (void)argv;
	startup();

	// get SceShell module info
	tai_module_info_t minfo;
	minfo.size = sizeof(minfo);
	GLZ(taiGetModuleInfo("SceShell", &minfo));

	SceKernelModuleInfo sce_minfo;
	sce_minfo.size = sizeof(sce_minfo);
	GLZ(sceKernelGetModuleInfo(minfo.modid, &sce_minfo));

	master_volume_widget_init = sce_minfo.segments[0].vaddr;

	int inject_offset, slidebar_callback_offset, music_widget_init_offset;

	switch(minfo.module_nid) {
		case 0x0552F692: // 3.60 retail
			inject_offset = 0x14D026;
			slidebar_callback_offset = 0x15358E;
			music_widget_init_offset = 0x156152;
			master_volume_widget_init += 0x152F6C;
			break;
		case 0x5549BF1F: // 3.65 retail
		case 0x34B4D82E: // 3.67 retail
		case 0x12DAC0F3: // 3.68 retail
		case 0x0703C828: // 3.69 retail
		case 0x2053B5A5: // 3.70 retail
		case 0xF476E785: // 3.71 retail
		case 0x939FFBE9: // 3.72 retail
		case 0x734D476A: // 3.73 retail
			inject_offset = 0x14D07E;
			slidebar_callback_offset = 0x1535E6;
			music_widget_init_offset = 0x1561AA;
			master_volume_widget_init += 0x152FC4;
			break;
		default:
			goto fail;
	}

	// set Thumb bit
	master_volume_widget_init = (void*)((int)master_volume_widget_init | 1);

	// disable the original call to master_volume_widget_init (mov.w r0, #0)
	GLZ(INJECT_DATA(0, minfo.modid, 0, inject_offset, "\x4f\xf0\x00\x00", 4));

	GLZ(HOOK_OFFSET(0, minfo.modid, slidebar_callback_offset, slidebar_callback));
	GLZ(HOOK_IMPORT(1, "SceShell", 0x79E0F03F, 0xC609B4D9, sceAVConfigGetMasterVol));
	GLZ(HOOK_IMPORT(2, "SceShell", 0x79E0F03F, 0x65F03D6A, sceAVConfigWriteMasterVol));
	GLZ(HOOK_OFFSET(3, minfo.modid, music_widget_init_offset, music_widget_init));

	return SCE_KERNEL_START_SUCCESS;

fail:
	cleanup();
	return SCE_KERNEL_START_FAILED;
}

int module_stop(SceSize argc, const void *argv) { (void)argc; (void)argv;
	cleanup();
	return SCE_KERNEL_STOP_SUCCESS;
}
