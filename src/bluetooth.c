#include "bluetooth.h"

#include "log.h"
#include "errors.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#define BLUETOOTH_L2CAP_RECV_TIMEOUT_MS (1500)
#define BLUETOOTH_DISCONNECT_REST_SECONDS (3)
#define BLUETOOTH_SUPERVISION_TIMEOUT_MS (5000)

err_t bluetooth_restart(void)
{
    if (system("sudo btmgmt power off 1>/dev/null 2>/dev/null") < 0)
        ERR_RET_SYS("BTMGMT_POWER_OFF", "Failed to power off Bluetooth adapter")

    if (system("sudo btmgmt power on 1>/dev/null 2>/dev/null") < 0)
        ERR_RET_SYS("BTMGMT_POWER_ON", "Failed to power on Bluetooth adapter")

    sleep(BLUETOOTH_DISCONNECT_REST_SECONDS);

    return NULL;
}

err_t bluetooth_connect(const bdaddr_t bdaddr, bool ssp_mode, bool restart, int *hci_sock_fd, int *l2cap_sock_fd, uint16_t *hci_handle)
{
    if (restart)
        ERR_FWD(bluetooth_restart());

    ERR_FWD(bluetooth_hci_set_ssp_mode(ssp_mode));
    ERR_FWD(bluetooth_hci_create(hci_sock_fd));

    ERR_FWD(bluetooth_l2cap_create(l2cap_sock_fd));
    ERR_FWD(bluetooth_l2cap_connect(*l2cap_sock_fd, bdaddr, hci_handle))

    // call hci_write_link_supervision_timeout
    if (hci_write_link_supervision_timeout(*hci_sock_fd, *hci_handle, BLUETOOTH_SUPERVISION_TIMEOUT_MS, BLUETOOTH_SUPERVISION_TIMEOUT_MS) < 0)
        ERR_RET_SYS("HCI_WRITE_LINK_SUPERVISION_TIMEOUT", "Failed to write link supervision timeout")

    return NULL;
}

err_t bluetooth_disconnect(int hci_sock_fd, int l2cap_sock_fd, uint16_t hci_handle)
{
    if (hci_sock_fd < 0)
        ERR_RET("INVALID_HCI_SOCK_FD", "Parameter hci_sock_fd cannot be less than 0");
    if (l2cap_sock_fd < 0)
        ERR_RET("INVALID_L2CAP_SOCK_FD", "Parameter l2cap_sock_fd cannot be less than 0");

    // This sometimes slows down the disconnection operation, so we'll skip it
    // if (hci_disconnect(hci_sock_fd, hci_handle, HCI_OE_USER_ENDED_CONNECTION, 0) < 0)
    //     ERR_RET_SYS("HCI_DISCONNECT", "Failed to disconnect from Bluetooth address")

    if (bluetooth_l2cap_close(l2cap_sock_fd) < 0)
        ERR_RET("L2CAP_CLOSE", "Failed to close L2CAP socket")

    if (bluetooth_hci_close(hci_sock_fd) < 0)
        ERR_RET("HCI_CLOSE", "Failed to close HCI socket")

    return NULL;
}

err_t bluetooth_hci_set_ssp_mode(bool ssp_mode)
{
    // Disable Simple Pairing mode
    // If we do not disable SSP, this will cause the connection to fail just after a few seconds
    // (because the phone will be waiting on a PIN code and timeout if we do not provide one)
    int btmgmt_ret = system(ssp_mode ? "sudo btmgmt ssp on 1>/dev/null 2>/dev/null" : "sudo btmgmt ssp off 1>/dev/null 2>/dev/null");
    int hciconfig_ret = system(ssp_mode ? "sudo hciconfig hci0 sspmode 1 1>/dev/null 2>/dev/null" : "sudo hciconfig hci0 sspmode 0 1>/dev/null 2>/dev/null");

    if (btmgmt_ret != 0 && hciconfig_ret != 0)
    {
        LOGL_DBG_VAR(btmgmt_ret, "%d");
        LOGL_DBG_VAR(hciconfig_ret, "%d");

        ERR_RET("DISABLE_SSP", "Failed to set Simple Pairing mode")
    }

    return NULL;
}

