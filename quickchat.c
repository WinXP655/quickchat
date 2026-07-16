// QuickChat LAN Messenger by WinXP655
// Minimalistic chat appplication written based on MicroChat.
// QuickChat Repository: https://github.com/WinXP655/quickchat.
// MicroChat Framework: https://github.com/WinXP655/microchat.

// ======= 1. Headers =======
#include <winsock2.h>
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <commctrl.h>
#include <process.h>
#include "key.h"

// ======= 2. Defines =======
#define SOUND_JOIN   0
#define SOUND_LEAVE  1
#define SOUND_MSG    2
#define BUFFER_SIZE 8192 // Unicode = 2 bytes
#define PORT_QCS 65501
#define PORT_QC  65502
#define QC_LABEL "QC:"
#define IDC_IP 1001
#define ID_MSG_DISPLAY 105
#define ID_EDIT 101
#define ID_SEND 102
#define IDM_EXIT 2001
#define IDM_ABOUT 2003
#define ID_FLASH_TOGGLE 40018
#define ID_SOUND_TOGGLE 40019
#define IDM_COMPUTER_INFO 2005
#define IDM_CLEAR_CHAT 2006
#define IDM_ALWAYS_ON_TOP 2007
#define IDM_PING_REMOTE 2008
#define IDM_CLOSE 2004

// ======= 3. Global variables =======
// ----- Control flags -----
bool isServer = false;
bool xorEnabled = true;
bool loggingEnabled = false;
bool isRunning = true;
bool soundEnabled = true;
bool flashEnabled = true;
bool alwaysOnTop = false;

// ----- Network state -----
SOCKET clientSocket = INVALID_SOCKET;
HANDLE hReceiveThread = NULL;
wchar_t serverIp[16] = L"127.0.0.1";
wchar_t peerIp[16] = L"";
wchar_t peerName[256] = L"";
wchar_t computerName[256] = L"";

// ----- Logging -----
FILE* chatLog = NULL;

// ----- UI handles -----
HWND hWndGlobal = NULL;
HWND hEdit = NULL;
HWND hSendBtn = NULL;
HWND hMsgDisplay = NULL;

// ----- UI resources -----
WNDPROC oldEditProc = NULL;
HFONT hFontBold = NULL;
HFONT hFontMono = NULL;

// ----- Thread sync -----
volatile BOOL mainWindowReady = FALSE;

// ======= 4. Prototypes =======
void EnableVisualStyles();
void GetLocalComputerName();
bool InitializeNetwork(bool server_mode, HINSTANCE hInstance, int nCmdShow);
void PlayNotifySound(int sound);
INT_PTR CALLBACK ConnectDialogProc(HWND, UINT, WPARAM, LPARAM);
void LogMessage(const wchar_t* message);
bool GetDefaultIP(wchar_t* ip_buffer, size_t size);
DWORD WINAPI ShowServerIPMessage(LPVOID lpParam);
void XorObf(unsigned char* data, int len);
void ShowMainWindow(HINSTANCE hInstance, int nCmdShow);
void AddMessage(const wchar_t* msg);
unsigned int __stdcall ReceiveMessages(void* arg);
void CleanupAndExit();
void DisableChatControls(BOOL disable);
void FlashMessageWindow(HWND hWnd);
bool IsValidTargetIP(const wchar_t* ip_str);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void CreateMenuBar(HWND hWnd);
LRESULT CALLBACK EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void SendCurrentMessage(HWND hWnd);
void CloseConnection();
void Disconnect();
void ShowError(const wchar_t* msg, DWORD err);

// ======== 5. Entry Point =======
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	(void)hPrevInstance;
	(void)lpCmdLine;

	EnableVisualStyles();
	GetLocalComputerName();

	int mode = MessageBoxW(NULL,
		L"Run as server?\n"
		L"Yes - Server\n"
		L"No - Client (enter server IP)\n"
		L"Cancel - Exit",
		L"QuickChat", MB_YESNOCANCEL | MB_ICONQUESTION);

	if (mode == IDCANCEL) return 0;
	isServer = (mode == IDYES);

	if (isServer) {
		int proto = MessageBoxW(NULL,
			L"Select a protocol to use for connection\n\n"
			L"Yes - QCS (QuickChat Obfuscated)\n"
			L"No - QC (QuickChat, plain text)\n\n"
			L"Warning: QC is not recommended as main protocol.",
			L"QuickChat", MB_YESNO | MB_ICONQUESTION);

		xorEnabled = (proto == IDYES);
		int enable_logs = MessageBoxW(NULL,
			L"Enable logs for this session?",
			L"QuickChat", MB_YESNO | MB_ICONQUESTION);
		loggingEnabled = (enable_logs == IDYES);

		if (loggingEnabled) {
			chatLog = _wfopen(L"chatlog.txt", L"a");
			if (chatLog == NULL) {
				MessageBoxW(NULL,
					L"Failed to open chat log file. Logging will be disabled for this session.",
					L"QuickChat", MB_OK | MB_ICONWARNING);
				loggingEnabled = false;
			} else {
				time_t now = time(NULL);
				struct tm *t = localtime(&now);
				wchar_t timestamp[64];
				wcsftime(timestamp, 64, L"%H:%M:%S %d/%m/%Y", t);
				fwprintf(chatLog, L"\n=== New Session Started at %ls ===\n", timestamp);
				fflush(chatLog);
			}
		}

		if (!InitializeNetwork(true, hInstance, nCmdShow)) return 0;
	} else {
		while (1) {
			INT_PTR dlg = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(1), NULL, ConnectDialogProc, 0);

			if (dlg == -1) {
				MessageBoxW(NULL,
					L"Could not load connection dialog.",
					L"QuickChat", MB_OK | MB_ICONERROR);
				return 0;
			}

			if (dlg != IDOK) return 0;

			int proto = MessageBoxW(NULL,
				L"Select a protocol to use for connection\n\n"
				L"Yes - QCS (QuickChat Obfuscated)\n"
				L"No - QC (QuickChat, plain text)\n"
				L"Cancel - Return to connection dialog\n\n"
				L"Warning: QC is not recommended as main protocol.",
				L"QuickChat", MB_YESNOCANCEL | MB_ICONQUESTION);

			if (proto == IDCANCEL) continue;
			xorEnabled = (proto == IDYES);
			break;
		}

		if (!InitializeNetwork(false, hInstance, nCmdShow)) return 0;
	}

	PlayNotifySound(SOUND_JOIN);

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}

