#ifndef L4SAP_H
#define L4SAP_H

#include <sys/socket.h>
#include <netinet/in.h>

#include "l2sap.h"

#define L4Framesize   (int)L2Payloadsize
#define L4Headersize  (int)(sizeof(L4Header))
#define L4Payloadsize (int)(L4Framesize-L4Headersize)

/* The 3 types of packet that exist in this L4 layer. */
#define L4_RESET    0x1 << 0
#define L4_DATA     0x1 << 1
#define L4_ACK      0x1 << 2

/* Special error codes that L5 expects with exactly these
 * values.
 */
#define L4_TIMEOUT          0
#define L4_QUIT             -100

#define L4_SEND_FAILED      -101
#define L4_ACK_RECEIVED     -102

#define L4_DATA_RECEIVED    -103
#define L4_NODATA_RECEIVED  -104


/* The design of the L4 layer is the following:
 *
 * The L4 layer provides a reliable datagram service using
 * the very simple stop-and-wait protocol. To be clear: this
 * means that only the sequence numbers 0 and 1 can be used
 * (even though the protocol header is a byte).
 *
 * The reliable datagram service should be full-duplex,
 * meaning that both the server and client can potentially
 * send packets at the same time. Due to the nature of the
 * stop-and-wait protocol, arriving packets may be discarded
 * if the receiving side is not ready.
 *
 * A client and a server do not negotiate a connection like
 * TCP, but unlike a UDP receiver, which can receive packets
 * from several UDP senders on the same IP address and UDP port,
 * this L4 server can only handle data from a single client,
 * and vice versa.
 *
 * This very simple L4 layer is meant to support exactly one
 * pair of client and server. Whem data arrives from an
 * unexpected source, it is allowed to misbehave in any way.
 *
 * If either client or server are terminated, the other one
 * is in an undefined state and should be restarted.
 *
 * When client or server received an L4_RESET message, they
 * quit.
 *
 * The specific design is yours.
 */

/*
 * The header for all packets must look like this.
 * You cannot change this.
 */
typedef struct L4Header L4Header;
struct L4Header
{
    uint8_t type;
    uint8_t seqno;
    uint8_t ackno;
    uint8_t mbz;
};

/*
 * You can add any number of data structures that are convenient for you.
 */

/* The data structure for maintaining the L4 entity should
 * be called L4SAP.
 */
typedef struct L4SAP L4SAP;

struct L4SAP
{
    L2SAP* l2;

    uint8_t next_seqno_send;
    uint8_t expected_seqno_recv;
};


/* Create an L4 client.
 */
L4SAP* l4sap_create( const char* server_ip, int server_port );

/* l4sap_send is a blocking function that sends data to
 *l4sap_create its peer entity.
 *
 * Blocking means that this function will not return until the
 * data has been delivered successfully and a suitable ACK has
 * been received.
 *
 * Send an L4_DATA packet with the given data of length len as
 * payload. If len exceed L4Payloadsize, the send is truncated
 * to L4Payloadsize. The rest is ignored.
 *
 * l4sap_send resends up to 5 times after a timeout of 1
 * second if it does not receive a correct ACK. After that, it
 * gives up and returns L4_TIMEOUT as an error code.
 *
 * While l4sap_send waits for a suitable ACK, it can also
 * receive DATA and RESET packets.
 *
 * RESET packets must be processed by deleting the L2 and L4
 * entities and all memory associated with them, and returning
 * the error code L4_QUIT.
 *
 * DATA packets must be handled to achieve a full duplex operation.
 */
int l4sap_send( L4SAP* l4, const uint8_t* data, int len );

/* l4sap_recv is a blocking function that receives data from
 * its peer entity.
 *
 * If a received data frame is larger than the buffer given
 * to this function as data, according to the value len, it
 * it truncated to len. The function returns the size that
 * was actually copied.
 *
 * When a DATA packet is received, l4sap_send sends the
 * appropriate ACK. When the received DATA packet is a
 * retransmission, l4sap_recv does not return to the caller
 * but continues to wait for a new DATA packet.
 *
 * While l4sap_recv waits for DATA, it can also receive ACK
 * and RESET packets.
 *
 * RESET packets must be processed by deleting the L2 and L4
 * entities and all memory associated with them, and returning
 * the error code L4_QUIT.
 *
 * ACKs must be dealt with according to your implementation.
 */
int l4sap_recv( L4SAP* l4, uint8_t* data, int len );

/* Send the L4_RESET message to the peer (OK to send it several
 * times, then delete the L2 and L4 entities and all memory
 * associated with them.
 */
void l4sap_destroy( L4SAP* l4 );

int l2sap_recvfrom(L2SAP* client, uint8_t* data, int len);


#endif
