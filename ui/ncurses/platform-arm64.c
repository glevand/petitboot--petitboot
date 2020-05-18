/*
 *  arm64 platform support
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "talloc/talloc.h"

#include "platform.h"


enum arm64_ui_sig {
	arm64_ui_sig = 111,
};

struct arm64_ui {
	enum arm64_ui_sig sig;
	struct cui *cui;
};

static __attribute__((unused)) struct arm64_ui *arm64_from_cui(
	struct cui *cui)
{
	struct arm64_ui *ui;

	assert(cui->sig == pb_cui_sig);
	ui = cui->platform_info;
	assert(ui->sig == arm64_ui_sig);
	return ui;
}

static __attribute__((unused)) struct arm64_ui *arm64_from_item(
	struct pmenu_item *item)
{
	struct cui *cui;
	struct arm64_ui *ui;

	assert(item->sig == pb_item_sig);

	cui = cui_from_item(item);
	assert(cui->sig == pb_cui_sig);

	ui = cui->platform_info;
	assert(ui->cui->sig == pb_cui_sig);

	return ui;
}

int platform_init(struct cui *cui)
{
	struct arm64_ui *ui;

	pb_debug_fl("->\n");

	ui = talloc_zero(cui, struct arm64_ui);

	ui->sig = arm64_ui_sig;
	ui->cui = cui;
	cui->platform_info = ui;

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