// ======= 6. Helper Functions =======
void EnableVisualStyles() {
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icex);
}

void GetLocalComputerName() {
	DWORD size = sizeof(computerName) / sizeof(wchar_t);
	GetComputerNameW(computerName, &size);
}

void PlayNotifySound(int sound) {
	if (!soundEnabled) return;

	const wchar_t* filename = NULL;

	switch (sound) {
		case SOUND_JOIN:  filename = L"join.wav";	break;
		case SOUND_LEAVE: filename = L"left.wav";	break;
		case SOUND_MSG:   filename = L"newmsg.wav";  break;
		default: return;
	}

	PlaySoundW(filename, NULL, SND_FILENAME | SND_ASYNC);
}

void LogMessage(const wchar_t* message) {
	// Witing logs in UTF-8 for compatibility
	if (!loggingEnabled || chatLog == NULL) return;

	SYSTEMTIME st;
	GetLocalTime(&st);

	wchar_t timestamp[32];
	swprintf(timestamp, 32,
		L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	char timestamp_utf8[64];
	char msg_utf8[1024];
	WideCharToMultiByte(CP_UTF8, 0, timestamp, -1, timestamp_utf8, sizeof(timestamp_utf8), NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, message, -1, msg_utf8, sizeof(msg_utf8), NULL, NULL);

	fprintf(chatLog, "[%s] %s\n", timestamp_utf8, msg_utf8);
	fflush(chatLog);
}

bool GetDefaultIP(wchar_t *ip_buffer, size_t size) {
	// UDP hack: connect to 8.8.8.8:53 (DNS), then getsockname returns local IP.
	// Works only when there is a route to the internet.
	// Fallback: gethostname + gethostbyname is not used because it's deprecated.
	// Returns 127.0.0.1 if no route.
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return false;

	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET) {
		MessageBoxW(NULL, L"Failed to initialize socket for UDP.", L"QuickChat", MB_OK | MB_ICONWARNING);
		WSACleanup();
		return false;
	}

	struct sockaddr_in remote = {0};
	remote.sin_family = AF_INET;
	remote.sin_port = htons(53);
	remote.sin_addr.s_addr = inet_addr("8.8.8.8");

	if (connect(s, (struct sockaddr*)&remote, sizeof(remote)) != 0) {
		MessageBoxW(NULL, L"Failed to connect to 8.8.8.8.", L"QuickChat", MB_OK | MB_ICONWARNING);
		closesocket(s);
		WSACleanup();
		return false;
	}

	struct sockaddr_in local;
	int len = sizeof(local);
	if (getsockname(s, (struct sockaddr*)&local, &len) != 0) {
		MessageBoxW(NULL, L"Failed to get server IP address.", L"QuickChat", MB_OK | MB_ICONWARNING);
		closesocket(s);
		WSACleanup();
		return false;
	}

	char ip_utf8[16];
	strncpy(ip_utf8, inet_ntoa(local.sin_addr), 15);
	ip_utf8[15] = '\0';
	MultiByteToWideChar(CP_UTF8, 0, ip_utf8, -1, ip_buffer, size);

	closesocket(s);
	WSACleanup();
	return true;
}

