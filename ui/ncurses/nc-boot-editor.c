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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "i18n/i18n.h"
#include "nc-boot-editor.h"
#include "nc-widgets.h"

struct boot_editor {
	struct nc_scr		*scr;
	struct cui		*cui;
	void			*data;
	struct pmenu_item	*item;
	enum {
		STATE_EDIT,
		STATE_CANCEL,
		STATE_SAVE,
		STATE_HELP,
	}			state;
	void			(*on_exit)(struct cui *cui,
					struct pmenu_item *item,
					struct pb_boot_data *bd);
	bool			need_redraw;
	bool			need_update;

	int			label_x;
	int			field_x;
	int			scroll_y;

	WINDOW			*pad;
	struct nc_widgetset	*widgetset;
	struct {
		struct nc_widget_label		*device_l;
		struct nc_widget_select		*device_f;
		struct nc_widget_label		*image_l;
		struct nc_widget_textbox	*image_f;
		struct nc_widget_label		*initrd_l;
		struct nc_widget_textbox	*initrd_f;
		struct nc_widget_label		*dtb_l;
		struct nc_widget_textbox	*dtb_f;
		struct nc_widget_label		*args_l;
		struct nc_widget_textbox	*args_f;
		struct nc_widget_label		*args_sig_file_l;
		struct nc_widget_textbox	*args_sig_file_f;
		struct nc_widget_button		*ok_b;
		struct nc_widget_button		*help_b;
		struct nc_widget_button		*cancel_b;
	} widgets;

	const char		*selected_device;
	char			*image;
	char			*initrd;
	char			*dtb;
	char			*args;
	char			*args_sig_file;

	bool			use_signature_files;
};

extern const struct help_text boot_editor_help_text;

static struct boot_editor *boot_editor_from_scr(struct nc_scr *scr)
{
	struct boot_editor *screen;

	assert(scr->sig == pb_boot_editor_sig);
	assert(scr->container);

	screen = scr->container;

	assert(screen->scr);
	assert(screen->scr->sig == pb_boot_editor_sig);

	return screen;
}

static void pad_refresh(struct boot_editor *boot_editor)
{
	int y, x, rows, cols;

	getmaxyx(boot_editor->scr->sub_ncw, rows, cols);
	getbegyx(boot_editor->scr->sub_ncw, y, x);

	prefresh(boot_editor->pad, boot_editor->scroll_y, 0,
			y, x, rows, cols);
}

static struct boot_editor *boot_editor_from_arg(void *arg)
{
	struct boot_editor *boot_editor = arg;

	assert(boot_editor->scr->sig == pb_boot_editor_sig);
	return boot_editor;
}

static int boot_editor_post(struct nc_scr *scr)
{
	struct boot_editor *boot_editor = boot_editor_from_scr(scr);

	if (boot_editor->need_update) {
		boot_editor_update(boot_editor, boot_editor->cui->sysinfo);
		boot_editor->need_update = false;
	} else {
		widgetset_post(boot_editor->widgetset);
	}

	nc_scr_frame_draw(scr);
	if (boot_editor->need_redraw) {
		redrawwin(scr->main_ncw);
		boot_editor->need_redraw = false;
	}
	wrefresh(boot_editor->scr->main_ncw);
	pad_refresh(boot_editor);
	return 0;
}

static int boot_editor_unpost(struct nc_scr *scr)
{
	widgetset_unpost(boot_editor_from_scr(scr)->widgetset);
	return 0;
}

struct nc_scr *boot_editor_scr(struct boot_editor *boot_editor)
{
	return boot_editor->scr;
}

static void boot_editor_resize(struct nc_scr *scr)
{
	/* FIXME: forms can't be resized, need to recreate here */
	boot_editor_unpost(scr);
	boot_editor_post(scr);
}

static char *conditional_prefix(struct pb_boot_data *ctx,
		const char *prefix, const char *value)
{
	const char *sep;

	if (!value || !*value)
		return NULL;

	sep = "";
	if (!prefix)
		prefix = "";
	else if ((prefix[strlen(prefix) - 1] != '/') &&
				(value[0] != '/'))
		sep = "/";

	return talloc_asprintf(ctx, "%s%s%s", prefix, sep, value);
}

static struct pb_boot_data *boot_editor_prepare_data(
		struct boot_editor *boot_editor)
{
	struct pb_boot_data *bd;
	char *s, *prefix;
	int idx;

