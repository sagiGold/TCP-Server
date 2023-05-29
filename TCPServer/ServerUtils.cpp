#include "ServerUtils.h"

bool addSocket(SOCKET id, SocketStatus what, SocketState* sockets, int& socketsCount)
{
	// Set the socket to be in non-blocking mode.
	unsigned long flag = 1;
	if (ioctlsocket(id, FIONBIO, &flag) != 0)
	{
		cout << "HTTP Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].DataLen = 0;
			sockets[i].LastActiveted = time(0);//reset responding time
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index, SocketState* sockets, int& socketsCount)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	sockets[index].LastActiveted = 0;
	socketsCount--;
	cout << "The socket in position: " << index << " removed." << endl;
}

void acceptConnection(int index, SocketState* sockets, int& socketsCount)
{
	SOCKET id = sockets[index].id;
	sockets[index].LastActiveted = time(0);//reset responding time
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "HTTP Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "HTTP Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	if (addSocket(msgSocket, RECEIVE, sockets, socketsCount) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index, SocketState* sockets, int& socketsCount)
{
	SOCKET msgSocket = sockets[index].id;
	int len = sockets[index].DataLen;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "HTTP Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index, sockets, socketsCount);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index, sockets, socketsCount);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "HTTP Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].DataLen += bytesRecv;

		if (sockets[index].DataLen > 0)
		{
			updateSendSubType(index, sockets);
		}
	}

}