void AddMessage(const wchar_t* msg) {
	// Safe display: checks window handle, buffer size, SendMessage result.
	if (!hMsgDisplay || !IsWindow(hMsgDisplay) || !msg || !*msg) return;
	if (wcslen(msg) > BUFFER_SIZE) {
		wchar_t longmsg[255] = L"[ERROR]: Message is too long to be displayed.";
		AddMessage(longmsg);
		if (isServer) LogMessage(longmsg);
		return;
	}
	int len = GetWindowTextLengthW(hMsgDisplay);

	SendMessageW(hMsgDisplay, EM_SETSEL, len, len);
	if (len > 0) SendMessageW(hMsgDisplay, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");

	if (!SendMessageW(hMsgDisplay, EM_REPLACESEL, FALSE, (LPARAM)msg)) {
		if (isServer) {
			wchar_t error_msg[256];
			swprintf(error_msg, 256, L"[ERROR]: Failed to display message. Error: %lu.", GetLastError());
			if (isServer) LogMessage(error_msg);
		}
		SetFocus(hEdit);
		return;
	}

	SendMessageW(hMsgDisplay, WM_VSCROLL, SB_BOTTOM, 0);
}

void CleanupAndExit() {
	isRunning = 0;

	if (clientSocket != INVALID_SOCKET) {
		shutdown(clientSocket, SD_BOTH);
		closesocket(clientSocket);
		clientSocket = INVALID_SOCKET;
	}

	// Never do waiting on thread with recv
	// It is always bad idea
	if (hReceiveThread != NULL) {
		CloseHandle(hReceiveThread);
		hReceiveThread = NULL;
	}

	if (chatLog != NULL) {
		time_t now = time(NULL);
		struct tm *t = localtime(&now);
		wchar_t timestamp[64];
		wcsftime(timestamp, 64, L"%H:%M:%S %d/%m/%Y", t);
		fwprintf(chatLog, L"=== Session Ended at %ls ===\n\n", timestamp);
		fclose(chatLog);
		chatLog = NULL;
	}

	WSACleanup();
	PostQuitMessage(0);
}

void DisableChatControls(BOOL disable) {
	if (hEdit && IsWindow(hEdit)) EnableWindow(hEdit, !disable);
	if (hSendBtn && IsWindow(hSendBtn)) EnableWindow(hSendBtn, !disable);
}

void FlashMessageWindow(HWND hWnd) {
	if (!flashEnabled) return;

	FLASHWINFO fi;
	fi.cbSize = sizeof(FLASHWINFO);
	fi.hwnd = hWnd;
	fi.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
	fi.uCount = 3;
	fi.dwTimeout = 0;

	FlashWindowEx(&fi);
}

bool IsValidTargetIP(const wchar_t* ip_str) {
	int o1, o2, o3, o4;
	if (swscanf(ip_str, L"%d.%d.%d.%d", &o1, &o2, &o3, &o4) != 4) return false;

	// Block:
	//   0.x.x.x
	//   x.x.x.0 (network address)
	//   x.x.x.255 (network broadcast)
	//   224.0.0.0 - 239.255.255.255 (multicast)
	//   240.0.0.0 - 255.255.255.255 (reserved + global broadcast)
	if (o1 == 0) return false;
	if (o4 == 0) return false;
	if (o4 == 255) return false;
	if (o1 >= 224 && o1 <= 239) return false;
	if (o1 >= 240) return false;

	// Allow any other IP
	return true;
}

void CloseConnection() {
	// Only server allowed to kick client from chat
	// This is by design
	if (!isServer) return;

	if (clientSocket == INVALID_SOCKET) {
		AddMessage(L"[INFO]: No active connection to close.");
		return;
	}

	isRunning = 0;

	shutdown(clientSocket, SD_BOTH);

	struct linger linger_opt = { 1, 0 };
	setsockopt(clientSocket, SOL_SOCKET, SO_LINGER, (char*)&linger_opt, sizeof(linger_opt));

	closesocket(clientSocket);
	clientSocket = INVALID_SOCKET;

	DisableChatControls(TRUE);

	wchar_t closeconn[256] = L"[DISCONNECT]: Server closed the connection.";
	AddMessage(closeconn);
	LogMessage(closeconn);
}

void Disconnect() {
	wchar_t leave_msg[256];
	swprintf(leave_msg, 256, L"[DISCONNECT]: %ls left the chat.", computerName);

	if (clientSocket != INVALID_SOCKET && isRunning) {
		// Convert wchat_t to UTF-8 before sending
		char utf8_msg[256];
		WideCharToMultiByte(CP_UTF8, 0, leave_msg, -1, utf8_msg, sizeof(utf8_msg), NULL, NULL);
		int msg_len = strlen(utf8_msg);

		unsigned char encbuf[256];
		memcpy(encbuf, utf8_msg, msg_len);
		XorObf(encbuf, msg_len);
		send(clientSocket, (char*)encbuf, msg_len, 0);
	}

	AddMessage(leave_msg);
	if (isServer) LogMessage(leave_msg);
	PlayNotifySound(SOUND_LEAVE);
	CleanupAndExit();
}

void ShowError(const wchar_t* msg, DWORD err) {
	wchar_t buffer[512];
	swprintf(buffer, 512, L"%ls. Error: %lu", msg, err);
	MessageBoxW(NULL, buffer, L"QuickChat", MB_OK | MB_ICONERROR);
}

// ======= 7. Network Core =======
bool InitializeNetwork(bool server_mode, HINSTANCE hInstance, int nCmdShow) {
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		ShowError(L"WSAStartup failed.", WSAGetLastError());
		return false;
	}

	if (server_mode) {
		SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (server_fd == INVALID_SOCKET) {
			ShowError(L"Failed to create socket.", WSAGetLastError());
			WSACleanup();
			return false;
		}

		int active_port = xorEnabled ? PORT_QCS : PORT_QC;
		struct sockaddr_in server_addr = {0};
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = INADDR_ANY;
		server_addr.sin_port = htons(active_port);

		if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
			DWORD err = WSAGetLastError();
			if (err == WSAEADDRINUSE) {
				MessageBoxW(NULL,
					L"Port is already in use.\nAnother QuickChat instance may be running.",
					L"QuickChat", MB_OK | MB_ICONWARNING);
			} else {
				ShowError(L"Bind failed.", err);
			}
			closesocket(server_fd);
			WSACleanup();
			return false;
		}

		wchar_t bind_msg[256];
		const wchar_t* mode_str = xorEnabled ? L"QCS (Encrypted)" : L"QC (Plaintext)";
		swprintf(bind_msg, 256, L"[INFO]: Server started: %ls on port %d.", mode_str, active_port);
		LogMessage(bind_msg);

		if (listen(server_fd, 1) == SOCKET_ERROR) {
			ShowError(L"Listen failed.", WSAGetLastError());
			closesocket(server_fd);
			WSACleanup();
			return false;
		}

		GetDefaultIP(serverIp, sizeof(serverIp) / sizeof(wchar_t));
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ShowServerIPMessage, NULL, 0, NULL);

		while (1) {
			struct sockaddr_in client_addr;
			int addr_len = sizeof(client_addr);

			SOCKET temp_client = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
			if (temp_client == INVALID_SOCKET) {
				ShowError(L"Accept failed.", WSAGetLastError());
				continue;
			}

			struct timeval tv;
			tv.tv_sec = 5;
			tv.tv_usec = 0;
			setsockopt(temp_client, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));

			char hs[256];
			int recv_len = recv(temp_client, hs, sizeof(hs) - 1, 0);

			if (recv_len <= 0) {
				LogMessage(L"[SECURITY]: Empty or timed-out handshake. Connection closed.");
				closesocket(temp_client);
				continue;
			}

			// Waiting for specific handshake
			XorObf((unsigned char*)hs, recv_len);
			hs[recv_len] = '\0';

			if (strncmp(hs, QC_LABEL, strlen(QC_LABEL)) != 0) {
				LogMessage(L"[SECURITY]: Invalid handshake. Connection closed.");
				closesocket(temp_client);
				continue;
			}

			const char* name_ptr = hs + strlen(QC_LABEL);
			if (*name_ptr == '\0') {
				LogMessage(L"[SECURITY]: Empty name in handshake. Connection closed.");
				closesocket(temp_client);
				continue;
			}

			clientSocket = temp_client;

			char ip_utf8[16];
			strncpy(ip_utf8, inet_ntoa(client_addr.sin_addr), 15);
			ip_utf8[15] = '\0';
			MultiByteToWideChar(CP_UTF8, 0, ip_utf8, -1, peerIp, sizeof(peerIp) / sizeof(wchar_t));

			MultiByteToWideChar(CP_UTF8, 0, name_ptr, -1, peerName, sizeof(peerName) / sizeof(wchar_t));

			break;
		}

		closesocket(server_fd);

		// Send handshake reply (with Unicode-to-ANSI conversion)
		char hs_reply[256];
		int pos = snprintf(hs_reply, sizeof(hs_reply), "%s", QC_LABEL);
		WideCharToMultiByte(CP_UTF8, 0, computerName, -1, hs_reply + pos, sizeof(hs_reply) - pos, NULL, NULL);
		int hs_r_len = strlen(hs_reply);
		XorObf((unsigned char*)hs_reply, hs_r_len);
		send(clientSocket, hs_reply, hs_r_len, 0);

		ShowMainWindow(hInstance, nCmdShow);

		while (!mainWindowReady) {
			Sleep(10);
			MSG msg;
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		wchar_t sys_msg[256];
		swprintf(sys_msg, 256, L"[CONNECT]: %ls connected from %ls.", peerName, peerIp);
		AddMessage(sys_msg);
		LogMessage(sys_msg);
	} else {
		clientSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (clientSocket == INVALID_SOCKET) {
			ShowError(L"Failed to create socket.", WSAGetLastError());
			WSACleanup();
			return false;
		}

		struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };
		setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

		int active_port = xorEnabled ? PORT_QCS : PORT_QC;
		struct sockaddr_in server_addr = {0};
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(active_port);

		char server_ip_utf8[16];
		WideCharToMultiByte(CP_UTF8, 0, serverIp, -1, server_ip_utf8, sizeof(server_ip_utf8), NULL, NULL);
		server_addr.sin_addr.s_addr = inet_addr(server_ip_utf8);

		if (connect(clientSocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
			int err = WSAGetLastError();
			switch (err) {
				case WSAETIMEDOUT:
					MessageBoxW(NULL, L"Connection timed out.", L"QuickChat", MB_OK | MB_ICONERROR);
					break;
				case WSAECONNREFUSED:
					MessageBoxW(NULL, L"Connection refused.", L"QuickChat", MB_OK | MB_ICONERROR);
					break;
				default:
					ShowError(L"Connection failed.", err);
			}
			closesocket(clientSocket);
			WSACleanup();
			return false;
		}

		timeout.tv_sec = 0;
		setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

		// Disabling "Weak Host Model" on pre-Vista versions
		// On Vista+ systems - switching from "soft bind" to "hard bind"
		struct sockaddr_in server_info;
		int len = sizeof(server_info);
		getsockname(clientSocket, (struct sockaddr*)&server_info, &len);
		wchar_t ip_w[16];
		DWORD ip_len = 16;
		WSAAddressToStringW((LPSOCKADDR)&server_info, sizeof(server_info), NULL, ip_w, &ip_len);
		wcscpy(peerIp, ip_w);

		// Send handshake (Unicode → UTF-8)
		char hs[256];
		int pos = snprintf(hs, sizeof(hs), "%s", QC_LABEL);
		WideCharToMultiByte(CP_UTF8, 0, computerName, -1, hs + pos, sizeof(hs) - pos, NULL, NULL);
		int hs_len = strlen(hs);
		XorObf((unsigned char*)hs, hs_len);
		send(clientSocket, hs, hs_len, 0);

		char hs_reply[256];
		int recv_len = recv(clientSocket, hs_reply, sizeof(hs_reply) - 1, 0);
		if (recv_len <= 0) {
			ShowError(L"Failed to receive peer handshake.", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return false;
		}

		XorObf((unsigned char*)hs_reply, recv_len);
		hs_reply[recv_len] = '\0';

		if (strncmp(hs_reply, QC_LABEL, strlen(QC_LABEL)) != 0) {
			MessageBoxW(NULL, L"Invalid QuickChat handshake.", L"QuickChat", MB_OK | MB_ICONERROR);
			closesocket(clientSocket);
			WSACleanup();
			return false;
		}

		const char* name_ptr = hs_reply + strlen(QC_LABEL);
		MultiByteToWideChar(CP_UTF8, 0, name_ptr, -1, peerName, 256);
		if (peerName[0] == L'\0') wcscpy(peerName, L"<Unknown>");

		ShowMainWindow(hInstance, nCmdShow);

		while (!mainWindowReady) {
			Sleep(10);
			MSG msg;
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		wchar_t sys_msg[256];
		swprintf(sys_msg, 256, L"[CONNECT]: Connected to %ls at %ls.", peerName, serverIp);
		AddMessage(sys_msg);
	}

	unsigned int threadID;
	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, ReceiveMessages, NULL, 0, &threadID);

	if (hThread == NULL) {
		ShowError(L"Failed to start receive thread.", GetLastError());
		CleanupAndExit();
		return false;
	}

	return true;
}

