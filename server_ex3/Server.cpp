#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <fstream>
#include <sstream>

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[1000];
	int len;
};

struct Request
{
	string method;
	string queryParameter;
	string path;
	string body;
};


struct Response
{
	string codeStatus;
	string  messageStatus;
	vector<string> headers;
	string body;
};

const int TIME_PORT = 27015;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;
const int GET = 1;
const int PUT = 2;
const int _DELETE = 3;
const int OPTIONS = 4;
const int HEAD = 5;
const int TRACE = 6;
const int POST = 7;



bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);
struct Request parseRequest(string httpRequest);
string getMethod(string httpRequest);
string getPath(string httpRequest);
string getParameter(string httpRequest);
string getBody(string httpRequest);
string getFullPath(Request httpRequest);
Response getGETResponse(Request request);
Response getPOSTResponse(Request request);
Response getDELETEResponse(Request request);
string createResponse(Response response);
Response getPutResponse(Request request);
Response getOptionsResponse(Request request);
void resetRequestsBuffer(int index);
Response getHeadResponse(Request request);
Response getTraceResponse(string request);

struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;


void main()
{

	// Initialize Winsock (Windows Sockets).

	// Create a WSADATA object called wsaData.
	// The WSADATA structure contains information about the Windows 
	// Sockets implementation.
	WSAData wsaData;

	// Call WSAStartup and return its value as an integer and check for errors.
	// The WSAStartup function initiates the use of WS2_32.DLL by a process.
	// First parameter is the version number 2.2.
	// The WSACleanup function destructs the use of WS2_32.DLL by a process.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Server: Error at WSAStartup()\n";
		return;
	}

	// Server side:
	// Create and bind a socket to an internet address.
	// Listen through the socket for incoming connections.

	// After initialization, a SOCKET object is ready to be instantiated.

	// Create a SOCKET object called listenSocket. 
	// For this application:	use the Internet address family (AF_INET), 
	//							streaming sockets (SOCK_STREAM), 
	//							and the TCP/IP protocol (IPPROTO_TCP).
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	// Error detection is a key part of successful networking code. 
	// If the socket call fails, it returns INVALID_SOCKET. 
	// The if statement in the previous code is used to catch any errors that
	// may have occurred while creating the socket. WSAGetLastError returns an 
	// error number associated with the last error that occurred.
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	// For a server to communicate on a network, it must bind the socket to 
	// a network address.

	// Need to assemble the required data for connection in sockaddr structure.

	// Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	// Address family (must be AF_INET - Internet address family).
	serverService.sin_family = AF_INET;
	// IP address. The sin_addr is a union (s_addr is a unsigned long 
	// (4 bytes) data type).
	// inet_addr (Iternet address) is used to convert a string (char *) 
	// into unsigned long.
	// The IP address is INADDR_ANY to accept connections on all interfaces.
	serverService.sin_addr.s_addr = INADDR_ANY;
	// IP Port. The htons (host to network - short) function converts an
	// unsigned short from host to TCP/IP network byte order 
	// (which is big-endian).
	serverService.sin_port = htons(TIME_PORT);

	// Bind the socket for client's requests.

	// The bind function establishes a connection to a specified socket.
	// The function uses the socket handler, the sockaddr structure (which
	// defines properties of the desired connection) and the length of the
	// sockaddr structure (in bytes).
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	// Listen on the Socket for incoming connections.
	// This socket accepts only one connection (no pending connections 
	// from other clients). This sets the backlog parameter.
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);

	// Accept connections and handles them one by one.
	while (true)
	{
		// The select function determines the status of one or more sockets,
		// waiting if necessary, to perform asynchronous I/O. Use fd_sets for
		// sets of handles for reading, writing and exceptions. select gets "timeout" for waiting
		// and still performing other operations (Use NULL for blocking). Finally,
		// select returns the number of descriptors which are ready for use (use FD_ISSET
		// macro to check which descriptor in each set is ready to be used).
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		//
		// Wait for interesting event.
		// Note: First argument is ignored. The fourth is for exceptions.
		// And as written above the last is a timeout, hence we are blocked if nothing happens.
		//
		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Server: Error at select(): " << WSAGetLastError() << endl;
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

	// Closing connections and Winsock.
	cout << "Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what)
{
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
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	//
	// Set the socket to be in non-blocking mode.
	//
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

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
	string method;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Server: Error at recv(): " << WSAGetLastError() << endl;
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
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		sockets[index].len += bytesRecv;

		if (sockets[index].len > 0)
		{
			method = getMethod(sockets[index].buffer);
			Request request = parseRequest(sockets[index].buffer);

			if (method == "GET")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = GET;
				return;
			}
			else if (method == "POST")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = POST;
				return;
			}
			else if (method == "PUT")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = PUT;
				return;
			}
			else if (method == "TRACE")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = TRACE;
				return;
			}
			else if (method == "OPTIONS")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = OPTIONS;
				return;
			}
			else if (method == "DELETE")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = _DELETE;
				return;
			}
			else if (method == "HEAD")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = HEAD;
				return;
			}
			else if (strncmp(sockets[index].buffer, "Exit", 4) == 0)
			{
				closesocket(msgSocket);
				removeSocket(index);
				return;
			}
		}
	}

}

