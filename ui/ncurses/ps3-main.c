/*
 * Petitboot cui bootloader for the PS3 game console
 *
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

/*
 * TODO
 * removable media event
 * ncurses mouse support
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
#include "ui/common/discover-client.h"
#include "ui/common/ps3.h"
#include "nc-cui.h"
#include <i18n/i18n.h>

static void print_version(void)
{
	printf("pb-cui (" PACKAGE_NAME ") " PACKAGE_VERSION "\n");
}

static void print_usage(void)
{
	print_version();
	printf(
"Usage: pb-cui [-h, --help] [-l, --log log-file] [-t, --timeout]\n"
"              [-v, --verbose] [-r, --reset-defaults] [-V, --version]\n");
}

/**
 * enum opt_value - Tri-state options variables.
 */

enum opt_value {opt_undef = 0, opt_yes, opt_no};

/**
 * struct opts - Values from command line options.
 */

struct opts {
	enum opt_value show_help;
	const char *log_file;
	unsigned long timeout;
	enum opt_value verbose;
	enum opt_value reset_defaults;
	enum opt_value show_version;
};

/**
 * opts_parse - Parse the command line options.
 */

static int opts_parse(struct opts *opts, int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"help",           no_argument,       NULL, 'h'},
		{"log",            required_argument, NULL, 'l'},
		{"timeout",        required_argument, NULL, 't'},
		{"verbose",        no_argument,       NULL, 'v'},
		{"reset-defaults", no_argument,       NULL, 'r'},
		{"version",        no_argument,       NULL, 'V'},
		{ NULL,            0,                 NULL, 0},
	};
	static const char short_options[] = "hl:t:vrV";
	static const struct opts default_values = {0};

	*opts = default_values;

	while (1) {
		int c = getopt_long(argc, argv, short_options, long_options,
			NULL);

		if (c == EOF)
			break;

		switch (c) {
		case 'h':
			opts->show_help = opt_yes;
			break;
		case 'l':
			opts->log_file = optarg;
			break;
		case 't':
			opts->timeout = strtoul(optarg, NULL, 10);
			break;
		case 'v':
			opts->verbose = opt_yes;
			break;
		case 'r':
			opts->reset_defaults = opt_yes;
			break;
		case 'V':
			opts->show_version = opt_yes;
			break;
		default:
			opts->show_help = opt_yes;
			return -1;
		}
	}

	return optind != argc;
}

/**
 * struct ps3_cui - Main cui program instance.
 * @mm: Main menu.
 * @svm: Set video mode menu.
 */

struct ps3_cui {
	struct pmenu *mm;
	struct pmenu *svm;
	struct cui *cui;
	struct ps3_flash_values values;
	int dirty_values;
};

static struct ps3_cui *ps3_from_cui(struct cui *cui)
{
	struct ps3_cui *ps3;

	assert(cui->sig == pb_cui_sig);
	ps3 = cui->platform_info;
	assert(ps3->cui->sig == pb_cui_sig);
	return ps3;
}

static struct ps3_cui *ps3_from_item(struct pmenu_item *item)
{
	return ps3_from_cui(cui_from_item(item));
}

/**
 * ps3_sixaxis_map - Map a Linux joystick event to an ncurses key code.
 *
 */

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

/**
 * ps3_set_mode - Set video mode helper.
 *
 * Runs ps3_set_video_mode().
 */

static void ps3_set_mode(struct ps3_cui *ps3, unsigned int mode)
{
	int result;

	if (ps3->values.video_mode == (uint16_t)mode)
		return;

	ps3->values.video_mode = (uint16_t)mode;
	ps3->dirty_values = 1;

	result = ps3_set_video_mode(mode);

	if (result)
		nc_scr_status_printf(ps3->cui->current_scr,
			"Failed: set_video_mode(%u)", mode);
}

static int ps3_save_flash_values(struct cui *cui, struct cui_opt_data *cod)
{
	struct ps3_cui *ps3 = ps3_from_cui(cui);
	int altered_args;
	char *orig_args;

	/* Save values to flash if needed */

	if ((cod->opt_hash && cod->opt_hash != cui->default_item)
		|| ps3->dirty_values) {
		ps3->values.default_item = cod->opt_hash;
		ps3_flash_set_values(&ps3->values);
	}

	/* Add a default kernel video mode. */

	if (!cod->bd->args) {
		altered_args = 1;
		orig_args = NULL;
		cod->bd->args = talloc_asprintf(NULL, "video=ps3fb:mode:%u",
			(unsigned int)ps3->values.video_mode);
	} else if (!strstr(cod->bd->args, "video=")) {
		altered_args = 1;
		orig_args = cod->bd->args;
		cod->bd->args = talloc_asprintf(NULL, "%s video=ps3fb:mode:%u",
			orig_args, (unsigned int)ps3->values.video_mode);
	} else
		altered_args = 0;

	if (altered_args) {
		talloc_free(cod->bd->args);
		cod->bd->args = orig_args;
	}

	return 0;
}

