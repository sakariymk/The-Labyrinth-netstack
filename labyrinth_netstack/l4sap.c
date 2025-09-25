#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stddef.h>

#include "l4sap.h"
#include "l2sap.h"

#define L4_MAX_RETRIES 5
#define L4_RETRY_TIMEOUT_SEC 1
#define L4_RETRY_TIMEOUT_USEC 0

/* Create an L4 client.
 * It returns a dynamically allocated struct L4SAP that contains the
 * data of this L4 entity (including the pointer to the L2 entity
 * used).
 */
L4SAP* l4sap_create(const char* server_ip, int server_port) {
    L4SAP* l4 = (L4SAP*)malloc(sizeof(L4SAP)); // Allokerer minne for L4SAP
    if (!l4) { // Hvis allokeringen mislykkes
        perror("Failed to allocate memory for L4SAP");
        return NULL;
    }

    // Oppretter L2 SAP.
    l4->l2 = l2sap_create(server_ip, server_port);
    if (!l4->l2) { // Sjekker om peker ble laget
        fprintf(stderr, "L4SAP creation failed: Could not create L2SAP.\n");
        free(l4);
        return NULL;
    }

    // Initialiserer Stop-and-Wait
    l4->next_seqno_send = 0; // sekvensnummeret for neste pakke som skal sendes
    l4->expected_seqno_recv = 0; // forventede sekvensnummeret for neste mottatte pakke

    fprintf(stderr, "L4SAP created.\n");
    return l4;
}

/* The functions sends a packet to the network. The packet's payload
 * is copied from the buffer that it is passed as an argument from
 * the caller at L5.
 * If the length of that buffer, which is indicated by len, is larger
 * than L4Payloadsize, the function truncates the message to L4Payloadsize.
 *
 * The function does not return until the correct ACK from the peer entity
 * has been received.
 * When a suitable ACK arrives, the function returns the number of bytes
 * that were accepted for sending (the potentially truncated packet length).
 *
 * Waiting for a correct ACK may fail after a timeout of 1 second
 * (timeval.tv_sec = 1, timeval.tv_usec = 0). The function retransmits
 * the packet in that case.
 * The function attempts up to 4 retransmissions. If the last retransmission
 * fails with a timeout as well, the function returns L4_SEND_FAILED.
 *
 * The function may also return:
 * - L4_QUIT if the peer entity has sent an L4_RESET packet.
 * - another value < 0 if an error occurred.
 */
