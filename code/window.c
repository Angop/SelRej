/* Angela Kerlin
 * Sliding window functionality for both server and rcopy
 */

#include "window.h"

typedef struct Window * window;
struct Window {
    pdu * queue;         // points to the 0th index in an array of Pdu's
    int winSize;         // size of the window
    int lower;           // the lowest unacked pdu's index
    int upper;           // upper bound of window
    int current;         // index of the last sent packet
    uint32_t lastSeq;    // FOR RCOPY ONLY last seqNum
    uint32_t firstSeq;   // FOR RCOPY ONLY first seqNum
}__attribute__((packed));

// GLOBAL VARIABLE
struct Window win;

int lessIndex(int a, int b);// compares two indicies in a circular queue

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

void printWindowMeta() {
    printf("Server Window - Window Size: %d, Lower: %d, Upper: %d, Current: %d LastSeq: %d firstSeq: %d\n",
        win.winSize, win.lower, win.upper, win.current, win.lastSeq, win.firstSeq);
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



// SERVER RELATED FUNCTIONS
int serverSent(pdu packet) {
    // TODO SHOULD I MALLOC PACKET HERE? or assume it was malloced before added here?
    // server sent this packet, return 1 on sucess, 0 on invalid send
    if (isWindowFull()) {
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

int recvRR(uint32_t seqNum) {
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
    if (isWindowEmpty()) {
        return NULL;
    }
    return win.queue[win.lower];
}

int isWindowFull() {
    // returns true on full window, else false
    return win.queue[win.upper] != NULL;
}

int isWindowEmpty() {
    // returns true on empty window, else false
    return win.queue[win.lower] == NULL;
}



// RCOPY RELATED FUNCTIONS
int skip(uint32_t seqNum) {
    // identifies seqNum as waiting on packet. Returns 0 on failure, 1 on success
    if (isBufEmpty()) {
        // handle the start of waiting period
        win.lower = seqNum % win.winSize;
        win.current = win.lower; // leaves a null space to indicate a skip
        win.upper = (win.lower + win.winSize - 1) % win.winSize;
        win.lastSeq = seqNum;
        win.firstSeq = seqNum;
        return 1;
    }
    if (isBufFull()) {
        // no room to skip another packet
        fprintf(stderr, "skip: buffer overflow");
        exit(EXIT_FAILURE);
    }
    if (seqNum % win.winSize != (win.current + 1) % win.winSize) {
        // skipping out of order
        return 0;
    }
    win.current = (win.current + 1) % win.winSize; // leaves a null space to indicate a skip
    win.lastSeq = seqNum;
    return 1;
}

int lessIndex(int a, int b) {
    // compares two indicies in a circular queue
    return (a + win.winSize - 1 - win.upper) % win.winSize <
            (b + win.winSize - 1 - win.upper) % win.winSize;
}

int buffer(pdu packet) {
    // buffer given packet, returns 0 on failure, 1 on successful buffer, 2 if given packet was the lowest SREJ
    int newCur = packet->seqNum % win.winSize;
    if (win.current == -1) {
        // nothing has been skipped/buffered yet, no reason to buffer
        return 0;
    }
    if (newCur == win.lower) {
        // this was the lowest skipped packet, client should start unbuffering
        win.queue[win.lower] = packet;
        return 2;
    }
    if (win.queue[newCur] != NULL) {
        // attempted to overwrite an existing packet in queue
        if (packet->seqNum == win.queue[newCur]->seqNum) {
            // same packet, take newer one
            // TODO: probably free older version??
            freePDU(win.queue[newCur]);
            win.queue[newCur] = packet;
            return 1;
        }
        else {
            return 0;
        }
    }
    win.queue[newCur] = packet;
    if (lessIndex(win.current, newCur)) {
        // if new packet is the greatest in buffer, set as new current
        win.current = newCur;
        win.lastSeq = packet->seqNum;
    }
    return 1;
}

pdu unbuffer() {
    // returns null if waiting on lowest pdu or empty else returns lowest pdu and removes it from queue
    if (win.queue[win.lower] == NULL) {
        return NULL;
    }
    pdu packet = win.queue[win.lower];
    win.firstSeq = packet->seqNum + 1;
    win.queue[win.lower] = NULL;
    if (win.current == win.lower) {
        // reset buffer if empty
        win.current = -1;
    }
    win.lower = (win.lower + 1) % win.winSize;
    win.upper = (win.upper + 1) % win.winSize;
    return packet;
}

uint32_t lastPacket() {
    // returns the last srej or recv packet number
    return win.lastSeq;
}

uint32_t firstPacket() {
    // returns the first srej or recv packet number
    return win.firstSeq;
}

int isBufFull() {
    // returns true on full buffer, else false
    return win.current == win.upper;
}

int isBufEmpty() {
    // returns true on empty buffer, else false
    return win.current == -1;
}