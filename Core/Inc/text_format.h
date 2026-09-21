#ifndef TEXT_FORMAT_H
#define TEXT_FORMAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

int TextFormat(char *buffer, size_t size, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* TEXT_FORMAT_H */
