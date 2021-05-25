/* Angela Kerlin
 * Test cases for sliding window functionality
 */

#include "window.h"

// test server
void simpleTest();
void harderTest();

// test rcopy
void lostTest(int num);
void unbufferTest(int num);

int main() {
    unbufferTest(0);
    return 1;
}

void simpleTest() {
    // simple test
    initWindow(4);
    uint8_t pduBuf[1407];
    int i, pduLen = 0;
    pdu packet;
    printf("VerifyEmpty: %s\n", (isWindowEmpty() ? "good" : "bad"));
    printf("VerifyFull: %s\n", (isWindowFull() ? "bad" : "good"));
    for (i=0; i < 4; i++) {
        // create the PDU struct
        pduLen = createPdu(pduBuf, i, 0, (uint8_t*)"test", 4);
        packet = (pdu)malloc(sizeof(struct PduS));
        interpPDU(packet, pduBuf, pduLen);

        // add it to sliding window
        sent(packet);
    }
    printf("VerifyEmpty: %s\n", (isWindowEmpty() ? "bad" : "good"));
    printf("VerifyFull: %s\n", (isWindowFull() ? "good" : "bad"));

    printWindow();
    printWindowMeta();

    printf("Pdu from window: ");
    printPDUS(srej(2));

    rr(4);
    printf("VerifyEmpty: %s\n", (isWindowEmpty() ? "good" : "bad"));
    printf("VerifyFull: %s\n", (isWindowFull() ? "bad" : "good"));
}

void harderTest() {
    initWindow(4);
    int i = 0;
    uint8_t pduBuf[1407];
    int pduLen, userRR = 0;
    pdu packet;
    while (1) {
        if (!isWindowFull()) {
            // create the PDU struct
            pduLen = createPdu(pduBuf, i, 0, (uint8_t*)"test", 4);
            packet = (pdu)malloc(sizeof(struct PduS));
            interpPDU(packet, pduBuf, pduLen);

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


void lostTest(int num) {
    uint8_t pduBuf[1407];
    int pduLen, ret, i;
    pdu packet;
    initWindow(4);
    printf("Check empty: %s\n", (isBufEmpty() ? "pass" : "fail"));
    printf("Check full: %s\n", (!isBufFull() ? "pass" : "fail"));

    // skip one packet
    skip(num); // lost first packet
    printf("Check empty: %s\n", (!isBufEmpty() ? "pass" : "fail"));
    printf("Check full: %s\n", (!isBufFull() ? "pass" : "fail"));
    printWindowMeta(); // expected: c = 0; l = 0; u = 3

    // create the PDU struct and buffer missing
    pduLen = createPdu(pduBuf, num, 0, (uint8_t*)"test", 4);
    packet = (pdu)malloc(sizeof(struct PduS));
    interpPDU(packet, pduBuf, pduLen);
    printf("Check buffer return: %s\n", (buffer(packet) == 2 ? "pass" : "fail"));

    // unbuffer missing
    printPDUS(unbuffer());

    printf("Check empty: %s\n", (isBufEmpty() ? "pass" : "fail"));
    printf("Check full: %s\n", (!isBufFull() ? "pass" : "fail"));
    printWindowMeta(); // expected: c = -1; l = 1; u = 0

    // fill window
    skip(num); // lost first packet
    for (i=num + 1; i < num + 4; i++) {
        // create the PDU struct
        pduLen = createPdu(pduBuf, i, 0, (uint8_t*)"test", 4);
        packet = (pdu)malloc(sizeof(struct PduS));
        interpPDU(packet, pduBuf, pduLen);

        // add it to sliding window
        if ((ret=buffer(packet)) != 1) {
            printf("BUFFER FAIL returned: %d seqNum: %d\n", ret, i);
            printWindowMeta();
        }
    }
    printf("Check empty: %s\n", (!isBufEmpty() ? "pass" : "fail"));
    printf("Check full: %s\n", (isBufFull() ? "pass" : "fail"));
    printWindow();
    printWindowMeta(); // expected: l = 0; u = 3; c = 3
}

void unbufferTest(int num) {
    uint8_t pduBuf[1407];
    int pduLen, ret, i;
    pdu packet;
    initWindow(4);

    // fill window
    skip(num); // lost first packet
    for (i=num + 1; i < num + 4; i++) {
        // create the PDU struct
        pduLen = createPdu(pduBuf, i, 0, (uint8_t*)"test", 4);
        packet = (pdu)malloc(sizeof(struct PduS));
        interpPDU(packet, pduBuf, pduLen);

        // add it to sliding window
        ret = buffer(packet);
        printf("Buffer returned: %d seqNum: %d\n", ret, i);
        printWindowMeta();
    }
    printf("Check empty: %s\n", (!isBufEmpty() ? "pass" : "fail"));
    printf("Check full: %s\n", (isBufFull() ? "pass" : "fail"));

    // empty window
    pduLen = createPdu(pduBuf, num, 0, (uint8_t*)"test", 4);
    packet = (pdu)malloc(sizeof(struct PduS));
    interpPDU(packet, pduBuf, pduLen);
    printf("Check buffer: %s\n", (buffer(packet) == 2 ? "pass" : "fail"));

    while ((packet=unbuffer()) != NULL) {
        printPDUS(packet);
    }
    printf("Check empty: %s\n", (isBufEmpty() ? "pass" : "fail"));
    printf("Check full: %s\n", (!isBufFull() ? "pass" : "fail"));
}