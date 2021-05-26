// Angela Kerlin
// Client side udp
// Original code from Hugh Smith 4/1/2017

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

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


void handleConnection(int socketNum, struct sockaddr_in6 * server, char *from, char *to, uint32_t winSize, uint16_t bufSize);
FILE *openOutput(char *to, int socketNum);
void initExchange(int socketNum, struct sockaddr_in6 * server, char *from, uint32_t winSize, uint16_t bufSize, pdu packet);
void getInitialResp(int socketNum, struct sockaddr_in6 * server, char *from, uint32_t winSize, uint16_t bufSize, pdu packet);
void ackInitialResp(int socketNum, struct sockaddr_in6 * server, pdu packet);
void recvDataLoop(int socketNum, struct sockaddr_in6 * server, pdu packet, FILE *output);

int checkArgs(int argc, char * argv[]);

void testConnection(int socketNum, struct sockaddr_in6 * server);
int readFromStdin(char * buffer);


int main (int argc, char *argv[]) {
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int portNumber = checkArgs(argc, argv);
	char *from = argv[1]; // CHECK FOR EXCESSIVELY LONG FILENAMES TODO
	char *to = argv[2];
	uint32_t windowSize = atoi(argv[3]); // TODO change to strtol and strtof
	uint16_t bufferSize = atoi(argv[4]); // TODO change to strtol and strtof
	float errRate = atof(argv[5]);

	printf("from: %s to: %s winSize: %u bufSize: %u errRate: %f\n", from, to, windowSize, bufferSize, errRate);
	
	sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);
	
	socketNum = setupUdpClientToServer(&server, argv[6], portNumber);
	
	// talkToServer(socketNum, &server);
	handleConnection(socketNum, &server, from, to, windowSize, bufferSize);
	
	close(socketNum);

	return 0;
}

void handleConnection(int socketNum, struct sockaddr_in6 * server, char *from, char *to, uint32_t winSize, uint16_t bufSize) {
	// establish connection: send filename/bufsize/winsize, recv data
	struct PduS packet;
	// open output file
	FILE *output = openOutput(to, socketNum);

	// send initial packet
	setupPollSet();
	addToPollSet(socketNum);
	initExchange(socketNum, server, from, winSize, bufSize, &packet);

	// TODO: close and reopen socket to switch to server child?

	// recieve data packets
	recvDataLoop(socketNum, server, &packet, output);

	// cleanup
	fclose(output); // should I check the return value? dd
}

FILE *openOutput(char *to, int socketNum) {
	FILE *out = fopen(to, "w");
	if (out == NULL) {
		printf("Invalid output file provided\n");
		close(socketNum);
		perror("open");
		exit(EXIT_FAILURE);
	}
	return out;
}

void initExchange(int socketNum, struct sockaddr_in6 * server, char *from, uint32_t winSize, uint16_t bufSize, pdu packet) {
	// TODO
	struct PduS respPacket;
	getInitialResp(socketNum, server, from, winSize, bufSize, &respPacket);

	if (respPacket.payload[0] == 0) {
		// bad filename
		printf("Invalid input file provided\n");
		close(socketNum);
		exit(EXIT_SUCCESS); // user error, so I consider it a success
	}

	ackInitialResp(socketNum, server, packet);
}

