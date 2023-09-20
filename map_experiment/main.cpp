#include <iostream>
#include <unordered_map>
#include <malloc.h>

typedef struct
{
    uint16_t event;
    uint16_t len;
    uint16_t offset;
    uint16_t layer_specific;
    uint8_t data[10];
} BT_HDR;

struct re_list_item
{
    struct re_list_item *prev;
    uint64_t id1;
    uint64_t id2;
    
    BT_HDR *data;
};

struct re_unordered_map
{
    BT_HDR **hashtable;
    uint64_t idk1;
    struct re_list_item *list_tail;
    uint64_t idk2;
    uint64_t idk3;
};

static std::unordered_map<uint16_t /* handle */, BT_HDR *> partial_packets;

void dump_hex(const uint8_t *buffer, size_t size);

BT_HDR *create_packet(uint16_t handle)
{
    BT_HDR *hdr = (BT_HDR *)malloc(sizeof(BT_HDR));
    hdr->event = handle;
    hdr->len = handle;
    hdr->offset = handle;
    hdr->layer_specific = handle;
    for (int i = 0; i < sizeof(hdr->data); i++)
        hdr->data[i] = i;

    partial_packets[handle] = hdr;
    printf("packet1 allocated at %p\n", hdr);

    return hdr;
}

void experiment1(void)
{
    printf("partial_packets before allocating anything:\n");
    dump_hex((uint8_t *)&partial_packets, sizeof(partial_packets));
    printf("\n");

    uint16_t handle = 0;

    for (size_t i = 0; i < 5; i++)
    {
        printf("======================= iteration %zu =======================\n", i);

        handle += 0x1111;
        BT_HDR *hdr1 = create_packet(handle);

        printf("partial_packets after allocating packets:\n");
        dump_hex((uint8_t *)&partial_packets, sizeof(partial_packets));
        printf("\n");

        void *first_ptr = ((void **)&partial_packets)[0];
        printf("first_ptr = %p\n", first_ptr);
        printf("dump of the first pointer:\n");
        dump_hex((uint8_t *)first_ptr, malloc_usable_size(first_ptr));
        printf("\n");

        void *third_ptr = ((void **)&partial_packets)[2];
        printf("third_ptr = %p\n", third_ptr);
        printf("dump of the third pointer:\n");
        dump_hex((uint8_t *)third_ptr, malloc_usable_size(third_ptr));
        printf("\n");

        void *third_first_ptr = ((void **)third_ptr)[0];
        printf("third_first_ptr = %p\n", third_first_ptr);
        if (third_first_ptr)
        {
            printf("dump of the third_first pointer:\n");
            dump_hex((uint8_t *)third_first_ptr, malloc_usable_size(third_first_ptr));
            printf("\n");
        }

        void *third_fourth_ptr = ((void **)third_ptr)[3];
        printf("third_fourth_ptr = %p\n", third_fourth_ptr);
        printf("dump of the third_fourth pointer:\n");
        dump_hex((uint8_t *)third_fourth_ptr, malloc_usable_size(third_fourth_ptr));
        printf("\n");
    }
}

void experiment2(void)
{
    printf("partial_packets before allocating anything:\n");
    dump_hex((uint8_t *)&partial_packets, sizeof(partial_packets));
    printf("\n");

    uint16_t handle = 0;
    struct re_unordered_map *map = (struct re_unordered_map *)&partial_packets;

    for (size_t i = 0; i < 5; i++)
    {
        printf("\n======================= iteration %zu =======================\n", i);
        handle += 0x1111;
        BT_HDR *hdr1 = create_packet(handle);

        printf("partial_packets now:\n");
        dump_hex((uint8_t *)&partial_packets, sizeof(partial_packets));
        printf("\n");

        printf("map->hashtable = %p\n", map->hashtable);
        printf("map->list_tail = %p\n", map->list_tail);

        printf("\ndump of the hashtable:\n");
        dump_hex((uint8_t *)map->hashtable, malloc_usable_size(map->hashtable));
        printf("\n");

        printf("hashtable items\n");
        for (size_t j = 0; j < malloc_usable_size(map->hashtable) / sizeof(BT_HDR *); j++)
        {
            if (map->hashtable[j] == 0)
                continue;

            printf("found list_item pointer: %p\n", map->hashtable[j]);
        }

        printf("\nlist items:\n");
        struct re_list_item *list_item = map->list_tail;
        while (list_item)
        {
            printf("\nfound list_item pointer at %p: prev=%p, id1=%04lx, id2=%04lx, data=%p: \n", list_item, list_item->prev, list_item->id1, list_item->id2, list_item->data);
            dump_hex((uint8_t *)list_item, sizeof(*list_item));

            printf("data:\n");
            dump_hex((uint8_t *)list_item->data, sizeof(BT_HDR));
            list_item = list_item->prev;
        }
    }
}

int main(void)
{
    experiment2();

    return 0;
}

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