err_t bluetooth_hci_create(int *hci_sock_fd)
{
    if (hci_sock_fd == NULL)
        ERR_RET("NULL_HCI_SOCK_FD", "Parameter hci_sock_fd cannot be NULL");

    // Get the device ID of the first available Bluetooth adapter
    int hci_dev_id = hci_get_route(NULL);
    if (hci_dev_id < 0)
        ERR_RET("HCI_GET_ROUTE", "Failed to get device ID")
    // LOGL_DBG_VAR(hci_dev_id, "%d");

    // Get the device info of the first available Bluetooth adapter
    struct hci_dev_info hci_dev_info;
    if (hci_devinfo(hci_dev_id, &hci_dev_info) < 0)
        ERR_RET("HCI_DEVINFO", "Failed to get device info")
    // LOGL_DBG_VAR(hci_dev_info.dev_id, "%d");
    // LOGL_INFO("Using HCI Device %" PRIu16 ": %s (%02X:%02X:%02X:%02X:%02X:%02X)\n", hci_dev_info.dev_id, hci_dev_info.name, hci_dev_info.bdaddr.b[5], hci_dev_info.bdaddr.b[4], hci_dev_info.bdaddr.b[3], hci_dev_info.bdaddr.b[2], hci_dev_info.bdaddr.b[1], hci_dev_info.bdaddr.b[0]);

    // Open a socket to the first available Bluetooth adapter
    int _hci_sock_fd = hci_open_dev(hci_dev_id);
    if (_hci_sock_fd < 0)
        ERR_RET("HCI_OPEN_DEV", "Failed to open socket")
    // LOGL_DBG_VAR(_hci_sock_fd, "%d");

    // if (hci_write_simple_pairing_mode(_hci_sock_fd, 0, 10000) < 0)
    //     ERR_RET_SYS("HCI_WRITE_SIMPLE_PAIRING_MODE", "Failed to write simple pairing mode")

    // ERR_RET_SYS("", "Failed to open socket")
    // {
    //     fprintf(stderr, "Can't set Simple Pairing mode on hci%d: %s (%d)\n",
    //             0, strerror(errno), errno);
    //     exit(1);
    // }

    // Disable encryption
    // cmd_enc(_hci_sock_fd, hci_dev_info.bdaddr, 0);
    // struct hci_conn_info_req *cr = malloc(sizeof(*cr) + sizeof(struct hci_conn_info));
    // if (!cr)
    // {
    //     perror("Can't allocate memory");
    //     exit(1);
    // }

    // bacpy(&cr->bdaddr, &bdaddr);
    // cr->type = ACL_LINK;
    // if (ioctl(dd, HCIGETCONNINFO, (unsigned long)cr) < 0)
    // {
    //     perror("Get connection info failed");
    //     exit(1);
    // }

    // encrypt = (argc > 1) ? atoi(argv[1]) : 1;

    // if (hci_encrypt_link(dd, htobs(cr->conn_info->handle), encrypt, 25000) < 0)
    // {
    //     perror("HCI set encryption request failed");
    //     exit(1);
    // }

    *hci_sock_fd = _hci_sock_fd;
    return NULL;
}

err_t bluetooth_hci_close(const int hci_sock_fd)
{
    if (hci_sock_fd < 0)
        ERR_RET("INVALID_HCI_SOCK_FD", "Parameter hci_sock_fd cannot be less than 0");

    if (hci_close_dev(hci_sock_fd) < 0)
        ERR_RET_SYS("CLOSE", "Failed to close socket")

    return NULL;
}

