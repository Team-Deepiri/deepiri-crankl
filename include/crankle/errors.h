#ifndef CRANKLE_ERRORS_H
#define CRANKLE_ERRORS_H

#ifdef __cplusplus
extern "C" {
#endif

#define CRANKLE_OK 0
#define CRANKLE_ERR_NULL -1
#define CRANKLE_ERR_INVALID -2
#define CRANKLE_ERR_IO -3
#define CRANKLE_ERR_FORMAT -4
#define CRANKLE_ERR_NO_METADATA 1

const char *crankle_strerror(int code);

#ifdef __cplusplus
}
#endif

#endif