void sendMessage(int index)
{
	int bytesSent = 0;
	char sendBuff[1000];

	SOCKET msgSocket = sockets[index].id;
	Request request = parseRequest(sockets[index].buffer);
	Response response;

	if (sockets[index].sendSubType == GET) {
		response=getGETResponse(request);
	}
	else if (sockets[index].sendSubType == PUT) {
		response = getPutResponse(request);
	}
	else if (sockets[index].sendSubType == POST) {
		response = getPOSTResponse(request);
	}
	else if (sockets[index].sendSubType == _DELETE) {
		response = getDELETEResponse(request);
	}
	else if (sockets[index].sendSubType == HEAD) {
		response = getHeadResponse(request);

	}
	else if (sockets[index].sendSubType == OPTIONS) {
		response = getOptionsResponse(request);

	}
	else if (sockets[index].sendSubType == TRACE) {
		string buffer(sockets[index].buffer);
		response = getTraceResponse(buffer);

	}


	strcpy(sendBuff,createResponse(response).c_str());

	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	sockets[index].send = IDLE;
	resetRequestsBuffer(index);
}



struct Request parseRequest(string httpRequest)
{
	struct Request request;

	request.method = getMethod(httpRequest);
	request.path = getPath(httpRequest);
	request.queryParameter = getParameter(httpRequest);
	request.body = getBody(httpRequest);

	return request;
}


string getMethod(string httpRequest) 
{
	int firstSpace = httpRequest.find(' ');
	return httpRequest.substr(0, firstSpace);
}

string getPath(string httpRequest) 
{
	int start = httpRequest.find('/');
	int end=httpRequest.find('?');

	if (end == -1)
		end = httpRequest.find(' ', start);

	return httpRequest.substr(start+1, end-start-1);
}


string getParameter(string httpRequest) 
{
	int start = httpRequest.find('?')+1;
	if (start == 0)
		return "";

	int end = httpRequest.find(' ',start);
	return httpRequest.substr(start, end-start);
}


string getBody(string httpRequest)
{
	int pos = httpRequest.find("\r\n\r\n");

	if (pos != -1) {
		return httpRequest.substr(pos + 4);
	}

	return "";
}

string getFullPath(Request httpRequest) {
	string fullPath;

	string startPath = "C:/temp";
	if (httpRequest.queryParameter != "") {
		int index = httpRequest.queryParameter.find("=");
		if (index != -1) {
			string queryKey = httpRequest.queryParameter.substr(0, index);
			string queryValue = httpRequest.queryParameter.substr(index + 1);

			if (queryKey == "lang" && (queryValue == "en" || queryValue == "fr" || queryValue == "he"))
				fullPath = startPath + '/' + queryValue +'/'+ httpRequest.path;
			else
				fullPath = "400";

		}
		else
			fullPath = "400";
	}
	else
		fullPath = startPath+'/' + httpRequest.path;

	if (httpRequest.path == "")
		fullPath = "400";

	return fullPath;
}


Response getGETResponse(Request request)
{
	int fileSize, bytesRead;
	Response response;
	FILE* file;
	string filePath = getFullPath(request);

	response.headers.push_back("Content-Type: text/html");
	response.headers.push_back("Content-Length: 0");

	if (filePath == "400")
	{
		response.codeStatus = "400";
		response.messageStatus = "Bad Request";
		return response;
	}
	file = fopen(filePath.c_str(), "rb");
	
	if (file == NULL) {
		response.codeStatus = "404";
		response.messageStatus = "Not Found";
		return response;
	}

	fseek(file, 0, SEEK_END);
	fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* buffer = (char*)malloc(fileSize + 1); 
	if (buffer == NULL) {
		fclose(file);
		response.codeStatus = "500";
		response.messageStatus = "Internal Server Error";
		return response;
	}

	bytesRead = fread(buffer, 1, fileSize, file);
	if (bytesRead != fileSize) {
		fclose(file);
		free(buffer);
		response.codeStatus = "500";
		response.messageStatus = "Internal Server Error";
		return response;
	}



	buffer[fileSize] = '\0';
	fclose(file);


	response.body = buffer;
	response.codeStatus = "200";
	response.messageStatus = "OK";
	sprintf(buffer, "Content-Length: %d", fileSize);
	response.headers[1] = buffer;

	free(buffer);
	return response;
}


