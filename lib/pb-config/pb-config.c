
#include <string.h>

#include <log/log.h>
#include <types/types.h>
#include <talloc/talloc.h>

#include "pb-config.h"

static struct interface_config *config_copy_interface(struct config *ctx,
		struct interface_config *src)
{
	struct interface_config *dest = talloc_zero(ctx,
						struct interface_config);

	pb_debug_fl("->\n");
	memcpy(dest->hwaddr, src->hwaddr, sizeof(src->hwaddr));
	dest->ignore = src->ignore;

	if (dest->ignore)
		return dest;

	dest->method = src->method;

	switch (src->method) {
	case CONFIG_METHOD_DHCP:
		break;
	case CONFIG_METHOD_STATIC:
		dest->static_config.address =
			talloc_strdup(dest, src->static_config.address);
		dest->static_config.gateway =
			talloc_strdup(dest, src->static_config.gateway);
		dest->static_config.url =
			talloc_strdup(dest, src->static_config.url);
		break;
	}
	dest->override = src->override;

	return dest;
}

struct config *config_copy(void *ctx, const struct config *src)
{
	struct config *dest;
	unsigned int i;

	if (!src)
		return NULL;

	dest = talloc_zero(ctx, struct config);
	dest->autoboot_enabled = src->autoboot_enabled;
	dest->autoboot_timeout_sec = src->autoboot_timeout_sec;
	dest->safe_mode = src->safe_mode;

	dest->network.n_interfaces = src->network.n_interfaces;
	dest->network.interfaces = talloc_array(dest, struct interface_config *,
					dest->network.n_interfaces);
	dest->network.n_dns_servers = src->network.n_dns_servers;
	dest->network.dns_servers = talloc_array(dest, const char *,
					dest->network.n_dns_servers);

	for (i = 0; i < src->network.n_interfaces; i++)
		dest->network.interfaces[i] = config_copy_interface(dest,
				src->network.interfaces[i]);

	for (i = 0; i < src->network.n_dns_servers; i++)
		dest->network.dns_servers[i] = talloc_strdup(dest,
				src->network.dns_servers[i]);

	dest->http_proxy = talloc_strdup(dest, src->http_proxy);
	dest->https_proxy = talloc_strdup(dest, src->https_proxy);

	dest->n_autoboot_opts = src->n_autoboot_opts;
	dest->autoboot_opts = talloc_array(dest, struct autoboot_option,
					dest->n_autoboot_opts);

	for (i = 0; i < src->n_autoboot_opts; i++) {
		dest->autoboot_opts[i].boot_type =
			src->autoboot_opts[i].boot_type;
		if (src->autoboot_opts[i].boot_type == BOOT_DEVICE_TYPE)
			dest->autoboot_opts[i].type =
				src->autoboot_opts[i].type;
		else
			dest->autoboot_opts[i].uuid =
				talloc_strdup(dest, src->autoboot_opts[i].uuid);
	}

	dest->ipmi_bootdev = src->ipmi_bootdev;
	dest->ipmi_bootdev_persistent = src->ipmi_bootdev_persistent;
	dest->ipmi_bootdev_mailbox = src->ipmi_bootdev_mailbox;

	dest->allow_writes = src->allow_writes;

	dest->n_consoles = src->n_consoles;
	if (src->consoles) {
		dest->consoles = talloc_array(dest, char *, src->n_consoles);
		for (i = 0; i < src->n_consoles && src->n_consoles; i++)
			if (src->consoles[i])
				dest->consoles[i] = talloc_strdup(
						dest->consoles,
						src->consoles[i]);
	}

	if (src->boot_console)
		dest->boot_console = talloc_strdup(dest, src->boot_console);
	dest->manual_console = src->manual_console;

	if (src->lang && strlen(src->lang))
		dest->lang = talloc_strdup(dest, src->lang);
	else
		dest->lang = NULL;

	return dest;
}
