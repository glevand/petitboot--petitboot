/*
 * Petitboot generic ncurses bootloader UI
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include <libintl.h>
#include <locale.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "waiter/waiter.h"
#include "i18n/i18n.h"
#include "ui/common/discover-client.h"
#include "nc-cui.h"

static void print_version(void)
{
	printf("petitboot-nc (" PACKAGE_NAME ") " PACKAGE_VERSION "\n");
}

static void print_usage(void)
{
	print_version();
	printf(
"%s: petitboot-nc [-h, --help] [-l, --log log-file] [-t, --timeout]\n"
"                    [-v, --verbose] [-V, --version]\n",
			_("Usage"));
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
	enum opt_value timeout;
	enum opt_value verbose;
	enum opt_value show_version;
};

/**
 * opts_parse - Parse the command line options.
 */

static int opts_parse(struct opts *opts, int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"help",         no_argument,       NULL, 'h'},
		{"log",          required_argument, NULL, 'l'},
		{"timeout",	 no_argument,	    NULL, 't'},
		{"verbose",      no_argument,       NULL, 'v'},
		{"version",      no_argument,       NULL, 'V'},
		{ NULL,          0,                 NULL, 0},
	};
	static const char short_options[] = "hl:tvV";
	static const struct opts default_values = { 0 };

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
			opts->timeout = opt_yes;
			break;
		case 'v':
			opts->verbose = opt_yes;
			break;
		case 'V':
			opts->show_version = opt_yes;
			break;
		default:
			opts->show_help = opt_yes;
			return -1;
		}
	}

	return 0;
}

static struct cui *cui;

/*
 * struct pb_cui - Main cui program instance.
 * @mm: Main menu.
 * @svm: Set video mode menu.
 */

static void sig_handler(int signum)
{
	DBGS("%d\n", signum);

	switch (signum) {
	case SIGWINCH:
		if (cui)
			cui_resize(cui);
		break;
	default:
		assert(0 && "unknown sig");
		/* fall through */
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
		if (cui)
			cui_abort(cui);
		break;
	}
}

/**
 * main - cui bootloader main routine.
 */

int main(int argc, char *argv[])
{
	static struct sigaction sa;
	int result;
	int cui_result;
	struct opts opts;

	result = opts_parse(&opts, argc, argv);

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

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
		"--- petitboot-nc ---");

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

	cui = cui_init(opts.timeout, NULL);
	if (!cui)
		return EXIT_FAILURE;

	cui_result = cui_run(cui);

	talloc_free(cui);

	pb_log("--- end ---\n");

	return cui_result ? EXIT_FAILURE : EXIT_SUCCESS;
}
