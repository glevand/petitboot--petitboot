/*
 *  Copyright (C) 2009 Sony Computer Entertainment Inc.
 *  Copyright 2009 Sony Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if !defined(_PB_NC_CUI_H)
#define _PB_NC_CUI_H

#include <signal.h>

#include "ui/common/joystick.h"
#include "nc-menu.h"
#include "nc-helpscreen.h"

struct cui_opt_data {
	char *name;
	union {
		struct pb_boot_data *bd;
		struct pb_plugin_data *pd;
	};

	/* optional data */
	const struct device *dev;
	const struct boot_option *opt;
	uint32_t opt_hash;
};

/**
 * struct cui - Data structure defining a cui state machine.
 * @c_sig: Signature for callback type checking, should be cui_sig.
 * @abort: When set to true signals the state machine to exit.
 * @current: Pointer to the active nc object.
 * @main: Pointer to the user supplied main menu.
 *
 * Device boot_options are dynamically added and removed from the @main
 * menu.
 */

struct cui {
	enum pb_nc_sig sig;
	bool has_input;
	struct autoboot_option *autoboot_opt;
	sig_atomic_t abort;
	sig_atomic_t resize;
	struct nc_scr *current_scr;
	struct pmenu *main;
	struct pmenu *plugin_menu;
	unsigned int n_plugins;
	struct waitset *waitset;
	struct discover_client *client;
	struct system_info *sysinfo;
	struct statuslog *statuslog;
	struct sysinfo_screen *sysinfo_screen;
	struct config *config;
	struct config_screen *config_screen;
	struct add_url_screen *add_url_screen;
	struct plugin_screen *plugin_screen;
	struct boot_editor *boot_editor;
	struct lang_screen *lang_screen;
	struct help_screen *help_screen;
	struct subset_screen *subset_screen;
	struct statuslog_screen *statuslog_screen;
	struct auth_screen *auth_screen;
	void *platform_info;
	unsigned int default_item;
	int (*on_boot)(struct cui *cui, struct cui_opt_data *cod);
	bool preboot_mode;
};

struct cui *cui_init(int timeout);
struct nc_scr *cui_set_current(struct cui *cui, struct nc_scr *scr);
int cui_run(struct cui *cui);
int cui_process_key(struct cui *cui);

void cui_item_edit(struct pmenu_item *item);
void cui_item_new(struct pmenu *menu);
void cui_show_sysinfo(struct cui *cui);
void cui_show_config(struct cui *cui);
void cui_show_lang(struct cui *cui);
void cui_show_statuslog(struct cui *cui);
void cui_show_help(struct cui *cui, const char *title,
		const struct help_text *text);
void cui_show_subset(struct cui *cui, const char *title,
		void *arg);
void cui_show_add_url(struct cui *cui);
void cui_show_plugin(struct pmenu_item *item);
void cui_show_plugin_menu(struct cui *cui);
void cui_show_auth(struct cui *cui, WINDOW *parent, bool set_password,
		void (*callback)(struct nc_scr *));
void cui_show_open_luks(struct cui *cui, WINDOW *parent,
		const struct device *dev);
int cui_send_config(struct cui *cui, struct config *config);
int cui_send_url(struct cui *cui, char *url);
int cui_send_plugin_install(struct cui *cui, char *file);
int cui_send_authenticate(struct cui *cui, char *password);
int cui_send_set_password(struct cui *cui, char *password, char *new_password);
int cui_send_open_luks_device(struct cui *cui, char *password, char *device_id);
void cui_send_reinit(struct cui *cui);

/* convenience routines */

void cui_abort(struct cui *cui);
void cui_resize(struct cui *cui);
void cui_on_exit(struct pmenu *menu);
void cui_abort_on_exit(struct pmenu *menu);
void cui_on_open(struct pmenu *menu);
int cui_run_cmd(struct cui *cui, const char **cmd_argv);
int cui_run_cmd_from_item(struct pmenu_item *item);
void cui_update_language(struct cui *cui, const char *lang);

static inline struct cui *cui_from_arg(void *arg)
{
	struct cui *cui = (struct cui *)arg;

	assert(cui->sig == pb_cui_sig);
	return cui;
}

static inline struct cui *cui_from_scr(struct nc_scr *scr)
{
	assert(scr->cui->sig == pb_cui_sig);
	return scr->cui;
}

static inline struct cui *cui_from_pmenu(struct pmenu *menu)
{
	return menu->scr.cui;
}

static inline struct cui *cui_from_item(struct pmenu_item *item)
{
	assert(item->sig == pb_item_sig);
	return cui_from_pmenu(item->pmenu);
}

#endif
