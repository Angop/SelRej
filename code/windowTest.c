/* Angela Kerlin
 * Test cases for sliding window functionality
 */

#include "window.h"

void simpleTest();
void harderTest();

int main() {
    harderTest();
}

void simpleTest() {
    // simple test
    initWindow(4);
    uint8_t pduBuf[1407];
    int i, pduLen = 0;
    pdu packet;
    for (i=0; i < 4; i++) {
        // create the PDU struct
        pduLen = createPdu(pduBuf, i, 0, (uint8_t*)"test", 4);
        packet = (pdu)malloc(sizeof(struct PduS));
        fillPDU(packet, pduBuf, pduLen);

        // add it to sliding window
        sent(packet);
    }

    printWindow();
    printWindowMeta();

    printf("Pdu from window: ");
    printPDUS(srej(2));
}

void harderTest() {
    initWindow(4);
    int i = 0;
    uint8_t pduBuf[1407];
    int pduLen, userRR = 0;
    pdu packet;
    while (1) {
        if (!isFull()) {
            // create the PDU struct
            pduLen = createPdu(pduBuf, i, 0, (uint8_t*)"test", 4);
            packet = (pdu)malloc(sizeof(struct PduS));
            fillPDU(packet, pduBuf, pduLen);

            // add it to sliding window
            if (!sent(packet)) {
                printf("invalid send\n");
                return;
            }
            i++;
            printWindowMeta();
        }
        else {
            printWindow();
            printWindowMeta();

            printf("\nEnter number to RR: ");
            scanf("%d", &userRR);

            if (!rr(userRR)) {
                printf("invalid rr\n");
            }
            printWindow();
            printWindowMeta();
        }
    }
}