void XorObf(unsigned char *data, int len) {
	if (!xorEnabled) return;

	// In case if you want old system back - static key
	// unsigned char key [] = { ... };

	unsigned char k[KEY_LEN];
	memcpy(k, key, KEY_LEN);

	for (int i = 0; i < len; i++) data[i] ^= k[i % KEY_LEN];

	memset(k, 0, KEY_LEN);
}

unsigned int __stdcall ReceiveMessages(void* arg) {
	(void)arg;

	char buffer[BUFFER_SIZE];

	while (isRunning) {
		int bytes = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
		if (!isRunning)
			break;

		if (bytes == SOCKET_ERROR) {
			int err = WSAGetLastError();
			if (err == WSAETIMEDOUT) continue;

			wchar_t msg[256];
			swprintf(msg, 256, L"[ERROR]: Connection with remote computer lost. (WSA error: %d)", err);
			AddMessage(msg);
			if (isServer) LogMessage(msg);

			isRunning = 0;
			break;
		}

		if (bytes == 0) {
			wchar_t msg[256] = L"[DISCONNECT]: Remote computer has closed the connection.";
			AddMessage(msg);
			if (isServer) LogMessage(msg);
			DisableChatControls(TRUE);
			isRunning = 0;
			break;
		}

		XorObf((unsigned char*)buffer, bytes);
		buffer[bytes] = '\0';

		// Custom ping
		if (strcmp(buffer, "QCPING") == 0) {
			char pong_msg[] = "QCPONG";
			int len = strlen(pong_msg);
			XorObf((unsigned char*)pong_msg, len);
			send(clientSocket, pong_msg, len, 0);
			continue;
		}

		if (strcmp(buffer, "QCPONG") == 0) {
			wchar_t pingmsg[256] = L"[INFO]: Client responded to PING packet.";
			LogMessage(pingmsg);
			AddMessage(pingmsg);
			continue;
		}

		FlashMessageWindow(hWndGlobal);

		if (strncmp(buffer, "[DISCONNECT]", 12) == 0 ||
			strncmp(buffer, "[DISCONNECTED]", 14) == 0) {
			DisableChatControls(TRUE);
			PlayNotifySound(SOUND_LEAVE);
		} else {
			PlayNotifySound(SOUND_MSG);
		}

		// Convert received buffer to Unicode for display
		wchar_t wbuffer[BUFFER_SIZE];
		MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wbuffer, BUFFER_SIZE);
		AddMessage(wbuffer);
		if (isServer) LogMessage(wbuffer);
	}

	_endthread();
	return 0;
}

