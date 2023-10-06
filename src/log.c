#include "log.h"

int log_level = 6;
char log_name[64] = "~";

void logl_hex(const uint8_t *buffer, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        if (i % 16 == 0)
        {
            if (i != 0)
                LOG_DBG_INFO("\n");
            LOGL_DBG_INFO("%02X ", buffer[i])
        }
        else
            LOG_DBG_INFO("%02X ", buffer[i])
    }

    LOG_DBG_INFO("\n");
}

bool logl_ask_yn(const char *question, bool default_answer)
{
    bool answer = default_answer;

    LOGL_ASK("%s? [%c/%c] ", question, default_answer ? 'Y' : 'y', default_answer ? 'n' : 'N')

    fflush(stdin);
    char answer_char = getchar();

    if (answer_char == 'y' || answer_char == 'Y' || answer_char == 'n' || answer_char == 'N')
    {
        if (getchar() == '\n') // If the second char is a newline, then the user only entered one char
        {
            if (answer_char == 'y' || answer_char == 'Y')
                return true;
            else if (answer_char == 'n' || answer_char == 'N')
                return false;
        }
    }

    fflush(stdin);

    return answer;
}