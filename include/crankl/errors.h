#ifndef CRANKL_ERRORS_H
#define CRANKL_ERRORS_H

#ifdef __cplusplus
extern "C" {
#endif

#define CRANKL_OK 0
#define CRANKL_ERR_NULL -1
#define CRANKL_ERR_INVALID -2
#define CRANKL_ERR_IO -3
#define CRANKL_ERR_FORMAT -4
#define CRANKL_ERR_NO_METADATA 1

const char *crankl_strerror(int code);

#ifdef __cplusplus
}
#endif

#endif