void SendCurrentMessage(HWND hWnd) {
	// Counting characters including formatting.
	int msglen = GetWindowTextLengthW(hEdit);
	int maxallowed = (BUFFER_SIZE - 1) - wcslen(computerName) - 4;
	if (msglen > maxallowed) {
		wchar_t longsend[255] = L"[ERROR]: Message is too long to be sent.";
		AddMessage(longsend);
		if (isServer) LogMessage(longsend);
		return;
	}
	
	wchar_t buffer[BUFFER_SIZE];
	int text_len = GetWindowTextW(hEdit, buffer, BUFFER_SIZE - 1);
	buffer[text_len] = L'\0';

	wchar_t* start = buffer;
	while (*start == L' ' || *start == L'\t' || *start == L'\r' || *start == L'\n') {
		start++;
	}

	int len = wcslen(start);
	if (len > 0) {
		wchar_t* end = start + len - 1;
		while (end >= start && (*end == L' ' || *end == L'\t' || *end == L'\r' || *end == L'\n')) {
			*end = L'\0';
			end--;
			len--;
		}
	}

	if (len > 0) {
		wchar_t full_msg[BUFFER_SIZE + 128];
		swprintf(full_msg, BUFFER_SIZE + 128, L"[%ls]: %ls", computerName, start);

		// Convert to UTF-8 for transmission
		char utf8_buffer[BUFFER_SIZE + 128];
		WideCharToMultiByte(CP_UTF8, 0, full_msg, -1, utf8_buffer, sizeof(utf8_buffer), NULL, NULL);
		int msg_len = strlen(utf8_buffer);

		unsigned char encbuf[BUFFER_SIZE + 128];
		memcpy(encbuf, utf8_buffer, msg_len);
		XorObf(encbuf, msg_len);
		AddMessage(full_msg);
		if (isServer) LogMessage(full_msg);
		int send_result = send(clientSocket, (char*)encbuf, msg_len, 0);

		if (send_result == SOCKET_ERROR) {
			int error_code = WSAGetLastError();
			const wchar_t* error_desc = L"Unknown socket error";

			switch(error_code) {
				case WSAECONNRESET:
					error_desc = L"Connection reset by peer (unexpected reset)";
					break;
				case WSAENOTCONN:
					error_desc = L"Socket is not connected";
					break;
				case WSAETIMEDOUT:
					error_desc = L"Connection timed out";
					break;
				case WSAECONNABORTED:
					error_desc = L"Connection aborted";
					break;
				default:
					error_desc = L"Unknown error";
					break;
			}

			wchar_t error_msg[512];
			swprintf(error_msg, 512, L"[ERROR]: Failed to send message. Error: %ls (WSA error: %d)", error_desc, error_code);
			AddMessage(error_msg);
			if (isServer) LogMessage(error_msg);
		}
	}

	SetWindowTextW(hEdit, L"");

	MSG nextMsg;
	while (PeekMessage(&nextMsg, hWnd, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {
		if (nextMsg.message == WM_KEYDOWN && nextMsg.wParam == VK_RETURN) continue;
		DispatchMessage(&nextMsg);
	}
}

// ======= 8. User Interface =======
INT_PTR CALLBACK ConnectDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	(void)lParam;
	
	switch (msg) {
		case WM_INITDIALOG:
			SetDlgItemTextW(hwnd, IDC_IP, L"127.0.0.1");
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK) {
				wchar_t ip[16];
				GetDlgItemTextW(hwnd, IDC_IP, ip, 16);

				wchar_t *p = ip;
				while (*p == L' ') p++;
				int len = wcslen(p);
				while (len > 0 && p[len-1] == L' ') {
					p[len-1] = L'\0';
					len--;
				}

				if (wcslen(p) == 0) {
					MessageBoxW(hwnd, L"Server IP is required for connection.", L"QuickChat", MB_OK | MB_ICONWARNING);
					SetFocus(GetDlgItem(hwnd, IDC_IP));
					return TRUE;
				}

				if (!IsValidTargetIP(p)) {
					MessageBoxW(hwnd, L"This IP address is valid, but cannot be used for connection.", L"QuickChat", MB_OK | MB_ICONWARNING);
					SetFocus(GetDlgItem(hwnd, IDC_IP));
					return TRUE;
				}

				wcscpy(serverIp, p);
				serverIp[sizeof(serverIp)/sizeof(wchar_t) - 1] = L'\0';
				EndDialog(hwnd, IDOK);
			} else if (LOWORD(wParam) == IDCANCEL) {
				EndDialog(hwnd, IDCANCEL);
			}
			return TRUE;
	}
	return FALSE;
}

