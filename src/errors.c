#include "errors.h"

#include "log.h"

#include <stdlib.h>
#include <string.h>

static void err_print1(err_t err);

err_t err_create(const char *name, const char *filename, const char *function, uint32_t line, char *message, int error_no, struct err_info *parent)
{
    struct err_info *err = malloc(sizeof(struct err_info));

    // These are values that cannot be NULL or 0
    err->name = name == 0 ? "UNKNOWN" : name;
    err->filename = filename == 0 ? "UNKNOWN" : filename;
    err->function = function == 0 ? "UNKNOWN" : function;

    // These are values that can be NULL or 0
    err->line = line;
    err->message = message == 0 ? NULL : strdup(message);
    err->error_no = error_no;
    err->parent = parent;

    return err;
}

err_t err_root(err_t err)
{
    if (err == NULL)
        return NULL;

    while (err->parent != NULL)
        err = err->parent;

    return err;
}

void err_print(err_t err)
{
    if (err == NULL)
        return;

    LOGL_ERROR(KBLK "ERROR: \n");
    err_print1(err);
    LOGL_ERROR("\n");
}

void err_free(err_t err)
{
    if (err == NULL)
        return;

    if (err->message)
        free(err->message);

    err_free(err->parent);
    free(err);
}

void err_print_free(err_t err)
{
    err_print(err);
    err_free(err);
}

static void err_print1(err_t err)
{
    if (err == NULL)
        return;

    // Calculate the number of digits in the line number
    uint32_t line_tmp = err->line;
    uint32_t line_digits = 0;
    while (line_tmp > 0)
    {
        line_tmp /= 10;
        line_digits++;
    }

    // LOGL_ERROR("    %*s:%" PRIu32 ", in %16s: %s", 31 - line_digits, err->filename, err->line, err->function, err->name);
    LOGL_ERROR("    %*s:%" PRIu32 ", in %s: %s", 31 - line_digits, err->filename, err->line, err->function, err->name);

    if (err->message)
        LOG_ERROR(": %s", err->message);
    if (err->error_no)
        LOG_ERROR(" [errno %d: %s]", err->error_no, strerror(err->error_no));

    LOG_ERROR("\n");

    err_print1(err->parent);
}
