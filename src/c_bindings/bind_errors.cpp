#include "crankl/errors.h"
#include "crankl/version.h"
#include "crankl/version_api.h"

extern "C" {

const char *crankl_version_string(void) { return CRANKL_VERSION_STRING; }

void crankl_version(int *major, int *minor, int *patch) {
    if (major)
        *major = CRANKL_VERSION_MAJOR;
    if (minor)
        *minor = CRANKL_VERSION_MINOR;
    if (patch)
        *patch = CRANKL_VERSION_PATCH;
}

const char *crankl_strerror(int code) {
    switch (code) {
    case CRANKL_OK:
        return "ok";
    case CRANKL_ERR_NULL:
        return "null argument";
    case CRANKL_ERR_INVALID:
        return "invalid argument";
    case CRANKL_ERR_IO:
        return "I/O error";
    case CRANKL_ERR_FORMAT:
        return "format error";
    case CRANKL_ERR_NO_METADATA:
        return "no metadata footer";
    default:
        return "unknown error";
    }
}

} // extern "C"
