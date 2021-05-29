// Angela Kerlin
// Server side - UDP Code
// Orignal code from Hugh Smith	4/1/2017
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cpe464.h"
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#ifndef PDUS
	#define PDUS
    #include "pdu.h"
#endif
#include "window.h"
#include "pollLib.h"

#define MAXBUF 1400
#define MAX_PDU_LEN 1407
#define DEBUG_FLAG 10

// TODO: go through and verify pdu buf is valid (should rarely be MAX_PDU_LEN)

void waitForClients(int socketNum);
int forkChild();
void handleClient(struct sockaddr_in6 *client,int socketNum, uint8_t *pduBuf, uint16_t pduLen);

FILE *handleSetup(int socketNum, uint32_t *winSize, uint16_t *bufSize, struct sockaddr_in6 *client, uint8_t *pduBuf, uint16_t pduLen);
FILE *setupResponse(int socketNum, char *infile, struct sockaddr_in6 *client);

void transferData(int socketNum, FILE *input, uint32_t winSize, uint16_t bufSize, struct sockaddr_in6 *client);
void sendNextPdu(int socketNum, FILE *input, uint32_t winSize, uint16_t bufSize, struct sockaddr_in6 *client, uint32_t seqNum);
uint16_t getNextData(FILE *input, uint16_t bufSize, char * dataBuf);
void checkForRRs(FILE *input, int socketNum, uint16_t bufSize, struct sockaddr_in6 *client);
void handleSrej(int socketNum, struct sockaddr_in6 *client, uint16_t bufSize, pdu packet);
void openWindow(FILE *input, int socketNum, uint16_t bufSize, struct sockaddr_in6 *client);
void resendLowest(int socketNum, struct sockaddr_in6 *client);

void waitOnRRs(FILE *input, int socketNum, uint16_t bufSize, struct sockaddr_in6 *client);
void sendEof(int socketNum, struct sockaddr_in6 *client, uint32_t mySeq);
void processEof(FILE *input, int socketNum, uint16_t bufSize, struct sockaddr_in6 *client, uint32_t seqNum);

int checkArgs(int argc, char *argv[], float *errRate);


int main (int argc, char *argv[]) { 
	int socketNum = 0;				
	int portNumber = 0;
	float errRate = 0;

	portNumber = checkArgs(argc, argv, &errRate);

	socketNum = udpServerSetup(portNumber);
	sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

	waitForClients(socketNum);

	return 0;
}

void waitForClients(int socketNum) {
	struct sockaddr_in6 client;
	int clientAddrLen = sizeof(client);
	int forked = 1;
	int pduLen = 0; 
	uint8_t pduBuf[MAX_PDU_LEN];
	setupPollSet();
	addToPollSet(socketNum);

	// while loop to fork children
	while (forked != 0) {
		pduLen = safeRecvfrom(socketNum, pduBuf, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);
		forked = forkChild();
	}
	close(socketNum); // child does not need server socket
	// create client socket
	socketNum = udpServerSetup(0);
	handleClient(&client, socketNum, pduBuf, pduLen);
	close(socketNum);
}

int forkChild() {
    pid_t forkr;
    pid_t pid;

    if ((forkr=fork()) == 0) {
       /* child */
       printf("This is the child, pid %d\n", (pid=getpid()));
       return 0;
    }
    else if (forkr == -1) {
       perror("fork\n");
       exit(1);
    }
    else {
       /* parent */
       printf("This is the parent, pid %d\n", (pid=getpid()));
	   return 1;
    }
}

void handleClient(struct sockaddr_in6 *client, int socketNum, uint8_t *pduBuf, uint16_t pduLen) {
	FILE *input = NULL;
	uint32_t winSize = 0;
	uint16_t bufSize = 0;

	if ((input=handleSetup(socketNum, &winSize, &bufSize, client, pduBuf, pduLen)) == NULL) {
		return;
	}

	if (initWindow(winSize)) {
		transferData(socketNum, input, winSize, bufSize, client);
	}
	freeWindow();
	fclose(input);
}

FILE *handleSetup(int socketNum, uint32_t *winSize, uint16_t *bufSize, struct sockaddr_in6 *client, uint8_t *pduBuf, uint16_t pduLen) {
	// handles setup phase of transfer and sets winSize and bufSize. Returns fd or -1 on failure
	struct PduS packet;
	packet.payload = NULL;
	char infile[MAXBUF];
	FILE *out = NULL;

	// recv the client's packet
	if (!interpPDU(&packet, pduBuf, pduLen) || packet.flag != INITIAL_FLAG) {
		// packet is corrupt or invalid
		return NULL;
	}
	memcpy(winSize, packet.payload, 4);
	*winSize = ntohl(*winSize);
	memcpy(bufSize, packet.payload + 4, 2);
	*bufSize = ntohs(*bufSize);
	strncpy(infile, (char*)packet.payload + 6, packet.payLen - 6);
	
	out = setupResponse(socketNum, infile, client);
	freePDU(&packet);
	return out;
}

