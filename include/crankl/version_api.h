#ifndef CRANKL_VERSION_API_H
#define CRANKL_VERSION_API_H

#ifdef __cplusplus
extern "C" {
#endif

const char *crankl_version_string(void);
void crankl_version(int *major, int *minor, int *patch);

#ifdef __cplusplus
}
#endif

#endif
