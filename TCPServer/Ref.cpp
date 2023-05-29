#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS 
#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <filesystem>
#include <winhttp.h>
#include <sys/stat.h>
struct SocketState
{
	SOCKET id;
	int	recv;
	int	send;
	int sendSubType;
	char buffer[128];
	char sendBuff[255];
	int len;
};

enum Methods {
	TRACE,
	MDELETE,
	PUT,
	POST,
	HEAD,
	GET,
	OPTIONS
};
Methods hashit(string const& method)
{
	if (method == "TRACE")return TRACE;
	if (method == "DELETE")return MDELETE;
	if (method == "PUT")return PUT;
	if (method == "POST")return POST;
	if (method == "HEAD")return HEAD;
	if (method == "GET")return GET;
	if (method == "OPTIONS")return OPTIONS;
}
const int WEB_PORT = 8080;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;
const int PAGE = 10;
const int FAIL = 20;
const int TEXT = 30;
const int HEADERS = 40;
const int MESG = 50;
const int OPT = 60;

bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);

struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;
string requestMethod, bufferString, queryString;
void main()
{
	WSAData wsaData;
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Web Server: Error at WSAStartup()\n";
		return;
	}
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Web Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}
	sockaddr_in serverService;
	serverService.sin_family = AF_INET;
	serverService.sin_addr.s_addr = INADDR_ANY;
	serverService.sin_port = htons(WEB_PORT);
	if (SOCKET_ERROR == (int)bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Web Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Web Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);
	while (true)
	{
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE)) && (sockets[i].send != SEND))
				FD_SET(sockets[i].id, &waitRecv);
		}
		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}
		struct timeval tv;
		tv.tv_sec = 120;
		tv.tv_usec = 0;
		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, &tv);
		if (nfd == 0)
			goto CLOSE;
		if (nfd == SOCKET_ERROR)
		{
			cout << "Web Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}
		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}
		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					sendMessage(i);
					break;
				}
			}
		}
	}
