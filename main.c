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

#define HOOK_IMPORT(idx, mod, libnid, funcnid, hookfunc)\
	(hook_id[idx] = taiHookFunctionImport(hook_ref+idx, mod, libnid, funcnid, hookfunc##_hook))

#define HOOK_OFFSET(idx, modid, offset, hookfunc)\
	(hook_id[idx] = taiHookFunctionOffset(hook_ref+idx, modid, 0, offset, 1, hookfunc##_hook))

#define N_INJECT 1
static SceUID inject_id[N_INJECT];

#define N_HOOK 3
static SceUID hook_id[N_HOOK];
static tai_hook_ref_t hook_ref[N_HOOK];

typedef int (*set_slidebar_ptr)(int, int, int);

// cannot TAI_CONTINUE due to lack of relocation by taiHEN
static int sliderbar_callback_hook(int a) {
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

	// get SceShell module info
	tai_module_info_t minfo;
	minfo.size = sizeof(minfo);
	GLZ(taiGetModuleInfo("SceShell", &minfo));

	int inject_offset, hook_offset;

	switch(minfo.module_nid) {
		case 0x0552F692: // 3.60 retail
			inject_offset = 0x14D01C;
			hook_offset = 0x15358E;
			break;
		case 0x5549BF1F: // 3.65 retail
		case 0x34B4D82E: // 3.67 retail
		case 0x12DAC0F3: // 3.68 retail
		case 0x0703C828: // 3.69 retail
		case 0x2053B5A5: // 3.70 retail
		case 0xF476E785: // 3.71 retail
		case 0x939FFBE9: // 3.72 retail
		case 0x734D476A: // 3.73 retail
			inject_offset = 0x14D074;
			hook_offset = 0x1535E6;
			break;
		default:
			goto fail;
	}

	// cmp r0, r0
	GLZ(inject_id[0] = taiInjectData(minfo.modid, 0, inject_offset, "\x80\x42", 2));

	GLZ(HOOK_OFFSET(0, minfo.modid, hook_offset, sliderbar_callback));
	GLZ(HOOK_IMPORT(1, "SceShell", 0x79E0F03F, 0xC609B4D9, sceAVConfigGetMasterVol));
	GLZ(HOOK_IMPORT(2, "SceShell", 0x79E0F03F, 0x65F03D6A, sceAVConfigWriteMasterVol));

	return SCE_KERNEL_START_SUCCESS;

fail:
	cleanup();
	return SCE_KERNEL_START_FAILED;
}

int module_stop(SceSize argc, const void *argv) { (void)argc; (void)argv;
	cleanup();
	return SCE_KERNEL_STOP_SUCCESS;
}
