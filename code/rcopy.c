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

int sockNum = 0;
struct sockaddr_in6 serverS;		// Supports 4 and 6 but requires IPv6 struct


void handleConnection(char *from, char *to, uint32_t winSize, uint16_t bufSize);
FILE *openOutput(char *to);
void initExchange(char *from, uint32_t winSize, uint16_t bufSize, pdu packet);
void getInitialResp(char *from, uint32_t winSize, uint16_t bufSize, pdu packet);
void ackInitialResp(pdu packet);
void recvDataLoop(pdu packet, FILE *output, uint16_t bufSize);

// recvDataLoop helpers
int recvPacket(pdu packet, uint16_t bufSize);
uint8_t processPacket(pdu packet, uint32_t *expectedSeq, uint32_t *mySeq, uint16_t bufSize, FILE *output);
uint8_t processEof(uint32_t mySeq);
uint8_t processDataPacket(pdu packet, uint32_t *expectedSeq, uint32_t *mySeq, uint16_t bufSize, FILE *output);
void writeData(uint8_t *data, uint16_t dataLen, FILE *output);
uint8_t handleSrej(pdu packet, uint32_t *expectedSeq, uint32_t *mySeq, uint16_t bufSize, FILE *output);
void sendRR(uint32_t readySeq, uint32_t *mySeq, uint16_t bufSize);
void sendSrejs(uint32_t expectedSeq, uint32_t recvSeq, uint32_t *mySeq);
uint8_t processWaitingPacket(pdu packet, uint32_t *mySeq, uint16_t bufSize, FILE *output);
void resendSrej(uint32_t *mySeq);
void unbufferSrej(FILE *output, uint32_t *mySeq, uint32_t bufSize);
int bufferPacket(pdu packet);

int checkArgs(int argc, char * argv[]);

void testConnection();
int readFromStdin(char * buffer);


int main (int argc, char *argv[]) {
	int portNumber = checkArgs(argc, argv);
	char *from = argv[1]; // CHECK FOR EXCESSIVELY LONG FILENAMES TODO
	char *to = argv[2];
	uint32_t windowSize = atoi(argv[3]); // TODO change to strtol and strtof
	uint16_t bufferSize = atoi(argv[4]); // TODO change to strtol and strtof
	float errRate = atof(argv[5]);

	// printf("from: %s to: %s winSize: %u bufSize: %u errRate: %f\n", from, to, windowSize, bufferSize, errRate); //dd
	
	sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);
	
	sockNum = setupUdpClientToServer(&serverS, argv[6], portNumber);
	
	// talkToServer(socketNum, &server);
	handleConnection(from, to, windowSize, bufferSize);
	
	close(sockNum);

	return 0;
}

void handleConnection(char *from, char *to, uint32_t winSize, uint16_t bufSize) {
	// establish connection: send filename/bufsize/winsize, recv data
	struct PduS packet;
	packet.payload = NULL;
	// open output file
	FILE *output = openOutput(to);

	// send initial packet
	setupPollSet();
	addToPollSet(sockNum);
	initExchange(from, winSize, bufSize, &packet);

	// TODO: close and reopen socket to switch to server child?

	// recieve data packets
	if (initWindow(winSize)) {
		recvDataLoop(&packet, output, bufSize);
	}

	// cleanup
	freeWindow();
	fclose(output); // should I check the return value? dd
}

FILE *openOutput(char *to) {
	FILE *out = fopen(to, "w");
	if (out == NULL) {
		printf("Invalid output file provided\n");
		close(sockNum);
		perror("open");
		exit(EXIT_FAILURE);
	}
	return out;
}

void initExchange(char *from, uint32_t winSize, uint16_t bufSize, pdu packet) {
	// TODO
	struct PduS respPacket;
	respPacket.payload = NULL;
	getInitialResp(from, winSize, bufSize, &respPacket);

	if (respPacket.payload[0] == 0) {
		// bad filename
		printf("Invalid input file provided\n");
		close(sockNum);
		exit(EXIT_SUCCESS); // user error, so I consider it a success
	}
	freePDU(&respPacket);

	ackInitialResp(packet);
}

