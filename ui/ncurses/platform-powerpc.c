/*
 *  powerpc platform support
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "talloc/talloc.h"

#include "platform.h"


enum powerpc_ui_sig {
	powerpc_ui_sig = 111,
};

struct powerpc_ui {
	enum powerpc_ui_sig sig;
	struct cui *cui;
};

static __attribute__((unused)) struct powerpc_ui *powerpc_from_cui(
	struct cui *cui)
{
	struct powerpc_ui *ui;

	assert(cui->sig == pb_cui_sig);
	ui = cui->platform.data;
	assert(ui->sig == powerpc_ui_sig);
	return ui;
}

static __attribute__((unused)) struct powerpc_ui *powerpc_from_item(
	struct pmenu_item *item)
{
	struct cui *cui;
	struct powerpc_ui *ui;

	assert(item->sig == pb_item_sig);

	cui = cui_from_item(item);
	assert(cui->sig == pb_cui_sig);

	ui = cui->platform.data;
	assert(ui->cui->sig == pb_cui_sig);

	return ui;
}

static int powerpc_screen_update(struct cui *cui)
{
	bool repost_menu;

	/* we'll need to update the menu: drop all items and repopulate */
	repost_menu = cui->current_scr == cui->main_scr ||
		cui->current_scr == cui->plugin_scr;
	if (repost_menu)
		nc_scr_unpost(cui->current_scr);

	talloc_free(cui->main_scr->pmenu);
	cui->main_scr = main_scr_init(cui);

	talloc_free(cui->plugin_scr->pmenu);
	cui->plugin_scr = plugin_scr_init(cui);

	if (repost_menu) {
		cui->current_scr = cui->main_scr;
		nc_scr_post(cui->current_scr);
	}

	return 0;
}

int platform_init(struct cui *cui)
{
	struct powerpc_ui *ui;

	pb_debug_fl("->\n");

	ui = talloc_zero(cui, struct powerpc_ui);

	ui->sig = powerpc_ui_sig;
	ui->cui = cui;

	cui->platform.data = ui;
	cui->platform.screen_update = powerpc_screen_update;

	ui->cui->main_scr = main_scr_init(ui->cui);
	
	if (!ui->cui->main_scr) {
		return -1;
	}

	cui->plugin_scr = plugin_scr_init(cui);

	if (!cui->plugin_scr) {
		return -1;
	}

	pb_debug_fl("<-\n");
	return 0;
}
