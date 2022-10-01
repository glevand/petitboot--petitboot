/*
 *  ps3 platform support
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
//#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
//#include <locale.h>
#include <string.h>
//#include <sys/ioctl.h>
//#include <sys/reboot.h>

//#include "log/log.h"
//#include "pb-protocol/pb-protocol.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
#include "platform/ps3.h"
//#include "process/process.h"
//#include "i18n/i18n.h"
//#include "ui/common/discover-client.h"
//#include "ui/common/ui-system.h"

//#include "nc-boot-editor.h"
//#include "nc-config.h"
//#include "nc-add-url.h"
//#include "nc-sysinfo.h"
//#include "nc-lang.h"
//#include "nc-helpscreen.h"
//#include "nc-statuslog.h"
//#include "nc-subset.h"
//#include "nc-plugin.h"
//#include "nc-auth.h"
//#include "console-codes.h"

#include "platform.h"

enum ps3_ui_sig {
	ps3_ui_sig = 111,
	ps3_svm_scr_sig = 222,
};

struct ps3_ui {
	enum ps3_ui_sig sig;
	struct cui *cui;
	struct nc_scr *svm_scr;
	struct pjs *pjs;
	//struct ps3_flash_values flash_values;
};

static struct ps3_ui *ps3_from_cui(struct cui *cui)
{
	struct ps3_ui *ps3;

	assert(cui->sig == pb_cui_sig);
	ps3 = cui->platform.data;
	assert(ps3->sig == ps3_ui_sig);
	return ps3;
}

static struct ps3_ui *ps3_from_item(struct pmenu_item *item)
{
	struct cui *cui;
	struct ps3_ui *ps3;

	assert(item->sig == pb_item_sig);

	cui = cui_from_item(item);
	assert(cui->sig == pb_cui_sig);

	ps3 = cui->platform.data;
	assert(ps3->cui->sig == pb_cui_sig);

	return ps3;
}

static int ps3_sixaxis_map(const struct js_event *e)
{
#if 0
	static const int axis_map[] = {
		0,		/*   0  Left thumb X	*/
		0,		/*   1  Left thumb Y	*/
		0,		/*   2  Right thumb X	*/
		0,		/*   3  Right thumb Y	*/
		0,		/*   4  nothing		*/
		0,		/*   5  nothing		*/
		0,		/*   6  nothing		*/
		0,		/*   7  nothing		*/
		0,		/*   8  Dpad Up		*/
		0,		/*   9  Dpad Right	*/
		0,		/*  10  Dpad Down	*/
		0,		/*  11  Dpad Left	*/
		0,		/*  12  L2		*/
		0,		/*  13  R2		*/
		0,		/*  14  L1		*/
		0,		/*  15  R1		*/
		0,		/*  16  Triangle	*/
		0,		/*  17  Circle		*/
		0,		/*  18  Cross		*/
		0,		/*  19  Square		*/
		0,		/*  20  nothing		*/
		0,		/*  21  nothing		*/
		0,		/*  22  nothing		*/
		0,		/*  23  nothing		*/
		0,		/*  24  nothing		*/
		0,		/*  25  nothing		*/
		0,		/*  26  nothing		*/
		0,		/*  27  nothing		*/
	};
#endif
	static const int button_map[] = {
		0,		/*   0  Select		*/
		0,		/*   1  L3		*/
		0,		/*   2  R3		*/
		0,		/*   3  Start		*/
		KEY_UP,		/*   4  Dpad Up		*/
		0,		/*   5  Dpad Right	*/
		KEY_DOWN,	/*   6  Dpad Down	*/
		0,		/*   7  Dpad Left	*/
		KEY_UP,		/*   8  L2		*/
		KEY_DOWN,	/*   9  R2		*/
		KEY_HOME,	/*  10  L1		*/
		KEY_END,	/*  11  R1		*/
		0,		/*  12  Triangle	*/
		0,		/*  13  Circle		*/
		13,		/*  14  Cross		*/
		0,		/*  15  Square		*/
		0,		/*  16  PS Button	*/
		0,		/*  17  nothing		*/
		0,		/*  18  nothing		*/
	};

	if (!e->value)
		return 0;

	if (e->type == JS_EVENT_BUTTON
		&& e->number < sizeof(button_map) / sizeof(button_map[0]))
		return button_map[e->number];