void sendMessage(int index, SocketState* sockets)
{
	int bytesSent = 0, buffLen = 0, fileSize = 0, returnCode;
	size_t found;
	char sendBuff[BUFFER_SIZE], readBuff[BUFFER_SIZE], deleteBuff[BUFFER_SIZE];
	ifstream httpFile;
	string content, returnedMsg, fileSizeInString, address, innerAddress = "C:\\Temp\\HTML_FILES\\",
		errorAddress = "C:\\Temp\\HTML_FILES\\Error\\error.html", url, msgLang;
	time_t currentTime;
	time(&currentTime); // Get current time
	SOCKET msgSocket = sockets[index].id;
	sockets[index].LastActiveted = time(0);//reset responding time
	switch (sockets[index].sendSubType)
	{
	case TRACE:
		fileSize = strlen("TRACE ");
		fileSize += strlen(sockets[index].buffer);
		returnedMsg = "HTTP/1.1 200 OK \r\nContent-type: message/http\r\nDate: ";
		returnedMsg += ctime(&currentTime);
		returnedMsg += "Content-length: ";
		fileSizeInString = to_string(fileSize);
		returnedMsg += fileSizeInString;
		returnedMsg += "\r\n\r\n";
		returnedMsg += "TRACE ";
		returnedMsg += sockets[index].buffer;
		buffLen = returnedMsg.size();
		strcpy(sendBuff, returnedMsg.c_str());
		break;
	case DELETE_REQ:
		address = innerAddress;
		address += strtok(sockets[index].buffer, " ");//get the file name
		strcpy(deleteBuff, address.c_str());//move address to char* variable
		if (remove(deleteBuff) != 0)
		{
			returnedMsg = "HTTP/1.1 204 No Content \r\nDate: "; // We treat 204 code as a case where delete wasn't successful
		}
		else
		{
			returnedMsg = "HTTP/1.1 200 OK \r\nDate: "; // File deleted succesfully
		}

		returnedMsg += ctime(&currentTime);
		returnedMsg += "Content-length: ";
		fileSizeInString = to_string(fileSize);
		returnedMsg += fileSizeInString;
		returnedMsg += "\r\n\r\n";
		buffLen = returnedMsg.size();
		strcpy(sendBuff, returnedMsg.c_str());
		break;
	case PUT:
		char fileName[BUFFER_SIZE];
		returnCode = PutRequest(index, fileName, sockets);
		switch (returnCode)
		{
		case 0:
		{
			cout << "PUT " << fileName << "Failed";
			returnedMsg = "HTTP/1.1 412 Precondition failed \r\nDate: ";
			break;
		}

		case 200:
		{
			returnedMsg = "HTTP/1.1 200 OK \r\nDate: ";
			break;
		}

		case 201:
		{
			returnedMsg = "HTTP/1.1 201 Created \r\nDate: ";
			break;
		}

		case 204:
		{
			returnedMsg = "HTTP/1.1 204 No Content \r\nDate: ";
			break;
		}

		default:
		{
			returnedMsg = "HTTP/1.1 501 Not Implemented \r\nDate: ";
			break;
		}
		}

		returnedMsg += ctime(&currentTime);
		returnedMsg += "Content-length: ";
		fileSizeInString = to_string(fileSize);
		returnedMsg += fileSizeInString;
		returnedMsg += "\r\n\r\n";
		buffLen = returnedMsg.size();
		strcpy(sendBuff, returnedMsg.c_str());
		break;
	case POST:
		returnedMsg = "HTTP/1.1 200 OK \r\nDate:";
		returnedMsg += ctime(&currentTime);
		returnedMsg += "Content-length: 0\r\n\r\n";
		content = (string)sockets[index].buffer;
		found = content.find("\r\n\r\n");
		cout << "------------------------------------------------------------\n"
			<< "Message received!\n"
			<< "\n------------------------------------------------------------\n"
			<< &content[found + 4]
			<< "\n------------------------------------------------------------\n\n";
		buffLen = returnedMsg.size();
		strcpy(sendBuff, returnedMsg.c_str());
		break;
	case HEAD:
		address = innerAddress;
		msgLang = getLangFromMessage(index, sockets);
		if (msgLang == "Error")
			httpFile.open(errorAddress); //Error::No Language matched: there is no supporting language
		else
		{
			address.append(msgLang);
			address += "\\";
			url = strtok(sockets[index].buffer, " ");
			found = url.find("?");
			if (found != std::string::npos)//lang Query String exist
			{
				url = url.substr(0, found);
			}
			address.append(url);
			httpFile.open(address);
		}
		if (!httpFile)
			returnedMsg = "HTTP/1.1 404 Not Found ";
		else
		{
			returnedMsg = "HTTP/1.1 200 OK ";
			// Read content from file to string and get its length
			httpFile.seekg(0, ios::end);
			fileSize = httpFile.tellg(); // get length of content in file
		}
		httpFile.close();
		returnedMsg += "\r\nContent-type: text/html";
		returnedMsg += "\r\nDate: ";
		returnedMsg += ctime(&currentTime);
		returnedMsg += "Content-length: ";
		fileSizeInString = to_string(fileSize);
		returnedMsg += fileSizeInString;
		returnedMsg += "\r\n\r\n";
		buffLen = returnedMsg.size();
		strcpy(sendBuff, returnedMsg.c_str());
		break;
	case GET:
		address = innerAddress;
		msgLang = getLangFromMessage(index, sockets);
		if (msgLang == "Error")
		{
			httpFile.open(errorAddress); //Error::No Language matched: there is no supporting language
			returnedMsg = "HTTP/1.1 404 Not Found ";
		}
		else
		{
			address.append(msgLang);
			address += "\\";
			url = strtok(sockets[index].buffer, " ");
			found = url.find("?");
			if (found != std::string::npos)//lang Query String exist
			{
				url = url.substr(0, found);
			}
			address.append(url);
			httpFile.open(address);
			returnedMsg = "HTTP/1.1 200 OK ";
		}
		if (httpFile)
		{
			// Read content from file to string and get its length
			while (httpFile.getline(readBuff, BUFFER_SIZE))
				content += readBuff;
		}
		httpFile.close();
		returnedMsg += "\r\nContent-type: text/html";
		returnedMsg += "\r\nDate: ";
		returnedMsg += ctime(&currentTime);
		returnedMsg += "Content-length: ";
		fileSizeInString = to_string(content.length());
		returnedMsg += fileSizeInString;
		returnedMsg += "\r\n\r\n";
		returnedMsg += content; // Get content
		buffLen = returnedMsg.size();
		strcpy(sendBuff, returnedMsg.c_str());
		break;
	case OPTIONS:
		returnedMsg = "HTTP/1.1 204 No Content\r\nAllow: OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE\r\n";
		returnedMsg += "Content-length: 0\r\n\r\n";
		buffLen = returnedMsg.size();
		strcpy(sendBuff, returnedMsg.c_str());
		break;
	default:
		break;
	}
	bytesSent = send(msgSocket, sendBuff, buffLen, 0);
	//clean buffer: memset put NULL in every spot of the buffer 
	memset(sockets[index].buffer, 0, BUFFER_SIZE);
	//reset buffer size to 0
	sockets[index].DataLen = 0;
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "HTTP Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "HTTP Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";

	sockets[index].send = IDLE;
}

