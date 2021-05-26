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
#include "shared.h"

#define MAXBUF 1400
#define MAX_PDU_LEN 1407
#define DEBUG_FLAG 10

void waitForClients(int socketNum);
void handleClient(int socketNum);
FILE *handleSetup(int socketNum, uint32_t *winSize, uint16_t *bufSize, struct sockaddr_in6 *client);
FILE *setupResponse(int socketNum, char *infile, struct sockaddr_in6 *client);
void transferData(int socketNum, FILE *input, uint32_t winSize, uint16_t bufSize, struct sockaddr_in6 *client);
int checkArgs(int argc, char *argv[], float *errRate);

void testConnection(int socketNum);

int main (int argc, char *argv[]) { 
	int socketNum = 0;				
	int portNumber = 0;
	float errRate = 0;

	portNumber = checkArgs(argc, argv, &errRate);

	socketNum = udpServerSetup(portNumber);
	sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

	waitForClients(socketNum);

	close(socketNum);
	
	return 0;
}

void waitForClients(int socketNum) {
	setupPollSet();
	addToPollSet(socketNum);
	// would be in a while loop to fork children, break if res = 0 I guess?
	pollCall(-1);
	handleClient(socketNum);
}

void handleClient(int socketNum) {
	// TODO: fork here?
	struct sockaddr_in6 client;
	FILE *input = NULL;
	uint32_t winSize = 0;
	uint16_t bufSize = 0;

	if ((input=handleSetup(socketNum, &winSize, &bufSize, &client)) == NULL) {
		return;
	}

	transferData(socketNum, input, winSize, bufSize, &client);
	fclose(input);
}

FILE *handleSetup(int socketNum, uint32_t *winSize, uint16_t *bufSize, struct sockaddr_in6 *client) {
	// handles setup phase of transfer and sets winSize and bufSize. Returns fd or -1 on failure
	int pduLen = 0; 
	int clientAddrLen = sizeof(*client);
	uint8_t pduBuf[MAX_PDU_LEN];
	struct PduS packet;
	char infile[MAXBUF];
	FILE *out = NULL;

	// recv the client's packet
	pduLen = safeRecvfrom(socketNum, pduBuf, MAXBUF, 0, (struct sockaddr *)client, &clientAddrLen);
	if (!interpPDU(&packet, pduBuf, pduLen) || packet.flag != INITIAL_FLAG) {
		// packet is corrupt or invalid
		return NULL;
	}
	memcpy(winSize, packet.payload, 4);
	*winSize = ntohl(*winSize);
	memcpy(bufSize, packet.payload + 4, 2);
	*bufSize = ntohs(*bufSize);
	strncpy(infile, (char*)packet.payload + 6, *bufSize - 6);
	
	out = setupResponse(socketNum, infile, client);
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
	uint8_t payLoad = (in == NULL ? 0 : 1);

	// create response then send
	pduLen = createPdu(pduBuf, 0, INITIALRESP_FLAG, &payLoad, 1);
	sendtoErr(socketNum, pduBuf, pduLen , 0, (struct sockaddr *)client, clientAddrLen);
	if (!payLoad) {
		// if filename is bad, just send once then give up
		return NULL;
	}
	do {
		if (pollCall(10000) == -1) {// wait 10 seconds max
			return NULL;
		}
		respLen = safeRecvfrom(socketNum, respBuf, MAXBUF, 0, (struct sockaddr *)client, &clientAddrLen);
	} while(interpPDU(&packet, respBuf, respLen) == 0 && packet.flag != SETUPRESP_FLAG); // if corrupt or invalid, keep listening

	// send ack to client ack
	pduLen = createPdu(pduBuf, 1, SETUPRESP_FLAG, &payLoad, 1);
	sendtoErr(socketNum, pduBuf, pduLen , 0, (struct sockaddr *)client, clientAddrLen);
	return in;
}

void transferData(int socketNum, FILE *input, uint32_t winSize, uint16_t bufSize, struct sockaddr_in6 *client) {
	// TODO
}

void testConnection(int socketNum) {
	// dd
	int dataLen = 0; 
	char buffer[MAXBUF + 1];	  
	struct sockaddr_in6 client;		
	int clientAddrLen = sizeof(client);	
	int pduLen = 0; 
	uint8_t pduBuf[MAX_PDU_LEN];
	uint16_t seqNum = 0;

	
	buffer[0] = '\0';
	while (buffer[0] != '.') {
		dataLen = safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);
	
		printf("Received message from client with ");
		printIPInfo(&client);
		// printf(" Len: %d \'%s\'\n", dataLen, buffer);
		outputPDU((uint8_t*)buffer, dataLen);

		// just for fun send back to client number of bytes received
		sprintf(buffer, "bytes: %d", dataLen);
		pduLen = createPdu(pduBuf, seqNum, DEBUG_FLAG, (uint8_t*)buffer, strlen(buffer));
		// outputPDU((uint8_t*)pduBuf, pduLen);
		sendtoErr(socketNum, pduBuf, pduLen , 0, (struct sockaddr *) & client, clientAddrLen);

		seqNum++;
	}
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