#if 0
	if (e->type == JS_EVENT_AXIS
		&& e->number < sizeof(axis_map) / sizeof(axis_map[0]))
		return axis_map[e->number];
#endif

	return 0;
}

static int ps3_process_js(void *arg)
{
	struct ps3_ui *ps3 = arg;
	int c;
	
	assert(ps3->sig == ps3_ui_sig);

	c = pjs_process_event(ps3->pjs);

	if (c) {
		ungetch(c);
		cui_process_key(ps3->cui);
	}

	return 0;
}

static void ps3_setup_js(struct ps3_ui *ps3)
{
	ps3->pjs = pjs_init(ps3->cui, ps3_sixaxis_map);

	if (ps3->pjs) {
		waiter_register_io(ps3->cui->waitset, pjs_get_fd(ps3->pjs),
			WAIT_IN, ps3_process_js, ps3);
	}
}

static int ps3_svm_start_cb(struct pmenu_item *item)
{
	struct ps3_ui *ps3 = ps3_from_item(item);

	assert(!ps3->svm_scr->return_scr);

	nc_scr_set_default_rtitle(ps3->svm_scr, ps3->cui->sysinfo);
	ps3->svm_scr->return_scr = cui_set_current(ps3->cui, ps3->svm_scr);

	return 0;
}

static int ps3_svm_return_cb(struct pmenu_item *item)
{
	struct ps3_ui *ps3 = ps3_from_item(item);
	struct nc_scr *__attribute__((unused))old;

	assert(ps3->svm_scr->return_scr);

	old = cui_set_current(ps3->cui, ps3->svm_scr->return_scr);
	ps3->svm_scr->return_scr = NULL;

	assert(old == ps3->svm_scr);

	return 0;
}

static void ps3_svm_on_exit_cb(struct pmenu *menu)
{
	ps3_svm_return_cb(pmenu_find_selected(menu));
}

static int ps3_svm_cb(struct pmenu_item *item)
{
	struct ps3_ui *ps3 = ps3_from_item(item);
	uint16_t mode;
	int result;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
	mode = (uint16_t)(item->data);
#pragma GCC diagnostic pop

	pb_debug_fl("-> mode: %hu\n", mode);

	result = 0;
#if 0
	pb_debug_fl("-> mode: %hu => %hu\n", ps3->flash_values.video_mode, mode);

	if (ps3->flash_values.video_mode == mode)
		return 0;

	ps3->flash_values.video_mode = mode;
	ps3->flash_values.dirty = true;

	result = ps3_set_video_mode(mode);
#endif
	if (result) {
		nc_scr_status_printf(ps3->cui->current_scr,
			"Failed: set_video_mode(%hu)", mode);
	}

	pb_debug_fl("<-\n");
	return 0;
}

static struct pmenu *ps3_svm_menu_init(struct cui *cui)
{
	struct ps3_ui *ps3 = ps3_from_cui(cui);
	struct pmenu_item *i;
	struct pmenu *m;
	int result;

	m = pmenu_init(ps3->svm_scr, 12, ps3_svm_on_exit_cb);

	if (!m) {
		pb_log_fn("failed\n");
		return NULL;
	}

#if 1
	m->n_hot_keys = 0;
	m->hot_keys = NULL;
#else
	m->n_hot_keys = 1;
	m->hot_keys = talloc_array(m, hot_key_fn, m->n_hot_keys);
	if (!m->hot_keys) {
		pb_log_fn("failed to allocate hot_keys\n");
		talloc_free(m);
		return NULL;
	}
	m->hot_keys[0] = ps3_hot_key;
#endif

	m->help_title = "Set Video Mode menu";
	//m->help_text = &ps3_svm_menu_help_text;

