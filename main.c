/*
This file is part of Quick Power
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
#include <psp2/kernel/modulemgr.h>
#include <psp2/shellsvc.h>
#include <taihen.h>

extern void ScePafWidget_16479BA7(int, int, int);

#define GLZ(x) do {\
	if ((x) < 0) { goto fail; }\
} while (0)

#define RNE(x, k) do {\
	if ((x) != (k)) { return -1; }\
} while(0)

#define INJECT_DATA(idx, modid, segidx, offset, data, size)\
	(inject_id[idx] = taiInjectData(modid, segidx, offset, data, size))

#define HOOK_OFFSET(idx, modid, offset, func)\
	(hook_id[idx] = taiHookFunctionOffset(hook_ref+idx, modid, 0, offset, 1, func##_hook))

#define N_INJECT 1
static SceUID inject_id[N_INJECT];

#define N_HOOK 1
static SceUID hook_id[N_HOOK];
static tai_hook_ref_t hook_ref[N_HOOK];

static int poweroff_btn_cb_addr;

static int decode_bl_t1(int bl, int *imm) {
	// split into two shorts
	short bl_1 = bl & 0xFFFF;
	short bl_2 = (bl >> 16) & 0xFFFF;

	// verify the form
	RNE(bl_1 & 0xF800, 0xF000);
	RNE(bl_2 & 0xD000, 0xD000);

	// decode
	int S = (bl_1 & 0x0400) >> 10;
	int J1 = (bl_2 & 0x2000) >> 13;
	int J2 = (bl_2 & 0x0800) >> 11;
	int I1 = ~(J1 ^ S) & 1;
	int I2 = ~(J2 ^ S) & 1;
	int imm10 = bl_1 & 0x03FF;
	int imm11 = bl_2 & 0x07FF;

	// combine to 25 bits and sign extend
	*imm = (S << 31) | (I1 << 30) | (I2 << 29) | (imm10 << 19) | (imm11 << 8);
	*imm >>= 7;
	return 0;
}

static int decode_movw_t3(int movw, int *imm) {
	// split into two shorts
	short movw_1 = movw & 0xFFFF;
	short movw_2 = (movw >> 16) & 0xFFFF;

	// verify the form
	RNE(movw_1 & 0xFBF0, 0xF240);
	RNE(movw_2 & 0x8000, 0x0000);

	// decode
	int imm4 = movw_1 & 0x000F;
	int i = (movw_1 & 0x0400) >> 10;
	int imm8 = movw_2 & 0x00FF;
	int imm3 = (movw_2 & 0x7000) >> 12;

	// combine to 16 bits
	*(short*)imm = (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8;
	return 0;
}

static int decode_movt_t1(int movt, int *imm) {
	// split into two shorts
	short movt_1 = movt & 0xFFFF;
	short movt_2 = (movt >> 16) & 0xFFFF;

	// verify the form
	RNE(movt_1 & 0xFBF0, 0xF2C0);
	RNE(movt_2 & 0x8000, 0x0000);

	// decode
	int imm4 = movt_1 & 0x000F;
	int i = (movt_1 & 0x0400) >> 10;
	int imm8 = movt_2 & 0x00FF;
	int imm3 = (movt_2 & 0x7000) >> 12;

	// combine to 16 bits
	*((short*)imm + 1) = (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8;
	return 0;
}

static void poweroff_btn_hold_cb(void) {
	sceShellUtilRequestColdReset(0);
}

static void btn_init_hook(int r0, int r1, int r2, int r3) {
	if (r1 == 0x10000008 && r2 == poweroff_btn_cb_addr) {
		// set holdable button with threshold 200ms
		// and repeat threshold 800ms
		ScePafWidget_16479BA7(r0, 200, 800);

		TAI_CONTINUE(void, hook_ref[0], r0, r1, r2, r3);
		TAI_CONTINUE(void, hook_ref[0], r0, 0x10000005, poweroff_btn_hold_cb, r3);

		// set holdable with physical button
		*(char*)(r0 + 0x197) &= 0xFD;
	} else {
		TAI_CONTINUE(void, hook_ref[0], r0, r1, r2, r3);
	}
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
	int seg0 = (int)sce_minfo.segments[0].vaddr;

	// template ID FAE265F7 from impose_plugin.rco
	int quick_menu_init_ofs;

	switch(minfo.module_nid) {
		case 0x0552F692: // 3.60 retail
		case 0x532155E5: // 3.61 retail
			quick_menu_init_ofs = 0x14C408;
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
			quick_menu_init_ofs = 0x14C460;
			break;
		case 0x587F9CED: // 3.65 testkit
			quick_menu_init_ofs = 0x144894;
			break;
		case 0x6CB01295: // 3.60 Devkit
			quick_menu_init_ofs = 0x143E40;
			break;
		default:
			goto fail;
	}

	// addr of branch to btn_init from quick_menu_init
	int btn_init_call_addr = seg0 + quick_menu_init_ofs + 0x5B8;

	// addr of the function poweroff_btn_cb
	GLZ(decode_movw_t3(*(int*)(btn_init_call_addr - 0x12), &poweroff_btn_cb_addr));
	GLZ(decode_movt_t1(*(int*)(btn_init_call_addr - 0x0C), &poweroff_btn_cb_addr));

	// addr of the function btn_init
	int btn_init_addr;
	GLZ(decode_bl_t1(*(int*)btn_init_call_addr, &btn_init_addr));
	btn_init_addr += btn_init_call_addr + 4;

	// enable power widget (cmp r0, r0)
	GLZ(INJECT_DATA(0, minfo.modid, 0, quick_menu_init_ofs + 0x44E, "\x80\x42", 2));

	GLZ(HOOK_OFFSET(0, minfo.modid, btn_init_addr - seg0, btn_init));

	return SCE_KERNEL_START_SUCCESS;

fail:
	cleanup();
	return SCE_KERNEL_START_FAILED;
}

int module_stop(SceSize argc, const void *argv) { (void)argc; (void)argv;
	cleanup();
	return SCE_KERNEL_STOP_SUCCESS;
}