FILE *setupResponse(int socketNum, char *infile, struct sockaddr_in6 *client) {
	// attempts to open file then responds to client
	// returns in file descriptor on success or -1 on failure
	int clientAddrLen = sizeof(*client);
	int respLen, pduLen = 0;
	FILE *in = fopen(infile, "r");
	uint8_t pduBuf[MAX_PDU_LEN], respBuf[MAX_PDU_LEN];
	struct PduS packet;
	packet.payload = NULL;
	uint8_t payLoad = (in == NULL ? 0 : 1);

	// create response then send
	pduLen = createPdu(pduBuf, 0, INITIALRESP_FLAG, &payLoad, 1);
	sendtoErr(socketNum, pduBuf, pduLen , 0, (struct sockaddr *)client, clientAddrLen);
	if (!payLoad) {
		// if filename is bad, just send once then give up
		return NULL;
	}
	// wait on client ack
	do {
		freePDU(&packet);
		if (pollCall(10000) == -1) {// wait 10 seconds max
			return NULL;
		}
		respLen = safeRecvfrom(socketNum, respBuf, MAXBUF, 0, (struct sockaddr *)client, &clientAddrLen);
	} while(interpPDU(&packet, respBuf, respLen) == 0 && packet.flag != SETUPRESP_FLAG); // if corrupt or invalid, keep listening
	freePDU(&packet);

	// send ack to client ack
	pduLen = createPdu(pduBuf, 1, SETUPRESP_FLAG, &payLoad, 1);
	sendtoErr(socketNum, pduBuf, pduLen , 0, (struct sockaddr *)client, clientAddrLen);
	return in;
}

void transferData(int socketNum, FILE *input, uint32_t winSize, uint16_t bufSize, struct sockaddr_in6 *client) {
	// sends data packets to rcopy
	uint32_t seqNum = 0;

	while (!feof(input)) {
		if (!isWindowFull()) {
			sendNextPdu(socketNum, input, winSize, bufSize, client, seqNum);
			seqNum++;
		}

		checkForRRs(input, socketNum, bufSize, client);

		if (isWindowFull()) {
			// printf("Window closed!\n");
			openWindow(input, socketNum, bufSize, client);
		}
	}
	processEof(input, socketNum, bufSize, client, seqNum);
}

void sendNextPdu(int socketNum, FILE *input, uint32_t winSize, uint16_t bufSize, struct sockaddr_in6 *client, uint32_t seqNum) {
	int clientAddrLen = sizeof(*client);
	char dataBuf[bufSize];
	uint8_t pduBuf[bufSize + HEADER_LEN];
	uint16_t dataLen = 0, pduLen = 0;
	pdu sent;

	// create next pdu
	dataLen = getNextData(input, bufSize, dataBuf);
	pduLen = createPdu(pduBuf, seqNum, DATA_FLAG, (uint8_t*)dataBuf, dataLen);
	// printf("Read %u bytes, sending %u byes\n", dataLen, pduLen); //dd

	// send next pdu and update window
	sendtoErr(socketNum, pduBuf, pduLen , 0, (struct sockaddr *)client, clientAddrLen);
	sent = (pdu)malloc(sizeof(struct PduS));
	interpPDU(sent, pduBuf, pduLen);
	serverSent(sent);
}

uint16_t getNextData(FILE *input, uint16_t bufSize, char * dataBuf) {
	// fills given character array with next data chunk from given file. Returns bytes of data read
	uint16_t bytesRead = fread(dataBuf, 1, bufSize, input);
	// printf("Read %u bytes out of %u\n", bytesRead, bufSize); //dd
	if (ferror(input)) {
		perror("fread");
		freeWindow();
		exit(EXIT_FAILURE);
	}
	return bytesRead;
}

void checkForRRs(FILE *input, int socketNum, uint16_t bufSize, struct sockaddr_in6 *client) {
	// checks for RR's and updates window accordingly
	int clientAddrLen = sizeof(*client);
	int respLen = 0;
	uint8_t respBuf[(bufSize < sizeof(uint32_t) ? sizeof(uint32_t) : bufSize)+ HEADER_LEN]; // room for at least an rr
	struct PduS packet;
	packet.payload = NULL;
	uint32_t rrSeq = 0;

	while (pollCall(0) != -1) {
		respLen = safeRecvfrom(socketNum, respBuf, (bufSize < sizeof(uint32_t) ? sizeof(uint32_t) : bufSize)+ HEADER_LEN,
				 0, (struct sockaddr *)client, &clientAddrLen);
		if (interpPDU(&packet, respBuf, respLen)) {
			if (packet.flag == RR_FLAG) {
				memcpy(&rrSeq, packet.payload, sizeof(uint32_t));
				recvRR(ntohl(rrSeq));
			}
			else if (packet.flag == SREJ_FLAG) {
				handleSrej(socketNum, client, bufSize, &packet);
			}
		}
		freePDU(&packet);
		// otherwise its corrupt or invalid, ignore it
	}
	// no packets left to recv
}

