/*
 * upgrade_http_server.h — HTTP service for upgrade firmware (based on vendored mongoose 7.x).
 *
 * During the upgrade flow the Payload needs an HTTP server to host/receive the firmware file:
 * the MQTT channel is only suited for small commands, while large firmware (MB-scale) is
 * better carried over HTTP. In the enter_upgrade_mode reply the Payload returns this service's
 * URL to the aircraft, which then GETs to download (or PUTs to upload new firmware).
 *
 * Supports both directions (matching the behavior of the legacy C++ https_server):
 *   · GET  /<file>   return web_root/<file> as a static file (aircraft downloads firmware)
 *   · PUT  /<file>   write the request body to web_root/<file> (aircraft/cloud uploads firmware to Payload)
 *
 * A background pthread runs the mongoose event loop; start may be called repeatedly (ignored if already running).
 * When the MCU has no filesystem/socket the whole module can be excluded from the build via a macro switch.
 */
#ifndef UPGRADE_HTTP_SERVER_H
#define UPGRADE_HTTP_SERVER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the firmware HTTP service (supports both GET download / PUT upload).
 * @param listen_url  listen address, e.g. "http://0.0.0.0:8000"
 * @param web_root    firmware storage directory, e.g. "/tmp/upgrade_share" (must already exist; GET reads from it, PUT writes to it)
 * @return 0 on success, non-zero on failure (returns 0 if already running).
 */
int  upgrade_http_server_start(const char *listen_url, const char *web_root);

/** Stop the service and join the background thread (no-op if not started). */
void upgrade_http_server_stop(void);

/** Whether it is running. */
int  upgrade_http_server_is_running(void);

/**
 * Probe the externally reachable IPv4 of this host (used to build the URL returned to the aircraft in the enter_upgrade_mode reply).
 * Picks the interface by priority when choosing the IP, ensuring the aircraft receives the address on the Payload's
 * "interface interconnected with the aircraft":
 *   1) exact match of prefer_iface (e.g. "usb0");
 *   2) an interface whose name starts with "usb" (a DJI Payload interconnects with the aircraft over the USB network, firmware goes through this port);
 *   3) any non-loopback IPv4 (fallback).
 * When none is found, write "127.0.0.1" and return non-zero.
 * @param out           output buffer (dotted-decimal string)
 * @param n             buffer size (recommended >= 16)
 * @param prefer_iface  the interface name to prefer (e.g. "usb0"); pass NULL/"" to auto-pick from usb*.
 * @return 0 if a real address is found; non-zero if falling back to 127.0.0.1 (only usable for local self-test).
 */
int  upgrade_http_local_ip(char *out, size_t n, const char *prefer_iface);

#ifdef __cplusplus
}
#endif

#endif /* UPGRADE_HTTP_SERVER_H */
