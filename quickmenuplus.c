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

#include <string.h>

#include <psp2/avconfig.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/shellsvc.h>
#include <psp2/vshbridge.h>

#include <taihen.h>

#include "common.h"
#include "opcode.h"

extern void ScePafWidget_16479BA7(int, int, int);

#define INJECT_ABS(idx, dest, data, size)\
	(inject_id[idx] = taiInjectAbs(dest, data, size))

#define HOOK_OFFSET(idx, modid, offset, thumb, func)\
	(hook_id[idx] = taiHookFunctionOffset(hook_ref+idx, modid, 0, offset, thumb, func##_hook))

#define N_INJECT 2
static SceUID inject_id[N_INJECT];

#define N_HOOK 5
static SceUID hook_id[N_HOOK];
static tai_hook_ref_t hook_ref[N_HOOK];

typedef void btn_cb(void);
typedef int set_slidebar_pos(int, int, int);

static btn_cb *poweroff_btn_cb;
static int (*vol_widget_init)(int, int);

static void poweroff_btn_hold_cb(void) {
	sceShellUtilRequestColdReset(0);
}

static void btn_init_hook(int r0, int r1, btn_cb *r2, int r3) {
	if (r1 == 0x10000008 && r2 == poweroff_btn_cb) {
		// set holdable button with threshold 200ms
		// and repeat threshold 800ms
		ScePafWidget_16479BA7(r0, 200, 800);

		TAI_NEXT(btn_init_hook, hook_ref[0], r0, r1, r2, r3);
		TAI_NEXT(btn_init_hook, hook_ref[0], r0, 0x10000005, poweroff_btn_hold_cb, r3);

		// set holdable with physical button
		*(char*)(r0 + 0x197) &= 0xFD;
	} else {
		TAI_NEXT(btn_init_hook, hook_ref[0], r0, r1, r2, r3);
	}
}

// cannot TAI_NEXT due to lack of relocation by taiHEN
static int vol_slidebar_cb_hook(int r0) {
	int obj = *(int*)(r0 + 0x8);
	int vptr = *(int*)obj;
	int pos = *(int*)(obj + 0x294);

	SceUInt32 ctrl;
	SceBool muted, avls;
	RLZ(sceAVConfigGetVolCtrlEnable(&ctrl, &muted, &avls));
	if (avls && pos > SCE_AVCONFIG_VOLUME_AVLS_MAX) {
		pos = SCE_AVCONFIG_VOLUME_AVLS_MAX;
		RLZ(sceAVConfigSetSystemVol(pos));
		(**(set_slidebar_pos**)(vptr + 0x188))(obj, pos, 0);
	} else {
		RLZ(sceAVConfigSetSystemVol(pos));
	}

	return 0;
}

static int sceAVConfigGetMasterVol_hook(int *v) {
	TAI_NEXT(sceAVConfigGetMasterVol_hook, hook_ref[2], v);
	return sceAVConfigGetSystemVol(v);
}

static int sceAVConfigWriteMasterVol_hook(void) {
	TAI_NEXT(sceAVConfigWriteMasterVol_hook, hook_ref[3]);

	int vol;
	RLZ(sceAVConfigGetSystemVol(&vol));
	return sceAVConfigWriteRegSystemVol(vol);
}

static int music_widget_init_hook(int r0, int r1) {
	vol_widget_init(r0 + 0x40, r1);
	return TAI_NEXT(music_widget_init_hook, hook_ref[4], r0, r1);
}

static void startup(void) {
	sceClibMemset(inject_id, 0xFF, sizeof(inject_id));
	sceClibMemset(hook_id, 0xFF, sizeof(hook_id));
	sceClibMemset(hook_ref, 0xFF, sizeof(hook_ref));
}

static void cleanup(void) {
	for (int i = 0; i < N_INJECT; i++) {
		if (inject_id[i] >= 0) { taiInjectRelease(inject_id[i]); }
	}
	for (int i = 0; i < N_HOOK; i++) {
		if (hook_id[i] >= 0) { taiHookRelease(hook_id[i], hook_ref[i]); }
	}
}

USED int module_start(UNUSED SceSize args, UNUSED const void *argp) {
	startup();

	vshPowerSetPsButtonPushTime(200 * 1000);

	// get SceShell module info
	tai_module_info_t minfo;
	minfo.size = sizeof(minfo);
	GLZ(taiGetModuleInfo("SceShell", &minfo));

	SceKernelModuleInfo sce_minfo;
	sce_minfo.size = sizeof(sce_minfo);
	GLZ(sceKernelGetModuleInfo(minfo.modid, &sce_minfo));
	int seg0 = (int)sce_minfo.segments[0].vaddr;

	int quick_menu_init = seg0;

	switch(minfo.module_nid) {
		case 0x0552F692: // 3.60 retail
		case 0x532155E5: // 3.61 retail
			quick_menu_init += 0x14C408;
			break;
		case 0xBB4B0A3E: // 3.63 retail
		case 0x5549BF1F: // 3.65 retail
		case 0x34B4D82E: // 3.67 retail
		case 0x12DAC0F3: // 3.68 retail
		case 0x0703C828: // 3.69 retail
		case 0x2053B5A5: // 3.70 retail
		case 0xF476E785: // 3.71 retail
		case 0x939FFBE9: // 3.72 retail
		case 0x734D476A: // 3.73 retail
			quick_menu_init += 0x14C460;
			break;
		case 0xEAB89D5C: // 3.60 testkit
			quick_menu_init += 0x14483C;
			break;
		case 0x587F9CED: // 3.65 testkit
			quick_menu_init += 0x144894;
			break;
		case 0x6CB01295: // 3.60 Devkit
			quick_menu_init += 0x143E40;
			break;
		default:
			goto fail;
	}

	// power widget
	// template ID FAE265F7 from impose_plugin.rco

	// addr of branch to btn_init from quick_menu_init
	int btn_init_bl = quick_menu_init + 0x5B8;

	// addr of the function poweroff_btn_cb
	GLZ(decode_movw_t3(*(int*)(btn_init_bl - 0x12), (int*)&poweroff_btn_cb));
	GLZ(decode_movt_t1(*(int*)(btn_init_bl - 0x0C), (int*)&poweroff_btn_cb));

	// addr of the function btn_init
	int btn_init;
	GLZ(decode_bl_t1(*(int*)btn_init_bl, &btn_init));
	btn_init += btn_init_bl + 4;

	GLZ(HOOK_OFFSET(0, minfo.modid, btn_init - seg0, 1, btn_init));

	if (!vshSblAimgrIsDolce()) {
		// enable power widget (cmp r0, r0)
		GLZ(INJECT_ABS(0, (void*)(quick_menu_init + 0x44E), "\x80\x42", 2));

		// volume widget
		// template ID 84E0F33A from impose_plugin.rco

		// addr of branch to vol_widget_init from quick_menu_init
		int vol_widget_init_bl = quick_menu_init + 0xC1E;

		// addr of the function vol_widget_init
		GLZ(decode_bl_t1(*(int*)vol_widget_init_bl, (int*)&vol_widget_init));
		vol_widget_init += vol_widget_init_bl + 4;

		// addr of pointer to vol_slidebar_cb
		int ptr_vol_slidebar_cb;
		GLZ(decode_movw_t3(*(int*)(vol_widget_init + 0x1E0), &ptr_vol_slidebar_cb));
		GLZ(decode_movt_t1(*(int*)(vol_widget_init + 0x1E6), &ptr_vol_slidebar_cb));

		// addr of vol_slidebar_cb
		int vol_slidebar_cb = *(int*)ptr_vol_slidebar_cb;

		// addr of SceAVConfig imports
		// These are hooked by import due to NID poisoning of SceShell
		// in non-Enso environment.
		int sceAVConfigGetMasterVol, sceAVConfigWriteMasterVol;
		GLZ(decode_blx_t2(*(int*)(vol_widget_init + 0x346), &sceAVConfigGetMasterVol));
		unsigned int pc = (unsigned int)vol_widget_init + 0x346 + 0x4;
		sceAVConfigGetMasterVol += pc - (pc % 4);
		sceAVConfigWriteMasterVol = sceAVConfigGetMasterVol - 0x80;

		// set Thumb bit because we will call this function
		vol_widget_init = (void*)((int)vol_widget_init | 1);

		// addr of branch to music_widget_init from quick_menu_init
		int music_widget_init_bl = quick_menu_init + 0x1006;

		// addr of the function music_widget_init
		int music_widget_init;
		GLZ(decode_bl_t1(*(int*)music_widget_init_bl, &music_widget_init));
		music_widget_init += music_widget_init_bl + 4;

		// disable original call to vol_widget_init just in case (mov.w r0, #0)
		GLZ(INJECT_ABS(1, (void*)vol_widget_init_bl, "\x4f\xf0\x00\x00", 4));

		GLZ(HOOK_OFFSET(1, minfo.modid, vol_slidebar_cb - seg0, 1, vol_slidebar_cb));
		GLZ(HOOK_OFFSET(2, minfo.modid, sceAVConfigGetMasterVol - seg0, 0, sceAVConfigGetMasterVol));
		GLZ(HOOK_OFFSET(3, minfo.modid, sceAVConfigWriteMasterVol - seg0, 0, sceAVConfigWriteMasterVol));
		GLZ(HOOK_OFFSET(4, minfo.modid, music_widget_init - seg0, 1, music_widget_init));
	}

	return SCE_KERNEL_START_SUCCESS;

fail:
	cleanup();
	return SCE_KERNEL_START_FAILED;
}

USED int module_stop(UNUSED SceSize args, UNUSED const void *argp) {
	cleanup();
	return SCE_KERNEL_STOP_SUCCESS;
}