int l4sap_send(L4SAP* l4, const uint8_t* data, int len) {
    if (!l4 || !l4->l2 || !data) { // Sjekker om argumentene er gyldige.
        fprintf(stderr, "L4SAP send: Invalid arguments.\n");
        return -1;
    }

     if (len < 0) { // Sjekker om den oppgitte datalengden er negativ
         fprintf(stderr, "L4SAP send: Invalid data length %d.\n");
         return -1;
     }

    // Regner ut payload-lengde (truncater om noedvendig)
    int payload_len = (len > L4Payloadsize) ? L4Payloadsize : len; // Setter payload_len til 'len', men begrenser den til L4Payloadsize hvis 'len' er stoerre.
    if (len > L4Payloadsize) { // Sjekker om den opprinnelige lengden 'len' overstiger L4Payloadsize.
        fprintf(stderr, "L4SAP send: Warning: Data length %d exceeds L4Payloadsize %d, truncating to %d bytes.\n",
                 len, L4Payloadsize, payload_len);
    }

    // Klargjoer L4-pakkebuffer (header + payload).
    uint8_t packet_buffer[L4Framesize]; // Deklarerer en buffer packet_buffer paa stacken med stoerrelse L4Framesize
    memset(packet_buffer, 0, L4Framesize); // Nullstiller hele packet_buffer

    // Deklarerer L4Header
    L4Header data_header;
    data_header.type = L4_DATA;
    data_header.seqno = l4->next_seqno_send; // Setter sekvensnummeret (seqno) i headeren til det neste som skal sendes.
    data_header.ackno = l4->expected_seqno_recv; // Setter ackno til det sekvensnummeret vi forventer aa motta neste gang.
    data_header.mbz = 0;

    // Kopierer header og payload inn i bufferen.
    memcpy(packet_buffer, &data_header, L4Headersize); // Kopierer innholdet av data_header inn i starten av packet_buffer
    if (payload_len > 0) {
        memcpy(packet_buffer + L4Headersize, data, payload_len); // Kopierer payload_len bytes fra data inn i packet_buffer, rett etter headeren.
    }
    int packet_len = L4Headersize + payload_len; // Beregner den totale lengden


    int attempts = 0;
    while (attempts < L4_MAX_RETRIES) {
        attempts++;
        fprintf(stderr, "L4 Send: Attempt %d: Sending DATA (Seq=%u, Payload=%d bytes)\n",
                attempts, data_header.seqno, payload_len);


        int l2_sent = l2sap_sendto(l4->l2, packet_buffer, packet_len); // Sender pakken (packet_buffer med lengde packet_len) via L2-laget.
        if (l2_sent < 0) {
            fprintf(stderr, "L4 Send: Attempt %d: L2 send failed.\n", attempts);
        }
        else if (l2_sent != packet_len) {
              fprintf(stderr, "L4 Send: Attempt %d: L2 send returned unexpected length %d (expected %d).\n", attempts, l2_sent, packet_len);

         }

        // Venter paa ACK med timeout.
        struct timeval timeout = { .tv_sec = L4_RETRY_TIMEOUT_SEC, .tv_usec = L4_RETRY_TIMEOUT_USEC }; // Setter opp en struct timeval for timeout-verdien definert tidligere.
        uint8_t recv_buffer[L4Framesize]; // lager en buffer recv_buffer for aa motta payload
        int recv_len;

        // haandtere ikke-ACK-pakker mottatt mens vi venter.
        while (1) {
             recv_len = l2sap_recvfrom_timeout(l4->l2, recv_buffer, L4Framesize, &timeout); // Ventre paa en pakke fra L2 i et gitt tidsrom (timeout).

             if (recv_len == L2_TIMEOUT) {
                 fprintf(stderr, "L4 Send: Attempt %d: Timeout waiting for ACK (Seq=%u expected).\n",
                         attempts, (l4->next_seqno_send + 1) % 2);
                 break;
             } else if (recv_len < 0) {
                 fprintf(stderr, "L4 Send: Attempt %d: Error receiving from L2.\n", attempts);
                 break;
             } else if (recv_len < L4Headersize) {
                   fprintf(stderr, "L4 Send: Attempt %d: Received runt L4 packet (%d bytes), ignoring.\n", attempts, recv_len);
                  continue;
             }

             L4Header* recv_header = (L4Header*)recv_buffer; // Tolker starten av recv_buffer som en L4Header-peker.

             if (recv_header->type == L4_RESET) {
                  fprintf(stderr, "L4 Send: Received L4_RESET. Terminating.\n");
                  return L4_QUIT;
             } else if (recv_header->type == L4_ACK) {
                  uint8_t expected_ackno = (l4->next_seqno_send + 1) % 2; // Beregner det forventede ackno basert paa det sist sendte sekvensnummeret.
                  if (recv_header->ackno == expected_ackno) {
                      fprintf(stderr, "L4 Send: Correct ACK (AckNo=%u) received for DATA (Seq=%u).\n",
                              recv_header->ackno, l4->next_seqno_send);
                      l4->next_seqno_send = expected_ackno; // Oppdaterer neste sekvensnummer som skal sendes (snur biten 0/1).
                      return payload_len;
                  } else { // Hvis ackno ikke var forventet.
                      fprintf(stderr, "L4 Send: Attempt %d: Received incorrect ACK (AckNo=%u, expected %u), ignoring.\n",
                              attempts, recv_header->ackno, expected_ackno);
                      continue;
                  }
             } else if (recv_header->type == L4_DATA) {
                 fprintf(stderr, "L4 Send: Attempt %d: Received unexpected L4_DATA (Seq=%u), ignoring while waiting for ACK.\n",
                         attempts, recv_header->seqno);
                 continue;
             } else { // Hvis den mottatte pakketypen er ukjent.
                   fprintf(stderr, "L4 Send: Attempt %d: Received unknown L4 packet type (%u), ignoring.\n",
                           attempts, recv_header->type);
                   continue;
             }
        }
    }

    // Maks antall gjensendinger overskredet.
    fprintf(stderr, "L4 Send: Max retries (%d) exceeded for DATA (Seq=%u). Send failed.\n",
            L4_MAX_RETRIES, l4->next_seqno_send);
    return L4_SEND_FAILED;
}