err_t bluetooth_remote_header_spoof(const uint16_t hci_handle, const uint8_t packet_id, const uint16_t continuation_size, const bool is_continuation, const uint16_t data_size, struct bluetooth_remote_header *const header)
{
    if (header == NULL)
        ERR_RET("NULL_HEADER", "Parameter header cannot be NULL");

    // HCI+ACL+L2CAP Packet format:
    // BT Header:       [0-1]   Event Code
    //                  [1-2]   len(hci_hdr) + len(acl_hdr) + len(l2cap_hdr) + len(data) = 4 + 4 + 4 + len(data) = 12 + len(data)
    //                  [2-3]   offset (we set that to zero because we don't use it and frankly idk what it does)
    //                  [3-4]   layer_specific (we set that to zero because we don't use it and frankly idk what it does either)
    // HCI Header:      [0-1]   HCI Handle | Continuation Flags
    //                  [1-2]   len(acl_hdr) + len(l2cap_hdr) + len(data) = 4 + 4 + len(data) = 8 + len(data)
    // ACL Header:      [3-4]   len(l2cap_hdr) + len(data) = 4 + len(data)  [+2 for the second packet]
    //                  [5-6]   cid
    // L2CAP Header:    [7]     0x8
    //                  [8]     Packet Identifier (zero-indexed)
    //                  [9-10]  len(data)                                   [+2 for the second packet]
    //                  [...]   data

    header->bt_header.event = htobs(BLUETOOTH_BT_EVT_TO_BTU_HCI_ACL);
    header->bt_header.len = htobs(sizeof(header->hci_header) + sizeof(header->acl_header) + sizeof(header->l2cap_header) + data_size);
    header->bt_header.offset = htobs(0);
    header->bt_header.layer_specific = htobs(0);

    header->hci_header.handle = htobs((BLUETOOTH_HCI_HANDLE_VALUE_MASK & hci_handle) | (BLUETOOTH_HCI_HANDLE_FLAGS_MASK & ((is_continuation ? BLUETOOTH_HCI_CONTINUATION_PACKET_BOUNDARY : BLUETOOTH_HCI_START_PACKET_BOUNDARY) << 12)));
    header->hci_header.len = htobs(sizeof(header->acl_header) + sizeof(header->l2cap_header) + data_size);

    header->acl_header.len = htobs(sizeof(header->l2cap_header) + data_size + continuation_size);
    header->acl_header.cid = htobs(1); // L2CAP Signaling Channel

    header->l2cap_header.code = L2CAP_ECHO_REQ; // L2CAP Connection Parameter Update Request
    header->l2cap_header.identifier = packet_id;
    header->l2cap_header.len = htobs(data_size + continuation_size);

    return NULL;
}

err_t bluetooth_hci_header_spoof(const uint16_t hci_handle, const uint8_t packet_id, const uint16_t continuation_size, const bool is_continuation, const uint16_t data_size, struct bluetooth_header *const header)
{
    if (header == NULL)
        ERR_RET("NULL_HEADER", "Parameter header cannot be NULL");

    // HCI+ACL+L2CAP Packet format:
    // HCI Header:      [0-1]   HCI Handle | Continuation Flags
    //                  [1-2]   len(acl_hdr) + len(l2cap_hdr) + len(data) = 4 + 4 + len(data) = 8 + len(data)
    // ACL Header:      [3-4]   len(l2cap_hdr) + len(data) = 4 + len(data)  [+2 for the second packet]
    //                  [5-6]   cid
    // L2CAP Header:    [7]     0x8
    //                  [8]     Packet Identifier (zero-indexed)
    //                  [9-10]  len(data)                                   [+2 for the second packet]
    //                  [...]   data

    header->hci_header.handle = htobs((BLUETOOTH_HCI_HANDLE_VALUE_MASK & hci_handle) | (BLUETOOTH_HCI_HANDLE_FLAGS_MASK & ((is_continuation ? 1 : 0) << 12)));
    header->hci_header.len = htobs(sizeof(header->acl_header) + sizeof(header->l2cap_header) + data_size);

    header->acl_header.len = htobs(sizeof(header->l2cap_header) + data_size + continuation_size);
    header->acl_header.cid = htobs(1); // L2CAP Signaling Channel

    header->l2cap_header.code = L2CAP_ECHO_REQ; // L2CAP Connection Parameter Update Request
    header->l2cap_header.identifier = packet_id;
    header->l2cap_header.len = htobs(data_size + continuation_size);

    return NULL;
}

err_t bluetooth_hci_packet_send(const int hci_sock_fd, struct bluetooth_header header, uint8_t *data, const uint16_t data_size)
{
    if (hci_sock_fd < 0)
        ERR_RET("INVALID_HCI_SOCK_FD", "Parameter hci_sock_fd cannot be less than 0");
    if (data == NULL)
        ERR_RET("NULL_DATA", "Parameter data cannot be NULL");

    // The type of the packet has to be appended before the packet
    uint8_t packet_type = HCI_ACLDATA_PKT;

    struct iovec iov[3];
    iov[0].iov_base = &packet_type;
    iov[0].iov_len = sizeof(packet_type);
    iov[1].iov_base = &header;
    iov[1].iov_len = sizeof(header);
    iov[2].iov_base = data;
    iov[2].iov_len = data_size;

    while (writev(hci_sock_fd, iov, sizeof(iov) / sizeof(iov[0])) < 0)
    {
        if (errno == EAGAIN || errno == EINTR)
            continue;
        ERR_RET_SYS("WRITEV", "Failed to write to socket")
    }

    return NULL;
}