void getInitialResp(int socketNum, struct sockaddr_in6 * server, char *from, uint32_t winSize, uint16_t bufSize, pdu packet) {
	// handles initial filename/buf/win exchange and recieves a response
	// TODO: double check all len maxes
	uint8_t payLoad[MAX_PDU_LEN];
	uint16_t pduLen = 0; 
	uint8_t pduBuf[MAX_PDU_LEN];
	uint16_t respLen = 0; 
	uint8_t respBuf[MAX_PDU_LEN];
	int serverAddrLen = sizeof(struct sockaddr_in6);
	int count = 0;

	// create initial packet
	winSize = htonl(winSize);
	memcpy(payLoad, &winSize, 4);
	bufSize = htons(bufSize);
	memcpy(payLoad + 4, &bufSize, sizeof(bufSize));
	memcpy(payLoad + 6, from, strlen(from) + 1); // includes the null

	pduLen = createPdu(pduBuf, 0, INITIAL_FLAG, (uint8_t*)payLoad, 6 + strlen(from) + 1);

	do {
		// send filename/winsize/buffsize packet
		sendtoErr(socketNum, pduBuf, pduLen, 0, (struct sockaddr *) server, serverAddrLen);
		
		// wait for valid response
		if (pollCall(1000) != -1) {
			respLen = safeRecvfrom(socketNum, respBuf, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);
		}
		count++;
		// TODO: close and reopen socket to switch to server child?
	} while ((interpPDU(packet, respBuf, respLen) == 0 || packet->flag != INITIALRESP_FLAG) && count < 10);
	
	if (count >= 10) {
		// timed out
		fprintf(stderr, "Server timed out\n");
		close(socketNum);
		exit(EXIT_FAILURE);
	}
	// packet is not corrupt and the flag is expected
}

void ackInitialResp(int socketNum, struct sockaddr_in6 * server, pdu packet) {
	// acknowledges server response and waits for their ack
	// if rcopy recv data packet, pass it along to next function
	uint16_t pduLen = 0; 
	uint8_t pduBuf[MAX_PDU_LEN];
	uint16_t respLen = 0; 
	uint8_t respBuf[MAX_PDU_LEN];
	int serverAddrLen = sizeof(struct sockaddr_in6);
	int count = 0;

	pduLen = createPdu(pduBuf, 1, SETUPRESP_FLAG, NULL, 0);

	do {
		// send filename/winsize/buffsize packet
		sendtoErr(socketNum, pduBuf, pduLen, 0, (struct sockaddr *) server, serverAddrLen);
		
		// wait for valid response
		if (pollCall(1000) != -1) {
			respLen = safeRecvfrom(socketNum, respBuf, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);
		}
		count++;
	} while ((interpPDU(packet, respBuf, respLen) == 0 || !(packet->flag == SETUPRESP_FLAG || packet->flag == DATA_FLAG)) && count < 10);

	if (count >= 10) {
		// timed out
		fprintf(stderr, "Server timed out\n");
		close(socketNum);
		exit(EXIT_FAILURE);
	}
	if (packet->flag == SETUPRESP_FLAG) {
		// clear pointer if its not a data packet
		packet = NULL;
	}
}

void recvDataLoop(int socketNum, struct sockaddr_in6 * server, pdu packet, FILE *output) {
	// TODO
}

void testConnection(int socketNum, struct sockaddr_in6 * server) {
	// dd
	int serverAddrLen = sizeof(struct sockaddr_in6);
	char * ipString = NULL;
	uint16_t seqNum = 0;
	int dataLen = 0; 
	char buffer[MAXBUF+1];
	int pduLen = 0; 
	uint8_t pduBuf[MAX_PDU_LEN];
	
	buffer[0] = '\0';
	while (buffer[0] != '.')
	{
		dataLen = readFromStdin(buffer);

		// printf("Sending: %s with len: %d\n", buffer,dataLen);
		pduLen = createPdu(pduBuf, seqNum, DEBUG_FLAG, (uint8_t*)buffer, dataLen - 1); // -1 to remove null
		// outputPDU(pduBuf, pduLen);
	
		sendtoErr(socketNum, pduBuf, pduLen, 0, (struct sockaddr *) server, serverAddrLen);
		
		pduLen = safeRecvfrom(socketNum, pduBuf, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);
		outputPDU(pduBuf, pduLen);
		
		// print out bytes received
		ipString = ipAddressToString(server);
		printf("Server with ip: %s and port %d said it received %s\n", ipString, ntohs(server->sin6_port), buffer);

		seqNum++;
	}
}

int readFromStdin(char * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

int checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 8)
	{
		printf("usage: %s from-file to-file window-size buffer-size error-rate host-name port-number \n", argv[0]);
		exit(1);
	}
	
	// Checks args and returns port number
	int portNumber = 0;

	portNumber = atoi(argv[7]); // TODO change to strtol
		
	return portNumber;
}





