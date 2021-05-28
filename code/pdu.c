/* functions related to PDU manipulation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "checksum.h"
#ifndef PDUS
	#define PDUS
    #include "pdu.h"
#endif

/* Populates given buffer with a formatted PDU
 * seqNumber = 4 byte sequence number in network order
 * checksum = 2 byte checksum
 * flag = 1 byte flag which indicates the type of the PDU
 * payload = data the PDU carries (not null terminated)
 * payLen = length of payload in bytes
 * returns lengeth of created PDU */

int createPdu(uint8_t *pduBuf, uint32_t seqNum, uint8_t flag, uint8_t *payload, uint16_t payLen) {
    uint16_t checksum = 0;
    seqNum = htonl(seqNum);

    memcpy(pduBuf, &seqNum, sizeof(uint32_t));
    memcpy(pduBuf + 4, &checksum, sizeof(uint16_t));
    memcpy(pduBuf + 6, &flag, sizeof(uint8_t));
    memcpy(pduBuf + 7, payload, payLen);

    // get the right checksum and put it in pdu
    checksum = in_cksum((unsigned short *)pduBuf, HEADER_LEN + payLen);
    memcpy(pduBuf + 4, &checksum, sizeof(uint16_t));

    return HEADER_LEN + payLen;
}

void outputPDU(uint8_t * aPDU, int pduLength) {
    uint32_t seqNum = 0;
    uint16_t checksum = 0;
    uint8_t flag = 0;
    memcpy(&seqNum, aPDU, sizeof(uint32_t));
    memcpy(&checksum, aPDU + 4, sizeof(uint16_t));
    memcpy(&flag, aPDU + 6, sizeof(uint8_t));

    if (in_cksum((unsigned short *)aPDU, pduLength) != 0) {
        // packet is corrupted
        printf("\tChecksum: Invalid\n");
        return;
    }

    printf("\tChecksum: Valid\n");
    printf("\tSequenceNum: %u\n", ntohl(seqNum));
    printf("\tFlag: %u\n", flag);
    printf("\tPayload: %.*s\n", pduLength - HEADER_LEN, aPDU + HEADER_LEN);
    printf("\tPayloadLen: %u\n", pduLength);
}

// below is for pdu struct

int interpPDU(pdu packet, uint8_t * apdu, uint16_t pduLength) {
    // fills given pdu struct with info from given pdu,
    // returns 1 on success 0 for invalid pdu
    if (in_cksum((unsigned short *)apdu, pduLength) != 0) {
        // packet is corrupted
        return 0;
    }
    memcpy(&packet->seqNum, apdu, sizeof(uint32_t));
    packet->seqNum = ntohl(packet->seqNum);
    memcpy(&packet->checksum, apdu + 4, sizeof(uint16_t));
    memcpy(&packet->flag, apdu + 6, sizeof(uint8_t));

    // TODO: handle RR and SREJ flags, or even remove them
    packet->rr = 0;
    packet->srej = 0;

    if ((packet->payload = (uint8_t *)malloc(pduLength - HEADER_LEN)) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memcpy(packet->payload, apdu + HEADER_LEN, pduLength - HEADER_LEN);
    packet->payLen = pduLength - HEADER_LEN;

    return 1;
}

void printPDUS(pdu packet) {
    printf("Seq Num: %u pduSize: %d\n", packet->seqNum, packet->payLen + HEADER_LEN);
}

int recreatePDUS(pdu packet, uint8_t *pduBuf) {
    // reconstructs a pdu struct into a literal buffer and sends it to specified destinatino
    return createPdu(pduBuf, packet->seqNum, packet->flag, packet->payload, packet->payLen);
}

void freePDU(pdu packet) {
    // frees pdu struct
    free(packet->payload);
    free(packet);
}