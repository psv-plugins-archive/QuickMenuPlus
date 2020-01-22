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

#define RNE(x, k) do {\
	if ((x) != (k)) { return -1; }\
} while(0)

#define INJECT_ABS(idx, dest, data, size)\
	(inject_id[idx] = taiInjectAbs(dest, data, size))

#define HOOK_OFFSET(idx, modid, offset, thumb, func)\
	(hook_id[idx] = taiHookFunctionOffset(hook_ref+idx, modid, 0, offset, thumb, func##_hook))

#define N_INJECT 1
static SceUID inject_id[N_INJECT];

#define N_HOOK 4
static SceUID hook_id[N_HOOK];
static tai_hook_ref_t hook_ref[N_HOOK];

static int (*master_volume_widget_init)(int r0, int r1);

typedef int (*set_slidebar_ptr)(int, int, int);

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

static int decode_blx_t2(int blx, int *imm) {
	// split into two shorts
	short blx_1 = blx & 0xFFFF;
	short blx_2 = (blx >> 16) & 0xFFFF;

	// verify the form
	RNE(blx_1 & 0xF800, 0xF000);
	RNE(blx_2 & 0xD001, 0xC000);

	// decode
	int S = (blx_1 & 0x0400) >> 10;
	int J1 = (blx_2 & 0x2000) >> 13;
	int J2 = (blx_2 & 0x0800) >> 11;
	int I1 = ~(J1 ^ S) & 1;
	int I2 = ~(J2 ^ S) & 1;
	int imm10H = blx_1 & 0x03FF;
	int imm10L = (blx_2 & 0x07FE) >> 1;

	// combine to 25 bits and sign extend
	*imm = (S << 31) | (I1 << 30) | (I2 << 29) | (imm10H << 19) | (imm10L << 9);
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

// cannot TAI_CONTINUE due to lack of relocation by taiHEN
static int slidebar_callback_hook(int a) {
	int obj = *(int*)(a + 0x8);
	int vptr = *(int*)obj;
	int pos = *(int*)(obj + 0x294);

	int r0, r1, avls;
	RLZ(sceAVConfigGetVolCtrlEnable(&r0, &r1, &avls));
	if (avls && pos > AVLS_MAX) {
		pos = AVLS_MAX;
		RLZ(sceAVConfigSetSystemVol(pos));
		(*(set_slidebar_ptr*)(vptr + 0x188))(obj, pos, 0);
	} else {
		RLZ(sceAVConfigSetSystemVol(pos));
	}

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
	int seg0 = (int)sce_minfo.segments[0].vaddr;

	// template ID 84E0F33A from impose_plugin.rco
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
		case 0x6CB01295: // 3.60 devkit
			quick_menu_init_ofs = 0x143E40;
			break;
		default:
			goto fail;
	}

	// addr of master_volume_widget_init
	int mvol_widget_init_call_addr = seg0 + quick_menu_init_ofs + 0xC1E;
	GLZ(decode_bl_t1(*(int*)mvol_widget_init_call_addr, (int*)&master_volume_widget_init));
	master_volume_widget_init += mvol_widget_init_call_addr + 4;

	// offset of slidebar_callback
	int slidebar_callback_offset;
	GLZ(decode_movw_t3(*(int*)(master_volume_widget_init + 0x1E0), &slidebar_callback_offset));
	GLZ(decode_movt_t1(*(int*)(master_volume_widget_init + 0x1E6), &slidebar_callback_offset));
	slidebar_callback_offset = *(int*)slidebar_callback_offset - seg0;

	// offset of sceAVConfig imports
	// We hook these by offset because if SceShell is loaded before taiHEN,
	// i.e. when not using Enso, the NIDs of its imports are overwritten.
	int sceAVConfigGetMasterVol_ofs, sceAVConfigWriteMasterVol_ofs;
	GLZ(decode_blx_t2(*(int*)(master_volume_widget_init + 0x346), &sceAVConfigGetMasterVol_ofs));
	unsigned int pc = (unsigned int)master_volume_widget_init + 0x346 + 0x4;
	sceAVConfigGetMasterVol_ofs += pc - (pc % 4) - seg0;
	sceAVConfigWriteMasterVol_ofs = sceAVConfigGetMasterVol_ofs - 0x80;

	// set Thumb bit
	master_volume_widget_init = (void*)((int)master_volume_widget_init | 1);

	// offset of music_widget_init
	int music_widget_init_offset;
	GLZ(decode_bl_t1(*(int*)(seg0 + quick_menu_init_ofs + 0x1006), &music_widget_init_offset));
	music_widget_init_offset += quick_menu_init_ofs + 0x1006 + 4;

	// disable the original call to master_volume_widget_init (mov.w r0, #0)
	GLZ(INJECT_ABS(0, (void*)mvol_widget_init_call_addr, "\x4f\xf0\x00\x00", 4));

	GLZ(HOOK_OFFSET(0, minfo.modid, slidebar_callback_offset, 1, slidebar_callback));
	GLZ(HOOK_OFFSET(3, minfo.modid, music_widget_init_offset, 1, music_widget_init));
	GLZ(HOOK_OFFSET(1, minfo.modid, sceAVConfigGetMasterVol_ofs, 0, sceAVConfigGetMasterVol));
	GLZ(HOOK_OFFSET(2, minfo.modid, sceAVConfigWriteMasterVol_ofs, 0, sceAVConfigWriteMasterVol));

	return SCE_KERNEL_START_SUCCESS;

fail:
	cleanup();
	return SCE_KERNEL_START_FAILED;
}

int module_stop(SceSize argc, const void *argv) { (void)argc; (void)argv;
	cleanup();
	return SCE_KERNEL_STOP_SUCCESS;
}