	bd = talloc(boot_editor, struct pb_boot_data);

	if (!bd)
		return NULL;

	idx = widget_select_get_value(boot_editor->widgets.device_f);
	if (idx == -1 || (unsigned int)idx >
			boot_editor->cui->sysinfo->n_blockdevs)
		prefix = NULL;
	else
		prefix = boot_editor->cui->sysinfo->blockdevs[idx]->mountpoint;

	s = widget_textbox_get_value(boot_editor->widgets.image_f);
	bd->image = conditional_prefix(bd, prefix, s);
	if (!bd->image) {
		talloc_free(bd);
		return NULL;
	}

	s = widget_textbox_get_value(boot_editor->widgets.initrd_f);
	bd->initrd = conditional_prefix(bd, prefix, s);

	s = widget_textbox_get_value(boot_editor->widgets.dtb_f);
	bd->dtb = conditional_prefix(bd, prefix, s);

	s = widget_textbox_get_value(boot_editor->widgets.args_f);
	bd->args = *s ? talloc_strdup(bd, s) : NULL;

	if (boot_editor->use_signature_files) {
		s = widget_textbox_get_value(
			boot_editor->widgets.args_sig_file_f);
		bd->args_sig_file = conditional_prefix(bd, prefix, s);
	}
	else {
		bd->args_sig_file = NULL;
	}

	return bd;
}

/**
 * boot_editor_process_key - Process a user keystroke.
 *
 * Called from the cui via the scr:process_key method.
 */

static void boot_editor_process_key(struct nc_scr *scr, int key)
{
	struct boot_editor *boot_editor = boot_editor_from_scr(scr);
	struct pmenu_item *item;
	struct pb_boot_data *bd;
	bool handled;

	handled = widgetset_process_key(boot_editor->widgetset, key);
	if (handled)
		pad_refresh(boot_editor);

	else if (key == 'x' || key == 27)
		boot_editor->state = STATE_CANCEL;

	else if (key == 'h')
		boot_editor->state = STATE_HELP;

	item = NULL;
	bd = NULL;

	switch (boot_editor->state) {
	case STATE_SAVE:
		item = boot_editor->item;
		bd = boot_editor_prepare_data(boot_editor);
		if (!bd) {
			/* Incomplete entry */
			boot_editor->state = STATE_EDIT;
			break;
		}
		/* fall through */
	case STATE_CANCEL:
		boot_editor->on_exit(boot_editor->cui, item, bd);
		break;
	case STATE_HELP:
		boot_editor->state = STATE_EDIT;
		boot_editor->need_redraw = true;
		cui_show_help(boot_editor->cui, _("Boot Option Editor"),
				&boot_editor_help_text);
		break;
	default:
		break;
	}
}

/**
 * boot_editor_destructor - The talloc destructor for a boot_editor.
 */

static int boot_editor_destructor(void *arg)
{
	struct boot_editor *boot_editor = boot_editor_from_arg(arg);
	boot_editor->scr->sig = pb_removed_sig;
	if (boot_editor->pad)
		delwin(boot_editor->pad);
	return 0;
}

static void ok_click(void *arg)
{
	struct boot_editor *boot_editor = arg;
	boot_editor->state = STATE_SAVE;
}

static void help_click(void *arg)
{
	struct boot_editor *boot_editor = arg;
	boot_editor->state = STATE_HELP;
}

static void cancel_click(void *arg)
{
	struct boot_editor *boot_editor = arg;
	boot_editor->state = STATE_CANCEL;
}

static int layout_pair(struct boot_editor *boot_editor, int y,
		struct nc_widget_label *label,
		struct nc_widget_textbox *field)
{
	struct nc_widget *label_w = widget_label_base(label);
	struct nc_widget *field_w = widget_textbox_base(field);
	widget_move(label_w, y, boot_editor->label_x);
	widget_move(field_w, y, boot_editor->field_x);
	return max(widget_height(label_w), widget_height(field_w));
}

static int pad_height(int blockdevs_height)
{
	return 10 + (2 * blockdevs_height);
}