void handleSrej(int socketNum, struct sockaddr_in6 *client, uint16_t bufSize, pdu packet) {
	// printf("\n\tHANDLING SREJ ~~~~~~~~~~~~~~~~~~~~~\n"); //dd
	// handles a recieved srej
	pdu resendPacket = NULL;
	int clientAddrLen = sizeof(*client);
	uint32_t srejSeq = 0;
	uint8_t pduBuf[bufSize + HEADER_LEN];
	uint16_t pduLen = 0;

	// recreate srej packet
	memcpy(&srejSeq, packet->payload, sizeof(uint32_t));
	srejSeq = ntohl(srejSeq);
	resendPacket = srej(srejSeq);

	// printf("\tHANDLING SREJ %d~~~~~~~~~~~~~~~~~~~~~\n\n", srejSeq); //dd
	if (resendPacket) {
		pduLen = recreatePDUS(resendPacket, pduBuf);
		sendtoErr(socketNum, pduBuf, pduLen , 0, (struct sockaddr *)client, clientAddrLen);
	}
}

void openWindow(FILE *input, int socketNum, uint16_t bufSize, struct sockaddr_in6 *client) {
	// forces the window open
	int count = 0, respLen = 0, clientAddrLen = sizeof(*client), rrFound=0;
	uint8_t respBuf[sizeof(uint32_t) + HEADER_LEN];
	struct PduS packet;
	packet.payload = NULL;
	uint32_t rrSeq = 0;
	packet.flag = -1;

	while (count < 10 && !rrFound) {
		freePDU(&packet);
		if (pollCall(1000) == -1) {
			// if no response in a second, resend eof packet
			resendLowest(socketNum, client);
			count++;
		}
		else {
			respLen = safeRecvfrom(socketNum, respBuf, sizeof(uint32_t) + HEADER_LEN, 0, (struct sockaddr *)client, &clientAddrLen);
			if (interpPDU(&packet, respBuf, respLen)) {
				// packet is not corrupt
				if (packet.flag == SREJ_FLAG) {
					handleSrej(socketNum, client, bufSize, &packet);
				}
				else if (packet.flag == RR_FLAG) {
					rrFound = 1;
				}
			}
		}
	}

	if (count >= 10) {
		// timed out
		fprintf(stderr, "Client timed out\n");
		close(socketNum);
		fclose(input);
		freeWindow();
		exit(EXIT_FAILURE);
	}
	memcpy(&rrSeq, packet.payload, sizeof(uint32_t));
	recvRR(ntohl(rrSeq));
	freePDU(&packet);
	// printf("Window open!\n");
}

void resendLowest(int socketNum, struct sockaddr_in6 *client) {
	int clientAddrLen = sizeof(*client);
	pdu packet = getLow(); 

	if (packet != NULL) {
		uint8_t pduBuf[packet->payLen + HEADER_LEN];
		int pduLen = recreatePDUS(packet, pduBuf);
		sendtoErr(socketNum, pduBuf, pduLen , 0, (struct sockaddr *)client, clientAddrLen);
	}
}

void processEof(FILE *input, int socketNum, uint16_t bufSize, struct sockaddr_in6 *client, uint32_t mySeq) {
	// closes the connection
	int clientAddrLen = sizeof(*client);
	int count = 0;
	uint8_t respBuf[bufSize + HEADER_LEN];
	int respLen = 0;
	struct PduS packet;
	packet.payload = NULL;
	packet.flag = -1;

	waitOnRRs(input, socketNum, bufSize, client);

	sendEof(socketNum, client, mySeq++);
	do {
		freePDU(&packet);
		if (pollCall(1000) == -1) {
			// if no response in a second, resend eof packet
			sendEof(socketNum, client, mySeq++);
			count++;
		}
		else {
			respLen = safeRecvfrom(socketNum, respBuf, bufSize + HEADER_LEN, 0, (struct sockaddr *)client, &clientAddrLen);
		}
	} while(count < 10 && interpPDU(&packet, respBuf, respLen) == 0 && packet.flag != ACKEOF_FLAG); // if corrupt or invalid, keep waiting
	if (count >= 10) {
		// timed out
		fprintf(stderr, "Client timed out\n");
		close(socketNum);
		fclose(input);
		freeWindow();
		exit(EXIT_FAILURE);
	}
	freePDU(&packet);
	// otherwise, successful transfer!
}

void waitOnRRs(FILE *input, int socketNum, uint16_t bufSize, struct sockaddr_in6 *client) {
	// keep forcing RR's until the window is empty
	while(!isWindowEmpty()) {
		openWindow(input, socketNum, bufSize, client);
	} 
}

void sendEof(int socketNum, struct sockaddr_in6 *client, uint32_t mySeq) {
	int clientAddrLen = sizeof(*client);
	uint8_t pduBuf[HEADER_LEN];
	int pduLen = 0;

	pduLen = createPdu(pduBuf, mySeq, EOF_FLAG, NULL, 0);
	sendtoErr(socketNum, pduBuf, pduLen , 0, (struct sockaddr *)client, clientAddrLen);
}

int checkArgs(int argc, char *argv[], float *errRate) {
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 3 || argc == 1)
	{
		fprintf(stderr, "Usage %s error-rate [optional port number]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}

	*errRate = atof(argv[1]);
	
	return portNumber;
}