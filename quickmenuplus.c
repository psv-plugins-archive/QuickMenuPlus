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

#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>

#include <psp2/avconfig.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/paf.h>
#include <psp2/registrymgr.h>
#include <psp2/shellsvc.h>
#include <psp2/vshbridge.h>

#include <taihen.h>

#include "common.h"
#include "config.h"
#include "opcode.h"
#include "scepaf.h"

#define INJECT_ABS(idx, dest, data, size)\
	(inject_id[idx] = taiInjectAbs(dest, data, size))

#define HOOK_OFFSET(idx, modid, offset, thumb, func)\
	(hook_id[idx] = taiHookFunctionOffset(hook_ref+idx, modid, 0, offset, thumb, func##_hook))

#define N_INJECT 3
static SceUID inject_id[N_INJECT];

#define N_HOOK 6
static SceUID hook_id[N_HOOK];
static tai_hook_ref_t hook_ref[N_HOOK];

typedef void btn_cb(void);
typedef int set_slidebar_pos(int, int, int);

static int (*vol_widget_init)(int, ScePafWidget*);

#define N_LANG 20

static const SceWChar16 *restart_text[N_LANG] = {
	u"再起動",
	u"Restart",
	u"Redémarrer",
	u"Reiniciar",
	u"Neustarten",
	u"Riavvio",
	u"Start opnieuw op",
	u"Reiniciar",
	u"Перезагрузка",
	u"재부팅",
	u"重新啟動",
	u"重新启動",
	u"Käynnistä uudelleen",
	u"Starta om",
	u"Genstart",
	u"Starte på nytt",
	u"Uruchom ponownie",
	u"Reiniciar",
	u"Restart",
	u"Yeniden başlatmak",
};

static bool standby_is_restart = false;

static atomic_int system_volume;

static void request_cold_reset(void) {
	sceShellUtilRequestColdReset(0);
}

static void set_widget_labelf(ScePafWidget *widget, const SceWChar16 *fmt, ...) {
	SceWChar16 *buf = sce_paf_malloc(0x100 * sizeof(SceWChar16));
	if (buf) {
		va_list args;
		va_start(args, fmt);
		sce_paf_vswprintf(buf, 0x100, fmt, args);
		va_end(args);

		ScePafWString wlabel = {buf, sce_paf_wcslen(buf)};
		widget->vptr->set_label(widget, &wlabel);

		// Always free wlabel.data, because the original pointer
		// might already have been freed.
		sce_paf_free(wlabel.data);
	}
}

static const SceWChar16 *get_text_lang(const SceWChar16 **texts) {
	int lang;
	if (0 == sceRegMgrUtilityGetInt(0x37502, &lang) && lang < N_LANG) {
		return texts[lang];
	} else {
		return texts[0];
	}
}

static const SceWChar16 *get_label(SceUInt32 id, const SceWChar16 *default_label) {
	const SceWChar16 *ret = NULL;

	ScePafPlugin *impose_plugin = scePafPluginGetByName("impose_plugin");
	if (impose_plugin) {
		ScePafResourceSearchParam param = {NULL, 0, 0, id};

		// The return value of this function should not be freed.
		ret = scePafLabelFindById(impose_plugin, &param);
	}

	if (!ret) {
		ret = default_label;
	}
	return ret;
}

static ScePafWidget *get_widget(ScePafWidget *parent, SceUInt32 id) {
	ScePafResourceSearchParam param = {NULL, 0, 0, id};
	return scePafWidgetFindById(parent, &param, 0);
}

static void set_btn_colour(ScePafWidget *widget, SceUInt32 colour) {
	float _colour[4] = {
		(float)((colour >> 0x18) & 0xFF) / 255.0,
		(float)((colour >> 0x10) & 0xFF) / 255.0,
		(float)((colour >> 0x08) & 0xFF) / 255.0,
		(float)((colour >> 0x00) & 0xFF) / 255.0,
	};

	// style objs are indexed in the order they appear in the RCO XML
	ScePafStyle *plane_obj = widget->vptr->get_style_obj(widget, 0);
	if (plane_obj) {
		plane_obj->vptr->set_colour(plane_obj, _colour);
	}
}

static void set_power_text(ScePafWidget *parent) {
	ScePafWidget *box_widget = get_widget(parent, QUICK_MENU_BOX_ID);
	if (box_widget) {
		ScePafWidget *text_widget = get_widget(box_widget, POWER_TEXT_ID);
		if (text_widget) {
			set_widget_labelf(text_widget, u"%s\xF8EB", get_label(POWER_LABEL_ID, u""));
		}
	}
}

static void set_volume_text(ScePafWidget *parent) {
	ScePafWidget *text_widget = get_widget(parent, VOLUME_TEXT_ID);
	if (text_widget) {
		set_widget_labelf(text_widget, u"%s\xF8EB", get_label(VOLUME_LABEL_ID, u""));
	}
}

static void btn_init_hook(ScePafWidget *widget, int cb_type, btn_cb *cb, int r3) {

	if (standby_is_restart && widget->id == STANDBY_BUTTON_ID && cb_type == 0x10000008) {
		set_power_text(((ScePafWidget**)r3)[1]);

		set_widget_labelf(widget, u"%s", get_text_lang(restart_text));
		set_btn_colour(widget, 0x156AA2FF);
		TAI_NEXT(btn_init_hook, hook_ref[0], widget, cb_type, request_cold_reset, r3);

	} else if (!standby_is_restart && widget->id == POWEROFF_BUTTON_ID && cb_type == 0x10000008) {
		set_power_text(((ScePafWidget**)r3)[1]);

		set_widget_labelf(widget, u"%s・%s", get_label(POWEROFF_LABEL_ID, u""), get_text_lang(restart_text));
		set_btn_colour(widget, 0xCC8F00FF);

		// set holdable button with threshold 200ms and repeat threshold 800ms
		scePafButtonSetRepeat(widget, 200, 800);

		TAI_NEXT(btn_init_hook, hook_ref[0], widget, cb_type, cb, r3);
		TAI_NEXT(btn_init_hook, hook_ref[0], widget, 0x10000005, request_cold_reset, r3);

		// set holdable with physical button
		widget->flags &= 0xFD;

	} else {
		TAI_NEXT(btn_init_hook, hook_ref[0], widget, cb_type, cb, r3);
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
	atomic_store(&system_volume, pos);

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

static int music_widget_init_hook(int r0, ScePafWidget *parent) {
	vol_widget_init(r0 + 0x40, parent);
	set_volume_text(parent);
	return TAI_NEXT(music_widget_init_hook, hook_ref[4], r0, parent);
}

// For reference on the function this function is hooking,
// see https://git.shotatoshounenwachigau.moe/vita/jav
static int process_volume_hook(int *audio_info, void *button_info) {
	int sys_vol = atomic_exchange(&system_volume, -1);
	if (sys_vol >= 0) {
		audio_info[5] = sys_vol;
	}
	return TAI_NEXT(process_volume_hook, hook_ref[5], audio_info, button_info);
}

static void startup(void) {
	sceClibMemset(inject_id, 0xFF, sizeof(inject_id));
	sceClibMemset(hook_id, 0xFF, sizeof(hook_id));
	sceClibMemset(hook_ref, 0xFF, sizeof(hook_ref));
	atomic_init(&system_volume, -1);
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

	int pushtime = config_read_key("pushtime");
	if (pushtime) {
		vshPowerSetPsButtonPushTime(pushtime);
	}

	standby_is_restart = config_read_key("standbyisrestart");

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
		GLZ(HOOK_OFFSET(5, minfo.modid, quick_menu_init - 0x6FE6 - seg0, 1, process_volume));
	}

	// Disable quick menu gradient effect (cmp r0, r0)
	GLZ(INJECT_ABS(2, (void*)(quick_menu_init + 0x186), "\x80\x42", 2));

	return SCE_KERNEL_START_SUCCESS;

fail:
	cleanup();
	return SCE_KERNEL_START_FAILED;
}

USED int module_stop(UNUSED SceSize args, UNUSED const void *argp) {
	cleanup();
	return SCE_KERNEL_STOP_SUCCESS;
}
