#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h> // For errno
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>   // For struct timeval
#include <inttypes.h>   // For uintX_t types
#include <stddef.h>

#include "l2sap.h"

static uint8_t compute_checksum(const uint8_t* frame, int len);

/**
 * @brief Creates an L2SAP entity (client-side).
 *
 * Initializes a UDP socket and prepares the peer address structure.
 *
 * @param server_ip The IP address of the peer L2 entity (server).
 * @param server_port The port number of the peer L2 entity.
 * @return L2SAP* Pointer to the created L2SAP structure, or NULL on error.
 */
 L2SAP* l2sap_create(const char* server_ip, int server_port) { //Funksjon for aa lage Lag2 server access point
     L2SAP* client = (L2SAP*)malloc(sizeof(L2SAP)); //minne allokerer stoerrelsen av L2SAP peker
     if (!client) { //Hvis client ikke har noen verdi altsaa NULL
         perror("Failed to allocate memory for L2SAP"); //printer error
         return NULL; //Returnerer null
     }

     // Lager UDP socket
     client->socket = socket(AF_INET, SOCK_DGRAM, 0); //for pekeren til socket av client, lager socket med funksjonen
     if (client->socket < 0) { //hvis socket verdi er mindre enn 0 altsaa negativ
         perror("L2SAP socket creation failed"); //da er ikke socket aapen og printer error
         free(client); //frigjoer client
         return NULL; //returnerer null
     }

     // Klargjoer peer addresse struktur
     memset(&client->peer_addr, 0, sizeof(client->peer_addr)); //setter peer_addr strukturen til 0 overskriver eksisterende verdier med funksjonen memset
     client->peer_addr.sin_family = AF_INET; // setter peer_addr familien til af_inet som definerer at vi bruker IPv4
     client->peer_addr.sin_port = htons(server_port); //setter server port nummer. htons() konverterer port nummeret fra host's byte rekkefoelge
     if (inet_pton(AF_INET, server_ip, &client->peer_addr.sin_addr) <= 0) {  //konverterer ip adresse fra tekst strengen til den binaere nettverksformatet som sockaddr_in strukturen trenger, resultatet blir lagret i peer_addr.sin.addr
         fprintf(stderr, "L2SAP invalid server IP address: %s\n", server_ip); //printer feilmelding
         close(client->socket); //lukker socket til klienten
         free(client); //frigjoer client
         return NULL; //returnerer null
     }


     fprintf(stderr, "L2SAP created for server %s:%d\n", server_ip, server_port); //hvis alt passerer over så får vi ut en melding som bekrefter at L2SAP er lagd for gitt server ip og port
     return client; //returnerer client
}

/**
 * @brief Destroys an L2SAP entity.
 *
 * Closes the socket and frees the allocated memory.
 *
 * @param client Pointer to the L2SAP structure to destroy.
 */
void l2sap_destroy(L2SAP* client) {
    if (!client) { // Om client er null, returner
        return;
    }
    if (client->socket >= 0) { // Hvis socket har en gyldig verdi
        close(client->socket); // Lukk socketen
        fprintf(stderr, "L2SAP socket closed.\n");
    }
    free(client); // Fjern client fra minne
    fprintf(stderr, "L2SAP destroyed.\n");
}

/**
 * @brief Sends data as an L2 frame to the configured peer.
 *
 * Constructs an L2 frame with header and checksum, then sends it via UDP.
 * Discards the data if the resulting frame exceeds L2Framesize.
 *
 * @param client Pointer to the L2SAP structure.
 * @param data Pointer to the payload data (L2 SDU) to send.
 * @param len Length of the payload data in bytes.
 * @return int The number of payload bytes accepted for sending (len),
 * or -1 if the frame would be too large or a send error occurs.
 */