static void boot_editor_layout_widgets(struct boot_editor *boot_editor)
{
	struct nc_widget *wf, *wl;
	int y = 1;

	wl = widget_label_base(boot_editor->widgets.device_l);
	wf = widget_select_base(boot_editor->widgets.device_f);
	widget_move(wl, y, boot_editor->label_x);
	widget_move(wf, y, boot_editor->field_x);

	y += widget_height(wf) + 1;


	y += layout_pair(boot_editor, y, boot_editor->widgets.image_l,
					 boot_editor->widgets.image_f);

	y += layout_pair(boot_editor, y, boot_editor->widgets.initrd_l,
					 boot_editor->widgets.initrd_f);

	y += layout_pair(boot_editor, y, boot_editor->widgets.dtb_l,
					 boot_editor->widgets.dtb_f);

	y += layout_pair(boot_editor, y, boot_editor->widgets.args_l,
					 boot_editor->widgets.args_f);

	if (boot_editor->use_signature_files) {
		y += layout_pair(boot_editor, y,
					boot_editor->widgets.args_sig_file_l,
					boot_editor->widgets.args_sig_file_f);
	}


	y++;
	widget_move(widget_button_base(boot_editor->widgets.ok_b), y,
		    boot_editor->field_x);
	widget_move(widget_button_base(boot_editor->widgets.help_b), y,
		    boot_editor->field_x + 14);
	widget_move(widget_button_base(boot_editor->widgets.cancel_b), y,
		    boot_editor->field_x + 28);
}

static void boot_editor_widget_focus(struct nc_widget *widget, void *arg)
{
	struct boot_editor *boot_editor = arg;
	int w_y, s_max;

	w_y = widget_y(widget) + widget_focus_y(widget);
	s_max = getmaxy(boot_editor->scr->sub_ncw) - 1;

	if (w_y < boot_editor->scroll_y)
		boot_editor->scroll_y = w_y;

	else if (w_y + boot_editor->scroll_y + 1 > s_max)
		boot_editor->scroll_y = 1 + w_y - s_max;

	else
		return;

	pad_refresh(boot_editor);
}

static void boot_editor_device_select_change(void *arg, int idx)
{
	struct boot_editor *boot_editor = arg;
	if (idx == -1)
		boot_editor->selected_device = NULL;
	else
		boot_editor->selected_device =
			boot_editor->cui->sysinfo->blockdevs[idx]->name;
}

static void boot_editor_populate_device_select(struct boot_editor *boot_editor,
		const struct system_info *sysinfo)
{
	struct nc_widget_select *select = boot_editor->widgets.device_f;
	unsigned int i;
	bool selected;

	widget_select_drop_options(select);

	for (i = 0; sysinfo && i < sysinfo->n_blockdevs; i++) {
		struct blockdev_info *bd_info = sysinfo->blockdevs[i];
		const char *name;

		name = talloc_asprintf(boot_editor, "%s [%s]",
				bd_info->name, bd_info->uuid);
		selected = boot_editor->selected_device &&
				!strcmp(bd_info->name,
						boot_editor->selected_device);

		widget_select_add_option(select, i, name, selected);
	}

	/* If we're editing an existing option, the paths will be fully-
	 * resolved. In this case, we want the manual device pre-selected.
	 * However, we only do this if the widget hasn't been manually
	 * changed. */
	selected = !boot_editor->selected_device;

	widget_select_add_option(select, -1, _("Specify paths/URLs manually"),
			selected);
}

static bool path_on_device(struct blockdev_info *bd_info,
		const char *path)
{
	int len;

	if (!bd_info->mountpoint)
		return false;

	len = strlen(bd_info->mountpoint);
	if (strncmp(bd_info->mountpoint, path, len))
		return false;

	/* if the mountpoint doesn't have a trailing slash, ensure that
	 * the path starts with one (so we don't match a "/mnt/sda1/foo" path
	 * on a "/mnt/sda" mountpoint) */
	return bd_info->mountpoint[len-1] == '/' || path[len] == '/';
}


static void boot_editor_find_device(struct boot_editor *boot_editor,
		struct pb_boot_data *bd, const struct system_info *sysinfo)
{
	struct blockdev_info *bd_info, *tmp;
	unsigned int i, len;

	if (!sysinfo || !sysinfo->n_blockdevs)
		return;

	/* find the device for our boot image, by finding the longest
	 * matching blockdev's mountpoint */
	for (len = 0, i = 0, bd_info = NULL; i < sysinfo->n_blockdevs; i++) {
		tmp = sysinfo->blockdevs[i];
		if (!path_on_device(tmp, bd->image))
			continue;
		if (strlen(tmp->mountpoint) <= len)
			continue;
		bd_info = tmp;
		len = strlen(tmp->mountpoint);
	}

