#include "..\\..\\Common.h"
#include <thread>
#include <string>
#include <cstring>

#define SERVERPORT 9000
#define BUFSIZE 512

std::vector<ClientInfo> clientInfos;
std::deque<ClientRequest> requestQueue;  // Common.h에 선언된 대로 std::deque로 정의
CRITICAL_SECTION cs;

int client_count = 0;

// 클라이언트 요청 처리 쓰레드
void execute() {
    while (true) {
        EnterCriticalSection(&cs);
        if (!requestQueue.empty()) {
            ClientRequest req = requestQueue.front();
            requestQueue.pop_front();  // pop_front() 사용 가능
            LeaveCriticalSection(&cs);

            if (req.requestType == LOGIN_TRY) {
                ClientInfo newClient;
                newClient.id = client_count++;
                strncpy(newClient.name, req.clientName, sizeof(newClient.name));
                newClient.x = 0;
                newClient.y = 0;

                EnterCriticalSection(&cs);
                clientInfos.push_back(newClient);
                LeaveCriticalSection(&cs);

                // 클라이언트에게 LOGIN_SUCCESS 응답 전송
                RequestType successMsg = LOGIN_SUCCESS;
                send(req.clientSocket, (char*)&successMsg, sizeof(successMsg), 0);
                printf("Client [%s] 로그인 성공 (LOGIN_SUCCESS 전송)\n", req.clientName);
            }
        }
        else {
            LeaveCriticalSection(&cs);
            Sleep(10);
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

        if (requestType == LOGIN_TRY) {
            ClientRequest req;
            req.requestType = LOGIN_TRY;
            req.clientSocket = client_sock;
            strncpy(req.clientName, buf + sizeof(int), sizeof(req.clientName) - 1);
            req.clientName[sizeof(req.clientName) - 1] = '\0';

            EnterCriticalSection(&cs);
            requestQueue.push_back(req);  // deque에 push_back 사용
            LeaveCriticalSection(&cs);
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

    InitializeCriticalSection(&cs);

    std::thread executeThread(execute);
    executeThread.detach();

    while (1) {
        SOCKET client_sock = accept(listen_sock, nullptr, nullptr);
        CreateThread(NULL, 0, ProcessClient, (LPVOID)client_sock, 0, NULL);
    }

    DeleteCriticalSection(&cs);
    closesocket(listen_sock);
    WSACleanup();
    return 0;
}
