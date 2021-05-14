/* functions related to PDU manipulation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "checksum.h"
#include "pdu.h"

/* Populates given buffer with a formatted PDU
 * seqNumber = 4 byte sequence number in network order
 * checksum = 2 byte checksum
 * flag = 1 byte flag which indicates the type of the PDU
 * payload = data the PDU carries (not null terminated)
 * payLen = length of payload in bytes
 * returns lengeth of created PDU */

int createPdu(uint8_t *pduBuf, uint32_t seqNum, uint8_t flag, uint8_t *payload, uint8_t payLen) {
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