int l2sap_sendto(L2SAP* client, const uint8_t* data, int len) {
    if (!client || client->socket < 0) { // Sjekk om client er null eller socket har ugyldig verdi
        fprintf(stderr, "L2SAP sendto: Invalid client or socket.\n");
        return -1;
    }
    if (len < 0) { // Sjekk om lengden er ugyldig
         fprintf(stderr, "L2SAP sendto: Invalid data length %d.\n", len);
         return -1;
    }

    int total_len = L2Headersize + len; // Beregn total lengde av frame

    // Sjekk om frame stoerrelsen er for stor
    if (total_len > L2Framesize) {
        fprintf(stderr, "L2SAP sendto: Data too large (%d bytes payload), exceeds L2Framesize (%d bytes total).\n", len, L2Framesize);
        return -1;
    }

    // Alloker buffer for hele framen
    uint8_t frame_buffer[L2Framesize]; // Bruk stack allocation
    memset(frame_buffer, 0, L2Framesize);

    // Konstruer headeren
    L2Header header;
    header.dst_addr = client->peer_addr.sin_addr.s_addr; // Allerede i netverk byte order
    header.len = htons((uint16_t)total_len); // setter lengden
    header.mbz = 0; // setter mbz (skal alltid vaere 0)
    header.checksum = 0; // Sett til 0 for checksum kalku

    // Kopier header til buffer (handle byte order)
    // dst_addr er allrede i nettverk
    memcpy(frame_buffer, &header.dst_addr, sizeof(header.dst_addr));
    // len maa konverteres
    uint16_t len_n = htons((uint16_t)total_len);
    memcpy(frame_buffer + offsetof(L2Header, len), &len_n, sizeof(header.len));
    // checksum og mbz er enkelt bytes, ingen konvertering trengs
    frame_buffer[offsetof(L2Header, checksum)] = 0; // dobbeltsjekker at det er 0
    frame_buffer[offsetof(L2Header, mbz)] = 0;

    // Kopier payload til buffer
    if (len > 0) {
        memcpy(frame_buffer + L2Headersize, data, len);
    }

    // Kalkuler og plasser checksum
    uint8_t checksum = compute_checksum(frame_buffer, total_len);
    frame_buffer[offsetof(L2Header, checksum)] = checksum;

    // Send framen
    ssize_t bytes_sent = sendto(client->socket, frame_buffer, total_len, 0,
                              (struct sockaddr*)&client->peer_addr, sizeof(client->peer_addr));

    if (bytes_sent < 0) {
        perror("L2SAP sendto failed");
        return -1;
    }

    if (bytes_sent != total_len) {
        fprintf(stderr, "L2SAP sendto: Warning: Sent %zd bytes, expected %d bytes.\n", bytes_sent, total_len);
    }

    // Returner lengden til payloaden som var akseptert
    return len;
}


/**
 * @brief Receives an L2 frame from the peer, with an optional timeout.
 *
 * Waits for a UDP packet, validates the L2 header and checksum.
 * If valid, copies the payload (L2 SDU) into the provided buffer.
 *
 * @param client Pointer to the L2SAP structure.
 * @param data Buffer to store the received payload data.
 * @param len Maximum number of bytes to store in the data buffer.
 * @param timeout Optional timeout value. If NULL, waits indefinitely.
 * @return int Number of payload bytes received and copied to data,
 * L2_TIMEOUT (0) if timeout occurred,
 * -1 on error or if an invalid/corrupted frame is received.
 */