	i =pmenu_item_create(m, "auto detect");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)0;
	pmenu_item_insert(m, i, 0);

	i =pmenu_item_create(m, "480i    (576 x 384)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)1;
	pmenu_item_insert(m, i, 1);

	i =pmenu_item_create(m, "480p    (576 x 384)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)2;
	pmenu_item_insert(m, i, 2);

	i =pmenu_item_create(m, "576i    (576 x 460)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)6;
	pmenu_item_insert(m, i, 3);

	i =pmenu_item_create(m, "576p    (576 x 460)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)7;
	pmenu_item_insert(m, i, 4);

	i =pmenu_item_create(m, "720p   (1124 x 644)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)3;
	pmenu_item_insert(m, i, 5);

	i =pmenu_item_create(m, "1080i  (1688 x 964)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)4;
	pmenu_item_insert(m, i, 6);

	i =pmenu_item_create(m, "1080p  (1688 x 964)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)5;
	pmenu_item_insert(m, i, 7);

	i =pmenu_item_create(m, "wxga   (1280 x 768)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)11;
	pmenu_item_insert(m, i, 8);

	i =pmenu_item_create(m, "sxga   (1280 x 1024)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)12;
	pmenu_item_insert(m, i, 9);

	i =pmenu_item_create(m, "wuxga  (1920 x 1200)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)13;
	pmenu_item_insert(m, i, 10);

	i =pmenu_item_create(m, "Return");
	i->on_execute = ps3_svm_return_cb;
	pmenu_item_insert(m, i, 11);

	result = pmenu_setup(m);

	if (result) {
		pb_log("%s:%d: pmenu_setup failed: %s\n", __func__, __LINE__,
			strerror(errno));
		goto fail_setup;
	}

	menu_opts_off(m->ncm, O_SHOWDESC);
	set_menu_mark(m->ncm, " *");

	return m;

fail_setup:
	talloc_free(m);
	return NULL;
}

static int ps3_svm_update(struct nc_scr *scr)
{
	(void)scr;
	return 0;
}

static void ps3_setup_svm(struct ps3_ui *ps3)
{
	ps3->svm_scr = nc_scr_init(ps3, (enum pb_nc_sig)ps3_svm_scr_sig,
		ps3->cui, 0, pmenu_process_key, pmenu_post, pmenu_unpost,
		ps3_svm_update, pmenu_resize);

	ps3->svm_scr->frame.ltitle = talloc_asprintf(ps3->svm_scr,
		"Select PS3 Video Mode");
	nc_scr_set_default_rtitle(ps3->svm_scr, ps3->cui->sysinfo);

	ps3->svm_scr->frame.help = talloc_strdup(ps3->svm_scr,
		"Enter=accept, x=exit, h=help");
	ps3->svm_scr->frame.status = talloc_strdup(ps3->svm_scr,
		"--frame.status todo--");

	ps3->svm_scr->pmenu = ps3_svm_menu_init(ps3->cui);

	// TODO: setupmain_menu_item
	//main_menu_item = pmenu_item_create(main_menu, "Set Video Mode");
	//main_menu_item->on_execute = ps3_mm_to_svm_cb;
	//cui_add_platform_menu(cui, main_menu, main_menu_item);
}

static struct nc_scr *ps3_main_init(struct cui *cui);

static int ps3_main_update(struct nc_scr *main)
{
	struct cui *cui = main->cui;

	if (main) {
		talloc_free(main);
	}

	main = ps3_main_init(cui);

	return 0;
}

static struct nc_scr *ps3_main_init(struct cui *cui)
{
	struct nc_scr *main;

	main = talloc_zero(cui, struct nc_scr);

	if (!main) {
		exit(EXIT_FAILURE);
	}

	main->update = ps3_main_update;
	
	ps3_main_update(NULL);

	return main;
}

int platform_init(struct cui *cui)
{
	struct ps3_ui *ps3;

	pb_debug_fl("->\n");

	ps3 = talloc_zero(cui, struct ps3_ui);

	ps3->sig = ps3_ui_sig;
	ps3->cui = cui;

	cui->platform.data = ps3;

	ps3->cui->main_scr = main_scr_init(ps3->cui);

	if (!ps3->cui->main_scr) {
		return -1;
	}

	cui->main_scr->pmenu = main_menu_init(cui->main_scr);

	cui->plugin_scr = plugin_scr_init(cui);

	if (!cui->plugin_scr) {
		return -1;
	}

	ps3_setup_svm(ps3);
	ps3_setup_js(ps3);
	
	pb_debug_fl("<-\n");

	return 0;
}