CLOSE:cout << "Web Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what)
{
	// Set the socket to be in non-blocking mode.
	unsigned long flag = 1;
	if (ioctlsocket(id, FIONBIO, &flag) != 0)
	{
		cout << "Web Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void acceptConnection(int index)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Web Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Web Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Web Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0';
		cout << "Web Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;

		if (sockets[index].len > 0)
		{
			bufferString = sockets[index].buffer;
			requestMethod = bufferString.substr(0, bufferString.find(' '));
			bufferString = bufferString.substr(bufferString.find(' ') + 1, string::npos);
			queryString = bufferString.substr(0, bufferString.find(' '));

			string fileName, fileSuffix, param, new_content;

			switch (hashit(requestMethod))
			{
			case TRACE:
			{
				try {
					string answer;
					answer.append(requestMethod);
					answer.append(" ");
					answer.append(bufferString);
					strcpy(sockets[index].sendBuff, answer.c_str());
					sockets[index].send = SEND;
					sockets[index].sendSubType = MESG;
				}
				catch (...) {
					strcpy(sockets[index].sendBuff, "unable to receive data from server");
					sockets[index].send = SEND;
					sockets[index].sendSubType = FAIL;
				}
				break;
			}
			case MDELETE:
			{
				try
				{
					fileName = queryString.substr(1, queryString.find('.') - 1);
					fileSuffix = queryString.substr(queryString.find('.'), string::npos);
					if ((fileSuffix != ".html" && fileSuffix != ".txt") || fileName.empty())
						throw new exception;

					string new_file;
					if (fileSuffix == ".txt")
						new_file.append("C:\\temp\\txt\\");
					if (fileSuffix == ".html")
						new_file.append("C:\\temp\\html\\");
					new_file.append(fileName.append(fileSuffix));
					if (std::remove(new_file.c_str()) == 0)//successful delete
					{
						strcpy(sockets[index].sendBuff, "File deleted successfully.");
						sockets[index].send = SEND;
						sockets[index].sendSubType = TEXT;
					}
					else
					{
						throw new exception;
					}

				}
				catch (...) {
					strcpy(sockets[index].sendBuff, "unable to delete file.");
					sockets[index].send = SEND;
					sockets[index].sendSubType = FAIL;
				}
				break;
			}
			case PUT:
			{
				try
				{
					fileName = queryString.substr(1, queryString.find('.') - 1);
					fileSuffix = queryString.substr(queryString.find('.'), queryString.find_last_of('/') - queryString.find('.'));
					new_content = queryString.substr(queryString.find_last_of('/'), string::npos);
					new_content.erase(0, 1);
					if ((fileSuffix != ".html" && fileSuffix != ".txt") || fileName.empty())
						throw new exception;
					string new_file;
					if (fileSuffix == ".txt")
						new_file.append("C:\\temp\\txt\\");
					if (fileSuffix == ".html")
						new_file.append("C:\\temp\\html\\");
					new_file.append(fileName.append(fileSuffix));
					struct stat buf;
					if (stat(new_file.c_str(), &buf) != -1)//file already exists
					{
						strcpy(sockets[index].sendBuff, "Successfully updated file.");
					}
					else//file doesn't exist
					{
						strcpy(sockets[index].sendBuff, "New source was created and updated on the server.");
					}
					ofstream myFile;
					myFile.open(new_file.c_str(), ios::out | ios::trunc);
					myFile << new_content;
					myFile.close();

					sockets[index].send = SEND;
					sockets[index].sendSubType = TEXT;

				}
				catch (...)
				{
					strcpy(sockets[index].sendBuff, "invalid query string");
					sockets[index].send = SEND;
					sockets[index].sendSubType = FAIL;
				}
				break;
			}
			case POST:
			{
				try {
					cout << endl << queryString << endl;
					strcpy(sockets[index].sendBuff, "Successfuly posted string to server's console.");
					sockets[index].send = SEND;
					sockets[index].sendSubType = TEXT;
				}
				catch (...) {
					strcpy(sockets[index].sendBuff, "Unable to print string to server's console.");
					sockets[index].send = SEND;
					sockets[index].sendSubType = FAIL;
				}
				break;
			}
			case GET:
			case HEAD:
			{
				try
				{
					fileName = queryString.substr(1, queryString.find('.') - 1);
					fileSuffix = queryString.substr(8, queryString.find('?') - queryString.find('.') - 1);
					param = queryString.substr(queryString.find('?') + 1, string::npos);
					if (fileName != "myFile" || (fileSuffix != "html" && fileSuffix != "txt") || (param != "lang=he" && param != "lang=fr" && param != "lang=en"))
					{
						throw new exception;
					}
					FILE* myFile = fopen("C:\\temp\\html\\en.html", "r");//open english html version as default
					strcpy(sockets[index].sendBuff, "C:\\temp\\html\\en.html");
					if (fileSuffix == "html")
					{
						if (param == "lang=he")
						{
							myFile = fopen("C:\\temp\\html\\he.html", "r");
							strcpy(sockets[index].sendBuff, "C:\\temp\\html\\he.html");
						}
						else if (param == "lang=fr")
						{
							myFile = fopen("C:\\temp\\html\\fr.html", "r");
							strcpy(sockets[index].sendBuff, "C:\\temp\\html\\fr.html");
						}
					}
					else if (fileSuffix == "txt")
					{
						if (param == "lang=he")
						{
							myFile = fopen("C:\\temp\\txt\\he.txt", "r");
							strcpy(sockets[index].sendBuff, "C:\\temp\\txt\\he.txt");
						}
						else if (param == "lang=en") {
							myFile = fopen("C:\\temp\\txt\\en.txt", "r");
							strcpy(sockets[index].sendBuff, "C:\\temp\\txt\\en.txt");
						}
						else if (param == "lang=fr") {
							myFile = fopen("C:\\temp\\txt\\fr.txt", "r");
							strcpy(sockets[index].sendBuff, "C:\\temp\\txt\\fr.txt");
						}
					}
					sockets[index].send = SEND;
					if (hashit(requestMethod) == GET)
						sockets[index].sendSubType = PAGE;
					else if (hashit(requestMethod) == HEAD)
						sockets[index].sendSubType = HEADERS;
					fclose(myFile);
				}
				catch (...) {
					strcpy(sockets[index].sendBuff, "invalid query string");
					sockets[index].send = SEND;
					sockets[index].sendSubType = FAIL;
				}

				break;
			}
			case OPTIONS:
			{
				try {
					size_t d_pos, s_pos;
					string options_str;
					bool file_exists = false;
					queryString.erase(0, 1);
					if (queryString == "myFile.txt?lang=fr" || queryString == "myFile.txt?lang=en" || queryString == "myFile.txt?lang=he"
						|| queryString == "myFile.html?lang=fr" || queryString == "myFile.html?lang=en" || queryString == "myFile.html?lang=he")
					{
						options_str = "OPTIONS, GET, HEAD, POST, TRACE";
					}
					else if (queryString == "*")
					{
						options_str = "OPTIONS, GET, HEAD, POST, TRACE, DELETE, PUT";
					}
					else
					{
						d_pos = queryString.find('.');
						s_pos = queryString.find('/');
						if (d_pos != string::npos)
						{
							fileName = queryString.substr(0, queryString.find('.'));
							if (s_pos != string::npos)
							{
								fileSuffix = queryString.substr(queryString.find('.'), queryString.find('/') - queryString.find('.'));
							}
							else
							{
								fileSuffix = queryString.substr(queryString.find('.'), string::npos);
								string new_file;
								if (fileSuffix == ".txt")
									new_file.append("C:\\temp\\txt\\");
								if (fileSuffix == ".html")
									new_file.append("C:\\temp\\html\\");
								new_file.append(fileName.append(fileSuffix));
								struct stat buf;
								if (stat(new_file.c_str(), &buf) != -1)//file already exists
									file_exists = true;

							}
							if ((fileSuffix != ".html" && fileSuffix != ".txt") || fileName.empty())
								goto STR;
							else
							{
								options_str = "POST, TRACE, OPTIONS, PUT";
								if (file_exists == true)
								{
									options_str.append(", DELETE");
								}

							}

						}
						else
						{
						STR:options_str = "POST, TRACE, OPTIONS";
						}
					}
					strcpy(sockets[index].sendBuff, options_str.c_str());
					sockets[index].send = SEND;
					sockets[index].sendSubType = OPT;
				}
				catch (...)
				{
					strcpy(sockets[index].sendBuff, "unable to receive information.");
					sockets[index].send = SEND;
					sockets[index].sendSubType = FAIL;
				}
				break;
			}
			}
		}
	}

}

