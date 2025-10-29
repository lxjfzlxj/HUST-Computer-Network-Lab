#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <fstream>
namespace fs = std::filesystem;

#pragma comment(lib, "ws2_32.lib")

struct HttpRequest {
	std::string firstLine;
	std::string method;
	std::string path;
};

class Client {
private:
	SOCKET socket;
	sockaddr_in addr;
public:
	Client() {}
	Client(SOCKET socket, sockaddr_in addr) : socket(socket), addr(addr) {}
	SOCKET* getSocket() {
		return &socket;
	}
	sockaddr_in* getAddr() {
		return &addr;
	}
};

HttpRequest parseRawRequest(std::string rawdata) {
	HttpRequest req;
	std::istringstream stream(rawdata);
	std::getline(stream, req.firstLine);
	std::istringstream lineStream(req.firstLine);
	lineStream >> req.method >> req.path;
	req.path.erase(0, 1);
	return req;
}

std::string readFile(fs::path path) {
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("The resource " + path.string() + " is not found");
	}
	std::ostringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

std::string constructResponse(int returnCode, std::string contentType, std::string content) {
	std::string resp = "HTTP/1.1 ";
	switch (returnCode) {
	case 200:
		resp += "200 OK\r\n";
		break;
	case 404:
		resp += "404 Not Found\r\n";
		content = "<h1>404 Not Found</h1>";
		break;
	case 405:
		resp += "405 Method Not Allowed\r\n";
		content = "Method Not Allowed";
	default:
		throw std::runtime_error("invalid return code");
	}
	resp += "Content-Type: " + contentType + "\r\n";
	if (returnCode == 200) {
		resp += "Content-Length: " + std::to_string(content.size()) + "\r\n";
	}
	resp += "Connection: close\r\n\r\n";
	resp += content;
	return resp;
}

std::string getContentType(HttpRequest &req) {
	if (req.path.ends_with(".html")) return "text/html";
	if (req.path.ends_with(".css")) return "text/css";
	if (req.path.ends_with(".js")) return "application/javascript";
	if (req.path.ends_with(".png")) return "image/png";
	if (req.path.ends_with(".jpg") || req.path.ends_with(".jpeg")) return "image/jpeg";
	if (req.path.ends_with(".gif")) return "image/gif";
	if (req.path.ends_with(".txt")) return "text/plain";
	return "application/octet-stream";
}

