/*
 * http_file_server.h — minimal static-file HTTP server (based on the vendored mongoose 7.x).
 *
 * Mounts a local directory as the HTTP root, so the app/cloud can download widget config / audio etc. by file name.
 * A C rewrite of the legacy C++ https_server (C++).
 *
 * A background thread runs the mongoose event loop; start may be called repeatedly (ignored if already running).
 * Pure non-protocol infrastructure; on MCUs without a filesystem/socket the whole module can be left out of the build (demo uses a macro switch).
 */
#ifndef HTTP_FILE_SERVER_H
#define HTTP_FILE_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Language directory resolution callback: returns the language subdirectory name to serve now (e.g. "en"/"zh").
 * Provided by the caller (usually reads AppWidget.current_lang recorded by get_file_list).
 * Falls back to "en" when it returns NULL/empty string.
 */
typedef const char *(*http_lang_fn)(void *user);

/**
 * Start the static-file server (serving by language subdirectory).
 * Actual root directory = base_dir/<lang_fn()>, where lang is resolved dynamically per request,
 * so after get_file_list switches language, subsequent downloads automatically land in the matching language directory.
 * @param listen_url  listen address, e.g. "http://0.0.0.0:8000"
 * @param base_dir    parent directory of the language subdirectories, e.g. "./config" (with en/ zh/ ... under it)
 * @param lang_fn     language resolution callback; when NULL, always serves base_dir/en
 * @param user        context passed through to lang_fn
 * @return 0 on success, non-zero on failure (also returns 0 if already running).
 */
int  http_file_server_start(const char *listen_url, const char *base_dir,
                            http_lang_fn lang_fn, void *user);

/** Stop the server and join the background thread (no-op if not started). */
void http_file_server_stop(void);

/** Whether it is running. */
int  http_file_server_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_FILE_SERVER_H */