void sendMessage(int index)
{
	int bytesSent = 0;
	char sendBuff[255];

	SOCKET msgSocket = sockets[index].id;
	std::stringstream make_content;
	std::string finished_content;
	std::stringstream make_response;

	// Create the HTTP Response
	if (sockets[index].sendSubType == FAIL)//error
	{
		make_response << "HTTP/1.1 400 Bad Request\r\n";
		make_response << "Cache-Control: no-cache, private\r\n";
		make_content << sockets[index].sendBuff;
		make_response << "Content-Type: text/plain\r\n";
	}
	else // successful request action
	{
		if (sockets[index].sendBuff == "New source was created and updated on the server.")
			make_response << "HTTP/1.1 201 OK\r\n";
		else
			make_response << "HTTP/1.1 200 OK\r\n";
		make_response << "Cache-Control: no-cache, private\r\n";
		if (sockets[index].sendSubType == PAGE || sockets[index].sendSubType == HEADERS)//get head
		{
			std::fstream grab_content(sockets[index].sendBuff);
			make_content << grab_content.rdbuf();
			make_response << "Content-Type: text/html\r\n";
		}
		else if (sockets[index].sendSubType == TEXT)//post
		{
			make_content << sockets[index].sendBuff;
			make_response << "Content-Type: text/plain\r\n";
		}
		else if (sockets[index].sendSubType == MESG)//trace
		{
			make_content << sockets[index].sendBuff;
			make_response << "Content-Type: message/http\r\n";
			make_response << "Via: 1.1 webServer\r\n";
		}
		else if (sockets[index].sendSubType == OPT)
		{
			make_response << "Allow: " << sockets[index].sendBuff << "\r\n";
		}

	}

	finished_content = make_content.str();
	make_response << "Content-Length: " << finished_content.length() << "\r\n";
	make_response << "\r\n";
	if (sockets[index].sendSubType != HEADERS)
		make_response << finished_content;// skip if HEAD
		// Transfer it to a std::string to send
	std::string finished_response = make_response.str();

	bytesSent = send(msgSocket, finished_response.c_str(), finished_response.length(), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Web Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "Web Server: Sent: " << bytesSent << "\\" << finished_response.length() << " bytes of \"" << finished_response.c_str() << "\" message.\n";

	sockets[index].send = IDLE;

}
