/*
 *  Copyright (C) 2013 IBM Corporation
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <string.h>

#include <talloc/talloc.h>
#include <types/types.h>
#include <log/log.h>
#include <fold/fold.h>
#include <util/util.h>
#include <i18n/i18n.h>

#include "nc-cui.h"
#include "nc-textscreen.h"

static struct text_screen *text_screen_from_scr(struct nc_scr *scr)
{
	struct text_screen *screen;

	assert(scr->sig == pb_text_screen_sig);
	assert(scr->container);

	screen = scr->container;

	assert(screen->scr);
	assert(screen->scr->sig == pb_text_screen_sig);

	return screen;
}

void text_screen_draw(struct text_screen *screen)
{
	int max_y, max_x, i, len;

	assert(screen);
	assert(screen->scr);
	assert(screen->scr->sig == pb_text_screen_sig);
	assert(screen->scr->sub_ncw);

	max_y = getmaxy(screen->scr->sub_ncw);
	max_x = getmaxx(screen->scr->sub_ncw) - 1;

	max_y = min(max_y, screen->scroll_y + screen->n_lines);

	for (i = screen->scroll_y; i < max_y; i++) {
		len = strncols(screen->lines[i]) > max_x ? max_x : -1;
		mvwaddnstr(screen->scr->sub_ncw, i, 1, screen->lines[i], len);
	}

	wrefresh(screen->scr->sub_ncw);
}

static void text_screen_scroll(struct text_screen *screen, int key)
{
	int win_lines = getmaxy(screen->scr->sub_ncw);
	int win_cols = getmaxx(screen->scr->sub_ncw) - 1;
	int delta, len, i;

	if (key == KEY_UP)
		delta = -1;
	else if (key == KEY_DOWN)
		delta = 1;
	else
		return;

	if (screen->scroll_y + delta < 0)
		return;
	if (screen->scroll_y + delta + win_lines > screen->n_lines)
		return;

	screen->scroll_y += delta;
	wscrl(screen->scr->sub_ncw, delta);


	if (delta > 0) {
		i = screen->scroll_y + win_lines - 1;
		len = strncols(screen->lines[i]) > win_cols ? win_cols : -1;
		mvwaddnstr(screen->scr->sub_ncw, win_lines - 1, 1,
				screen->lines[i], len);
	} else if (delta < 0) {
		i = screen->scroll_y;
		len = strncols(screen->lines[i]) > win_cols ? win_cols : -1;
		mvwaddnstr(screen->scr->sub_ncw, 0, 1, screen->lines[i], len);
	}

	wrefresh(screen->scr->sub_ncw);
}

void text_screen_clear(struct text_screen *screen)
{
	talloc_free(screen->lines);
	screen->n_lines = 0;
	screen->n_alloc_lines = 16;
	screen->lines = talloc_array(screen, const char *,
			screen->n_alloc_lines);
}

static void __text_screen_append_line(struct text_screen *screen,
		const char *line)
{
	if (screen->n_lines == screen->n_alloc_lines) {
		screen->n_alloc_lines *= 2;
		screen->lines = talloc_realloc(screen, screen->lines,
						const char *,
						screen->n_alloc_lines);
	}

	screen->lines[screen->n_lines] = line;
	screen->n_lines++;
}

void text_screen_append_line(struct text_screen *screen, const char *fmt, ...)
{
	char *line;
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		line = talloc_vasprintf(screen->lines, fmt, ap);
		va_end(ap);
	} else {
		line = "";
	}

	__text_screen_append_line(screen, line);
}

static int text_screen_fold_cb(void *arg, const char *buf, int len)
{
	struct text_screen *screen = arg;

	buf = len ? talloc_strndup(screen->lines, buf, len) : "";
	__text_screen_append_line(screen, buf);

	return 0;
}

void text_screen_set_text(struct text_screen *screen, const char *text)
{
	fold_text(text, getmaxx(screen->scr->sub_ncw) - 1, text_screen_fold_cb,
			screen);
}

void text_screen_process_key(struct nc_scr *scr, int key)
{
	struct text_screen *screen = text_screen_from_scr(scr);

	switch (key) {
	case 'x':
	case 27: /* esc */
		screen->on_exit(screen->cui);
		break;
	case KEY_DOWN:
	case KEY_UP:
		text_screen_scroll(screen, key);
		break;
	case 'h':
		if (screen->help_text)
			cui_show_help(screen->cui, screen->help_title,
					screen->help_text);
		break;
	default:
		break;
	}
}

static void text_screen_resize(struct nc_scr *scr)
{
	struct text_screen *screen = text_screen_from_scr(scr);
	text_screen_draw(screen);
}

struct nc_scr *text_screen_scr(struct text_screen *screen)
{
	assert(screen->scr);
	assert(screen->scr->sig == pb_text_screen_sig);

	return screen->scr;
}

void text_screen_set_help(struct text_screen *screen, const char *title,
		const struct help_text *text)
{
	screen->help_title = title;
	screen->help_text = text;
	screen->scr->frame.help = _("x=exit, h=help");
}

static int text_screen_post(struct nc_scr *scr)
{
	struct text_screen *screen = text_screen_from_scr(scr);

	if (screen->need_update) {
		text_screen_draw(screen);
		screen->need_update = false;
	}

	nc_scr_frame_draw(scr);
	redrawwin(scr->main_ncw);
	wrefresh(scr->main_ncw);
	return 0;
}

void text_screen_init(struct text_screen *screen, struct cui *cui,
		const char *title, void (*on_exit)(struct cui *))
{
	screen->scr = nc_scr_init(screen, pb_text_screen_sig, cui, 0,
			text_screen_process_key, text_screen_post,
			NULL, text_screen_resize);

	/* this will establish our array of lines */
	screen->lines = NULL;
	text_screen_clear(screen);

	screen->cui = cui;
	screen->on_exit = on_exit;
	screen->need_update = false;

	screen->scr->frame.ltitle = talloc_strdup(screen, title);
	screen->scr->frame.rtitle = NULL;
	screen->scr->frame.help = _("x=exit");
	scrollok(screen->scr->sub_ncw, true);
}