err_t bluetooth_hci_packet_spoof_send(const int hci_sock_fd, const uint16_t hci_handle, const uint8_t packet_id, const uint16_t continuation_size, const bool is_continuation, uint8_t *data, const uint16_t data_size)
{
    struct bluetooth_header header;

    ERR_FWD(bluetooth_hci_header_spoof(hci_handle, packet_id, continuation_size, is_continuation, data_size, &header));
    ERR_FWD(bluetooth_hci_packet_send(hci_sock_fd, header, data, data_size));

    return NULL;
}

err_t bluetooth_str_to_bdaddr(const char *bdaddr_str, bdaddr_t *bdaddr)
{
    if (bdaddr_str == NULL)
        ERR_RET("NULL_BDADDR_STR", "Parameter bdaddr_str cannot be NULL");

    if (str2ba(bdaddr_str, bdaddr) < 0)
        ERR_RET("STR2BA", "Failed to convert string to bdaddr_t")

    return NULL;
}

err_t bluetooth_l2cap_create(int *l2cap_sock_fd)
{
    if (l2cap_sock_fd == NULL)
        ERR_RET("NULL_L2CAP_SOCK_FD", "Parameter l2cap_sock_fd cannot be NULL");

    int _l2cap_sock_fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_L2CAP);
    if (_l2cap_sock_fd < 0)
        ERR_RET_SYS("SOCKET", "Failed to create socket")
    // LOGL_DBG_VAR(_l2cap_sock_fd, "%d");

    *l2cap_sock_fd = _l2cap_sock_fd;
    return NULL;
}

