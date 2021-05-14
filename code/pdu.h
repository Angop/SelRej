#include <netinet/in.h>

#define HEADER_LEN 7
int createPdu(uint8_t *pduBuf, uint32_t seqNum, uint8_t flag, uint8_t *payload, uint8_t payLen);
void outputPDU(uint8_t * aPDU, int pduLength);