/* The functions receives a packet from the network. The packet's
 * payload is copy into the buffer that it is passed as an argument
 * from the caller at L5.
 * The function blocks endlessly, meaning that experiencing a timeout
 * does not terminate this function.
 * The function returns the number of bytes copied into the buffer
 * (only the payload of the L4 packet).
 * The function may also return:
 * - L4_QUIT if the peer entity has sent an L4_RESET packet.
 * - another value < 0 if an error occurred.
 */
int l4sap_recv(L4SAP* l4, uint8_t* data, int len) {
    if (!l4 || !l4->l2 || !data || len < 0) { // Sjekker for ugyldige argumenter
         fprintf(stderr, "L4SAP recv: Invalid arguments.\n");
         return -1;
     }

    uint8_t recv_buffer[L4Framesize]; // en mottaksbuffer recv_buffer for payload
    int recv_len;

    fprintf(stderr, "L4 Recv: Waiting for DATA (Expected Seq=%u)\n", l4->expected_seqno_recv);

    while (1) { // Starter en uendelig loop for aa vente paa pakker.
        recv_len = l2sap_recvfrom(l4->l2, recv_buffer, L4Framesize); // vente paa en pakke (blokkerende kall, NULL timeout).

        if (recv_len < 0) {
            fprintf(stderr, "L4 Recv: Error receiving from L2.\n");
            return -1;
        } else if (recv_len == L2_TIMEOUT) { // Sjekker om L2_TIMEOUT ble returnert (boer ikke skje)
             fprintf(stderr, "L4 Recv: Unexpected L2_TIMEOUT from l2sap_recvfrom.\n");
             continue;
        } else if (recv_len < L4Headersize) {
             fprintf(stderr, "L4 Recv: Received runt L4 packet (%d bytes), ignoring.\n", recv_len);
             continue;
        }

        L4Header* recv_header = (L4Header*)recv_buffer; // Tolker starten av recv_buffer som en L4Header-peker.

        if (recv_header->type == L4_RESET) {
            fprintf(stderr, "L4 Recv: Received L4_RESET. Terminating.\n");
            return L4_QUIT;
        } else if (recv_header->type == L4_ACK) { //Mottok ACK mens vi ventet paa DATA, ignorer den
             fprintf(stderr, "L4 Recv: Received unexpected L4_ACK (AckNo=%u), ignoring.\n", recv_header->ackno);
             continue;
        } else if (recv_header->type == L4_DATA) {
            fprintf(stderr, "L4 Recv: Received L4_DATA (Seq=%u, Expected Seq=%u)\n",
                   recv_header->seqno, l4->expected_seqno_recv);

            if (recv_header->seqno == l4->expected_seqno_recv) { // sjekker om det mottatte sekvensnummeret er det vi forventet.
                int payload_len = recv_len - L4Headersize; // regner lengden paa payload
                int copy_len = (payload_len < len) ? payload_len : len; // Bestemmer hvor mange bytes som skal kopieres avhengig av hva som er minst.

                if (copy_len > 0) {
                    memcpy(data, recv_buffer + L4Headersize, copy_len); // Kopierer antall bytes over til data
                }
                 if (payload_len > len) {
                      fprintf(stderr, "L4 Recv: Warning: Received L4 payload (%d bytes) larger than buffer (%d bytes), truncated.\n",
                              payload_len, len);
                 }

                // Oppdaterer forventet sekvensnummer for neste mottak.
                l4->expected_seqno_recv = (l4->expected_seqno_recv + 1) % 2; // Snur det forventede sekvensnummeret (0/1).

                // Sender ACK for den mottatte pakken.
                L4Header ack_header;
                ack_header.type = L4_ACK;
                ack_header.seqno = 0;
                ack_header.ackno = l4->expected_seqno_recv; // Setter ackno til det neste sekvensnummeret vi forventer
                ack_header.mbz = 0;

                fprintf(stderr, "L4 Recv: Sending ACK (AckNo=%u) for received DATA (Seq=%u)\n",
                       ack_header.ackno, recv_header->seqno);

                int ack_sent = l2sap_sendto(l4->l2, (uint8_t*)&ack_header, L4Headersize); // Sender ACK-headeren via L2-laget.
                if (ack_sent < 0) {
                    fprintf(stderr, "L4 Recv: Failed to send ACK.\n");
                } else if (ack_sent != L4Headersize) {
                     fprintf(stderr, "L4 Recv: Warning: Sent ACK length %d, expected %d.\n", ack_sent, L4Headersize);
                }

                return copy_len; // Returnerer antall mottatte og kopierte payload-bytes

            } else { // Hvis det mottatte sekvensnummeret ikke var forventet
                fprintf(stderr, "L4 Recv: Received duplicate/old DATA (Seq=%u, Expected=%u), discarding payload.\n",
                        recv_header->seqno, l4->expected_seqno_recv);

                // Sender ACK paa nytt for den sist korrekt mottatte pakken.
                L4Header ack_header;
                ack_header.type = L4_ACK;
                ack_header.seqno = 0;
                ack_header.ackno = l4->expected_seqno_recv; // Setter ackno til det neste sekvensnummeret vi forventer
                ack_header.mbz = 0;

                 fprintf(stderr, "L4 Recv: Re-sending ACK (AckNo=%u) for duplicate DATA (Seq=%u)\n",
                         ack_header.ackno, recv_header->seqno);

                int ack_sent = l2sap_sendto(l4->l2, (uint8_t*)&ack_header, L4Headersize); // Sender den nye ACK-headeren via L2.
                 if (ack_sent < 0) {
                      fprintf(stderr, "L4 Recv: Failed to re-send ACK for duplicate.\n");
                 }
                 continue;
            }
        } else { // Hvis den mottatte pakketypen var ukjent.
             fprintf(stderr, "L4 Recv: Received unknown L4 packet type (%u), ignoring.\n", recv_header->type);
             continue;
        }
    }
}

/** This function is called to terminate the L4 entity and
 *  free all of its resources.
 *  We recommend that you send several L4_RESET packets from
 *  this function to ensure that the peer entity is also
 *  terminating correctly.
 */
void l4sap_destroy(L4SAP* l4) {
    if (!l4) { // Sjekker om  null
        return;
    }

    if (l4->l2) { // sjekker om  l4->l2 ikke er null
        fprintf(stderr, "L4 Destroy: Sending L4_RESET packets.\n");

        L4Header reset_header;
        reset_header.type = L4_RESET;
        reset_header.seqno = 0;
        reset_header.ackno = 0;
        reset_header.mbz = 0;
        for (int i = 0; i < 3; ++i) { // starter en loop for aa sende reset pakken 3 ganger.
            l2sap_sendto(l4->l2, (uint8_t*)&reset_header, L4Headersize); // Sender RESET-headeren via L2.
        }

        l2sap_destroy(l4->l2);
        l4->l2 = NULL; // Setter L2-pekeren til null for aa unngaa double free hvis l4sap_destroy kalles igjen.
    }

    free(l4);
    fprintf(stderr, "L4SAP destroyed.\n");
}
