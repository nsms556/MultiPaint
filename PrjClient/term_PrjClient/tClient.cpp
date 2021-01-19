#pragma comment(lib, "ws2_32")
#pragma warning(disable:4996)
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <Windowsx.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Commctrl.h>
#include "resource.h"
#include "NickName.h"
#include "Network.c"
#include <time.h>

#define SERVERIPV4  "127.0.0.1"
#define SERVERIPV6  "::1"
#define SERVERPORT  9000

#define BUFSIZE			256                    // 전송 메시지 전체 크기
#define MSGSIZE			(BUFSIZE-sizeof(int))  // 채팅 메시지 최대 길이

#define NAMESIZE		15					//유저이름

#define USERINFO		900				//유저정보
#define NIGHTBOT		901				//나이트봇
#define WHISPER			999				//귓속말

#define CHATTING		1000					// 메시지 타입: 채팅
#define DRAWLINE		1001					// 메시지 타입: 선 그리기
#define DRAWTRI			1002					// 메시지 타입: 삼각형 그리기
#define DRAWRECT		1003					// 메시지 타입: 사각형 그리기
#define DRAWELLIPSE		1004					// 메시지 타입: 원 그리기
#define DRAWSTRICTLINE	1005					// 메시지 타입: 직선 그리기
#define DRAWERASER		1006					// 메시지 타입: 지우개
#define DRAWRHOMBUS     1007					// 메시지 타입: 마름모 그리기
#define DRAWHEXAGON		1008					// 메시지 타입: 육각형 그리기
#define DRAWSTAR		1009					// 메시지 타입: 별 그리기
#define DRAWRIGHTTRI	1010					// 메시지 타입: 직각삼각형 그리기

#define WM_DRAWIT		(WM_USER+1)            // 사용자 정의 윈도우 메시지
#define WM_ADDUSER		(WM_USER+2)			
#define WM_RESETUSER	(WM_USER+3)

// 공통 메시지 형식
// sizeof(COMM_MSG) == 256
struct COMM_MSG
{
	int  type;
	int index = -1;
	char name[NAMESIZE];
	int size;
	//char dummy[MSGSIZE];
};

//유저 이름
char randomName[15];
char inputName[15];

// 채팅 메시지 형식
// sizeof(CHAT_MSG) == 256
struct CHAT_MSG
{
	int  type;
	char name[NAMESIZE];
	char buf[MSGSIZE];
};

// 선 그리기 메시지 형식
// sizeof(DRAWLINE_MSG) == 256
struct DRAWLINE_MSG
{
	int  type;
	int  color;
	int  x0, y0;
	int  x1, y1;
	int lineWidth;
	BOOL isFill;
	char dummy[BUFSIZE - 6 * sizeof(int)];
};

struct NIGHTBOT_MSG {
	int type = NIGHTBOT;
	char buf[BUFSIZE];
	char servername[15];
};

COLORREF Color = RGB(0, 0, 0);				//좌 색상전달
COLORREF RColor = RGB(255, 255, 255);		//우 색상전달

static HINSTANCE     g_hInst; // 응용 프로그램 인스턴스 핸들
static HWND          g_hDrawWnd; // 그림을 그릴 윈도우
static HWND          g_hButtonSendMsg; // '메시지 전송' 버튼
static HWND          g_hEditStatus; // 받은 메시지 출력
static HWND			 g_hDlg;//유저 리스트 추가
static char          g_ipaddr[64]; // 서버 IP 주소
static u_short       g_port; // 서버 포트 번호
static HWND			 hUserID;	//사용자 ID
static BOOL          g_isIPv6; // IPv4 or IPv6 주소?
static HANDLE        g_hClientThread; // 스레드 핸들
static volatile BOOL g_bStart; // 통신 시작 여부
static SOCKET        g_sock; // 클라이언트 소켓
static HANDLE        g_hReadEvent, g_hWriteEvent; // 이벤트 핸들
static CHAT_MSG      g_chatmsg; // 채팅 메시지 저장
static DRAWLINE_MSG  g_drawmsg; // 선 그리기 메시지 저장
static int           g_drawcolor; // 선 그리기 색상
static HWND			 g_hShapeFill;
static HWND			 g_hWidth;
static HWND			 g_hClear;
static HWND			 g_RColor;
static COMM_MSG		 g_comm_msg;
static char			 g_username[15];

int g_index = -1; //선택된 유저 리스트 값 default -1;
char c_listNum[10]; //현재 접속중인 클라이언트 수
int listNum = 0;
int count = 0;

//유저 리스트 플래그
int USERLIST_FLAG = 0;
int SEND_NBOT_FLAG = 0;

// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg);
DWORD WINAPI ReadThread(LPVOID arg);
DWORD WINAPI WriteThread(LPVOID arg);