DWORD WINAPI ShowServerIPMessage(LPVOID lpParam) {
	(void)lpParam;

	wchar_t message[512];
	const wchar_t* mode = xorEnabled ? L"QCS (QuickChat Obfuscated)" : L"QC (QuickChat, plain text)";
	swprintf(message, 512,
		L"Server IP: %ls\n"
		L"Protocol: %ls\n"
		L"Share with users to connect to server.",
		serverIp, mode);

	MessageBoxW(NULL, message, L"QuickChat", MB_OK | MB_ICONINFORMATION);
	return 0;
}

void ShowMainWindow(HINSTANCE hInstance, int nCmdShow) {
	WNDCLASSW wc = {0};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"QuickChatWndClass";
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.hIcon = LoadIconW(hInstance, L"ICON");
	RegisterClassW(&wc);

	wchar_t title[256];
	const wchar_t* protocol_label = xorEnabled ? L"QCS" : L"QC";
	swprintf(title, 256, L"QuickChat (%ls) - %ls", protocol_label, peerName);

	HWND hWnd = CreateWindowW(L"QuickChatWndClass", title,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 600, 395,
		NULL, NULL, hInstance, NULL);

	if (!hWnd) {
		MessageBoxW(NULL, L"Failed to create main window.", L"QuickChat", MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}

	hWndGlobal = hWnd;
	CreateMenuBar(hWnd);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			hFontBold = CreateFontW(
				-11, 0, 0, 0, FW_BOLD,
				FALSE, FALSE, FALSE, DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
				L"Tahoma"
			);

			hFontMono = CreateFontW(
				-12, 0, 0, 0, FW_NORMAL,
				FALSE, FALSE, FALSE, DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
				L"Lucida Console"
			);

			hMsgDisplay = CreateWindowW(L"EDIT", L"", 
				WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | 
				ES_READONLY | ES_AUTOVSCROLL,
				0, 0, 594, 278, hWnd, (HMENU)ID_MSG_DISPLAY, NULL, NULL);

			hEdit = CreateWindowW(L"EDIT", L"", 
				WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL,
				0, 278, 510, 43, hWnd, (HMENU)ID_EDIT, NULL, NULL);

			hSendBtn = CreateWindowW(L"BUTTON", L"Send", 
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				510, 278, 85, 43, hWnd, (HMENU)ID_SEND, NULL, NULL);

			SendMessageW(hEdit, EM_SETLIMITTEXT, BUFFER_SIZE - 1, 0);
			SendMessageW(hMsgDisplay, WM_SETFONT, (WPARAM)hFontMono, TRUE);
			SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFontMono, TRUE);
			SendMessageW(hSendBtn, WM_SETFONT, (WPARAM)hFontBold, TRUE);

			SetLastError(0);
			oldEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);
			if (!oldEditProc && GetLastError() != 0) {
				AddMessage(L"[WARNING]: Edit subclass setup failed. Enter and Ctrl+A may not work as expected.");
			}

			mainWindowReady = TRUE;
			return 0;
		}

		case WM_COMMAND: {
			if (LOWORD(wParam) == ID_SEND) {
				SendCurrentMessage(hWnd);
				SetFocus(hEdit);
			} else if (LOWORD(wParam) == IDM_CLOSE) {
				if (isServer) {
					if (!isRunning) {
						AddMessage(L"[ERROR]: Remote computer is not connected.");
						return 0L;
					}
					
					if (MessageBoxW(hWnd, L"Close current connection?", L"QuickChat", MB_ICONQUESTION | MB_YESNO) == IDYES) {
						CloseConnection();
					}
				}
			} else if (LOWORD(wParam) == IDM_EXIT) {
				if (!isRunning) CleanupAndExit();

				if (MessageBoxW(hWnd, L"Leave current chat?", L"QuickChat", MB_ICONQUESTION | MB_YESNO) == IDYES) {
					Disconnect();
				}
			} else if (LOWORD(wParam) == IDM_ABOUT) {
				MessageBoxW(hWnd, L"QuickChat\nWinXP655, 2026", L"About QuickChat", MB_OK | MB_ICONINFORMATION);
			} else if (LOWORD(wParam) == ID_SOUND_TOGGLE) {
				soundEnabled = !soundEnabled;
				CheckMenuItem(GetMenu(hWnd), ID_SOUND_TOGGLE, MF_BYCOMMAND | (soundEnabled ? MF_CHECKED : MF_UNCHECKED));
				return 0;
			} else if (LOWORD(wParam) == ID_FLASH_TOGGLE) {
				flashEnabled = !flashEnabled;
				CheckMenuItem(GetMenu(hWnd), ID_FLASH_TOGGLE, MF_BYCOMMAND | (flashEnabled ? MF_CHECKED : MF_UNCHECKED));
				return 0;
			} else if (LOWORD(wParam) == IDM_COMPUTER_INFO) {
				// We get information from all available data
				const wchar_t* connected = isRunning ? L"Yes" : L"No";
				const wchar_t* protocol_label = xorEnabled ? L"QCS" : L"QC";
				const wchar_t* ip = isServer ? peerIp : serverIp;
				const wchar_t* display_name = (peerName[0] != L'\0') ? peerName : L"N/A";

				wchar_t info_msg[512];
				swprintf(info_msg, 512,
					L"Remote Name: %ls\n"
					L"Remote IP: %ls\n"
					L"Protocol: %ls\n"
					L"Connected: %ls",
					display_name, ip, protocol_label, connected);

				MessageBoxW(hWnd, info_msg, L"QuickChat - Remote Computer", MB_OK | MB_ICONINFORMATION);
			} else if (LOWORD(wParam) == IDM_CLEAR_CHAT) {
				SetWindowTextW(hMsgDisplay, L"");
			} else if (LOWORD(wParam) == IDM_ALWAYS_ON_TOP) {
				alwaysOnTop = !alwaysOnTop;
				
				SetWindowPos(hWnd,
					alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
					0, 0, 0, 0,
					SWP_NOMOVE | SWP_NOSIZE);

				CheckMenuItem(GetMenu(hWnd), IDM_ALWAYS_ON_TOP, MF_BYCOMMAND | (alwaysOnTop ? MF_CHECKED : MF_UNCHECKED));
				return 0;
			} else if (LOWORD(wParam) == IDM_PING_REMOTE) {
				if (!isRunning || clientSocket == INVALID_SOCKET) {
					AddMessage(L"[ERROR]: Remote computer not connected.");
					return 0;
				}

				char ping_msg[] = "QCPING";
				int len = strlen(ping_msg);
				XorObf((unsigned char*)ping_msg, len);
				send(clientSocket, ping_msg, len, 0);
				return 0;
			}
			return 0;
		}

		case WM_CLOSE: {
			if (!isRunning) CleanupAndExit();

			if (MessageBoxW(hWnd, L"Leave current chat?", L"QuickChat", MB_ICONQUESTION | MB_YESNO) == IDYES) {
				Disconnect();
			}

			return 0;
		}

		case WM_DESTROY: {
			// Preventing GDI leaks
			if (oldEditProc) {
				SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)oldEditProc);
			}

			if (hFontBold) {
				DeleteObject(hFontBold);
				hFontBold = NULL;
			}

			if (hFontMono) {
				DeleteObject(hFontMono);
				hFontMono = NULL;
			}

			CleanupAndExit();
			return 0;
		}
		
		case WM_SIZE: {
			int w = LOWORD(lParam);
			int h = HIWORD(lParam);

			int edit_height = 43;
			int send_width = 85;

			SetWindowPos(hMsgDisplay, NULL, 0, 0, w, h - edit_height, SWP_NOZORDER);
			SetWindowPos(hEdit, NULL, 0, h - edit_height, w - send_width, edit_height, SWP_NOZORDER);
			SetWindowPos(hSendBtn, NULL, w - send_width, h - edit_height, send_width, edit_height, SWP_NOZORDER);
			return 0;
		}

		case WM_SETFOCUS: {
			SetFocus(hEdit);
			break;
		}
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void CreateMenuBar(HWND hWnd) {
	HMENU hMenu = CreateMenu();

	HMENU hConn = CreatePopupMenu();
	if (isServer) AppendMenuW(hConn, MF_STRING, IDM_CLOSE, L"Close Connection");
	AppendMenuW(hConn, MF_STRING, IDM_PING_REMOTE, L"Ping Remote");
	AppendMenuW(hConn, MF_STRING, IDM_COMPUTER_INFO, L"Computer Info");
	AppendMenuW(hConn, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hConn, MF_STRING, IDM_EXIT, L"Leave Chat");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hConn, L"Connection");

	HMENU hView = CreatePopupMenu();
	AppendMenuW(hView, MF_STRING | MF_UNCHECKED, IDM_ALWAYS_ON_TOP, L"Always On Top");
	AppendMenuW(hView, MF_STRING, IDM_CLEAR_CHAT, L"Clear Chat");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hView, L"View");

	HMENU hOpts = CreatePopupMenu();
	AppendMenuW(hOpts, MF_STRING | MF_UNCHECKED, ID_FLASH_TOGGLE, L"Window Flash");
	AppendMenuW(hOpts, MF_STRING | MF_UNCHECKED, ID_SOUND_TOGGLE, L"Sound");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hOpts, L"Options");

	HMENU hHelp = CreatePopupMenu();
	AppendMenuW(hHelp, MF_STRING, IDM_ABOUT, L"About");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"Help");

	SetMenu(hWnd, hMenu);

	CheckMenuItem(hMenu, IDM_ALWAYS_ON_TOP, MF_BYCOMMAND | (alwaysOnTop ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hMenu, ID_SOUND_TOGGLE, MF_BYCOMMAND | (soundEnabled ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hMenu, ID_FLASH_TOGGLE, MF_BYCOMMAND | (flashEnabled ? MF_CHECKED : MF_UNCHECKED));
}

LRESULT CALLBACK EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_GETDLGCODE) {
		return DLGC_WANTALLKEYS | CallWindowProcW(oldEditProc, hWnd, uMsg, wParam, lParam);
	} else if (uMsg == WM_KEYDOWN) {
		// Overriding defaults for custom handlers

		// Ctrl+A
		if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
			SendMessageW(hWnd, EM_SETSEL, 0, -1);
			return 0;
		}

		// Enter
		if (wParam == VK_RETURN) {
			if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
				return CallWindowProcW(oldEditProc, hWnd, uMsg, wParam, lParam);
			} else {
				PostMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(ID_SEND, 0), 0);
				return 0;
			}
		}
	}

	return CallWindowProcW(oldEditProc, hWnd, uMsg, wParam, lParam);
}