/* Angela Kerlin
 * Sliding window functionality for both server and rcopy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#ifndef PDUS
	#define PDUS
    #include "pdu.h"
#endif

int initWindow();          // initializes window for server or rcopy, returns 0 for failure, 1 for success
void freeWindow();         // frees the window and everything malloced inside
void printWindowMeta();
void printWindow();

// SERVER RELATED FUNCTIONS
int sent(pdu packet);      // server sent this packet, return 1 on sucess, 0 on invalid send
int rr(uint32_t seqNum);   // rcopy is ready for this seqNum, return 1 on sucess, 0 on invalid seqNum
pdu srej(uint32_t seqNum); // rcopy rejected this seqNum, return the pdu to resend or null if its out of the window
pdu getLow();              // server needs the lowest unacked packet, return pdu or null if none are unacked
int isWindowFull();        // returns true on full window
int isWindowEmpty();       // returns true on empty window

// RCOPY RELATED FUNCTIONS
int skip(uint32_t seqNum);  // identifies seqNum as waiting on packet. Returns 0 on failure, 1 on success
int buffer(pdu packet);     // buffer given packet, returns 0 on failure, 1 on successful buffer, 2 if given packet was the lowest SREJ
pdu unbuffer();             // returns null if waiting on lowest pdu or empty else returns lowest pdu and removes it from queue
uint32_t lastPacket();      // returns the last srej or recv packet number
int isBufFull();            // returns true on full buffer, else false
int isBufEmpty();           // returns true on empty buffer, else false