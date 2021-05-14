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
#include "pdu.h"

#define MAXBUF 1400
#define MAX_PDU_LEN 1407
#define DEBUG_FLAG 10

void talkToServer(int socketNum, struct sockaddr_in6 * server);
int readFromStdin(char * buffer);
int checkArgs(int argc, char * argv[], float *errRate);


int main (int argc, char *argv[])
 {
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;
	float errRate = 0;
	
	portNumber = checkArgs(argc, argv, &errRate);
	sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);
	
	socketNum = setupUdpClientToServer(&server, argv[2], portNumber);
	
	talkToServer(socketNum, &server);
	
	close(socketNum);

	return 0;
}


void talkToServer(int socketNum, struct sockaddr_in6 * server)
{
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

int checkArgs(int argc, char * argv[], float *errRate)
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s error-rate host-name port-number \n", argv[0]);
		exit(1);
	}
	
	// Checks args and returns port number
	int portNumber = 0;

	// if (argc != 3)
	// {
	// 	fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
	// 	exit(-1);
	// }
	
	portNumber = atoi(argv[3]);
	*errRate = atof(argv[1]);
		
	return portNumber;
}





