
#include "Header.h"

char givenIP[32] = {};
int givenPORT = 0;
int givenCOM = 0;
int givenBitrate = 9600; // default bitrate for COM port
HANDLE hCOM = NULL; // COM hanndle

int main()
{
	char comPort_id[64] = { 0 };
	char tempVar[64] = { 0 };
	DCB dcbCOM = { 0 }; // COM port structure
	COMMTIMEOUTS timeouts = { 0 };
	BOOL status = 0;

	// asking COM port number:
	printf("\n Enter COM-PORT(only number) : ");
	scanf("%d", &givenCOM);

	// asking COM port bitrate number:
	printf("\n Enter COM-Bitrate(only number, default=9600) : ");
	scanf("%d", &givenBitrate);

	//default bitrate:
	if (givenBitrate == 0)
	{
		givenBitrate = 9600;
	}

	// making the COM port id format for WINDOWS to open it
	memset(comPort_id, 0, 64);
	memset(tempVar, 0, 64);

	itoa(givenCOM, tempVar, 10);

	strcpy(comPort_id, "\\\\.\\COM");
	strcat(comPort_id, tempVar);

	hCOM = CreateFileA(comPort_id, GENERIC_READ | GENERIC_WRITE,
		0,  // default
		NULL,  // default
		OPEN_EXISTING,  // Open existing 
		0,  // default
		NULL);  // default

	if (hCOM == INVALID_HANDLE_VALUE)
	{
		showError("Error - can not open COM by given COM-port");
		return -100;
	}
	else
	{
		showError("COM port is opened successfully.");
	}

	dcbCOM.DCBlength = sizeof(dcbCOM);
	status = GetCommState(hCOM, &dcbCOM); //retreives  the current settings
	if (status == FALSE)
	{
		showError("Error- can NOT get Com state");
		return -31;
	}
	
	dcbCOM.BaudRate = givenBitrate;      // default bit Rate = 9600
	dcbCOM.ByteSize = 8;             //ByteSize = 8
	dcbCOM.StopBits = ONESTOPBIT;    //StopBits = 1
	dcbCOM.Parity = NOPARITY;      //Parity = None
	status = SetCommState(hCOM, &dcbCOM);
	if (status == FALSE)
	{
		showError("Error- unknown error on setting com state");
		return -32;
	}

	//configure timeouts:
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 150;
	timeouts.ReadIntervalTimeout = 150;
	timeouts.ReadTotalTimeoutConstant = 150;
	timeouts.WriteTotalTimeoutMultiplier = 10;

	if (SetCommTimeouts(hCOM, &timeouts) == FALSE)
	{
		showError("Error- error on setting timeouts");
		return -33;
	}

	// asking ip & port:
	printf("\n Enter Net-IP to listen : ");
	scanf("%s", givenIP);

	printf("\n Enter Net-PORT to listen : ");
	scanf("%d", &givenPORT);

	initTCP4_sk(givenIP, givenPORT);

	return 0;
}

void showError(char *str)
{
	printf("\r\n %s \r\n", str);
}

void initTCP4_sk(char *ip, int port)
{
	// socket variables :
	struct sockaddr_in saddr;
	SOCKET so = 0, soa; // socket nums
	int sockType = 0;
	int retx = 0;
	int addr_size = 0;
	fd_set soall;
	WSADATA wsad;

	// socket version on WINDOWS:
	WSAStartup(MAKEWORD(2, 2), &wsad);

	// set socket type to TCP
	so = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// reseting socket
	FD_ZERO(&soall);

	// setting ip&port to listen on
	saddr.sin_family = AF_INET;
	saddr.sin_addr.S_un.S_addr = inet_addr(ip);
	saddr.sin_port = htons(port);
	memset(saddr.sin_zero, 0, 8);

	// prepare&bind socket on given ip&port:
	if (bind(so, (struct sockaddr*)&saddr, sizeof(sockaddr)))
	{
		showError("Error: can not bind to given ip/port.");
		return;
	}

	FD_SET(so, &soall);

	// listen on:
	if (listen(so, SOMAXCONN))
	{
		showError("Error: can not listen on.");
		return;
	}

	addr_size = sizeof(sockaddr);

	// infinite waiting to incoming socket
	while (1)
	{
		retx = select(so + 1, &soall, NULL, NULL, NULL);
		if (retx <= 0)
		{
			showError("Error: socket terminate");
			return;
		}

		soa = accept(so, (sockaddr*)&saddr, &addr_size); // new incoming socket accepted
		if (soa == -1)
		{
			showError("Error: connection failed during accepting due unknown error.");
			continue;
		}

		// create a thread for new connection
		HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)sendPacket2COM, &soa, 0, NULL);
		if (h == NULL)
		{
			showError("Error: unknown error");
		}
		else
		{
			CloseHandle(h);
		}

	}

	return;
}

void sendPacket2COM(void *newSocket)
{
	SOCKET socketNEW = *(SOCKET*)newSocket;
	char net_buffer[16384] = { 0 };
	char com_buffer[16384] = { 0 };
	int retx = 0, got = 0, wrote = 0, read = 0;
	fd_set sk_cfg;
	BOOL status = 0;
	DWORD dwEventFlag = 0; // flag for wait on COM port

	FD_ZERO(&sk_cfg);
	FD_SET(socketNEW, &sk_cfg);

	// infinite wait for handle packets
	while (1)
	{
		memset(net_buffer, 0, 16384);

		retx = select(socketNEW + 1, &sk_cfg, NULL, NULL, NULL);
		if (retx <= 0) //if socket closed or err
		{
			showError("Connection-Error: socket is terminated.");
			closesocket(socketNEW);
			return;
		}

		got = recv(socketNEW, net_buffer, 16384, 0);
		if (got <= 0)
		{
			showError("Connection-Error: socket is closed.");
			closesocket(socketNEW);
			return;
		}

		// add CR-LF characters if needed
		if ((net_buffer[got - 2] != '\r') || (net_buffer[got - 1] != '\n'))
		{
			strcat(net_buffer, "\r\n");
			got = got + 2;
		}

			status = WriteFile(hCOM, net_buffer, got, (LPDWORD)&wrote, 0); // write net_buffer to com_buffer
		if (status == FALSE)
		{
			showError("Connection-Error on WriteFile for serial port.");
			closesocket(socketNEW);
			return;
		}

		status = SetCommMask(hCOM, EV_RXCHAR);
		if (status == FALSE)
		{
			showError("Connection-Error on SetCommMask.");
			closesocket(socketNEW);
			return;
		}

		//Setting wait to receive from COM port:
		status = WaitCommEvent(hCOM, &dwEventFlag, NULL);
		if (status == FALSE)
		{
			showError("Connection-Error on WaitCommEvent.");
			closesocket(socketNEW);
			return;
		}

		// receive from COM port:
		status = ReadFile(hCOM, com_buffer, 16384, (LPDWORD)&read, 0);
		if (status == FALSE)
		{
			showError("Connection-Error on ReadFile for serial port.");
			closesocket(socketNEW);
			return;
		}

		got = send(socketNEW, com_buffer, read, 0);
		if (got == -1)
		{
			showError("Connection-Error: error on send, socket is closed.");
			closesocket(socketNEW);
			return;
		}

	}

	return;
}
