#include "..\\..\\Common.h"
#include <thread>
#include <string>
#include <cstring>
#include <algorithm>  // 정렬을 위해 추가

#define SERVERPORT 9000
#define BUFSIZE 512

std::vector<ClientInfo> clientInfos;
std::deque<QueueBasket> requestQueue;
CRITICAL_SECTION csQueue;

int client_count = 0;

RequestType hton_requestType(RequestType type) {
    return static_cast<RequestType>(htonl(static_cast<int>(type)));
}

RequestType ntoh_requestType(RequestType type) {
    return static_cast<RequestType>(ntohl(static_cast<int>(type)));
}

// ClientInfo 변환 함수 (네트워크 바이트 정렬)
void hton_clientInfo(ClientInfo& client) {
    client.id = htonl(client.id);
}

void ntoh_clientInfo(ClientInfo& client) {
    client.id = ntohl(client.id);
}

// 클라이언트 요청 처리 쓰레드
void execute() {
    while (true) {
        EnterCriticalSection(&csQueue);
        if (!requestQueue.empty()) {
            QueueBasket req = requestQueue.front();
            requestQueue.pop_front();
            LeaveCriticalSection(&csQueue);

            if (req.requestType == LOGIN_TRY) {
                ClientInfo newClient;
                newClient.id = client_count++;
                //strncpy(newClient.name, req.clientName, sizeof(newClient.name));
                newClient.clientSocket = req.clientSocket;
                hton_clientInfo(newClient);
                EnterCriticalSection(&csQueue);
                clientInfos.push_back(newClient);
                LeaveCriticalSection(&csQueue);

                // 클라이언트에게 LOGIN_SUCCESS 응답 전송
                RequestType successMsg = LOGIN_SUCCESS;
                successMsg = hton_requestType(successMsg);
                send(req.clientSocket, (char*)&successMsg, sizeof(successMsg), 0);
                printf("Client [%s] 로그인 성공 (LOGIN_SUCCESS 전송)\n", req.clientName);


            }
        }
        else {
            LeaveCriticalSection(&csQueue);
            //Sleep(10);
        }
    }
}

// 클라이언트 쓰레드 함수
DWORD WINAPI ProcessClient(LPVOID arg) {
    SOCKET client_sock = (SOCKET)arg;
    char buf[BUFSIZE];
    int retval;

    while ((retval = recv(client_sock, buf, BUFSIZE, 0)) > 0) {
        RequestType requestType;
        memcpy(&requestType, buf, sizeof(RequestType));
        requestType = ntoh_requestType(requestType);
        if (requestType == LOGIN_TRY) {
            QueueBasket req;
            req.requestType = LOGIN_TRY;
            req.clientSocket = client_sock;
            strncpy(req.clientName, buf + sizeof(int), sizeof(req.clientName) - 1);
            req.clientName[sizeof(req.clientName) - 1] = '\0';

            EnterCriticalSection(&csQueue);
            requestQueue.push_back(req);
            LeaveCriticalSection(&csQueue);
        }
    }

    closesocket(client_sock);
    return 0;
}

int main(int argc, char* argv[]) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);
    bind(listen_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    listen(listen_sock, SOMAXCONN);

    InitializeCriticalSection(&csQueue);

    std::thread executeThread(execute);
    executeThread.detach();

    while (1) {
        SOCKET client_sock = accept(listen_sock, nullptr, nullptr);
        CreateThread(NULL, 0, ProcessClient, (LPVOID)client_sock, 0, NULL);
    }

    DeleteCriticalSection(&csQueue);
    closesocket(listen_sock);
    WSACleanup();
    return 0;
}