// 자식 윈도우 프로시저
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

//현재 접속 유저 리스트 표시 GUI
static HWND hUserList;

// 편집 컨트롤 출력 함수
void DisplayText(const char *fmt, ...);

/*----------------------------------------------------------------------------*/
char* getNickName();
void childWndInit(HWND dlg);
void DisplayThickness(const char* fmt, ...);

void drawRightTri(HDC hDC, int x0, int y0, int x1, int y1);
void drawRhombus(HDC hDC, int x0, int y0, int x1, int y1);
void drawHexagon(HDC hDC, int x0, int y0, int x1, int y1);
void drawStar(HDC hDC, int x0, int y0, int x1, int y1);


// 메인 함수
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
	

	// 이벤트 생성
	g_hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (g_hReadEvent == NULL) return 1;
	g_hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (g_hWriteEvent == NULL) return 1;

	// 변수 초기화(일부)
	g_chatmsg.type = CHATTING;
	g_drawmsg.type = DRAWLINE;
	g_drawmsg.lineWidth = 3;
	g_drawmsg.color = Color;
	g_comm_msg.size = sizeof(DRAWLINE_MSG);
	memset(&randomName, 0x00, sizeof(randomName));
	memset(&inputName, 0x00, sizeof(inputName));
	strcpy(randomName, getNickName());
	// 대화상자 생성
	g_hInst = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// 이벤트 제거
	CloseHandle(g_hReadEvent);
	CloseHandle(g_hWriteEvent);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hButtonIsIPv6;
	static HWND hEditIPaddr;
	static HWND hEditPort;
	static HWND hButtonConnect;
	static HWND hEditMsg;
	//static HWND hUserList;
	static HWND hColorButton;
	static HWND hShapeCombo;
	static HWND hLineWidthSlider;
	static HWND hWidth;
	static HWND hClear;

	g_hDlg = hDlg;
	CHOOSECOLOR COLOR;
	COLORREF crTmp[16];
	switch (uMsg) {
	case WM_INITDIALOG:
		// 컨트롤 핸들 얻기
		hButtonIsIPv6 = GetDlgItem(hDlg, IDC_ISIPV6);
		hEditIPaddr = GetDlgItem(hDlg, IDC_IPADDR);
		hEditPort = GetDlgItem(hDlg, IDC_PORT);
		hButtonConnect = GetDlgItem(hDlg, IDC_CONNECT);
		g_hButtonSendMsg = GetDlgItem(hDlg, IDC_SENDMSG);
		hUserID = GetDlgItem(hDlg, IDC_USERNAME);
		hUserList = GetDlgItem(hDlg, IDC_USERLIST);
		hEditMsg = GetDlgItem(hDlg, IDC_MSG);
		g_hEditStatus = GetDlgItem(hDlg, IDC_STATUS);
		hShapeCombo = GetDlgItem(hDlg, IDC_COMBO_SHAPE);
		hColorButton = GetDlgItem(hDlg, IDC_COLORBUTTON);// rc추가
		hLineWidthSlider = GetDlgItem(hDlg, IDC_LINEWIDTH);// rc추가
		g_hShapeFill = GetDlgItem(hDlg, IDC_ISFILL_CHECK);   // rc추가
		hWidth = GetDlgItem(hDlg, IDC_DISWIDTH);
		hClear = GetDlgItem(hDlg, IDC_CLEAR);


		// 컨트롤 초기화
		SendMessage(hEditMsg, EM_SETLIMITTEXT, MSGSIZE, 0);
		EnableWindow(g_hButtonSendMsg, FALSE);
		SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
		SetDlgItemText(hDlg, IDC_USERNAME, NULL);
		SetDlgItemInt(hDlg, IDC_PORT, SERVERPORT, FALSE);
		SendMessage(hLineWidthSlider, TBM_SETRANGE, (WPARAM)1, (LPARAM)MAKELONG(1, 100));
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"펜");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"삼각형");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"사각형");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"원형");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"직선");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"직각 삼각형");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"마름모");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"육각형");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"별");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"지우개");
		SendMessage(hShapeCombo, CB_SETCURSEL, 0, (LPARAM)"펜");
		SetDlgItemInt(hDlg, IDC_DISWIDTH, 3, FALSE);
		EnableWindow(g_hClear, TRUE);
		EnableWindow(g_RColor, TRUE);

		// 윈도우 클래스 등록
		WNDCLASS wndclass;
		wndclass.style = CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc = WndProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = g_hInst;
		wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = "MyWndClass";
		if (!RegisterClass(&wndclass)) return 1;

		// 자식 윈도우 생성
		childWndInit(hDlg);
		return TRUE;

	case WM_HSCROLL:
		g_drawmsg.lineWidth = SendDlgItemMessage(hDlg, IDC_LINEWIDTH, TBM_GETPOS, 0, 0);
		SetDlgItemInt(hDlg, IDC_DISWIDTH, g_drawmsg.lineWidth, FALSE);
		//g_drawmsg.lineWidth = pos;
		return 0;
	case WM_ADDUSER:
	{
		SendMessage(hUserList, LB_ADDSTRING, 0, (LPARAM)lParam);
		count++;
		if (count == listNum) {
			USERLIST_FLAG = 0;
			count = 0;
		}
		return TRUE;
	}
	case WM_RESETUSER:
	{
		SendMessage(hUserList, LB_RESETCONTENT, 0, (LPARAM)lParam);
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_USERLIST:
			switch (HIWORD(wParam)) {
			case LBN_SELCHANGE:
				//DisplayText("%d", index);
				if (g_index >= 0) {
					SendMessage(hUserList, LB_SETCURSEL, -1, 0);
				}
				g_index = SendMessage(hUserList, LB_GETCURSEL, 0, 0);
				return TRUE;
			}
		case IDC_ISIPV6:
			g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);
			if (g_isIPv6 == false)
				SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
			else
				SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV6);
			return TRUE;

		case IDC_CONNECT:
			GetDlgItemText(hDlg, IDC_IPADDR, g_ipaddr, sizeof(g_ipaddr));
			g_port = GetDlgItemInt(hDlg, IDC_PORT, NULL, FALSE);
			g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);
			GetDlgItemText(hDlg, IDC_USERNAME, (LPSTR)g_comm_msg.name, NAMESIZE);
			if (strlen(g_comm_msg.name) == 0) {
				sprintf(g_comm_msg.name, "%s", randomName);
				SetDlgItemText(hDlg, IDC_USERNAME, randomName);
			}
			// 소켓 통신 스레드 시작
			g_hClientThread = CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);
			if (g_hClientThread == NULL) {
				MessageBox(hDlg, "클라이언트를 시작할 수 없습니다."
					"\r\n프로그램을 종료합니다.", "실패!", MB_ICONERROR);
				EndDialog(hDlg, 0);
			}
			else {
				EnableWindow(hButtonConnect, FALSE);
				while (g_bStart == FALSE); // 서버 접속 성공 기다림
				EnableWindow(hButtonIsIPv6, FALSE);
				EnableWindow(hEditIPaddr, FALSE);
				EnableWindow(hEditPort, FALSE);
				EnableWindow(g_hButtonSendMsg, TRUE);
				SetFocus(hEditMsg);
			}
			return TRUE;

		case IDC_SENDMSG:
			// 읽기 완료를 기다림
			WaitForSingleObject(g_hReadEvent, INFINITE);
			GetDlgItemText(hDlg, IDC_MSG, g_chatmsg.buf, MSGSIZE);
			// 쓰기 완료를 알림t
			SetEvent(g_hWriteEvent);
			// 입력된 텍스트 전체를 선택 표시
			SendMessage(hEditMsg, EM_SETSEL, 0, -1);
			SetDlgItemText(hDlg, IDC_MSG, "");
			return TRUE;

		case IDC_CLEAR:
			if (MessageBox(hDlg, "내 화면이 초기화됩니다.", "질문", MB_YESNO | MB_ICONQUESTION) == IDYES) {
				childWndInit(hDlg);
			}
			return TRUE;

		case IDCANCEL:
			if (MessageBox(hDlg, "정말로 종료하시겠습니까?",
				"질문", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				closesocket(g_sock);
				EndDialog(hDlg, IDCANCEL);
			}
			return TRUE;

		case IDC_COMBO_SHAPE:		// 모양 combobox 선택할 때 
			switch (ComboBox_GetCurSel(hShapeCombo))
			{
			case 0:	
				g_drawmsg.type = DRAWLINE;
				EnableWindow(g_hShapeFill, TRUE);
				break;
			case 1:	
				g_drawmsg.type = DRAWTRI; 
				EnableWindow(g_hShapeFill, TRUE);
				break;
			case 2:	
				g_drawmsg.type = DRAWRECT; 
				EnableWindow(g_hShapeFill, TRUE);
				break;
			case 3:	
				g_drawmsg.type = DRAWELLIPSE; 
				EnableWindow(g_hShapeFill, TRUE);
				break;
			case 4:
				g_drawmsg.type = DRAWSTRICTLINE;
				EnableWindow(g_hShapeFill, TRUE);
				break;
			case 5:	
				g_drawmsg.type = DRAWRIGHTTRI;
				SendMessage(g_hShapeFill, BM_SETCHECK, BST_UNCHECKED, 0);
				EnableWindow(g_hShapeFill, FALSE);
				break;
			case 6:	
				g_drawmsg.type = DRAWRHOMBUS; 
				SendMessage(g_hShapeFill, BM_SETCHECK, BST_UNCHECKED, 0);
				EnableWindow(g_hShapeFill, FALSE);
				break;
			case 7:	
				g_drawmsg.type = DRAWHEXAGON;
				SendMessage(g_hShapeFill, BM_SETCHECK, BST_UNCHECKED, 0);
				EnableWindow(g_hShapeFill, FALSE);
				break;
			case 8:	
				g_drawmsg.type = DRAWSTAR;
				SendMessage(g_hShapeFill, BM_SETCHECK, BST_UNCHECKED, 0);
				EnableWindow(g_hShapeFill, FALSE);
				break;
			case 9:
				g_drawmsg.type = DRAWERASER; 
				EnableWindow(g_hShapeFill, TRUE);
				break;
			default:
				g_drawmsg.type = DRAWLINE;
				break;
			}

		case IDC_ISFILL_CHECK:
			g_drawmsg.isFill = SendMessage(g_hShapeFill, BM_GETCHECK, 0, 0);
			return TRUE;

		case IDC_COLORBUTTON:
			memset(&COLOR, 0, sizeof(CHOOSECOLOR));
			COLOR.lStructSize = sizeof(CHOOSECOLOR);
			COLOR.hwndOwner = hDlg;
			COLOR.lpCustColors = crTmp;
			COLOR.Flags = 0;
			if (ChooseColor((LPCHOOSECOLORA)&COLOR) != 0) {
				Color = COLOR.rgbResult;
				InvalidateRect(hDlg, NULL, TRUE);
			}
			return TRUE;

		case IDC_COLORBUTTON2:
			memset(&COLOR, 0, sizeof(CHOOSECOLOR));
			COLOR.lStructSize = sizeof(CHOOSECOLOR);
			COLOR.hwndOwner = hDlg;
			COLOR.lpCustColors = crTmp;
			COLOR.Flags = 0;
			if (ChooseColor((LPCHOOSECOLORA)& COLOR) != 0) {
				RColor = COLOR.rgbResult;
				InvalidateRect(hDlg, NULL, TRUE);
			}
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;

	if (g_isIPv6 == false) {
		// socket()
		g_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (g_sock == INVALID_SOCKET) err_quit("socket()");
		// connect()
		SOCKADDR_IN serveraddr;
		ZeroMemory(&serveraddr, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_addr.s_addr = inet_addr(g_ipaddr);
		serveraddr.sin_port = htons(g_port);
		retval = connect(g_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
		if (retval == SOCKET_ERROR) err_quit("connect()");
		//유저이름 받아 입장 알림
	}
	else {
		// socket()
		g_sock = socket(AF_INET6, SOCK_STREAM, 0);
		if (g_sock == INVALID_SOCKET) err_quit("socket()");

		// connect()
		SOCKADDR_IN6 serveraddr;
		ZeroMemory(&serveraddr, sizeof(serveraddr));
		serveraddr.sin6_family = AF_INET6;
		int addrlen = sizeof(serveraddr);
		WSAStringToAddress(g_ipaddr, AF_INET6, NULL,
			(SOCKADDR *)&serveraddr, &addrlen);
		serveraddr.sin6_port = htons(g_port);
		retval = connect(g_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
		if (retval == SOCKET_ERROR) err_quit("connect()");
	}
	MessageBox(NULL, "서버에 접속했습니다.", "성공!", MB_ICONINFORMATION);
	//12-4추가 유저 이름 보내기
	retval = send(g_sock, g_comm_msg.name, 15, 0);
	if (retval == SOCKET_ERROR) err_quit("send()");

	// 읽기 & 쓰기 스레드 생성
	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, ReadThread, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, WriteThread, NULL, 0, NULL);
	if (hThread[0] == NULL || hThread[1] == NULL) {
		MessageBox(NULL, "스레드를 시작할 수 없습니다."
			"\r\n프로그램을 종료합니다.",
			"실패!", MB_ICONERROR);
		exit(1);
	}

	g_bStart = TRUE;

	// 스레드 종료 대기
	retval = WaitForMultipleObjects(2, hThread, FALSE, INFINITE);
	retval -= WAIT_OBJECT_0;
	if (retval == 0)
		TerminateThread(hThread[1], 1);
	else
		TerminateThread(hThread[0], 1);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);
	g_bStart = FALSE;

	MessageBox(NULL, "서버가 접속을 끊었습니다", "알림", MB_ICONINFORMATION);
	EnableWindow(g_hButtonSendMsg, FALSE);

	closesocket(g_sock);
	return 0;
}

// 데이터 받기
DWORD WINAPI ReadThread(LPVOID arg)
{
	int retval;
	COMM_MSG comm_msg;
	CHAT_MSG chat_msg;
	DRAWLINE_MSG draw_msg;
	NIGHTBOT_MSG n_msg;
	while (1) {
		retval = recvn(g_sock, (char *)&comm_msg, sizeof(COMM_MSG), 0);
		if (retval == 0 || retval == SOCKET_ERROR) {
			break;
		}
		if (comm_msg.type == WHISPER) {
			retval = recvn(g_sock, comm_msg.name, 15, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			retval = recvn(g_sock, (char *)&chat_msg, comm_msg.size, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			DisplayText("[%s의 귓속말] %s\r\n", comm_msg.name, chat_msg.buf);
		}
		else if (comm_msg.type == NIGHTBOT) { //서버 전송 내용 받기
			retval = recvn(g_sock, n_msg.servername, 15, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			retval = recvn(g_sock, n_msg.buf, comm_msg.size, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			if (!strcmp(n_msg.buf, "없는 명령")) {
				if (SEND_NBOT_FLAG) {
					DisplayText("[%s] %s\r\n", n_msg.servername, n_msg.buf);
					SEND_NBOT_FLAG = 0;
				}
			}
			else {
				DisplayText("[%s] %s\r\n", n_msg.servername, n_msg.buf);
				SEND_NBOT_FLAG = 0;
			}
		}

		else if (comm_msg.type == CHATTING) {//일반 채팅
			retval = recvn(g_sock, comm_msg.name, 15, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			retval = recv(g_sock, (char *)&chat_msg, comm_msg.size, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			DisplayText("[%s] %s\r\n", comm_msg.name, chat_msg.buf);
		}

		else if (comm_msg.type == DRAWLINE || comm_msg.type == DRAWERASER) {
			retval = recvn(g_sock, comm_msg.name, 15, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			retval = recvn(g_sock, (char *)&draw_msg, comm_msg.size, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			if (comm_msg.type == DRAWLINE) {
				g_drawcolor = draw_msg.color;
			}
			else {
				g_drawcolor = RGB(255, 255, 255);
			}

			SendMessage(g_hDrawWnd, WM_DRAWIT,
				(WPARAM)&draw_msg,
				MAKELPARAM(draw_msg.x1, draw_msg.y1));
		}
		else if (comm_msg.type == USERINFO) {
			retval = recvn(g_sock, c_listNum, sizeof(c_listNum), 0);
			listNum = atoi(c_listNum);
			retval = recvn(g_sock, comm_msg.name, 15, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			if (!USERLIST_FLAG) {
				SendMessage(g_hDlg, WM_RESETUSER, 0, 0);
				USERLIST_FLAG = 1;
			}
			SendMessage(g_hDlg, WM_ADDUSER, 0, (LPARAM)comm_msg.name);
		}
		else {
			retval = recvn(g_sock, comm_msg.name, 15, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			retval = recvn(g_sock, (char *)&draw_msg, comm_msg.size, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			g_drawcolor = draw_msg.color;

			SendMessage(g_hDrawWnd, WM_DRAWIT,
				(WPARAM)&draw_msg,
				MAKELPARAM(draw_msg.x1, draw_msg.y1));
		}
	}
	return 0;
}

// 데이터 보내기
DWORD WINAPI WriteThread(LPVOID arg)
{
	int retval;
	COMM_MSG w_comm_msg;
	//유저 리스트 선택 값
	int index = 0;
	// 서버와 데이터 통신
	while (1) {
		// 쓰기 완료 기다리기
		WaitForSingleObject(g_hWriteEvent, INFINITE);
		// 문자열 길이가 0이면 보내지 않음
		if (strlen(g_chatmsg.buf) == 0) {
			// '메시지 전송' 버튼 활성화
			EnableWindow(g_hButtonSendMsg, TRUE);
			// 읽기 완료 알리기
			SetEvent(g_hReadEvent);
			continue;
		}
		w_comm_msg.size = strlen(g_chatmsg.buf) + 1 + NAMESIZE + sizeof(int);
		//나이트봇 기능
		if (g_chatmsg.buf[0] == '!') {
			SEND_NBOT_FLAG = 1;
			w_comm_msg.type = NIGHTBOT;
		}
		else {
			w_comm_msg.type = g_chatmsg.type;
		}
		// 데이터 보내기
		strcpy(w_comm_msg.name, g_comm_msg.name);

		w_comm_msg.index = SendMessage(hUserList, LB_GETCURSEL, 0, 0);
		//유저 리스트 박스 선택시 해당 인덱스 값에 해당하는 유저에게 귓속말 전송
		if (w_comm_msg.index != LB_ERR) { //LB_ERR = -1
			w_comm_msg.type = WHISPER;
			retval = send(g_sock, (char *)&w_comm_msg, sizeof(COMM_MSG), 0);
			if (retval == SOCKET_ERROR) {
				break;
			}
			if (w_comm_msg.type == NIGHTBOT) {
				retval = send(g_sock, g_chatmsg.buf, w_comm_msg.size, 0);
				if (retval == SOCKET_ERROR) {
					break;
				}
			}
			else {
				retval = send(g_sock, (char *)&g_chatmsg, w_comm_msg.size, 0);
				if (retval == SOCKET_ERROR) {
					break;
				}
			}
		}
		//아무도 선택 안할시 전체 채팅
		else {
			retval = send(g_sock, (char *)&w_comm_msg, sizeof(COMM_MSG), 0);
			if (retval == SOCKET_ERROR) {
				break;
			}
			if (w_comm_msg.type == NIGHTBOT) {
				retval = send(g_sock, g_chatmsg.buf, w_comm_msg.size, 0);
				if (retval == SOCKET_ERROR) {
					break;
				}
			}
			else {
				retval = send(g_sock, (char *)&g_chatmsg, w_comm_msg.size, 0);
				if (retval == SOCKET_ERROR) {
					break;
				}
			}
		}
		// '메시지 전송' 버튼 활성화
		EnableWindow(g_hButtonSendMsg, TRUE);
		// 읽기 완료 알리기
		SetEvent(g_hReadEvent);
	}
	return 0;
}

// 자식 윈도우 프로시저
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hDC;
	int cx, cy;
	PAINTSTRUCT ps;
	RECT rect;
	HPEN hPen, hOldPen;
	static HBITMAP hBitmap;
	static HDC hDCMem;
	static int x0, y0;
	static int x1, y1;
	static BOOL bDrawing = FALSE;
	static BOOL bRDrawing = FALSE;
	LOGBRUSH LogBrush;

	switch (uMsg) {
	case WM_CREATE:
		hDC = GetDC(hWnd);

		// 화면을 저장할 비트맵 생성
		cx = GetDeviceCaps(hDC, HORZRES);
		cy = GetDeviceCaps(hDC, VERTRES);
		hBitmap = CreateCompatibleBitmap(hDC, cx, cy);

		// 메모리 DC 생성
		hDCMem = CreateCompatibleDC(hDC);

		// 비트맵 선택 후 메모리 DC 화면을 흰색으로 칠함
		SelectObject(hDCMem, hBitmap);
		SelectObject(hDCMem, GetStockObject(WHITE_BRUSH));
		SelectObject(hDCMem, GetStockObject(WHITE_PEN));
		Rectangle(hDCMem, 0, 0, cx, cy);

		ReleaseDC(hWnd, hDC);
		return 0;
	case WM_LBUTTONDOWN:
		x0 = LOWORD(lParam);
		y0 = HIWORD(lParam);
		bDrawing = TRUE;
		return 0;
	case WM_LBUTTONUP:
		if (g_drawmsg.type != DRAWLINE) {
			g_drawmsg.color = Color;
			g_comm_msg.type = g_drawmsg.type;
			send(g_sock, (char *)&g_comm_msg, sizeof(COMM_MSG), 0);
			send(g_sock, (char *)&g_drawmsg, g_comm_msg.size, 0);
			//유저 정보 전송
		}
		bDrawing = FALSE;
		return 0;
	case WM_RBUTTONDOWN:
		x0 = LOWORD(lParam);
		y0 = HIWORD(lParam);
		bRDrawing = TRUE;
		return 0;
	case WM_RBUTTONUP:
		if (g_drawmsg.type != DRAWLINE) {
			g_drawmsg.color = RColor;
			g_comm_msg.type = g_drawmsg.type;
			send(g_sock, (char*)& g_comm_msg, sizeof(COMM_MSG), 0);
			send(g_sock, (char*)& g_drawmsg, g_comm_msg.size, 0);
			//유저 정보 전송
		}
		bRDrawing = FALSE;
		return 0;
	case WM_MOUSEMOVE:
		if ((bDrawing || bRDrawing) && g_bStart) {
			x1 = LOWORD(lParam);
			y1 = HIWORD(lParam);
			// 선 그리기 메시지 보내기
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y1;
			if (g_drawmsg.type == DRAWLINE || g_drawmsg.type == DRAWERASER) {
				if (g_drawmsg.type == DRAWLINE) {
					g_comm_msg.type = DRAWLINE;
					if (bDrawing) g_drawmsg.color = Color;
					else if (bRDrawing) g_drawmsg.color = RColor;
				}
				else {
					g_comm_msg.type = DRAWERASER;
					g_drawmsg.color = RGB(255, 255, 255);
				}
				send(g_sock, (char*)& g_comm_msg, sizeof(COMM_MSG), 0);
				send(g_sock, (char*)& g_drawmsg, g_comm_msg.size, 0);
				x0 = x1;
				y0 = y1;
			}
		}
		return 0;

	case WM_DRAWIT:
	{
		hDC = GetDC(hWnd);
		DRAWLINE_MSG *newLine = (DRAWLINE_MSG *)wParam;
		hPen = CreatePen(PS_SOLID, newLine->lineWidth, g_drawcolor); // get lineWidth parameter

		// 화면에 그리기
		hOldPen = (HPEN)SelectObject(hDC, hPen);    // change brush 
		switch (newLine->type)
		{
		case DRAWLINE:
			MoveToEx(hDC, newLine->x0, newLine->y0, NULL);
			LineTo(hDC, LOWORD(lParam), HIWORD(lParam));
			break;
		case DRAWTRI:
		{
			POINT trianglePoints[4] = { { LOWORD(lParam), HIWORD(lParam) }, { newLine->x0, HIWORD(lParam) }, { (LOWORD(lParam) - newLine->x0) / 2 + newLine->x0, newLine->y0 }, { LOWORD(lParam), HIWORD(lParam) } };
			if (newLine->isFill)
			{
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else
			{
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}

			Polygon(hDC, trianglePoints, 4);
			break;
		}
		case DRAWRECT:
			if (newLine->isFill)
			{
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else
			{
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Rectangle(hDC, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;
		case DRAWELLIPSE://원일때
		{
			if (newLine->isFill)
			{
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else
			{
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}

			Ellipse(hDC, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;
		}
		case DRAWSTRICTLINE:
		{
			MoveToEx(hDC, newLine->x0, newLine->y0, NULL);
			LineTo(hDC, newLine->x1, newLine->y1);
			break;
		}
		case DRAWRIGHTTRI:
			drawRightTri(hDC, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;

		case DRAWRHOMBUS:
			drawRhombus(hDC, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;

		case DRAWHEXAGON:
			drawHexagon(hDC, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;

		case DRAWSTAR:
			drawStar(hDC, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;

		case DRAWERASER:
			hPen = CreatePen(PS_SOLID, 10, RGB(255,255,255));

			hOldPen = (HPEN)SelectObject(hDC, hPen);
			MoveToEx(hDC, newLine->x0, newLine->y0, NULL);
			LineTo(hDC, LOWORD(lParam), HIWORD(lParam));
		default:
			break;
		}
		SelectObject(hDC, hOldPen);

		// 메모리 비트맵에 그리기
		hOldPen = (HPEN)SelectObject(hDCMem, hPen);
		switch (newLine->type)
		{
		case DRAWLINE:
			MoveToEx(hDCMem, newLine->x0, newLine->y0, NULL);
			LineTo(hDCMem, LOWORD(lParam), HIWORD(lParam));
			break;
		case DRAWTRI:
		{
			POINT trianglePoints[4] = { { LOWORD(lParam), HIWORD(lParam) }, { newLine->x0, HIWORD(lParam) }, 
			{ (LOWORD(lParam) - newLine->x0) / 2 + newLine->x0, newLine->y0 }, { LOWORD(lParam), HIWORD(lParam) } };

			if (newLine->isFill)
			{
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else
			{
				SelectObject(hDCMem, GetStockObject(NULL_BRUSH));
			}

			Polygon(hDCMem, trianglePoints, 4);
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWRECT:
		{
			if (newLine->isFill)
			{
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else
			{
				SelectObject(hDCMem, GetStockObject(NULL_BRUSH));
			}

			Rectangle(hDCMem, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWELLIPSE:
		{
			if (newLine->isFill)
			{
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else
			{
				SelectObject(hDCMem, GetStockObject(NULL_BRUSH));
			}

			Ellipse(hDCMem, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWSTRICTLINE:
		{
			MoveToEx(hDCMem, newLine->x0, newLine->y0, NULL);
			LineTo(hDCMem, newLine->x1, newLine->y1);
			break;
		}

		case DRAWRIGHTTRI:
			drawRightTri(hDCMem, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;

		case DRAWRHOMBUS:
			drawRhombus(hDCMem, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;

		case DRAWHEXAGON:
			drawHexagon(hDCMem, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;

		case DRAWSTAR:
			drawStar(hDCMem, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;

		case DRAWERASER:
			MoveToEx(hDCMem, newLine->x0, newLine->y0, NULL);
			LineTo(hDCMem, LOWORD(lParam), HIWORD(lParam));
		default:
			break;
		}
		SelectObject(hDC, hOldPen);
		DeleteObject(hPen);
		ReleaseDC(hWnd, hDC);
		return 0;
	}

	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);

		// 메모리 비트맵에 저장된 그림을 화면에 전송
		GetClientRect(hWnd, &rect);
		BitBlt(hDC, 0, 0, rect.right - rect.left,
			rect.bottom - rect.top, hDCMem, 0, 0, SRCCOPY);

		EndPaint(hWnd, &ps);
		return 0;
	case WM_DESTROY:
		DeleteObject(hBitmap);
		DeleteDC(hDCMem);
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// 에디트 컨트롤에 문자열 출력
void DisplayText(const char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);

	char cbuf[1024];
	vsprintf(cbuf, fmt, arg);

	int nLength = GetWindowTextLength(g_hEditStatus);
	SendMessage(g_hEditStatus, EM_SETSEL, nLength, nLength);
	SendMessage(g_hEditStatus, EM_REPLACESEL, FALSE, (LPARAM)cbuf);

	va_end(arg);
}

char* getNickName() {
	int index;
	srand(time(NULL));
	index = rand() % 10;

	return NickName[index];
}

void childWndInit(HWND dlg) {
	g_hDrawWnd = CreateWindow("MyWndClass", "그림 그릴 윈도우", WS_CHILD,
		450, 68, 425, 390, dlg, (HMENU)NULL, g_hInst, NULL);
	if (g_hDrawWnd == NULL) exit(1);
	ShowWindow(g_hDrawWnd, SW_SHOW);
	UpdateWindow(g_hDrawWnd);
}

void DisplayThickness(const char* fmt, ...) {
	va_list arg;
	va_start(arg, fmt);

	char cbuf[1024];
	vsprintf_s(cbuf, fmt, arg);

	SendMessage(g_hWidth, WM_SETTEXT, NULL, (LPARAM)cbuf);

	va_end(arg);
}

void drawRightTri(HDC hDC, int x0, int y0, int x1, int y1) {
	MoveToEx(hDC, x0, y0, NULL);
	LineTo(hDC, x1, y1);
	MoveToEx(hDC, x1, y1, NULL);
	LineTo(hDC, x0, y1);
	MoveToEx(hDC, x0, y1, NULL);
	LineTo(hDC, x0, y0);
}

void drawRhombus(HDC hDC, int x0, int y0, int x1, int y1) {
	MoveToEx(hDC, (x0 + x1) / 2, y0, NULL);
	LineTo(hDC, x0, (y0 + y1) / 2);
	MoveToEx(hDC, x0, (y0 + y1) / 2, NULL);
	LineTo(hDC, (x0 + x1) / 2, y1);
	MoveToEx(hDC, (x0 + x1) / 2, y1, NULL);
	LineTo(hDC, x1, (y1 + y0) / 2);
	MoveToEx(hDC, x1, (y1 + y0) / 2, NULL);
	LineTo(hDC, (x0 + x1) / 2, y0);
}

void drawHexagon(HDC hDC, int x0, int y0, int x1, int y1) {
	MoveToEx(hDC, (unsigned int)((x0 + x1) / 2 - (x1 - x0) / 4), y0, NULL);
	LineTo(hDC, (unsigned int)((x0 + x1) / 2 + (x1 - x0) / 4), y0);
	MoveToEx(hDC, (unsigned int)((x0 + x1) / 2 - (x1 - x0) / 4), y1, NULL);
	LineTo(hDC, (unsigned int)((x0 + x1) / 2 + (x1 - x0) / 4), y1);
	MoveToEx(hDC, (unsigned int)((x0 + x1) / 2 - (x1 - x0) / 4), y0, NULL);
	LineTo(hDC, x0, (unsigned int)((y0 + y1) / 2));
	MoveToEx(hDC, (unsigned int)((x0 + x1) / 2 - (x1 - x0) / 4), y1, NULL);
	LineTo(hDC, x0, (unsigned int)((y0 + y1) / 2));
	MoveToEx(hDC, (unsigned int)((x0 + x1) / 2 + (x1 - x0) / 4), y0, NULL);
	LineTo(hDC, x1, (unsigned int)((y0 + y1) / 2));
	MoveToEx(hDC, (unsigned int)((x0 + x1) / 2 + (x1 - x0) / 4), y1, NULL);
	LineTo(hDC, x1, (unsigned int)((y0 + y1) / 2));
}

void drawStar(HDC hDC, int x0, int y0, int x1, int y1) {
	MoveToEx(hDC, (unsigned int)(x0 + x1) / 2, y0, NULL);
	LineTo(hDC, x0, y1);
	MoveToEx(hDC, (unsigned int)(x0 + x1) / 2, y0, NULL);
	LineTo(hDC, x1, y1);
	MoveToEx(hDC, x0, (unsigned int)((y0 + y1) / 2) - ((y1 - y0) / 8), NULL);
	LineTo(hDC, x1, y1);
	MoveToEx(hDC, x1, (unsigned int)((y0 + y1) / 2) - ((y1 - y0) / 8), NULL);
	LineTo(hDC, x0, y1);
	MoveToEx(hDC, x1, (unsigned int)((y0 + y1) / 2) - ((y1 - y0) / 8), NULL);
	LineTo(hDC, x0, (unsigned int)((y0 + y1) / 2) - ((y1 - y0) / 8));
}
