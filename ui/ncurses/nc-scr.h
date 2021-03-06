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

#if !defined(_PB_NC_SCR_H)
#define _PB_NC_SCR_H

#include <linux/input.h> /* This must be included before ncurses.h */
#if defined HAVE_NCURSESW_CURSES_H
#  include <ncursesw/curses.h>
#elif defined HAVE_NCURSESW_H
#  include <ncursesw.h>
#elif defined HAVE_NCURSES_CURSES_H
#  include <ncurses/curses.h>
#elif defined HAVE_NCURSES_H
#  include <ncurses.h>
#elif defined HAVE_CURSES_H
#  include <curses.h>
#else
#  error "Curses header file not found."
#endif

#include "types/types.h"

#define DBG(fmt, args...) pb_debug("DBG: " fmt, ## args)
#define DBGS(fmt, args...) \
	pb_debug("DBG:%s:%d: " fmt, __func__, __LINE__, ## args)


enum pb_nc_sig {
	pb_cui_sig		= 111,
	pb_cui_opt_data_sig,
	pb_pmenu_sig		= 222,
	pb_item_sig		= 333,

	pb_screen_sig_min	= 444,
	pb_screen_sig,
	pb_main_screen_sig,
	pb_boot_editor_sig,
	pb_text_screen_sig,
	pb_config_screen_sig,
	pb_lang_screen_sig,
	pb_add_url_screen_sig,
	pb_subset_screen_sig,
	pb_plugin_screen_sig,
	pb_auth_screen_sig,
	pb_screen_sig_max,

	pb_removed_sig		= -999,
};

static inline const char *sig_str(enum pb_nc_sig sig)
{
#if !defined(DEBUG)
	if (0) {
#endif
	switch (sig) {
	case pb_cui_sig:
		return "cui_sig";
	case pb_cui_opt_data_sig:
		return "cui_opt_data_sig";
	case pb_pmenu_sig:
		return "pmenu_sig";
	case pb_item_sig:
		return "item_sig";

	case pb_screen_sig_min:
		return "screen_sig_min";
	case pb_screen_sig:
		return "screen_sig";
	case pb_main_screen_sig:
		return "main_screen_sig";
	case pb_boot_editor_sig:
		return "boot_editor_sig";
	case pb_text_screen_sig:
		return "text_screen_sig";
	case pb_config_screen_sig:
		return "config_screen_sig";
	case pb_lang_screen_sig:
		return "lang_screen_sig";
	case pb_add_url_screen_sig:
		return "add_url_screen_sig";
	case pb_subset_screen_sig:
		return "subset_screen_sig";
	case pb_plugin_screen_sig:
		return "plugin_screen_sig";
	case pb_auth_screen_sig:
		return "auth_screen_sig";
	case pb_screen_sig_max:
		return "screen_sig_max";

	case pb_removed_sig:
		return "removed_sig";
	default:
		pb_debug_fl("ERROR: unknown sig");
		return "ERROR: unknown sig";
	}
#if !defined(DEBUG)
	}
	return "";
#endif
};

#define pb_debug_scr_sig(_scr1) pb_debug_fl("%s:%p:%d\n", \
	sig_str(_scr1->sig), _scr1, _scr1->sig)
#define pb_debug_scr_sig_cmp(_scr1, _scr2) pb_debug_fl("%s:%p:%d -> %s:%p:%d\n", \
	sig_str(_scr1->sig), _scr1, _scr1->sig, \
	sig_str(_scr2->sig), _scr2, _scr2->sig)

static inline bool scr_sig_check(enum pb_nc_sig sig)
{
	return (sig > pb_screen_sig_min && sig < pb_screen_sig_max);
}

static inline void nc_flush_keys(void)
{
	while (getch() != ERR)
		(void)0;
}

enum nc_scr_pos {
	nc_scr_pos_title = 0,
	nc_scr_pos_title_sep = 1,
	nc_scr_pos_lrtitle_space = 2,
	nc_scr_pos_sub = 2,

	nc_scr_pos_help_sep = 3,
	nc_scr_pos_help = 2,
	nc_scr_pos_status = 1,

	nc_scr_frame_lines = 5,
	nc_scr_frame_cols = 1,
};

struct nc_frame {
	char *ltitle;
	char *rtitle;
	char *help;
	char *status;
};

struct nc_scr {
	enum pb_nc_sig sig;
	void *container;
	struct cui *cui;
	struct nc_scr *return_scr;
	struct pmenu *pmenu;
	struct nc_frame frame;
	WINDOW *main_ncw;
	WINDOW *sub_ncw;
	void (*process_key)(struct nc_scr *scr, int key);
	int (*post)(struct nc_scr *scr);
	int (*unpost)(struct nc_scr *scr);
	int (*update)(struct nc_scr *scr);
	void (*resize)(struct nc_scr *scr);
};

struct nc_scr *nc_scr_init(void *container, enum pb_nc_sig sig,
	struct cui *cui, int begin_x,
	void (*process_key)(struct nc_scr *scr, int key),
	int (*post)(struct nc_scr *scr),
	int (*unpost)(struct nc_scr *scr),
	int (*update)(struct nc_scr *scr),
	void (*resize)(struct nc_scr *scr));

void nc_scr_status_free(struct nc_scr *scr);
void nc_scr_status_printf(struct nc_scr *scr, const char *format, ...);
void nc_scr_frame_draw(struct nc_scr *scr);
void nc_scr_set_default_rtitle(struct nc_scr *scr,
	const struct system_info *sysinfo);

int nc_scr_post(struct nc_scr *scr);
int nc_scr_unpost(struct nc_scr *scr);

#endif
