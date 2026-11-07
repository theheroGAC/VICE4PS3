#ifndef PS3_PATHS_H
#define PS3_PATHS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void ps3_paths_init(int argc, char **argv);
const char *ps3_base_dir(void);
const char *ps3_resolve_path(const char *path, char *buf, size_t sz);

#ifdef __cplusplus
}
#endif

#endif
