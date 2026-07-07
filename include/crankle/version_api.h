#ifndef CRANKLE_VERSION_API_H
#define CRANKLE_VERSION_API_H

#ifdef __cplusplus
extern "C" {
#endif

const char *crankle_version_string(void);
void crankle_version(int *major, int *minor, int *patch);

#ifdef __cplusplus
}
#endif

#endif
