// wl-copy-multi: put one clipboard selection on Wayland that offers different content per
// MIME type, e.g. HTML for rich-text apps and markdown for plain-text ones. wl-copy can't:
// it serves a single payload under every type it offers.
//
//   wl-copy-multi text/html=page.html text/plain=page.md
//
// FILE may be "-" for stdin (once). text/plain also answers the usual text aliases
// (text/plain;charset=utf-8, UTF8_STRING, STRING, TEXT). Like wl-copy, it forks and keeps
// serving in the background until another client takes the clipboard.
//
// Uses ext-data-control-v1, so it needs no focused surface.
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include "ext-data-control-v1-client-protocol.h"

struct offer {
	const char *mime;
	char *data;
	size_t len;
};

static struct offer offers[64];
static int noffers;
static struct wl_seat *seat;
static struct ext_data_control_manager_v1 *manager;

static void die(const char *msg) {
	fprintf(stderr, "wl-copy-multi: %s\n", msg);
	exit(1);
}

static char *read_all(const char *path, size_t *len) {
	FILE *f = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
	if (!f) die(strerror(errno));
	char *buf = NULL;
	size_t cap = 0, n = 0, r;
	do {
		if (n == cap) {
			cap = cap ? cap * 2 : 65536;
			if (!(buf = realloc(buf, cap))) die("out of memory");
		}
		r = fread(buf + n, 1, cap - n, f);
		n += r;
	} while (r > 0);
	if (f != stdin) fclose(f);
	*len = n;
	return buf;
}

static void add_offer(const char *mime, char *data, size_t len) {
	if (noffers == (int)(sizeof offers / sizeof *offers)) die("too many types");
	offers[noffers++] = (struct offer){ mime, data, len };
}

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version) {
	if (!seat && strcmp(interface, wl_seat_interface.name) == 0)
		seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
	else if (strcmp(interface, ext_data_control_manager_v1_interface.name) == 0)
		manager = wl_registry_bind(registry, name, &ext_data_control_manager_v1_interface, 1);
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static void source_send(void *data, struct ext_data_control_source_v1 *source,
		const char *mime, int32_t fd) {
	for (int i = 0; i < noffers; i++) {
		if (strcmp(offers[i].mime, mime) != 0) continue;
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
		size_t off = 0;
		while (off < offers[i].len) {
			ssize_t w = write(fd, offers[i].data + off, offers[i].len - off);
			if (w < 0 && errno == EINTR) continue;
			if (w <= 0) break;
			off += w;
		}
		break;
	}
	close(fd);
}

static void source_cancelled(void *data, struct ext_data_control_source_v1 *source) {
	exit(0);
}

static const struct ext_data_control_source_v1_listener source_listener = {
	.send = source_send,
	.cancelled = source_cancelled,
};

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: wl-copy-multi MIME=FILE [MIME=FILE ...]   (FILE may be -)\n");
		return 2;
	}
	for (int i = 1; i < argc; i++) {
		char *eq = strchr(argv[i], '=');
		if (!eq || eq == argv[i]) die("arguments are MIME=FILE");
		*eq = '\0';
		size_t len;
		char *buf = read_all(eq + 1, &len);
		add_offer(argv[i], buf, len);
		if (strcmp(argv[i], "text/plain") == 0) {
			add_offer("text/plain;charset=utf-8", buf, len);
			add_offer("UTF8_STRING", buf, len);
			add_offer("STRING", buf, len);
			add_offer("TEXT", buf, len);
		}
	}

	signal(SIGPIPE, SIG_IGN);
	struct wl_display *display = wl_display_connect(NULL);
	if (!display) die("cannot connect to Wayland display");
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);
	if (!seat) die("no seat");
	if (!manager) die("compositor lacks ext-data-control-v1");

	struct ext_data_control_source_v1 *source =
		ext_data_control_manager_v1_create_data_source(manager);
	ext_data_control_source_v1_add_listener(source, &source_listener, NULL);
	for (int i = 0; i < noffers; i++)
		ext_data_control_source_v1_offer(source, offers[i].mime);

	// The device is only needed to set the selection; dropping it right away avoids
	// handling the offer events it would send about every later clipboard change.
	struct ext_data_control_device_v1 *device =
		ext_data_control_manager_v1_get_data_device(manager, seat);
	ext_data_control_device_v1_set_selection(device, source);
	ext_data_control_device_v1_destroy(device);
	if (wl_display_roundtrip(display) < 0) die("lost the Wayland connection");

	pid_t pid = fork();
	if (pid < 0) die("fork failed");
	if (pid > 0) _exit(0);
	setsid();
	int null = open("/dev/null", O_RDWR);
	if (null >= 0) {
		dup2(null, 0);
		dup2(null, 1);
		dup2(null, 2);
		if (null > 2) close(null);
	}
	while (wl_display_dispatch(display) != -1)
		;
	return 0;
}
