// Angela Kerlin
// Server side - UDP Code
// Orignal code from Hugh Smith	4/1/2017

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cpe464.h"
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "pdu.h"

#define MAXBUF 1400
#define MAX_PDU_LEN 1407
#define DEBUG_FLAG 10

void processClient(int socketNum);
int checkArgs(int argc, char *argv[], float *errRate);

int main ( int argc, char *argv[]  )
{ 
	int socketNum = 0;				
	int portNumber = 0;
	float errRate = 0;

	portNumber = checkArgs(argc, argv, &errRate);
		
	socketNum = udpServerSetup(portNumber);
	sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

	processClient(socketNum);

	close(socketNum);
	
	return 0;
}

void processClient(int socketNum)
{
	int dataLen = 0; 
	char buffer[MAXBUF + 1];	  
	struct sockaddr_in6 client;		
	int clientAddrLen = sizeof(client);	
	int pduLen = 0; 
	uint8_t pduBuf[MAX_PDU_LEN];
	uint16_t seqNum = 0;

	
	buffer[0] = '\0';
	while (buffer[0] != '.')
	{
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

int checkArgs(int argc, char *argv[], float *errRate)
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 3 || argc == 1)
	{
		fprintf(stderr, "Usage %s [error rate] [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}

	*errRate = atof(argv[1]);
	
	return portNumber;
}


