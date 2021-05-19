/* Angela Kerlin
 * Sliding window functionality for both server and rcopy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "pdu.h"

// SERVER RELATED FUNCTIONS
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


// RCOPY RELATED FUNCTIONS
