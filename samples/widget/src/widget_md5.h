/*
 * widget_md5.h — minimal MD5 (public-domain implementation by Solar Designer / Alexander Peslyak).
 *
 * Used only by the widget demo's get_file_list to compute the md5 of config files, avoiding an openssl header dependency.
 * Usage:
 *   char hex[33];
 *   widget_md5_file("config/en/widget_config.json", hex);   // returns non-zero on failure
 */
#ifndef WIDGET_MD5_H
#define WIDGET_MD5_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compute the md5 of buf[0..len), writing 32 lowercase hex bytes + '\0' to out_hex33 (must be >=33 bytes). */
void widget_md5_buf(const void *buf, size_t len, char out_hex33[33]);

/* Compute the md5 of the file contents, writing hex to out_hex33. Returns 0 on success, -1 if open/read fails. */
int  widget_md5_file(const char *path, char out_hex33[33]);

#ifdef __cplusplus
}
#endif

#endif /* WIDGET_MD5_H */