// A utility to check for the inactive sockets (if the response time is greater then 2 minutes) and remove them
void updateSocketsByResponseTime(SocketState* sockets, int& socketsCount)
{
	time_t currentTime;
	for (int i = 1; i < MAX_SOCKETS; i++)
	{
		currentTime = time(0);
		if ((currentTime - sockets[i].LastActiveted > 120) && (sockets[i].LastActiveted != 0))
		{
			removeSocket(i, sockets, socketsCount);
		}
	}
}

void updateSendSubType(int index, SocketState* sockets)
{
	int toSub;
	string buffer, firstWord;
	static map<string, HTTPRequest> request = { {"TRACE",HTTPRequest::TRACE},{"DELETE",HTTPRequest::DELETE_REQ},
												{"PUT",HTTPRequest::PUT},{"POST",HTTPRequest::POST},
												{"HEAD",HTTPRequest::HEAD},{"GET",HTTPRequest::GET},{"OPTIONS",HTTPRequest::OPTIONS} };
	//import buffer to string
	buffer = (string)sockets[index].buffer;
	//get the first word in buffer
	firstWord = buffer.substr(0, buffer.find(" "));
	//update request in buffer
	sockets[index].send = SEND;
	sockets[index].sendSubType = request[firstWord];
	//delete the method from buffer
	toSub = firstWord.length() + 2;//1 for space and 1 for '/'
	sockets[index].DataLen -= toSub;
	memcpy(sockets[index].buffer, &sockets[index].buffer[toSub], sockets[index].DataLen);
	sockets[index].buffer[sockets[index].DataLen] = '\0';
}

string getLangFromMessage(int index, SocketState* sockets)
{
	static map<string, string> request = { {"en","English"},{"en-AU","English"},{"en-BZ","English"},{"en-CA","English"},{"en-CB","English"},{"en-GB","English"},{"en-IE","English"},
										{"en-JM","English"},{"en-NZ","English"},{"en-PH","English"},{"en-TT","English"},{"en-US","English"},{"en-ZA","English"},{"en-ZW","English"},
										{"fr","French"},{"fr-BE","French"},{"fr-CA","French"},{"fr-CH","French"},{"fr-FR","French"},{"fr-LU","French"},{"fr-MC","French"},
										{"he","Hebrew"},{"he-IL","Hebrew"} };
	string buffer, lookWord, langWord, bufferAtLangWord;
	size_t found;
	//import buffer to string
	buffer = (string)sockets[index].buffer;
	lookWord = "?lang=";
	found = buffer.find(lookWord);
	if (found == std::string::npos)//lang Query String doesn't exist
		return "English";//defult language
	//else get the Language word in buffer
	bufferAtLangWord = &buffer[found + lookWord.length()];
	langWord = bufferAtLangWord.substr(0, bufferAtLangWord.find(" "));
	auto search = request.find(langWord);
	if (search != request.end()) {
		return request[langWord];
	}
	else {
		return "Error";//Error::No Language matched: there is no supporting language
	}
}

int PutRequest(int index, char* filename, SocketState* sockets)
{
	string content, buffer = (string)sockets[index].buffer, address = "C:\\Temp\\HTML_FILES\\";
	int buffLen = 0;
	int retCode = 200; // 'OK' code
	size_t found;
	filename = strtok(sockets[index].buffer, " ");
	address += filename;

	fstream outPutFile;
	outPutFile.open(address);

	if (!outPutFile.good())
	{
		outPutFile.open(address, ios::out);
		retCode = 201; // New file created
	}

	if (!outPutFile.good())
	{
		cout << "HTTP Server: Error writing file to local storage: " << WSAGetLastError() << endl;
		return 0; // Error opening file
	}
	found = buffer.find("\r\n\r\n");
	content = &buffer[found + 4];
	if (content.length() == 0)
		retCode = 204; // No content
	else
		outPutFile << content;

	outPutFile.close();
	return retCode;
}