	if (!bd_info)
		return;

	/* ensure that other paths are on this device */
	if (bd->initrd && !path_on_device(bd_info, bd->initrd))
		return;

	if (bd->dtb && !path_on_device(bd_info, bd->dtb))
		return;

	if (boot_editor->use_signature_files)
		if (bd->args_sig_file && !path_on_device(bd_info,
			bd->args_sig_file))
			return;

	/* ok, we match; preselect the device option, and remove the common
	 * prefix */
	boot_editor->selected_device = bd_info->name;
	boot_editor->image += len;

	if (boot_editor->initrd)
		boot_editor->initrd += len;
	if (boot_editor->dtb)
		boot_editor->dtb += len;
	if (boot_editor->use_signature_files)
		if (boot_editor->args_sig_file)
			boot_editor->args_sig_file += len;
}

static void boot_editor_setup_widgets(struct boot_editor *boot_editor,
		const struct system_info *sysinfo)
{
	struct nc_widgetset *set = boot_editor->widgetset;
	int field_size;

	field_size = COLS - 1 - boot_editor->field_x;

	boot_editor->widgets.device_l = widget_new_label(set, 0, 0,
			_("Device:"));
	boot_editor->widgets.device_f = widget_new_select(set, 0, 0,
						field_size);
	widget_select_on_change(boot_editor->widgets.device_f,
			boot_editor_device_select_change, boot_editor);

	boot_editor_populate_device_select(boot_editor, sysinfo);

	boot_editor->widgets.image_l = widget_new_label(set, 0, 0,
			_("Kernel:"));
	boot_editor->widgets.image_f = widget_new_textbox(set, 0, 0,
						field_size, boot_editor->image);

	boot_editor->widgets.initrd_l = widget_new_label(set, 0, 0,
			_("Initrd:"));
	boot_editor->widgets.initrd_f = widget_new_textbox(set, 0, 0,
						field_size,
						boot_editor->initrd);

	boot_editor->widgets.dtb_l = widget_new_label(set, 0, 0,
			_("Device tree:"));
	boot_editor->widgets.dtb_f = widget_new_textbox(set, 0, 0,
						field_size, boot_editor->dtb);

	boot_editor->widgets.args_l = widget_new_label(set, 0, 0,
			_("Boot arguments:"));
	boot_editor->widgets.args_f = widget_new_textbox(set, 0, 0,
					field_size, boot_editor->args);

	if (boot_editor->use_signature_files) {
		boot_editor->widgets.args_sig_file_l = widget_new_label(set,
				0, 0, _("Argument signature file:"));
		boot_editor->widgets.args_sig_file_f = widget_new_textbox(set,
				0, 0, field_size, boot_editor->args_sig_file);
	}
	else {
		boot_editor->widgets.args_sig_file_l = NULL;
		boot_editor->widgets.args_sig_file_f = NULL;
	}

	boot_editor->widgets.ok_b = widget_new_button(set, 0, 0, 10,
					_("OK"), ok_click, boot_editor);
	boot_editor->widgets.help_b = widget_new_button(set, 0, 0, 10,
					_("Help"), help_click, boot_editor);
	boot_editor->widgets.cancel_b = widget_new_button(set, 0, 0, 10,
					_("Cancel"), cancel_click, boot_editor);
}

static void boot_editor_draw(struct boot_editor *boot_editor,
		const struct system_info *sysinfo)
{
	bool repost = false;
	int height;

	height = pad_height(sysinfo ? sysinfo->n_blockdevs : 0);

	if (!boot_editor->pad || getmaxy(boot_editor->pad) < height) {
		if (boot_editor->pad)
			delwin(boot_editor->pad);
		boot_editor->pad = newpad(height, COLS);
	}

	if (boot_editor->widgetset) {
		widgetset_unpost(boot_editor->widgetset);
		talloc_free(boot_editor->widgetset);
		repost = true;
	}

	boot_editor->widgetset = widgetset_create(boot_editor,
			boot_editor->scr->main_ncw,
			boot_editor->pad);
	widgetset_set_widget_focus(boot_editor->widgetset,
			boot_editor_widget_focus, boot_editor);

	boot_editor_setup_widgets(boot_editor, sysinfo);
	boot_editor_layout_widgets(boot_editor);

	if (repost)
		widgetset_post(boot_editor->widgetset);
}