int main() {
	WSADATA wsaData;
	fd_set rfds;				//���ڼ��socket�Ƿ������ݵ����ĵ��ļ�������������socket������ģʽ�µȴ������¼�֪ͨ�������ݵ�����
	fd_set wfds;				//���ڼ��socket�Ƿ���Է��͵��ļ�������������socket������ģʽ�µȴ������¼�֪ͨ�����Է������ݣ�
	char addr[20];              //������������ַ
	int port;                   //�����˿�
	char file_rootpath[MAX_PATH];   //�ļ�ϵͳ��Ŀ¼
	char workspace[MAX_PATH];
	fs::path config_path;
	GetCurrentDirectoryA(MAX_PATH, workspace);
	config_path = fs::path(workspace) / fs::path("config.ini");
	GetPrivateProfileStringA("Server", "IP", "127.0.0.1", addr, sizeof(addr), config_path.string().c_str());
	port = GetPrivateProfileIntA("Server", "Port", 8000, config_path.string().c_str());
	GetPrivateProfileStringA("Server", "Path", "", file_rootpath, sizeof(file_rootpath), config_path.string().c_str());
	printf("============Config Parsed============\n");
	printf("Addr: %s\nPort: %d\nFilesystem Root Path: %s\n", addr, port, file_rootpath);
	int nRc = WSAStartup(0x0202, &wsaData);
	if (nRc) {
		printf("[Socket] Winsock startup failed with error!\n");
		return 0;
	}
	if (wsaData.wVersion != 0x0202) {
		printf("[Socket] Winsock version is not correct!\n");
		return 0;
	}
	printf("[Socket] Winsock startup Ok!\n");
	//����socket
	SOCKET srvSocket;
	//��������ַ
	sockaddr_in server_addr;
	//ip��ַ����
	int addrLen;
	//��������socket
	srvSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (srvSocket != INVALID_SOCKET)
		printf("[Socket] Socket create Ok!\n");
	else {
		printf("[Socket] Socket create Failed!\n");
		return 0;
	}
	//���÷������Ķ˿ں͵�ַ
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, addr, &server_addr.sin_addr);
	//binding
	int rtn = bind(srvSocket, (LPSOCKADDR)&server_addr, sizeof(server_addr));
	if (rtn != SOCKET_ERROR)
		printf("[Socket] Socket bind Ok!\n");
	else {
		printf("[Socket] Socket bind Failed!\n");
		return 0;
	}
	//����
	rtn = listen(srvSocket, 5);
	if (rtn != SOCKET_ERROR)
		printf("[Socket] Socket listen Ok!\n");
	else {
		printf("[Socket] Socket listen Failed!\n");
		return 0;
	}
	//���ý��ջ�����
	char recvBuf[4096];
	u_long blockMode = 1;//��srvSock��Ϊ������ģʽ�Լ����ͻ���������
	//����ioctlsocket����srvSocket��Ϊ������ģʽ���ĳɷ������fd_setԪ�ص�״̬����ÿ��Ԫ�ض�Ӧ�ľ���Ƿ�ɶ����д
	if ((rtn = ioctlsocket(srvSocket, FIONBIO, &blockMode) == SOCKET_ERROR)) { //FIONBIO��������ֹ�׽ӿ�s�ķ�����ģʽ��
		printf("[Socket] ioctlsocket() failed with error!\n");
		return 0;
	}
	printf("[Socket] ioctlsocket() for server socket ok!	Waiting for client connection and data\n");
	std::vector<Client> clients;
	while (true) {
		//���rfds��wfds����
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(srvSocket, &rfds);
		//���õȴ��ͻ���������
		for (Client client: clients) {
			FD_SET(*client.getSocket(), &rfds);
			FD_SET(*client.getSocket(), &wfds);
		}
		//�����ܹ����Զ���д�ľ������
		int nTotal = select(0, &rfds, &wfds, NULL, NULL);
		//���ỰSOCKET�Ƿ������ݵ��������Ƿ���Է�������
		if (nTotal > 0) {
			//���srvSock�յ��������󣬽��ܿͻ���������
			if (FD_ISSET(srvSocket, &rfds)) {
				//�����ỰSOCKET
				sockaddr_in clientAddr;
				int addrLen;
				clientAddr.sin_family = AF_INET;
				addrLen = sizeof(clientAddr);
				SOCKET sessionSocket = accept(srvSocket, (LPSOCKADDR)&clientAddr, &addrLen);
				//if (sessionSocket != INVALID_SOCKET)
					//printf("Socket listen one client request!\n");
				//�ѻỰSOCKET��Ϊ������ģʽ
				if ((rtn = ioctlsocket(sessionSocket, FIONBIO, &blockMode) == SOCKET_ERROR)) { //FIONBIO��������ֹ�׽ӿ�s�ķ�����ģʽ��
					printf("ioctlsocket() failed with error!\n");
					return 0;
				}
				//printf("ioctlsocket() for session socket ok!	Waiting for client connection and data\n");
				clients.push_back(Client(sessionSocket, clientAddr));
			}
			for (auto it = clients.begin(); it != clients.end();) {
				//�Ựsocket�����ݵ��������ܿͻ�������
				Client client = *it;
				SOCKET sessionSocket = *client.getSocket();
				sockaddr_in clientAddr = *client.getAddr();
				if (FD_ISSET(sessionSocket, &rfds)) {
					//receiving data from client
					memset(recvBuf, '\0', 4096);
					rtn = recv(sessionSocket, recvBuf, 4096, 0);
					if (rtn > 0) {
						char clientIP[INET_ADDRSTRLEN];
						inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
						int clientPort = ntohs(clientAddr.sin_port);
						//printf("Received %d bytes from client: %s\n", rtn, recvBuf);
						//printf("len: %d\n", strlen(recvBuf));
						HttpRequest req = parseRawRequest(recvBuf);
						printf("[Request] Client IP: %s, Port: %d, HTTP: %s\n", clientIP, clientPort, req.firstLine.c_str());
						//printf("[Request] Method: %s Path: %s\n", req.method.c_str(), req.path.c_str());
						std::string resp;
						if (req.method != "GET") {
							printf("[Response] 405 Method Not Allowed\n");
							resp = constructResponse(405, "", "");
						}
						else {
							try {
								std::string content = readFile(fs::path(file_rootpath) / fs::path(req.path));
								printf("[Response] 200 OK\n");
								resp = constructResponse(200, getContentType(req), content);
							}
							catch (const std::exception& e) {
								printf("[Response] 404 Not Found\n");
								resp = constructResponse(404, "", "");
							}
						}
						send(sessionSocket, resp.c_str(), resp.size(), 0);
					}
					else { //�������յ��˿ͻ��˶Ͽ����ӵ�����Ҳ��ɶ��¼�����rtn = 0
						//printf("Client leaving ...\n");
						closesocket(sessionSocket);  //��Ȼclient�뿪�ˣ��͹ر�sessionSocket
						it = clients.erase(it);
						sessionSocket = INVALID_SOCKET; //��sessionSocket��ΪINVALID_SOCKET
						continue;
					}
				}
				it++;
			}
		}
	}
	return 0;
}