#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#pragma warning(disable:4996)
#define SERVERPORT 9000
#define BUFSIZE    256
// 소켓 정보 저장을 위한 구조체와 변수
char usr_name[15];
char current_time[100];
char buf[10];
int radix = 10;
struct SOCKETINFO
{
	SOCKET sock;
	bool   isIPv6;
	char   buf[BUFSIZE];
	//int recvbytes;
	char username[15];
};
struct NIGHTBOT_MSG {
	int type = 901;
	char buf[BUFSIZE];
	char servername[15];
};
struct COMM_MSG {
	int type;
	int index = -1;
	char name[15];
	int size;
	//char dummy[BUFSIZE];
};
char name[15]; //접속한 유저이름
int nTotalSockets = 0;
SOCKETINFO *SocketInfoArray[FD_SETSIZE];

// 소켓 관리 함수
BOOL AddSocketInfo(SOCKET sock, bool isIPv6, char * userName);
void RemoveSocketInfo(int nIndex);

// 오류 출력 함수
void err_quit(const char *msg);
void err_display(const char *msg);
//현재시간 구하는 함수
void CurrentTime();
DWORD WINAPI SendUserList(LPVOID arg);
static HANDLE g_hSendUserListEvent;
static HANDLE g_hSendDataEvent;
//유저 리스트 스레드
HANDLE hThread;
int main(int argc, char *argv[])
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
	/*----- IPv4 소켓 초기화 시작 -----*/
	// socket()
	SOCKET listen_sockv4 = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sockv4 == INVALID_SOCKET) err_quit("socket()");
	//이벤트 생성
	g_hSendUserListEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (g_hSendUserListEvent == NULL) return 1;
	SOCKADDR_IN serveraddrv4;
	ZeroMemory(&serveraddrv4, sizeof(serveraddrv4));
	serveraddrv4.sin_family = AF_INET;
	serveraddrv4.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddrv4.sin_port = htons(SERVERPORT);
	retval = bind(listen_sockv4, (SOCKADDR *)&serveraddrv4, sizeof(serveraddrv4));
	if (retval == SOCKET_ERROR) err_quit("bind()");
	// listen()
	retval = listen(listen_sockv4, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");
	/*----- IPv4 소켓 초기화 끝 -----*/
	/*----- IPv6 소켓 초기화 시작 -----*/
	// socket()
	SOCKET listen_sockv6 = socket(AF_INET6, SOCK_STREAM, 0);
	if (listen_sockv6 == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN6 serveraddrv6;
	ZeroMemory(&serveraddrv6, sizeof(serveraddrv6));
	serveraddrv6.sin6_family = AF_INET6;
	serveraddrv6.sin6_addr = in6addr_any;
	serveraddrv6.sin6_port = htons(SERVERPORT);
	retval = bind(listen_sockv6, (SOCKADDR *)&serveraddrv6, sizeof(serveraddrv6));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sockv6, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");
	/*----- IPv6 소켓 초기화 끝 -----*/

	// 데이터 통신에 사용할 변수(공통)
	FD_SET rset;
	SOCKET client_sock;
	int addrlen, i, j;
	// 데이터 통신에 사용할 변수(IPv4)
	SOCKADDR_IN clientaddrv4;
	// 데이터 통신에 사용할 변수(IPv6)
	SOCKADDR_IN6 clientaddrv6;

	while (1) {
		// 소켓 셋 초기화
		FD_ZERO(&rset);
		FD_SET(listen_sockv4, &rset);
		FD_SET(listen_sockv6, &rset);
		for (i = 0; i < nTotalSockets; i++) {
			FD_SET(SocketInfoArray[i]->sock, &rset);
		}

		// select()
		retval = select(0, &rset, NULL, NULL, NULL);
		if (retval == SOCKET_ERROR) {
			err_display("select()");
			break;
		}

		// 소켓 셋 검사(1): 클라이언트 접속 수용
		if (FD_ISSET(listen_sockv4, &rset)) {
			addrlen = sizeof(clientaddrv4);
			client_sock = accept(listen_sockv4, (SOCKADDR *)&clientaddrv4, &addrlen);
			if (client_sock == INVALID_SOCKET) {
				err_display("accept()");
				break;
			}
			else {
				// 접속한 클라이언트 정보 출력
				printf("[TCPv4 서버] 클라이언트 접속: [%s]:%d\n",
					inet_ntoa(clientaddrv4.sin_addr), ntohs(clientaddrv4.sin_port));
				// 소켓 정보 추가
				retval = recv(client_sock, usr_name, sizeof(usr_name), 0);
				printf("%s 접속함.\n", usr_name);
				AddSocketInfo(client_sock, false, usr_name);
				//접속한 유저이름을 전체 채팅방으로 전송
				hThread = CreateThread(NULL, 0, SendUserList, NULL, 0, NULL);
			}
		}
		if (FD_ISSET(listen_sockv6, &rset)) {
			addrlen = sizeof(clientaddrv6);
			client_sock = accept(listen_sockv6, (SOCKADDR *)&clientaddrv6, &addrlen);
			if (client_sock == INVALID_SOCKET) {
				err_display("accept()");
				break;
			}
			else {
				// 접속한 클라이언트 정보 출력
				char ipaddr[50];
				DWORD ipaddrlen = sizeof(ipaddr);
				WSAAddressToString((SOCKADDR *)&clientaddrv6, sizeof(clientaddrv6),
					NULL, ipaddr, &ipaddrlen);
				printf("[TCPv6 서버] 클라이언트 접속: %s\n", ipaddr);
				// 소켓 정보 추가
				retval = recv(client_sock, usr_name, sizeof(usr_name), 0);
				printf("%s 접속함.\n", usr_name);
				AddSocketInfo(client_sock, false, usr_name);
				//접속한 유저이름을 전체 채팅방으로 전송
				hThread = CreateThread(NULL, 0, SendUserList, NULL, 0, NULL);
			}
		}
		// 소켓 셋 검사(2): 데이터 통신
		for (i = 0; i < nTotalSockets; i++) {
			COMM_MSG comm_msg;
			SOCKETINFO *ptr = SocketInfoArray[i];
			if (FD_ISSET(ptr->sock, &rset)) {
				// 데이터 받기
				//printf("COMM 기다림\n");
				retval = recv(ptr->sock, (char *)&comm_msg,sizeof(COMM_MSG),0);
				if (retval == 0 || retval == SOCKET_ERROR) {
					RemoveSocketInfo(i);
					continue;
				}
				retval = recv(ptr->sock, ptr->buf, comm_msg.size, 0);
				if (retval == 0 || retval == SOCKET_ERROR) {
					RemoveSocketInfo(i);
					continue;
				}
				strcpy(ptr->username, comm_msg.name);
				for (j = 0; j < nTotalSockets; j++) {
					SOCKETINFO *ptr2 = SocketInfoArray[j];
					WaitForSingleObject(g_hSendUserListEvent, NULL);
					//귓속말 모드
					if (comm_msg.type == 999) {
						if (i == j || j==comm_msg.index) {
							retval = send(ptr2->sock, (char *)&comm_msg, sizeof(COMM_MSG), 0);
							if (retval == SOCKET_ERROR) {
								err_display("send()");
								RemoveSocketInfo(i);
								continue;
							}
							//이름보내기
							retval = send(ptr2->sock, ptr->username, 15, 0);
							if (retval == SOCKET_ERROR) {
								err_display("send()");
								RemoveSocketInfo(j);
								--j; // 루프 인덱스 보정
								continue;
							}
							//내용 보내기
							retval = send(ptr2->sock, ptr->buf, comm_msg.size, 0);
							if (retval == SOCKET_ERROR) {
								err_display("send()");
								RemoveSocketInfo(i);

								continue;
							}
						}
					}
					//전체채팅 모드
					else if (comm_msg.type == 1000) {
						retval = send(ptr2->sock, (char *)&comm_msg, sizeof(COMM_MSG), 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(i);
							continue;
						}
						//이름보내기
						retval = send(ptr2->sock, ptr->username, 15, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(j);
							--j; // 루프 인덱스 보정
							continue;
						}
						//내용 보내기
						retval = send(ptr2->sock, ptr->buf, comm_msg.size, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(i);

							continue;
						}
					}//채팅 타입 지정
					else if (comm_msg.type == 901) {
						NIGHTBOT_MSG n_msg;
						retval = send(ptr2->sock, (char *)&comm_msg, sizeof(COMM_MSG), 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(i);
							continue;
						}
						//내용 보내기
						strcpy(n_msg.servername, "server");
						retval = send(ptr2->sock, n_msg.servername, 15, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(i);
							continue;
						}
						if (!strcmp(ptr->buf, "!time")) {
							CurrentTime();
							strcpy(n_msg.buf, current_time);
							retval = send(ptr2->sock, n_msg.buf, 27, 0);
							if (retval == SOCKET_ERROR) {
								err_display("send()");
								RemoveSocketInfo(i);
								continue;
							}
						}
						else {
							strcpy(n_msg.buf, "없는 명령");
							retval = send(ptr2->sock, n_msg.buf, comm_msg.size, 0);
							if (retval == SOCKET_ERROR) {
								err_display("send()");
								RemoveSocketInfo(i);
								continue;
							}
						}
					}//나이트봇
					else { //나머지 타입
						retval = send(ptr2->sock, (char *)&comm_msg, sizeof(COMM_MSG), 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(i);
							continue;
						}
						retval = send(ptr2->sock, ptr->username, 15, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(j);
							--j; // 루프 인덱스 보정
							continue;
						}
						retval = send(ptr2->sock, ptr->buf, comm_msg.size, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(i);
							continue;
						}
					}
				}
				//}
			}
		}
	}
	
	CloseHandle(g_hSendUserListEvent);
	//CloseHandle(hThread);
	return 0;
}
//현재시간 구하기
void CurrentTime() {
	int rc;
	time_t temp;
	struct tm *timeptr;

	temp = time(NULL);
	timeptr = localtime(&temp);
	rc = strftime(current_time, sizeof(current_time), "Today is %b %d %r", timeptr);
}
//실시간 유저정보 보내기

DWORD WINAPI SendUserList(LPVOID arg) {
	int retval;
	int k,h;
	//while (1) {
		for (k = 0; k < nTotalSockets; k++) {
			COMM_MSG comm_msg;
			SOCKETINFO *ptr = SocketInfoArray[k];
			comm_msg.type = 900;

			for (h = 0; h < nTotalSockets; h++) {
				SOCKETINFO *ptr3 = SocketInfoArray[h];
				retval = send(ptr->sock, (char *)&comm_msg, sizeof(COMM_MSG), 0);
				if (retval == SOCKET_ERROR) {
					err_display("send()");
					RemoveSocketInfo(k);
					continue;
				}
				itoa(nTotalSockets, buf, radix);
				retval = send(ptr->sock,buf,sizeof(buf), 0);
				if (retval == SOCKET_ERROR) {
					err_display("send()");
				}
				retval = send(ptr->sock,ptr3->username, 15, 0);
				if (retval == SOCKET_ERROR) {
					err_display("send()");
					RemoveSocketInfo(h);
					--h; // 루프 인덱스 보정
					continue;
				}
			}
		}
		Sleep(500);
	return 0;
}
// 소켓 정보 추가
BOOL AddSocketInfo(SOCKET sock, bool isIPv6, char * userName)
{
	if (nTotalSockets >= FD_SETSIZE) {
		printf("[오류] 소켓 정보를 추가할 수 없습니다!\n");
		return FALSE;
	}

	SOCKETINFO *ptr = new SOCKETINFO;
	if (ptr == NULL) {
		printf("[오류] 메모리가 부족합니다!\n");
		return FALSE;
	}
	ptr->sock = sock;
	ptr->isIPv6 = isIPv6;
	strcpy(ptr->username, userName);
	//ptr->recvbytes = 0;
	SocketInfoArray[nTotalSockets++] = ptr;
	return TRUE;
}

// 소켓 정보 삭제
void RemoveSocketInfo(int nIndex)
{
	SOCKETINFO *ptr = SocketInfoArray[nIndex];

	// 종료한 클라이언트 정보 출력
	if (ptr->isIPv6 == false) {
		SOCKADDR_IN clientaddrv4;
		int addrlen = sizeof(clientaddrv4);
		getpeername(ptr->sock, (SOCKADDR *)&clientaddrv4, &addrlen);
		printf("[TCPv4 서버] 클라이언트 종료: [%s]:%d\n",
			inet_ntoa(clientaddrv4.sin_addr), ntohs(clientaddrv4.sin_port));
	}
	else {
		SOCKADDR_IN6 clientaddrv6;
		int addrlen = sizeof(clientaddrv6);
		getpeername(ptr->sock, (SOCKADDR *)&clientaddrv6, &addrlen);

		char ipaddr[50];
		DWORD ipaddrlen = sizeof(ipaddr);
		WSAAddressToString((SOCKADDR *)&clientaddrv6, sizeof(clientaddrv6),
			NULL, ipaddr, &ipaddrlen);
		printf("[TCPv6 서버] 클라이언트 종료: %s\n", ipaddr);
	}

	closesocket(ptr->sock);
	delete ptr;

	if (nIndex != (nTotalSockets - 1))
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets - 1];

	--nTotalSockets;
	//접속한 유저이름을 전체 채팅방으로 전송
	hThread = CreateThread(NULL, 0, SendUserList, NULL, 0, NULL);
}
// 소켓 함수 오류 출력 후 종료
void err_quit(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 소켓 함수 오류 출력
void err_display(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}