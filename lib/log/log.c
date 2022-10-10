
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "log.h"

static FILE *logf;
static bool debug;

static void __log_timestamp(void)
{
	char hms[20] = {'\0'};
	time_t t;

	if (!logf)
		return;

	t = time(NULL);
	strftime(hms, sizeof(hms), "%T", localtime(&t));
	fprintf(logf, "[%s] ", hms);
}

static void __log(const char *fmt, va_list ap)
{
	if (!logf)
		return;

	vfprintf(logf, fmt, ap);
	fflush(logf);
}

void pb_log(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	__log_timestamp();
	__log(fmt, ap);
	va_end(ap);
}

void _pb_log_fn(const char *func, const char *fmt, ...)
{
	va_list ap;
	pb_log("%s: ", func);
	va_start(ap, fmt);
	__log(fmt, ap);
	va_end(ap);
}

void pb_debug(const char *fmt, ...)
{
	va_list ap;
	if (!debug)
		return;
	va_start(ap, fmt);
	__log_timestamp();
	__log(fmt, ap);
	va_end(ap);
}

void _pb_debug_fn(const char *func, const char *fmt, ...)
{
	va_list ap;
	if (!debug)
		return;
	pb_log("%s: ", func);
	va_start(ap, fmt);
	__log(fmt, ap);
	va_end(ap);
}

void _pb_debug_fl(const char *func, int line, const char *fmt, ...)
{
	va_list ap;
	if (!debug)
		return;
	pb_log("%s:%d: ", func, line);
	va_start(ap, fmt);
	__log(fmt, ap);
	va_end(ap);
}

void __pb_log_init(FILE *fp, bool _debug)
{
	if (logf)
		fflush(logf);
	logf = fp;
	debug = _debug;
}

void pb_log_set_debug(bool _debug)
{
	debug = _debug;
}

bool pb_log_get_debug(void)
{
	return debug;
}

FILE *pb_log_get_stream(void)
{
	static FILE *null_stream;
	if (!logf) {
		if (!null_stream)
			null_stream = fopen("/dev/null", "a");
		return null_stream;
	}
	return logf;
}

static char *pb_log_default_filename(void)
{
	const char *base = "/var/log/petitboot/petitboot-nc";
	static char name[PATH_MAX];
	char *tty;
	int i;

	tty = ttyname(STDIN_FILENO);

	/* strip /dev/ */
	if (tty && !strncmp(tty, "/dev/", 5))
		tty += 5;

	/* change slashes to hyphens */
	for (i = 0; tty && tty[i]; i++)
		if (tty[i] == '/')
			tty[i] = '-';

	if (!tty || !*tty)
		tty = "unknown";

	snprintf(name, sizeof(name), "%s.%s.log", base, tty);

	return name;
}

void pb_log_open(const char *log_file_name, bool vebose, const char *greeting)
{
	const char *name;
	FILE *log;

	name = log_file_name ? log_file_name : pb_log_default_filename();
	log = stderr;

	if (strcmp(name, "-")) {
		log = fopen(name, "a");

		if (!log) {
			fprintf(stderr,
				"can't open log file '%s', logging to stderr\n",
				name);
			log = stderr;
		}
	}

	pb_log_init(log);

	if (vebose)
		pb_log_set_debug(true);

	if (greeting)
		pb_log("%s\n", greeting);
}

void pb_log_close(const char *signoff)
{
	if (signoff)
		pb_log("%s\n", signoff);

	if (logf != stderr)
		fclose(logf);
}