void getInitialResp(char *from, uint32_t winSize, uint16_t bufSize, pdu packet) {
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
		freePDU(packet);
		// send filename/winsize/buffsize packet
		sendtoErr(sockNum, pduBuf, pduLen, 0, (struct sockaddr *) &serverS, serverAddrLen);
		
		// wait for valid response
		if (pollCall(1000) != -1) {
			respLen = safeRecvfrom(sockNum, respBuf, MAXBUF, 0, (struct sockaddr *) &serverS, &serverAddrLen);
		}
		count++;
		// TODO: close and reopen socket to switch to server child?
	} while ((interpPDU(packet, respBuf, respLen) == 0 || packet->flag != INITIALRESP_FLAG) && count < 10);
	
	if (count >= 10) {
		// timed out
		fprintf(stderr, "Server timed out\n");
		close(sockNum);
		exit(EXIT_FAILURE);
	}
	// packet is not corrupt and the flag is expected
}

void ackInitialResp(pdu packet) {
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
		freePDU(packet);
		// send filename/winsize/buffsize packet
		sendtoErr(sockNum, pduBuf, pduLen, 0, (struct sockaddr *) &serverS, serverAddrLen);
		
		// wait for valid response
		if (pollCall(1000) != -1) {
			respLen = safeRecvfrom(sockNum, respBuf, MAXBUF, 0, (struct sockaddr *) &serverS, &serverAddrLen);
		}
		count++;
	} while ((interpPDU(packet, respBuf, respLen) == 0 || !(packet->flag == SETUPRESP_FLAG || packet->flag == DATA_FLAG)) && count < 10);

	if (count >= 10) {
		// timed out
		fprintf(stderr, "Server timed out\n");
		close(sockNum);
		exit(EXIT_FAILURE);
	}
	if (packet->flag == SETUPRESP_FLAG) {
		// clear pointer if its not a data packet
		packet = NULL;
	}
}

void recvDataLoop(pdu lastPacket, FILE *output, uint16_t bufSize) {
	// TODO
	uint8_t lastFlag = 0;
	int pollRes = 0;
	struct PduS packet;
	packet.payload = NULL;
	uint32_t expectedSeq = 0, mySeq = 0;

	// handle the last packet in case it was a data packet
	processPacket(lastPacket, &expectedSeq, &mySeq, bufSize, output);
	lastFlag = lastPacket->flag;
	freePDU(lastPacket);

	// handle each packet that arrives
	while (lastFlag != EOF_FLAG && (pollRes=pollCall(10000)) != -1) {
		if (recvPacket((pdu)&packet, bufSize)) {
			lastFlag = processPacket((pdu)&packet, &expectedSeq, &mySeq, bufSize, output);
			freePDU(&packet);
		}
	}
	if (pollRes == -1) {
		// timed out
		fprintf(stderr, "Server timed out\n");
		close(sockNum);
		fclose(output);
		freeWindow();
		exit(EXIT_FAILURE);
	}
}

int recvPacket(pdu packet, uint16_t bufSize) {
	// recieves a packet on 
	int serverAddrLen = sizeof(struct sockaddr_in6);
	uint16_t pduLen = 0; 
	uint8_t pduBuf[bufSize + HEADER_LEN];

	pduLen = safeRecvfrom(sockNum, pduBuf, bufSize + HEADER_LEN, 0, (struct sockaddr *) &serverS, &serverAddrLen);
	return interpPDU(packet, pduBuf, pduLen);
}

uint8_t processPacket(pdu packet, uint32_t *expectedSeq, uint32_t *mySeq, uint16_t bufSize, FILE *output) {
	// processes any recieved packet in the data transfer sequence. Updates sequence pointers
	if (packet->flag == DATA_FLAG) {
		return processDataPacket(packet, expectedSeq, mySeq, bufSize, output);
	}
	if (packet->flag == EOF_FLAG) {
		processEof(*mySeq);
		return EOF_FLAG;
	}
	return packet->flag;
}