err_t bluetooth_l2cap_connect(const int l2cap_sock_fd, const bdaddr_t bdaddr, uint16_t *hci_handle)
{
    if (l2cap_sock_fd < 0)
        ERR_RET("INVALID_L2CAP_SOCK_FD", "Parameter l2cap_sock_fd cannot be less than 0");
    if (hci_handle == NULL)
        ERR_RET("NULL_HCI_HANDLE", "Parameter hci_handle cannot be NULL");

    // Create a sockaddr_l2 struct representing the Bluetooth address to connect to
    struct sockaddr_l2 sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.l2_family = AF_BLUETOOTH;
    sockaddr.l2_bdaddr = bdaddr;

    // Connect to the Bluetooth address
    if (connect(l2cap_sock_fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
        ERR_RET_SYS("CONNECT", "Failed to connect to Bluetooth address")

    // Get the connection info to note the HCI handle
    uint16_t _hci_handle;
    struct l2cap_conninfo conninfo;
    socklen_t conninfo_len = sizeof(conninfo);

    if (getsockopt(l2cap_sock_fd, SOL_L2CAP, L2CAP_CONNINFO, &conninfo, &conninfo_len) < 0)
        ERR_RET_SYS("GETSOCKOPT", "Failed to get connection info")
    _hci_handle = conninfo.hci_handle;
    // LOGL_DBG_VAR(_hci_handle, "%" PRIu16);

    // Reduce the timeout to speed up the disconnection process
    struct timeval recv_timeout = {
        .tv_sec = BLUETOOTH_L2CAP_RECV_TIMEOUT_MS / 1000,
        .tv_usec = (BLUETOOTH_L2CAP_RECV_TIMEOUT_MS % 1000) * 1000,
    };
    if (setsockopt(l2cap_sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&recv_timeout, sizeof(recv_timeout)) < 0)
        ERR_RET_SYS("SETSOCKOPT", "Failed to set SO_RCVTIMEO socket option")

    *hci_handle = _hci_handle;
    return NULL;
}

// err_t bluetooth_l2cap_disconnect(const int hci_sock_fd, const int l2cap_sock_fd, const uint16_t hci_handle)
// {
//     if (l2cap_sock_fd < 0)
//         ERR_RET("INVALID_L2CAP_SOCK_FD", "Parameter l2cap_sock_fd cannot be less than 0");

//     if (hci_disconnect(hci_sock_fd, hci_handle, ) < 0)
//         ERR_RET_SYS("DISCONNECT", "Failed to disconnect from Bluetooth address")

// }
err_t bluetooth_l2cap_close(const int l2cap_sock_fd)
{
    if (l2cap_sock_fd < 0)
        ERR_RET("INVALID_L2CAP_SOCK_FD", "Parameter l2cap_sock_fd cannot be less than 0");

    if (close(l2cap_sock_fd) < 0)
        ERR_RET_SYS("CLOSE", "Failed to close socket")

    return NULL;
}

err_t bluetooth_l2cap_recv(const int l2cap_sock_fd, uint8_t *buffer, size_t buffer_max_size, size_t *read_amount)
{
    if (l2cap_sock_fd < 0)
        ERR_RET("INVALID_L2CAP_SOCK_FD", "Parameter l2cap_sock_fd cannot be less than 0");
    if (buffer == NULL)
        ERR_RET("NULL_BUFFER", "Parameter buffer cannot be NULL");
    if (buffer_max_size == 0)
        ERR_RET("ZERO_BUFFER_MAX_SIZE", "Parameter buffer_max_size cannot be 0");
    if (read_amount == NULL)
        ERR_RET("NULL_READ_AMOUNT", "Parameter read_amount cannot be NULL");

    ssize_t _read_amount = recv(l2cap_sock_fd, buffer, buffer_max_size, 0);
    if (_read_amount < 0)
        ERR_RET_SYS("RECV", "Failed to receive data")
    // LOGL_DBG_VAR(_read_amount, "%zd");

    *read_amount = _read_amount;
    return NULL;
}

err_t bluetooth_l2cap_recv_packet(const int l2cap_sock_fd, struct bluetooth_l2cap_header **const packet)
{
    if (l2cap_sock_fd < 0)
        ERR_RET("INVALID_L2CAP_SOCK_FD", "Parameter l2cap_sock_fd cannot be less than 0");
    if (packet == NULL)
        ERR_RET("NULL_PACKET", "Parameter packet cannot be NULL");

    struct bluetooth_l2cap_header header;

    // Receive the header
    while (1)
    {
        ssize_t read_amount = recv(l2cap_sock_fd, (uint8_t *)&header, sizeof(header), MSG_PEEK);
        if (read_amount < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                continue;

            ERR_RET_SYS("RECV", "Failed to receive data")
        }

        if (read_amount != sizeof(header))
            ERR_RET("INVALID_HEADER_SIZE", "Received header size is not the expected size")

        break;
    }

    // Calculate the read size
    header.len = btohs(header.len);
    size_t data_size = header.len + sizeof(header);

    // Receive the data of the packet
    size_t read_amount = 0;
    *packet = (struct bluetooth_l2cap_header *)malloc(data_size);
    memset(*packet, 0, data_size);
    ERR_FWD(bluetooth_l2cap_recv(l2cap_sock_fd, (uint8_t *)*packet, data_size, &read_amount));

    if (read_amount != data_size)
        ERR_RET("INVALID_DATA_SIZE", "Received data size is not the expected size")

    return NULL;
}

err_t bluetooth_l2cap_recv_packet_id(const int l2cap_sock_fd, struct bluetooth_l2cap_header **const packet, const uint8_t packet_id)
{
    while (1)
    {
        struct bluetooth_l2cap_header *_packet;

        ERR_FWD(bluetooth_l2cap_recv_packet(l2cap_sock_fd, &_packet));
        if (_packet->code == BLUETOOTH_L2CAP_CMD_ECHO_RSP && _packet->identifier == packet_id)
        {
            // logl_hex(_packet->data, _packet->len);
            *packet = _packet;
            return NULL;
        }

        LOGL_WARNING("Skipping packet with code %02X and identifier %02X\n", _packet->code, _packet->identifier)
        free(_packet);
    }

    // Just a fallback
    return NULL;
}

inline err_t bluetooth_inc_packet_id(uint8_t *const packet_id)
{
    if (packet_id == NULL)
        ERR_RET("NULL_PACKET_ID", "Parameter packet_id cannot be NULL");

    *packet_id += *packet_id == 0xff ? 2 : 1;

    return NULL;
}