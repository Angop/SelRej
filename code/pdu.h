#include <netinet/in.h>

#define HEADER_LEN 7
int createPdu(uint8_t *pduBuf, uint32_t seqNum, uint8_t flag, uint8_t *payload, uint8_t payLen);
void outputPDU(uint8_t * aPDU, int pduLength);

typedef struct PduS * pdu;
struct PduS {
    // interpreted version of a PDU
    uint32_t seqNum;
    uint16_t checksum;
    uint8_t flag;
    uint32_t rr;
    uint32_t srej;
    uint8_t * payload;
    uint8_t payLen
}__attribute__((packed));

int fillPDU(pdu packet, uint8_t * apdu, int pduLength); // fills given pdu struct with info from given pdu
void printPDUS(pdu packet);
void freePDU(pdu packet); // frees pdu struct

