/* Angela Kerlin
 * Sliding window functionality for both server and rcopy
 */

#include "window.h"

typedef struct Window * window;
struct Window {
    pdu * queue; // points to the 0th index in an array of Pdu's
    int winSize; // size of the window
    int lower; // the lowest unacked pdu's index
    int upper; // upper bound of window
    int current; // index of the last sent packet
}__attribute__((packed));

// GLOBAL VARIABLE
struct Window win;


int initWindow(); // initializes window for server, returns 0 for failure, 1 for success
void freeWindow(); // frees the window and everything malloced inside
int sent(pdu packet); // server sent this packet, return 1 on sucess, 0 on invalid send
int rr(uint32_t seqNum); // rcopy is ready for this seqNum, return 1 on sucess, 0 on invalid seqNum
pdu srej(uint32_t seqNum); // rcopy rejected this seqNum, return the pdu to resend or null if its out of the window
pdu getLow(); // server needs the lowest unacked packet, return pdu or null if none are unacked
int isFull(); // returns true on full window, else 0
int isEmpty(); // returns true on empty window, else 0

void printWindowMeta();
void printWindow();

int initWindow(int windowSize) {
    // initializes window returns 0 for failure, 1 for success
    int i;
    if (windowSize < 1) {
        // invalid window size
        return 0;
    }
    win.winSize = windowSize;
    win.lower = 0;
    win.upper = windowSize - 1;
    win.current = -1;
    if ((win.queue = malloc(sizeof(struct PduS) * windowSize)) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    // initialize queue to null pointers
    for (i=0; i < windowSize; i++) {
        win.queue[i] = NULL;
    }
    return 1;
}

void freeWindow() {
    // frees the window and everything malloced inside
    // TODO
}

int sent(pdu packet) {
    // server sent this packet, return 1 on sucess, 0 on invalid send
    if (isFull()) {
        // window was full, packet should not have been sent
        return 0;
    }
    if (packet->seqNum % win.winSize != (win.current + 1) % win.winSize) {
        // a sent packet must have been skipped
        return 0;
    }
    win.current = (win.current + 1) % win.winSize;
    win.queue[win.current] = packet;

    return 1;
}

int rr(uint32_t seqNum) {
    // rcopy is ready for this seqNum, return 1 on sucess, 0 on invalid seqNum
    int newLower, curNum, lowNum, oldLowSeq, i = 0;
    if (win.current == -1 || seqNum  > (curNum=win.queue[win.current]->seqNum) + 1) {
        // rr higher than greatest seq number in window + 1
        return 0;
    }
    if (seqNum  < (lowNum=win.queue[win.lower]->seqNum)) {
        // rr less than lower bound's seqNum, no action needed
        return 1;
    }

    newLower = seqNum % win.winSize;
    oldLowSeq = win.queue[win.lower]->seqNum;
    for (i=oldLowSeq; i < seqNum; i++) {
        freePDU(win.queue[i % win.winSize]);
        free(win.queue[i % win.winSize]);
        win.queue[i % win.winSize] = NULL;
    }

    win.lower = newLower;
    win.upper = (newLower + win.winSize - 1) % win.winSize;
    return 1;
}

pdu srej(uint32_t seqNum) {
    // rcopy rejected this seqNum, return the pdu to resend or null if its out of the window
    if (win.current == -1 || seqNum > win.queue[win.current]->seqNum + 1 || seqNum  < win.queue[win.lower]->seqNum) {
        // srej is out of window's range
        return NULL;
    }
    return win.queue[seqNum % win.winSize];
}

pdu getLow() {
    // server needs the lowest unacked packet, return pdu or null if none are unacked
    if (isEmpty()) {
        return NULL;
    }
    return win.queue[win.lower];
}

int isFull() {
    // returns true on full window, else false
    return win.queue[win.upper] != NULL;
}

int isEmpty() {
    // returns true on empty window, else false
    return win.queue[win.upper] == NULL;
}

void printWindowMeta() {
    printf("Server Window - Window Size: %d, Lower: %d, Upper: %d, Current: %d\n",
        win.winSize, win.lower, win.upper, win.current);
}

void printWindow() {
    int i;
    pdu packet;
    printf("Window size is: %d\n", win.winSize);
    for (i=0; i < win.winSize; i++) {
        if ((packet=win.queue[i]) == NULL) {
            printf("\t%d not valid\n", i);
        }
        else {
            printf("\t%d sequenceNumber: %u pduSize: %u\n",
                i, packet->seqNum, packet->payLen + HEADER_LEN);
        }
    }
}