Response getPOSTResponse(Request request)
{
	Response response;
	if (request.body != "")
	{
		cout << "\n" << request.body << "\n";
		response.codeStatus = "200";
		response.messageStatus = "OK";
		response.headers.push_back("Content-Type: text/html");
		response.headers.push_back("Content-Length: 0");
	}
	else 
	{
		response.codeStatus = "204";
		response.messageStatus = "No Content";
	}
	
	return response;
}

Response getPutResponse(Request request) {

	string fullPath = getFullPath(request);
	Response response;
	response.codeStatus = "200";
	response.messageStatus = "OK";
	response.body = "";
	response.headers.push_back("Content-Type: text/html");
	response.headers.push_back("Content-Length: 0");

	if (fullPath == "400") {
		response.codeStatus = "400";
		response.messageStatus = "Bad Request";
		return response;
	}

	if (request.body == "") {
		response.codeStatus = "204";
		response.messageStatus = "No Content";
	}

	ifstream checkFile;
	checkFile.open(fullPath);
	if (!checkFile) {
		response.codeStatus = "201";
		response.messageStatus = "Created";
	}
	else 
		checkFile.close();



	ofstream file(fullPath, std::ios::out);

	if (!file.is_open()) {
		response.codeStatus = "500";
		response.messageStatus = "Internal Server Error";
		return response;
	}

	
	file << request.body;
	file.close();
	

	return response;

}


Response getDELETEResponse(Request request)
{
	string fullPath = getFullPath(request);
	Response response;
	response.codeStatus = "200";
	response.messageStatus = "OK";
	response.body = "";
	response.headers.push_back("Content-Type: text/html");
	response.headers.push_back("Content-Length: 0");


	if (fullPath == "400") {
		response.codeStatus = "400";
		response.messageStatus = "Bad Request";
		return response;
	}

	ifstream checkFile;
	checkFile.open(fullPath);
	if (!checkFile) {
		response.codeStatus = "404";
		response.messageStatus = "Not Found";
		return response;
	}
	else
		checkFile.close();

	if (remove(fullPath.c_str()) != 0)
	{
		response.codeStatus = "500";
		response.messageStatus = "Internal Server Error";
	}
	return response;
}



Response getOptionsResponse(Request request)
{
	Response response;
	response.codeStatus = "200";
	response.messageStatus = "OK";
	response.body = "";
	response.headers.push_back("Content-Type: text/html");
	response.headers.push_back("Content-Length: 0");

	if (request.path != "*") {

		string fullPath = getFullPath(request);

		if (fullPath == "400") {
			response.codeStatus = "400";
			response.messageStatus = "Bad Request";
			return response;
		}

		ifstream checkFile;
		checkFile.open(fullPath);
		if (!checkFile) {
			response.codeStatus = "404";
			response.messageStatus = "Not Found";
			return response;
		}
		else
			checkFile.close();
	}

	response.headers.push_back("Allow: OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE\r\n");
	return response;
}


string createResponse(Response response) {
	string httpResponse = "HTTP/1.1 ";
	httpResponse +=response.codeStatus;
	httpResponse +=" ";
	httpResponse += response.messageStatus;
	httpResponse += "\r\n";

	for (int i = 0; i < response.headers.size(); i++) {
		httpResponse += response.headers[i];
		httpResponse += "\r\n";
	}
	httpResponse += "\r\n";
	httpResponse += response.body;
	return httpResponse;
}


Response getHeadResponse(Request request)
{
	Response response = getGETResponse(request);
	response.body = "";
	return response;
}

Response getTraceResponse(string request) {
	Response response;
	response.codeStatus = "200";
	response.messageStatus = "OK";
	response.body = request;
	response.headers.push_back("Content-Type: message/http");
	ostringstream oss;
	oss << "Content-Length: " << request.length();
	response.headers.push_back(oss.str());

	return response;
}

void resetRequestsBuffer(int index) {
	memset(sockets[index].buffer, '\0', sizeof(sockets[index].buffer));
	sockets[index].len = 0;
}

