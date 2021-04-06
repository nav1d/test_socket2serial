#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <WS2tcpip.h>
#include <Windows.h>
#pragma comment(lib,"ws2_32")

void initTCP4_sk(char *ip, int port);
void sendPacket2COM(void *newSocket);
void showError(char *str);
