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
// ���� ���� ������ ���� ����ü�� ����
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
char name[15]; //������ �����̸�
int nTotalSockets = 0;
SOCKETINFO *SocketInfoArray[FD_SETSIZE];

// ���� ���� �Լ�
BOOL AddSocketInfo(SOCKET sock, bool isIPv6, char * userName);
void RemoveSocketInfo(int nIndex);

// ���� ��� �Լ�
void err_quit(const char *msg);
void err_display(const char *msg);
//����ð� ���ϴ� �Լ�
void CurrentTime();
DWORD WINAPI SendUserList(LPVOID arg);
static HANDLE g_hSendUserListEvent;
static HANDLE g_hSendDataEvent;
//���� ����Ʈ ������
HANDLE hThread;
int main(int argc, char *argv[])
{
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
	/*----- IPv4 ���� �ʱ�ȭ ���� -----*/
	// socket()
	SOCKET listen_sockv4 = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sockv4 == INVALID_SOCKET) err_quit("socket()");
	//�̺�Ʈ ����
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
	/*----- IPv4 ���� �ʱ�ȭ �� -----*/
	/*----- IPv6 ���� �ʱ�ȭ ���� -----*/
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
	/*----- IPv6 ���� �ʱ�ȭ �� -----*/

	// ������ ��ſ� ����� ����(����)
	FD_SET rset;
	SOCKET client_sock;
	int addrlen, i, j;
	// ������ ��ſ� ����� ����(IPv4)
	SOCKADDR_IN clientaddrv4;
	// ������ ��ſ� ����� ����(IPv6)
	SOCKADDR_IN6 clientaddrv6;

	while (1) {
		// ���� �� �ʱ�ȭ
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

		// ���� �� �˻�(1): Ŭ���̾�Ʈ ���� ����
		if (FD_ISSET(listen_sockv4, &rset)) {
			addrlen = sizeof(clientaddrv4);
			client_sock = accept(listen_sockv4, (SOCKADDR *)&clientaddrv4, &addrlen);
			if (client_sock == INVALID_SOCKET) {
				err_display("accept()");
				break;
			}
			else {
				// ������ Ŭ���̾�Ʈ ���� ���
				printf("[TCPv4 ����] Ŭ���̾�Ʈ ����: [%s]:%d\n",
					inet_ntoa(clientaddrv4.sin_addr), ntohs(clientaddrv4.sin_port));
				// ���� ���� �߰�
				retval = recv(client_sock, usr_name, sizeof(usr_name), 0);
				printf("%s ������.\n", usr_name);
				AddSocketInfo(client_sock, false, usr_name);
				//������ �����̸��� ��ü ä�ù����� ����
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
				// ������ Ŭ���̾�Ʈ ���� ���
				char ipaddr[50];
				DWORD ipaddrlen = sizeof(ipaddr);
				WSAAddressToString((SOCKADDR *)&clientaddrv6, sizeof(clientaddrv6),
					NULL, ipaddr, &ipaddrlen);
				printf("[TCPv6 ����] Ŭ���̾�Ʈ ����: %s\n", ipaddr);
				// ���� ���� �߰�
				retval = recv(client_sock, usr_name, sizeof(usr_name), 0);
				printf("%s ������.\n", usr_name);
				AddSocketInfo(client_sock, false, usr_name);
				//������ �����̸��� ��ü ä�ù����� ����
				hThread = CreateThread(NULL, 0, SendUserList, NULL, 0, NULL);
			}
		}
		// ���� �� �˻�(2): ������ ���
		for (i = 0; i < nTotalSockets; i++) {
			COMM_MSG comm_msg;
			SOCKETINFO *ptr = SocketInfoArray[i];
			if (FD_ISSET(ptr->sock, &rset)) {
				// ������ �ޱ�
				//printf("COMM ��ٸ�\n");
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
					//�ӼӸ� ���
					if (comm_msg.type == 999) {
						if (i == j || j==comm_msg.index) {
							retval = send(ptr2->sock, (char *)&comm_msg, sizeof(COMM_MSG), 0);
							if (retval == SOCKET_ERROR) {
								err_display("send()");
								RemoveSocketInfo(i);
								continue;
							}
							//�̸�������
							retval = send(ptr2->sock, ptr->username, 15, 0);
							if (retval == SOCKET_ERROR) {
								err_display("send()");
								RemoveSocketInfo(j);
								--j; // ���� �ε��� ����
								continue;
							}
							//���� ������
							retval = send(ptr2->sock, ptr->buf, comm_msg.size, 0);
							if (retval == SOCKET_ERROR) {
								err_display("send()");
								RemoveSocketInfo(i);

								continue;
							}
						}
					}
					//��üä�� ���
					else if (comm_msg.type == 1000) {
						retval = send(ptr2->sock, (char *)&comm_msg, sizeof(COMM_MSG), 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(i);
							continue;
						}
						//�̸�������
						retval = send(ptr2->sock, ptr->username, 15, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(j);
							--j; // ���� �ε��� ����
							continue;
						}
						//���� ������
						retval = send(ptr2->sock, ptr->buf, comm_msg.size, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(i);

							continue;
						}
					}//ä�� Ÿ�� ����
					else if (comm_msg.type == 901) {
						NIGHTBOT_MSG n_msg;
						retval = send(ptr2->sock, (char *)&comm_msg, sizeof(COMM_MSG), 0);
						if (retval == SOCKET_ERROR) {
							err_display("send()");
							RemoveSocketInfo(i);
							continue;
						}
						//���� ������
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
							strcpy(n_msg.buf, "���� ���");
							retval = send(ptr2->sock, n_msg.buf, comm_msg.size, 0);
							if (retval == SOCKET_ERROR) {
								err_display("send()");
								RemoveSocketInfo(i);
								continue;
							}
						}
					}//����Ʈ��
					else { //������ Ÿ��
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
							--j; // ���� �ε��� ����
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
//����ð� ���ϱ�
void CurrentTime() {
	int rc;
	time_t temp;
	struct tm *timeptr;

	temp = time(NULL);
	timeptr = localtime(&temp);
	rc = strftime(current_time, sizeof(current_time), "Today is %b %d %r", timeptr);
}
//�ǽð� �������� ������

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
					--h; // ���� �ε��� ����
					continue;
				}
			}
		}
		Sleep(500);
	return 0;
}
// ���� ���� �߰�
BOOL AddSocketInfo(SOCKET sock, bool isIPv6, char * userName)
{
	if (nTotalSockets >= FD_SETSIZE) {
		printf("[����] ���� ������ �߰��� �� �����ϴ�!\n");
		return FALSE;
	}

	SOCKETINFO *ptr = new SOCKETINFO;
	if (ptr == NULL) {
		printf("[����] �޸𸮰� �����մϴ�!\n");
		return FALSE;
	}
	ptr->sock = sock;
	ptr->isIPv6 = isIPv6;
	strcpy(ptr->username, userName);
	//ptr->recvbytes = 0;
	SocketInfoArray[nTotalSockets++] = ptr;
	return TRUE;
}

// ���� ���� ����
void RemoveSocketInfo(int nIndex)
{
	SOCKETINFO *ptr = SocketInfoArray[nIndex];

	// ������ Ŭ���̾�Ʈ ���� ���
	if (ptr->isIPv6 == false) {
		SOCKADDR_IN clientaddrv4;
		int addrlen = sizeof(clientaddrv4);
		getpeername(ptr->sock, (SOCKADDR *)&clientaddrv4, &addrlen);
		printf("[TCPv4 ����] Ŭ���̾�Ʈ ����: [%s]:%d\n",
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
		printf("[TCPv6 ����] Ŭ���̾�Ʈ ����: %s\n", ipaddr);
	}

	closesocket(ptr->sock);
	delete ptr;

	if (nIndex != (nTotalSockets - 1))
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets - 1];

	--nTotalSockets;
	//������ �����̸��� ��ü ä�ù����� ����
	hThread = CreateThread(NULL, 0, SendUserList, NULL, 0, NULL);
}
// ���� �Լ� ���� ��� �� ����
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

// ���� �Լ� ���� ���
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