#ifndef OS_COMPATIBILITY_H
#define OS_COMPATIBILITY_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <sys/types.h>

int
os_create_anonymous_file(off_t size);

#ifdef  __cplusplus
}
#endif

#endif /* OS_COMPATIBILITY_H */
