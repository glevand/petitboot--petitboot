#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <file/file.h>
#include <log/log.h>
#include <talloc/talloc.h>

#include "platform.h"
#include "platform/ps3.h"


enum pb_platform_sig {
	pb_ps3_sig = 333,
};

struct ps3_discover_plat {
	enum pb_platform_sig sig;
	struct ps3_flash_values flash_values;
};

static struct ps3_discover_plat *platform_to_ps3(struct platform *p)
{
	struct ps3_discover_plat *ps3 = p->platform_data;

	assert(ps3->sig == pb_ps3_sig);
	return ps3;
}

static bool ps3_probe(struct platform *p, void *ctx)
{
	static const char devtree[] = "/proc/device-tree/";
	struct ps3_discover_plat *ps3;
	struct stat statbuf;
	int result;
	char *path;
	char *model;
	int model_len;

	static const bool fake = true;

	result = stat(devtree, &statbuf);

	if (result) {
		pb_log_fn("ERROR: stat device-tree failed (%s).\n",
			strerror(errno));
		if (!fake) return false;
	}

	if (!S_ISDIR(statbuf.st_mode)) {
		pb_log_fn("ERROR: bad device-tree.\n");
		if (!fake) return false;
	}

	path = talloc_asprintf(NULL, "%smodel", devtree);
	result = read_file(path, path, &model, &model_len);

	if (result) {
		pb_debug_fl("%s: Read model failed.\n", devtree);
		if (!fake) 
			goto exit;
		else {
			model = talloc_asprintf(path, "%s", "SonyPS3");
			model_len = 7;
			result = 0;
		}

	}

	if (strncmp(model, "SonyPS3", model_len)) {
		pb_debug_fl("%s: wrong model '%s'.\n", devtree, model);
		goto exit;
	}

	ps3 = talloc_zero(ctx, struct ps3_discover_plat);

	ps3->sig = pb_ps3_sig;
	ps3->flash_values = ps3_flash_defaults;

	p->platform_data = ps3;

exit:
	talloc_free(path);
	return !result;
}

static int ps3_load_config(struct platform *p, struct config *config)
{
	struct ps3_discover_plat *ps3 = platform_to_ps3(p);
	unsigned int mode;
	int result;

	ps3_flash_get_values(&ps3->flash_values);

	result = ps3_get_video_mode(&mode);

	/* Current mode becomes default if ps3_flash_get_values() failed. */
	if (ps3->flash_values.dirty && !result) {
		ps3->flash_values.video_mode = mode;
	}

	/* Set mode if not at default. */
	if (!result && (ps3->flash_values.video_mode != (uint16_t)mode)) {
		ps3_set_video_mode(ps3->flash_values.video_mode);
	}

	config_set_defaults(config);

	config->autoboot_timeout_sec = ps3->flash_values.timeout;
	config->autoboot_enabled =
		(ps3->flash_values.timeout < ps3_timeout_forever);

	pb_debug_fl("timeout = %hhu\n", ps3->flash_values.timeout);

	return 0;
}

static int ps3_save_config(struct platform *p, struct config *config)
{
	struct ps3_discover_plat *ps3 = platform_to_ps3(p);
	uint8_t new_timeout;

	new_timeout = (config->autoboot_enabled
		&& (config->autoboot_timeout_sec < ps3_timeout_forever))
		? (uint8_t)config->autoboot_timeout_sec : ps3_timeout_forever;

	if (new_timeout != ps3->flash_values.timeout) {
		ps3->flash_values.timeout = new_timeout;
		ps3->flash_values.dirty = true;
	}

	pb_debug_fl("timeout = %hhu\n", ps3->flash_values.timeout);

	if (ps3->flash_values.dirty) {
		ps3_flash_set_values(&ps3->flash_values);
		ps3->flash_values.dirty = false;
	}

	return 0;
}

static int ps3_get_sysinfo(struct platform *p, struct system_info *sysinfo)
{
	static const char ver_path[] = "/proc/ps3/firmware-version";
	struct ps3_discover_plat *ps3 = platform_to_ps3(p);
	char *ver;
	int result;

	sysinfo->type = talloc_asprintf(ps3, "PlayStation 3");

	result = read_file(NULL, ver_path, &ver, NULL);

	if (result) {
		pb_debug_fl("Read fw version failed '%s'.\n", ver_path);
	} else {
		sysinfo->identifier = talloc_asprintf(ps3, "| FW %s", ver);
		talloc_free(ver);
	}

	return result;
}

static bool ps3_restrict_clients(struct platform *p)
{
	(void)p;
	pb_debug_fl("\n");
	return false;
}

static struct platform ps3_discover_plat = {
	.name			= "ps3",
	.probe			= ps3_probe,
	.load_config		= ps3_load_config,
	.save_config		= ps3_save_config,
	.pre_boot		= NULL,
	.get_sysinfo		= ps3_get_sysinfo,
	.restrict_clients	= ps3_restrict_clients,
	.set_password		= NULL,
	.preboot_check		= NULL,
	.dhcp_arch_id		= 0x000e,
	.platform_data 		= 0,
};

register_platform(ps3_discover_plat);
