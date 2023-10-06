#pragma once

#include <errno.h>
#include <stdint.h>

#define ERR_RET(name, message) return err_create(name, __FILE__, __func__, __LINE__, message, 0, NULL);
#define ERR_RET_SYS(name, message) return err_create(name, __FILE__, __func__, __LINE__, message, errno, NULL);
#define ERR_FWD_MSG_CLEANUP(message, err, cleanup)                                          \
    {                                                                                       \
        err_t _err = (err);                                                                 \
        if (_err)                                                                           \
        {                                                                                   \
            {                                                                               \
                cleanup                                                                     \
            }                                                                               \
            return err_create("FORWARDED", __FILE__, __func__, __LINE__, message, 0, _err); \
        }                                                                                   \
    }
#define ERR_FWD(err) ERR_FWD_MSG_CLEANUP(NULL, err, )
#define ERR_FWD_MSG(nessage, err) ERR_FERR_FWD_MSG_CLEANUPWD_MSG(nessage, err, )
#define ERR_FWD_CLEANUP(err, cleanup) ERR_FWD_MSG_CLEANUP(NULL, err, cleanup)

typedef struct err_info *err_t;

struct err_info
{
    const char *name;
    const char *filename;
    const char *function;
    uint32_t line;

    char *message;
    int error_no;

    struct err_info *parent;
};

err_t err_create(const char *name, const char *filename, const char *function, uint32_t line, char *message, int error_no, struct err_info *parent);
err_t err_root(err_t err);

void err_print(err_t err);
void err_free(err_t err);
void err_print_free(err_t err);