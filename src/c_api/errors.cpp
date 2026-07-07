#include "crankle/errors.h"
#include "crankle/version.h"
#include "crankle/version_api.h"

extern "C" {

const char *crankle_version_string(void) { return CRANKLE_VERSION_STRING; }

void crankle_version(int *major, int *minor, int *patch) {
    if (major)
        *major = CRANKLE_VERSION_MAJOR;
    if (minor)
        *minor = CRANKLE_VERSION_MINOR;
    if (patch)
        *patch = CRANKLE_VERSION_PATCH;
}

const char *crankle_strerror(int code) {
    switch (code) {
    case CRANKLE_OK:
        return "ok";
    case CRANKLE_ERR_NULL:
        return "null argument";
    case CRANKLE_ERR_INVALID:
        return "invalid argument";
    case CRANKLE_ERR_IO:
        return "I/O error";
    case CRANKLE_ERR_FORMAT:
        return "format error";
    case CRANKLE_ERR_NO_METADATA:
        return "no metadata footer";
    default:
        return "unknown error";
    }
}

} // extern "C"