int l2sap_recvfrom_timeout(L2SAP* client, uint8_t* data, int len, struct timeval* timeout) {
    if (!client || client->socket < 0 || !data || len < 0) { // Sjekk om argumentene er gyldige
        fprintf(stderr, "L2SAP recvfrom: Invalid arguments.\n");
        return -1;
    }

    fd_set readfds;
    int activity;
    uint8_t recv_buffer[L2Framesize]; // Buffer for frame data
    struct sockaddr_in sender_addr; // Lagre senderens adresse
    socklen_t sender_addr_len = sizeof(sender_addr); // Regn ut ut stoerelsen av sender_addr

    while (1) {
        FD_ZERO(&readfds); // Clear setet
        FD_SET(client->socket, &readfds); // Legg til client socket

        // Lag lokal kopi av timeout verdier
        struct timeval tv;
        struct timeval* p_tv = NULL;
        if (timeout) { // Sett timeout verdier
            tv = *timeout;
            p_tv = &tv;
        }

        activity = select(client->socket + 1, &readfds, NULL, NULL, p_tv);

        if (activity < 0) {
            // Ignorer EINTR error,  proev select paa nytt
            if (errno == EINTR) {
                continue;
            }
            perror("L2SAP select failed"); // Annet error
            return -1;
        }

        if (activity == 0) {
            // Timeout skjedde
            return L2_TIMEOUT;
        }

        // Data er tilgjengelig, motta det
        ssize_t bytes_received = recvfrom(client->socket, recv_buffer, L2Framesize, 0,
                                        (struct sockaddr*)&sender_addr, &sender_addr_len);

        if (bytes_received < 0) {
            // Ignorer EINTR error,  proev paa nytt
             if (errno == EINTR) {
                continue;
            }
            perror("L2SAP recvfrom failed"); // Annen error
            return -1;
        }

        // Hvis vi har motatt mindre bytes enn header-stoerrelsen saa maa vi avvise
        if (bytes_received < L2Headersize) {
            fprintf(stderr, "L2SAP recv: Received runt frame (%zd bytes), discarding.\n", bytes_received);
            continue; // Vent for neste frame
        }

        L2Header received_header; // Hold header felt
        uint16_t len_n; // Temp variabel for lengde i byte order

        // Kopier dataen
        memcpy(&received_header.dst_addr, recv_buffer + offsetof(L2Header, dst_addr), sizeof(received_header.dst_addr));
        memcpy(&len_n,                 recv_buffer + offsetof(L2Header, len), sizeof(received_header.len));
        received_header.checksum = recv_buffer[offsetof(L2Header, checksum)];

        received_header.len = ntohs(len_n); // Konverter byte order til host byte order

        // Valider frame header lengden
        if (received_header.len < L2Headersize || received_header.len > bytes_received) {
             fprintf(stderr, "L2SAP recv: Invalid header length (%u bytes) for received size (%zd bytes), discarding.\n",
                    received_header.len, bytes_received);
             continue;
        }

        // Checksum validering
        uint8_t received_checksum = received_header.checksum;

        // Sett checksum felt til 0 midlertidig for begregning (checksum er 0 i opprinnelig beregning)
        recv_buffer[offsetof(L2Header, checksum)] = 0;
        uint8_t calculated_checksum = compute_checksum(recv_buffer, received_header.len); // Beregn checksum

        // Restorer checksum i buffer
        recv_buffer[offsetof(L2Header, checksum)] = received_checksum;

        if (calculated_checksum != received_checksum) {
            fprintf(stderr, "L2SAP recv: Checksum mismatch (received 0x%02x, calculated 0x%02x), discarding frame.\n",
                   received_checksum, calculated_checksum);
            continue;
        }

        int payload_len = received_header.len - L2Headersize; // Regn ut lengde paa payload

        int copy_len = (payload_len < len) ? payload_len : len; // Min(payload_len, user_buffer_len)


        if (copy_len > 0) {
             memcpy(data, recv_buffer + L2Headersize, copy_len); // Kopier payload
        }
        if (payload_len > len) {
             fprintf(stderr, "L2SAP recv: Warning: Received payload (%d bytes) larger than provided buffer (%d bytes), truncated.\n",
                    payload_len, len);
        }

        // Faatt og validert et frame
        return copy_len;

    }
}

// Vi gjoer en for-lokke som sjekker saa lenge i er mindre enn lengden av framen
// Deretter gjoer vi en XOR operasjon mellom checksummen og i-te index til framen
// Tilslutt returnerer vi checksummen
static uint8_t compute_checksum(const uint8_t* frame, int len) {
    uint8_t checksum = 0;
    for (int i = 0; i < len; ++i) {
        checksum ^= frame[i];
    }
    return checksum;
}

int l2sap_recvfrom( L2SAP* client, uint8_t* data, int len )
{
    return l2sap_recvfrom_timeout( client, data, len, NULL );
}
