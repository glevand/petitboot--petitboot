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
	ui = cui->platform_info;
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

	ui = cui->platform_info;
	assert(ui->cui->sig == pb_cui_sig);

	return ui;
}

int platform_init(struct cui *cui)
{
	struct powerpc_ui *ui;

	pb_debug_fl("->\n");

	ui = talloc_zero(cui, struct powerpc_ui);

	ui->sig = powerpc_ui_sig;
	ui->cui = cui;
	cui->platform_info = ui;

	pb_debug_fl("<-\n");
	return 0;
}
