#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "Ws2_32.lib")
#include <iostream>
#include <winsock2.h>
#include <fstream>
#include <string.h>
#include <string>
#include <time.h>
#include <map>
using namespace std;
using std::map;

constexpr int HTTP_PORT = 27015;
constexpr int MAX_SOCKETS = 60;
constexpr int BUFFER_SIZE = 2048;

enum SocketStatus { EMPTY, LISTEN, RECEIVE, IDLE, SEND };
enum HTTPRequest { TRACE, DELETE_REQ, PUT, POST, HEAD, GET, OPTIONS };


struct SocketState
{
	SOCKET id;
	SocketStatus recv;
	SocketStatus send;
	HTTPRequest sendSubType;
	time_t LastActiveted;
	char buffer[BUFFER_SIZE];
	int DataLen;
};

bool addSocket(SOCKET id, SocketStatus what, SocketState* sockets, int& socketsCount);
void removeSocket(int index, SocketState* sockets, int& socketsCount);
void acceptConnection(int index, SocketState* sockets, int& socketsCount);
void receiveMessage(int index, SocketState* sockets, int& socketsCount);
void sendMessage(int index, SocketState* sockets);
void updateSocketsByResponseTime(SocketState* sockets, int& socketsCount);
void updateSendSubType(int index, SocketState* sockets);
string getLangFromMessage(int index, SocketState* sockets);
int PutRequest(int index, char* filename, SocketState* sockets);