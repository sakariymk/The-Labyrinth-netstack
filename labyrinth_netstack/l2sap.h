#ifndef L2SAP_H
#define L2SAP_H

#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

/* This is the maximum size of a frame in bytes.
 * Frames that are sent over our emulated network can never
 * be longer than this number.
 * The maximum size includes the L2Header.
 */
#define L2Framesize   1024
#define L2Headersize  (int)(sizeof(struct L2Header))
#define L2Payloadsize (int)(L2Framesize-L2Headersize)

#define L2_TIMEOUT    0

typedef struct L2Header L2Header;

struct L2Header
{
    /* This is the mac_addr to which we send this frame.
     * It is meant to be L2 layer's MAC address, but since
     * we are emulating the L2 layer over UDP, we use the
     * IPv4 address of the test computers.
     * If you convert the dotted decimal notation of a
     * computer, e.g. the string 127.0.0.1, you could
     * use
     * struct sockaddr_in addr;
     * inet_pton(AF_INET,"127.0.0.1", &addr);
     * and copy the 4 bytes that are contained in addr.s_addr
     * (which are the IPv4 address in network byte order)
     * into this field.
     */
    uint32_t dst_addr;

    /* This is the number of bytes that are used in this
     * frame. It is stored in network byte order.
     */
    uint16_t len;

    /* Compute the checksum by setting this field to zero,
     * compute XOR of all other bytes in the frame that are
     * used for transporting the payload, and store it in
     * the field checksum.
     */
    uint8_t  checksum;

    /* mbz: must be zero.
     * This field exists in the header to because that makes
     * it 8 bytes long instead of 7. If it was 7 bytes long,
     * some compilers would magically extend it to 8 bytes
     * and others wouldn't. That doesn't happen with 8 bytes.
     */
    uint8_t  mbz;
};

typedef struct L2SAP L2SAP;

struct L2SAP
{
    int                socket;
    struct sockaddr_in peer_addr;
};

struct L2SAP* l2sap_server_create( int port );

L2SAP* l2sap_create( const char* server_ip, int server_port );
void l2sap_destroy( L2SAP* client );
int  l2sap_sendto( L2SAP* client, const uint8_t* data, int len );
int  l2sap_recvfrom_timeout( L2SAP* client, uint8_t* data, int len, struct timeval* timeout );

#endif