/**
 * ps3_svm_cb - The set video mode callback.
 */

static int ps3_svm_cb(struct pmenu_item *item)
{
	ps3_set_mode(ps3_from_item(item),
		(unsigned int)(unsigned long int)(item->data));
	return 0;
}

/**
 * ps3_mm_to_svm_cb - Callback to switch to the set video mode menu.
 */

static int ps3_mm_to_svm_cb(struct pmenu_item *item)
{
	struct ps3_cui *ps3 = ps3_from_item(item);
	struct nc_scr *old;

	old = cui_set_current(ps3->cui, &ps3->svm->scr);
	assert(old == &ps3->mm->scr);

	return 0;
}

/**
 * ps3_svm_to_mm_cb - Callback to switch back to the main menu.
 */

static int ps3_svm_to_mm_cb(struct pmenu_item *item)
{
	struct ps3_cui *ps3 = ps3_from_item(item);
	struct nc_scr *old;

	old = cui_set_current(ps3->cui, &ps3->mm->scr);
	assert(old == &ps3->svm->scr);

	return 0;
}

/**
 * ps3_svm_to_mm_helper - The svm exit callback.
 */

static void ps3_svm_to_mm_helper(struct pmenu *menu)
{
	ps3_svm_to_mm_cb(pmenu_find_selected(menu));
}

/**
 * ps3_hot_key - PS3 specific hot keys.
 *
 * '@' = Set video mode to auto (mode 0)
 * '$' = Set video mode to safe (480i)
 * '+' = Cycles through a set of common video modes.
 * '-' = Cycles through a set of common video modes in reverse.
 */

static int ps3_hot_key(struct pmenu __attribute__((unused)) *menu,
	struct pmenu_item *item, int c)
{
	static const unsigned int modes[] = {0, 1, 6, 3, 11, 12};
	static const unsigned int *const end = modes
		+ sizeof(modes) / sizeof(modes[0]) - 1;
	static const unsigned int *p = modes;

	switch (c) {
	default:
		/* DBGS("%d (%o)\n", c, c); */
		break;
	case '@':
		p = modes + 0;
		ps3_set_mode(ps3_from_item(item), *p);
		break;
	case '$':
		p = modes + 1;
		ps3_set_mode(ps3_from_item(item), *p);
		break;
	case '+':
		p = (p < end) ? p + 1 : modes;
		ps3_set_mode(ps3_from_item(item), *p);
		break;
	case '-':
		p = (p > modes) ? p - 1 : end;
		ps3_set_mode(ps3_from_item(item), *p);
		break;
	}

	return c;
}

/**
 * ps3_mm_init - Setup the main menu instance.
 */

static struct pmenu *ps3_mm_init(struct cui *cui)
{
	int result;
	struct pmenu *m;
	struct pmenu_item *i;
	static const char *const bgo[] = {"/usr/sbin/ps3-boot-game-os", NULL};

	m = pmenu_init(cui, 3, cui_on_exit);

	if (!m) {
		pb_debug_fl("failed\n");
		return NULL;
	}

#if defined(DEBUG)
	m->scr.frame.ltitle = talloc_strdup(m,
		"Petitboot PS3 (" PACKAGE_VERSION ")");
#else
	m->scr.frame.ltitle = talloc_strdup(m, "Petitboot PS3");
#endif
	m->scr.frame.rtitle = NULL;
	m->scr.frame.help = talloc_strdup(m,
		"Enter=accept, e=edit, o=open, x=exit");
	m->scr.frame.status = talloc_strdup(m, "Welcome to Petitboot");

	i = pmenu_item_create(m, _("Boot GameOS"));
	i->on_execute = cui_run_cmd_from_item;
	i->data = (void *)bgo;
	pmenu_item_insert(m, i, 0);

	i = pmenu_item_create(m, _("Set Video Mode"));
	i->on_execute = ps3_mm_to_svm_cb;
	pmenu_item_insert(m, i, 1);

	i = pmenu_item_create(m, _("Exit to shell"));
	i->on_execute = pmenu_exit_cb;
	pmenu_item_insert(m, i, 2);

	result = pmenu_setup(m);

	if (result) {
		pb_debug_fl("pmenu_setup failed: %s\n", strerror(errno));
		goto fail_setup;
	}

	menu_opts_off(m->ncm, O_SHOWDESC);
	set_menu_mark(m->ncm, " *");
	set_current_item(m->ncm, i->nci);

	return m;

fail_setup:
	talloc_free(m);
	return NULL;
}

/**
 * ps3_svm_init - Setup the set video mode menu instance.
 */