void boot_editor_update(struct boot_editor *boot_editor,
		const struct system_info *sysinfo)
{
	const char *str;

	if (boot_editor->cui->current_scr != boot_editor_scr(boot_editor)) {
		boot_editor->need_update = true;
		return;
	}

	str = widget_textbox_get_value(boot_editor->widgets.image_f);
	if (str) {
		talloc_free(boot_editor->image);
		boot_editor->image = talloc_strdup(boot_editor, str);
	}

	str = widget_textbox_get_value(boot_editor->widgets.initrd_f);
	if (str) {
		talloc_free(boot_editor->initrd);
		boot_editor->initrd = talloc_strdup(boot_editor, str);
	}

	str = widget_textbox_get_value(boot_editor->widgets.dtb_f);
	if (str) {
		talloc_free(boot_editor->dtb);
		boot_editor->dtb = talloc_strdup(boot_editor, str);
	}

	str = widget_textbox_get_value(boot_editor->widgets.args_f);
	if (str) {
		talloc_free(boot_editor->args);
		boot_editor->args = talloc_strdup(boot_editor, str);
	}

	if (boot_editor->use_signature_files) {
		str = widget_textbox_get_value(boot_editor->widgets.args_sig_file_f);
		if (str) {
			talloc_free(boot_editor->args_sig_file);
			boot_editor->args_sig_file = talloc_strdup(boot_editor, str);
		}
	}

	boot_editor_draw(boot_editor, sysinfo);
	pad_refresh(boot_editor);
}

struct boot_editor *boot_editor_init(struct cui *cui,
		struct pmenu_item *item,
		const struct system_info *sysinfo,
		void (*on_exit)(struct cui *cui,
				struct pmenu_item *item,
				struct pb_boot_data *bd))
{
	struct boot_editor *boot_editor;
	int ncols1, ncols2, ncols3;

	boot_editor = talloc_zero(cui, struct boot_editor);

	if (!boot_editor)
		return NULL;

#if defined(SIGNED_BOOT)
#if !defined(HARD_LOCKDOWN)
	if (access(LOCKDOWN_FILE, F_OK) == -1)
		boot_editor->use_signature_files = false;
	else
#endif
		boot_editor->use_signature_files = true;
#else
	boot_editor->use_signature_files = false;
#endif

	talloc_set_destructor(boot_editor, boot_editor_destructor);
	boot_editor->cui = cui;
	boot_editor->item = item;
	boot_editor->on_exit = on_exit;
	boot_editor->state = STATE_EDIT;
	boot_editor->need_redraw = false;
	boot_editor->need_update = false;

	ncols1 = strncols(_("Device tree:"));
	ncols2 = strncols(_("Boot arguments:"));
	if (boot_editor->use_signature_files)
		ncols3 = strncols(_("Argument signature file:"));
	else
		ncols3 = 0;

	boot_editor->label_x = 1;
	boot_editor->field_x = 2 + max(max(ncols1, ncols2), ncols3);

	boot_editor->scr = nc_scr_init(boot_editor, pb_boot_editor_sig, cui, 0,
		boot_editor_process_key, boot_editor_post, boot_editor_unpost,
		boot_editor_resize);

	boot_editor->scr->frame.ltitle = talloc_strdup(boot_editor,
			_("Petitboot Option Editor"));
	boot_editor->scr->frame.rtitle = NULL;
	boot_editor->scr->frame.help = talloc_strdup(boot_editor,
			_("tab=next, shift+tab=previous, x=exit, h=help"));
	nc_scr_frame_draw(boot_editor->scr);

	if (item) {
		struct pb_boot_data *bd = cod_from_item(item)->bd;
		boot_editor->image = talloc_strdup(boot_editor, bd->image);
		boot_editor->initrd = talloc_strdup(boot_editor, bd->initrd);
		boot_editor->dtb = talloc_strdup(boot_editor, bd->dtb);
		boot_editor->args = talloc_strdup(boot_editor, bd->args);
		if (boot_editor->use_signature_files)
			boot_editor->args_sig_file = talloc_strdup(boot_editor,
					bd->args_sig_file);
		else
			boot_editor->args_sig_file = talloc_strdup(boot_editor,
					"");
		boot_editor_find_device(boot_editor, bd, sysinfo);
	}

	boot_editor_draw(boot_editor, sysinfo);
	wrefresh(boot_editor->scr->main_ncw);

	return boot_editor;
}