uint8_t processEof(uint32_t mySeq) {
	// sends a eof ack
	int serverAddrLen = sizeof(struct sockaddr_in6);
	int pduLen = 0;
	uint8_t pduBuf[HEADER_LEN];

	pduLen = createPdu(pduBuf, mySeq, ACKEOF_FLAG, NULL, 0);
	sendtoErr(sockNum, pduBuf, pduLen, 0, (struct sockaddr *) &serverS, serverAddrLen);

	return EOF_FLAG;
}

uint8_t processDataPacket(pdu packet, uint32_t *expectedSeq, uint32_t *mySeq, uint16_t bufSize, FILE *output) {
	// processes a data packet, handles sending RRs and SREJs
	if (packet->seqNum > *expectedSeq) {
		// seqNum is higher than expected
		return handleSrej(packet, expectedSeq, mySeq, bufSize, output);
	}
	else if (packet->seqNum == *expectedSeq) {
		// seqNum is expected -> send new RR
		// printf("Norm Writing %u bytes from packet: %u", packet->payLen, packet->seqNum);
		writeData(packet->payload, packet->payLen, output);
		(*expectedSeq) = (*expectedSeq) + 1;
		sendRR(*expectedSeq, mySeq, bufSize);
		return DATA_FLAG;
	}
	// seqNum is lower than expected -> send last RR
	sendRR(*expectedSeq, mySeq, bufSize);
	return DATA_FLAG;
}

void writeData(uint8_t *data, uint16_t dataLen, FILE *output) {
	uint16_t bytesWritten = fwrite(data, 1, dataLen, output);
	// printf(" ~~ wrote %d bytes\n", bytesWritten);
}

uint8_t handleSrej(pdu prevPacket, uint32_t *expectedSeq, uint32_t *mySeq, uint16_t bufSize, FILE *output) {
	// TODO
	// sends SREJ(s) and waits on missing packets
	int pollRes = 0;
	uint8_t lastFlag = DATA_FLAG;
	struct PduS curPacket;
	curPacket.payload = NULL;

	// printf("SREJ TIME ~~~~~~~~~~~~~~~~~~~\n"); //dd
	sendSrejs(*expectedSeq, prevPacket->seqNum, mySeq);
	if (!bufferPacket(prevPacket)) {
		fprintf(stderr, "Buffer error\n");
		close(sockNum);
		fclose(output);
		freeWindow();
		exit(EXIT_FAILURE);
	}
	// printf("\tPacket: %d buffered\n", packet->seqNum); //dd

	while (!isBufEmpty() && (pollRes=pollCall(10000)) != -1) {
		if (recvPacket(&curPacket, bufSize)) {
			lastFlag = processWaitingPacket(&curPacket, mySeq, bufSize, output);
			freePDU(&curPacket);
		}
	}
	if (pollRes == -1) {
		// timed out
		fprintf(stderr, "Server timed out\n");
		close(sockNum);
		fclose(output);
		freeWindow();
		exit(EXIT_FAILURE);
	}
	(*expectedSeq) = lastPacket() + 1;
	// printf("END SREJ TIME ~~~~~~~~~~~~~~~~~~~\n"); //dd
	return lastFlag;
}

void sendRR(uint32_t readySeq, uint32_t *mySeq, uint16_t bufSize) {
	// sends RR readySeq to server
	int serverAddrLen = sizeof(struct sockaddr_in6);
	int pduLen = 0;
	uint8_t pduBuf[bufSize + HEADER_LEN];

	readySeq = htonl(readySeq);
	pduLen = createPdu(pduBuf, *mySeq, RR_FLAG, (uint8_t*)(&readySeq), sizeof(uint32_t));
	sendtoErr(sockNum, pduBuf, pduLen, 0, (struct sockaddr *) &serverS, serverAddrLen);
	(*mySeq) = (*mySeq) + 1;
}