static struct pmenu *ps3_svm_init(struct ps3_cui *ps3_cui)
{
	int result;
	struct pmenu *m;
	struct pmenu_item *i;

	m = pmenu_init(ps3_cui->cui, 12, ps3_svm_to_mm_helper);

	if (!m) {
		pb_debug_fl("failed\n");
		return NULL;
	}

	m->scr.frame.ltitle = talloc_strdup(m, "Select PS3 Video Mode");
	m->scr.frame.rtitle = NULL;
	m->scr.frame.help = talloc_strdup(m, "Enter=accept, x=exit");

	i = pmenu_item_create(m, _("auto detect"));
	i->on_execute = ps3_svm_cb;
	i->data = (void *)0;
	pmenu_item_insert(m, i, 0);

	i = pmenu_item_create(m, "480i    (576 x 384)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)1;
	pmenu_item_insert(m, i, 1);

	i = pmenu_item_create(m, "480p    (576 x 384)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)2;
	pmenu_item_insert(m, i, 2);

	i = pmenu_item_create(m, "576i    (576 x 460)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)6;
	pmenu_item_insert(m, i, 3);

	i = pmenu_item_create(m, "576p    (576 x 460)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)7;
	pmenu_item_insert(m, i, 4);

	i = pmenu_item_create(m, "720p   (1124 x 644)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)3;
	pmenu_item_insert(m, i, 5);

	i = pmenu_item_create(m, "1080i  (1688 x 964)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)4;
	pmenu_item_insert(m, i, 6);

	i = pmenu_item_create(m, "1080p  (1688 x 964)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)5;
	pmenu_item_insert(m, i, 7);

	i = pmenu_item_create(m, "wxga   (1280 x 768)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)11;
	pmenu_item_insert(m, i, 8);

	i = pmenu_item_create(m, "sxga   (1280 x 1024)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)12;
	pmenu_item_insert(m, i, 9);

	i = pmenu_item_create(m, "wuxga  (1920 x 1200)");
	i->on_execute = ps3_svm_cb;
	i->data = (void *)13;
	pmenu_item_insert(m, i, 10);

	i = pmenu_item_create(m, _("Return"));
	i->on_execute = ps3_svm_to_mm_cb;
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

static struct ps3_cui ps3;

static void sig_handler(int signum)
{
	DBGS("%d\n", signum);

	switch (signum) {
	case SIGWINCH:
		if (ps3.cui)
			cui_resize(ps3.cui);
		break;
	default:
		assert(0 && "unknown sig");
		/* fall through */
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
		if (ps3.cui)
			cui_abort(ps3.cui);
		break;
	}
}

/**
 * main - cui bootloader main routine.
 */

int main(int argc, char *argv[])
{
	static struct sigaction sa;
	static struct opts opts;
	int result;
	int cui_result;
	unsigned int mode;

	result = opts_parse(&opts, argc, argv);

	if (result) {
		print_usage();
		return EXIT_FAILURE;
	}

	if (opts.show_help == opt_yes) {
		print_usage();
		return EXIT_SUCCESS;
	}

	if (opts.show_version == opt_yes) {
		print_version();
		return EXIT_SUCCESS;
	}

	pb_log_open(opts.log_file, opts.verbose == opt_yes,
		"--- PS3 petitboot-nc ---");

	sa.sa_handler = sig_handler;
	result = sigaction(SIGALRM, &sa, NULL);
	result += sigaction(SIGHUP, &sa, NULL);
	result += sigaction(SIGINT, &sa, NULL);
	result += sigaction(SIGTERM, &sa, NULL);
	result += sigaction(SIGWINCH, &sa, NULL);

	if (result) {
		pb_log_fn("sigaction failed.\n");
		return EXIT_FAILURE;
	}

	ps3.values = ps3_flash_defaults;

	if (opts.reset_defaults != opt_yes)
		ps3.dirty_values = ps3_flash_get_values(&ps3.values);

	result = ps3_get_video_mode(&mode);

	/* Current becomes default if ps3_flash_get_values() failed. */

	if (ps3.dirty_values && !result)
		ps3.values.video_mode = mode;

	/* Set mode if not at default. */

	if (!result && (ps3.values.video_mode != (uint16_t)mode))
		ps3_set_video_mode(ps3.values.video_mode);

	ps3.cui = cui_init(opts.timeout, &ps3_mm_init);

	if (!ps3.cui)
		return EXIT_FAILURE;

	ps3.mm = ps3.cui->main;
	ps3.svm = ps3_svm_init(&ps3);
	ps3.cui->platform_info = &ps3;

	cui_result = cui_run(ps3.cui);

	if (ps3.dirty_values)
		ps3_flash_set_values(&ps3.values);

	talloc_free(ps3.cui);

	pb_log("--- end ---\n");

	return cui_result ? EXIT_FAILURE : EXIT_SUCCESS;
}