void sendSrejs(uint32_t expectedSeq, uint32_t recvSeq, uint32_t *mySeq) {
	// sends selective rejects for each missing packet and skips in buffer
	// TODO
	int pduLen = 0;
	int serverAddrLen = sizeof(struct sockaddr_in6);
	uint32_t i = 0;
	uint8_t pduBuf[HEADER_LEN + sizeof(uint32_t)];

	for (i=expectedSeq; i < recvSeq; i++) {
		// notify buffer of each missing seqNum
		// printf("\tSKIP: %d\n", i); //dd
		skip(i);
		i = htonl(i);
		pduLen = createPdu(pduBuf, *mySeq, SREJ_FLAG, (uint8_t*)&i, sizeof(uint32_t));
		sendtoErr(sockNum, pduBuf, pduLen, 0, (struct sockaddr *) &serverS, serverAddrLen);
		(*mySeq) = (*mySeq) + 1;
		i = ntohl(i);
	}
}

uint8_t processWaitingPacket(pdu packet, uint32_t *mySeq, uint16_t bufSize, FILE *output) {
	// Processes data packets while waiting on a SREJ packet
	// TODO
	int bufRet = 0;
	if (packet->flag != DATA_FLAG) {
		return packet->flag;
	}
	uint32_t expectedSeq = lastPacket() + 1;
	if (packet->seqNum > expectedSeq) {
		// seqNum is higher than expected
		sendSrejs(expectedSeq, packet->seqNum, mySeq);
	}

	if (packet->seqNum >= firstPacket()) {
		// seqNum is in expected range (or greater than expected and already SREJed) -> buffer it
		bufRet = bufferPacket(packet);
		// printf("\tPacket: %d buffered Ret: %d\n", packet->seqNum, bufRet); //dd
		if (bufRet == 0) {
			fprintf(stderr, "Buffer error\n");
			close(sockNum);
			fclose(output);
			freeWindow();
			exit(EXIT_FAILURE);
		}
		else if (bufRet == 2) {
			unbufferSrej(output, mySeq, bufSize);
		}
	}
	else {
		// seqNum is lower than expected -> send last RR and SREJ
		sendRR(firstPacket(), mySeq, bufSize);
		resendSrej(mySeq);
	}
	return DATA_FLAG;
}

void resendSrej(uint32_t *mySeq) {
	// resend the lowest SREJ packet
	uint32_t srejSeq = htonl(firstPacket());
	int serverAddrLen = sizeof(struct sockaddr_in6);
	int pduLen = 0;
	uint8_t pduBuf[HEADER_LEN + sizeof(uint32_t)];
	
	pduLen = createPdu(pduBuf, *mySeq, SREJ_FLAG, (uint8_t*)&srejSeq, sizeof(uint32_t));
	sendtoErr(sockNum, pduBuf, pduLen, 0, (struct sockaddr *) &serverS, serverAddrLen);
	(*mySeq) = (*mySeq) + 1;
}

void unbufferSrej(FILE *output, uint32_t *mySeq, uint32_t bufSize) {
	// printf("\tUNBUFFER TIME~~~~~~\n"); //dd
	// printWindowMeta(); //dd
	// printWindow(); //dd
	// unbuffers, writes, and frees each packet until end of buffer or next waiting on packet
	pdu packet = NULL;
	while((packet=unbuffer()) != NULL) {
		// printf("Buff Writing %u bytes from packet: %u", packet->payLen, packet->seqNum);
		writeData(packet->payload, packet->payLen, output);
		sendRR(packet->seqNum + 1, mySeq, bufSize);
		freePDU(packet);
		free(packet);
		// printWindowMeta(); //dd
		// printWindow(); //dd
	}
	// printf("\tEND UNBUFFER TIME~~~~~~\n"); //dd
}

int bufferPacket(pdu packet) {
	pdu toBuffer = NULL;
	uint8_t *payBuf = (uint8_t*)malloc(sizeof(uint8_t) * packet->payLen);

	toBuffer = (pdu)malloc(sizeof(struct PduS));
	memcpy(toBuffer, packet, sizeof(struct PduS));

	memcpy(payBuf, packet->payload, packet->payLen);
	toBuffer->payload = payBuf;
	return buffer(toBuffer);
}

void testConnection(struct sockaddr_in6 * server) {
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
	
		sendtoErr(sockNum, pduBuf, pduLen, 0, (struct sockaddr *) server, serverAddrLen);
		
		pduLen = safeRecvfrom(sockNum, pduBuf, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);
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





