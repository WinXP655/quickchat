// QuickChat LAN Messenger by WinXP655.
// Minimalistic chat appplication written based on MicroChat.
// QuickChat Repository: https://github.com/WinXP655/quickchat.
// MicroChat Framework: https://github.com/WinXP655/microchat.
// Distributed under MIT License.

// ======= 1. Headers =======
#include <winsock2.h>
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <commctrl.h>
#include <process.h>
#include <shellapi.h>
#include "key.h" // XOR key here

// ======= 2. Defines =======
// --- UI Controls ---
#define ID_EDIT 101
#define ID_SEND 102
#define ID_MSG_DISPLAY 103

// --- Dialog ---
#define IDC_IP 1001

// --- Menu: Connection ---
#define IDM_CLOSE 2001
#define IDM_LEAVE 2002
#define IDM_SAVE  2003

// --- Menu: View ---
#define IDM_ALWAYS_ON_TOP 2101
#define IDM_CLEAR_CHAT 2102

// --- Menu: Options ---
#define ID_FLASH_TOGGLE 2201
#define ID_SOUND_TOGGLE 2202

// --- Menu: Help ---
#define IDM_ABOUT 2301

// --- Menu: Other ---
#define IDM_COMPUTER_INFO 2401
#define IDM_PING_REMOTE 2402

// --- Non-changable ---
#define SOUND_JOIN 0
#define SOUND_LEAVE 1
#define SOUND_MSG 2
#define BUFFER_SIZE 8192 // Unicode = 2 bytes
#define PORT_QCS 65501
#define PORT_QC 65502
#define QC_LABEL "QC:"
#define INI_FILE L"quickchat.ini"

// ======= 3. Global variables =======
// ----- Control flags -----
bool is_server = false;
bool xor_enabled = true;
bool logging_enabled = false;
bool is_running = true;
bool sound_enabled = true;
bool flash_enabled = true;
bool always_on_top = false;

// ----- Network state -----
SOCKET client_socket = INVALID_SOCKET;
HANDLE hReceiveThread = NULL;
wchar_t server_ip[16] = L"127.0.0.1";
wchar_t peer_ip[16] = L"";
wchar_t peer_name[256] = L"";
wchar_t computer_name[256] = L"";

// ----- Logging -----
FILE* chat_log = NULL;

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
void LoadSettings();
static bool IsValidTextExtension(const wchar_t *path);
static wchar_t* ReadTextFileContent(const wchar_t *path, HWND hWnd);
static void InsertTextIntoEdit(const wchar_t *text);

// ======== 5. Entry Point and Settings =======
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	(void)hPrevInstance;
	(void)lpCmdLine;

	EnableVisualStyles();
	GetLocalComputerName();

	LoadSettings();

	int mode = MessageBoxW(NULL,
		L"Welcome to QuickChat!\n\n"
		L"What do you want to do?\n"
		L"Yes - Host (wait for connections)\n"
		L"No - Join (connect to existing chat)\n"
		L"Cancel - Exit",
		L"QuickChat", MB_YESNOCANCEL | MB_ICONQUESTION);
	if (mode == IDCANCEL) return 0;

	is_server = (mode == IDYES);

	if (is_server) {
		int proto = MessageBoxW(NULL,
			L"Select a protocol to use for connection\n\n"
			L"Yes - QCS (QuickChat Obfuscated)\n"
			L"No - QC (QuickChat, plain text)\n\n"
			L"Warning: QC is not recommended as main protocol.",
			L"QuickChat", MB_YESNO | MB_ICONQUESTION);
		xor_enabled = (proto == IDYES);

		int enable_logs = MessageBoxW(NULL,
			L"Enable logs for this session?",
			L"QuickChat", MB_YESNO | MB_ICONQUESTION);
		logging_enabled = (enable_logs == IDYES);

		if (logging_enabled) {
			chat_log = _wfopen(L"chatlog.txt", L"a");
			if (chat_log == NULL) {
				wchar_t log_err[512];
				swprintf(log_err, 512, L"Failed to open chat log file. Logging will be disabled for this session. Error: %d.", GetLastError());
				MessageBoxW(NULL, log_err, L"QuickChat", MB_OK | MB_ICONWARNING);

				logging_enabled = false;
			} else {
				time_t now = time(NULL);
				struct tm *t = localtime(&now);
				wchar_t timestamp[64];
				wcsftime(timestamp, 64, L"%H:%M:%S %d/%m/%Y", t);
				fwprintf(chat_log, L"\n=== New session started at %ls ===\n", timestamp);
				fflush(chat_log);
			}
		}

		if (!InitializeNetwork(true, hInstance, nCmdShow)) return 0;
	} else {
		while (1) {
			INT_PTR dlg = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(1), NULL, ConnectDialogProc, 0);

			if (dlg < 0) {
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
			xor_enabled = (proto == IDYES);
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

void LoadSettings() {
	FILE *f = _wfopen(INI_FILE, L"r");
	if (!f) return;

	wchar_t line[128];
	while (fgetws(line, 128, f)) {
		size_t len = wcslen(line);
		if (len && line[len-1] == L'\n') line[--len] = L'\0';
		if (len && line[len-1] == L'\r') line[--len] = L'\0';

		if (line[0] == L'\0' || line[0] == L';' || line[0] == L'#') continue;
		if (line[0] == L'[') continue;

		wchar_t *eq = wcschr(line, L'=');
		if (!eq) continue;
		*eq = L'\0';
		wchar_t *key = line;
		wchar_t *val = eq + 1;

		while (*key == L' ') key++;
		while (*val == L' ') val++;

		if (wcscmp(key, L"always_on_top") == 0)
			always_on_top = _wtoi(val) != 0;
		else if (wcscmp(key, L"Flash") == 0)
			flash_enabled = _wtoi(val) != 0;
		else if (wcscmp(key, L"Sound") == 0)
			sound_enabled = _wtoi(val) != 0;
	}
	fclose(f);
}

void SaveSettings() {
	FILE *f = _wfopen(INI_FILE, L"w");
	if (!f) return;

	fwprintf(f, L"[QuickChat]\n");
	fwprintf(f, L"always_on_top=%d\n", always_on_top ? 1 : 0);
	fwprintf(f, L"Flash=%d\n", flash_enabled ? 1 : 0);
	fwprintf(f, L"Sound=%d\n", sound_enabled ? 1 : 0);
	fclose(f);
}

// ======= 6. Helper Functions =======
void EnableVisualStyles() {
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icex);
}

void GetLocalComputerName() {
	DWORD size = sizeof(computer_name) / sizeof(wchar_t);
	GetComputerNameW(computer_name, &size);
}

void PlayNotifySound(int sound) {
	if (!sound_enabled) return;

	const wchar_t* filename = NULL;

	switch (sound) {
		case SOUND_JOIN:
			filename = L"join.wav";
			break;

		case SOUND_LEAVE:
			filename = L"leave.wav";
			break;

		case SOUND_MSG:
			filename = L"newmsg.wav";
			break;

		default:
			return;
	}

	PlaySoundW(filename, NULL, SND_FILENAME | SND_ASYNC);
}

void LogMessage(const wchar_t* message) {
	if (!logging_enabled || chat_log == NULL) return;

	SYSTEMTIME st;
	GetLocalTime(&st);

	wchar_t timestamp[64];
	swprintf(timestamp, sizeof(timestamp) / sizeof(wchar_t),
		L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	// Writing logs in UTF-8 for compatibility.
	char timestamp_utf8[64];
	char msg_utf8[1024];
	WideCharToMultiByte(CP_UTF8, 0, timestamp, -1, timestamp_utf8, sizeof(timestamp_utf8), NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, message, -1, msg_utf8, sizeof(msg_utf8), NULL, NULL);

	fprintf(chat_log, "[%s] %s\n", timestamp_utf8, msg_utf8);
	fflush(chat_log);
}

bool GetDefaultIP(wchar_t *ip_buffer, size_t size) {
	// UDP hack: connect to 8.8.8.8:53 (DNS), then getsockname returns local IP.
	// Works only when there is a route to the internet. Returns 0.0.0.0 if no route.

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
		MessageBoxW(NULL, L"Failed to get host IP address.", L"QuickChat", MB_OK | MB_ICONWARNING);
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
		wchar_t longmsg_err[511] = L"[ERROR]: Message is too long to be displayed.";
		AddMessage(longmsg_err);
		if (is_server) LogMessage(longmsg_err);
		return;
	}
	int len = GetWindowTextLengthW(hMsgDisplay);

	SendMessageW(hMsgDisplay, EM_SETSEL, len, len);
	if (len > 0) SendMessageW(hMsgDisplay, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");

	if (!SendMessageW(hMsgDisplay, EM_REPLACESEL, FALSE, (LPARAM)msg)) {
		if (is_server) {
			wchar_t addmsg_err[512];
			swprintf(addmsg_err, sizeof(addmsg_err) / sizeof(wchar_t), L"[ERROR]: Failed to display message. Error: %lu.", GetLastError());
			if (is_server) LogMessage(addmsg_err);
		}
		SetFocus(hEdit);
		return;
	}

	SendMessageW(hMsgDisplay, WM_VSCROLL, SB_BOTTOM, 0);
}

void CleanupAndExit() {
	SaveSettings();
	is_running = 0;

	if (client_socket != INVALID_SOCKET) {
		shutdown(client_socket, SD_BOTH);
		closesocket(client_socket);
		client_socket = INVALID_SOCKET;
	}

	// Never do object wait on thread with recv.
	// It is always bad idea - adds extra delay (~5 sec).
	if (hReceiveThread != NULL) {
		CloseHandle(hReceiveThread);
		hReceiveThread = NULL;
	}

	if (chat_log != NULL) {
		time_t now = time(NULL);
		struct tm *t = localtime(&now);
		wchar_t timestamp[64];
		wcsftime(timestamp, sizeof(timestamp) / sizeof(wchar_t), L"%H:%M:%S %d/%m/%Y", t);
		fwprintf(chat_log, L"=== Session ended at %ls ===\n\n", timestamp);
		fclose(chat_log);
		chat_log = NULL;
	}

	WSACleanup();
	PostQuitMessage(0);
}

void DisableChatControls(BOOL disable) {
	if (hEdit && IsWindow(hEdit)) EnableWindow(hEdit, !disable);
	if (hSendBtn && IsWindow(hSendBtn)) EnableWindow(hSendBtn, !disable);
}

void FlashMessageWindow(HWND hWnd) {
	if (!flash_enabled) return;

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
	//   240.0.0.0 - 255.255.255.254 (reserved)
	//   255.255.255.255 (global broadcast)
	if (o1 == 0) return false;
	if (o4 == 0) return false;
	if (o4 == 255) return false;
	if (o1 >= 224 && o1 <= 239) return false;
	if (o1 >= 240) return false;

	// Allow any other IP
	return true;
}

void CloseConnection() {
	// Only server/host (call whatever you want) allowed to disconnect client from chat.
	// This is by design.
	if (!is_server) return;

	if (client_socket == INVALID_SOCKET) {
		AddMessage(L"[INFO]: No active connection to close.");
		return;
	}

	is_running = 0;

	shutdown(client_socket, SD_BOTH);

	struct linger linger_opt = { 1, 0 };
	setsockopt(client_socket, SOL_SOCKET, SO_LINGER, (char*)&linger_opt, sizeof(linger_opt));

	closesocket(client_socket);
	client_socket = INVALID_SOCKET;

	DisableChatControls(TRUE);

	wchar_t closeconn[512] = L"[DISCONNECT]: Host closed the connection.";
	AddMessage(closeconn);
	LogMessage(closeconn);
}

void Disconnect() {
	wchar_t leave_msg[512];
	swprintf(leave_msg, sizeof(leave_msg) / sizeof(wchar_t), L"[DISCONNECT]: %ls left the chat.", computer_name);
	AddMessage(leave_msg);

	if (client_socket != INVALID_SOCKET && is_running) {
		// Convert wchat_t to UTF-8 before sending.
		char utf8_msg[256];
		WideCharToMultiByte(CP_UTF8, 0, leave_msg, -1, utf8_msg, sizeof(utf8_msg), NULL, NULL);
		int msg_len = strlen(utf8_msg);

		unsigned char encbuf[256];
		memcpy(encbuf, utf8_msg, msg_len);
		XorObf(encbuf, msg_len);
		send(client_socket, (char*)encbuf, msg_len, 0);
	}

	if (is_server) LogMessage(leave_msg);
	CleanupAndExit();
}

void ShowError(const wchar_t* msg, DWORD err) {
	wchar_t buffer[512];
	swprintf(buffer, sizeof(buffer) / sizeof(wchar_t), L"%ls. Error: %lu", msg, err);
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

		int active_port = xor_enabled ? PORT_QCS : PORT_QC;
		struct sockaddr_in server_addr = {0};
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = INADDR_ANY;
		server_addr.sin_port = htons(active_port);

		if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
			DWORD err = WSAGetLastError();
			if (err == WSAEADDRINUSE) {
				MessageBoxW(NULL,
					L"Port is already in use.\nAnother QuickChat host may be running.",
					L"QuickChat", MB_OK | MB_ICONWARNING);
			} else {
				ShowError(L"Bind failed.", err);
			}
			closesocket(server_fd);
			WSACleanup();
			return false;
		}

		GetDefaultIP(server_ip, sizeof(server_ip) / sizeof(wchar_t));

		wchar_t bind_msg[512];
		const wchar_t* mode_str = xor_enabled ? L"QCS (Obfuscated)" : L"QC (Plaintext)";
		swprintf(bind_msg, sizeof(bind_msg) / sizeof(wchar_t), L"Host started: %ls on address %ls port %d.", mode_str, server_ip, active_port);
		LogMessage(bind_msg);

		if (listen(server_fd, 1) == SOCKET_ERROR) {
			ShowError(L"Listen failed.", WSAGetLastError());
			closesocket(server_fd);
			WSACleanup();
			return false;
		}

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

			client_socket = temp_client;

			char ip_utf8[16];
			strncpy(ip_utf8, inet_ntoa(client_addr.sin_addr), 15);
			ip_utf8[15] = '\0';
			MultiByteToWideChar(CP_UTF8, 0, ip_utf8, -1, peer_ip, sizeof(peer_ip) / sizeof(wchar_t));
			MultiByteToWideChar(CP_UTF8, 0, name_ptr, -1, peer_name, sizeof(peer_name) / sizeof(wchar_t));
			break;
		}

		closesocket(server_fd);

		// Send handshake reply (with Unicode-to-ANSI conversion).
		char hs_reply[256];
		int pos = snprintf(hs_reply, sizeof(hs_reply), "%s", QC_LABEL);
		WideCharToMultiByte(CP_UTF8, 0, computer_name, -1, hs_reply + pos, sizeof(hs_reply) - pos, NULL, NULL);
		int hs_r_len = strlen(hs_reply);
		XorObf((unsigned char*)hs_reply, hs_r_len);
		send(client_socket, hs_reply, hs_r_len, 0);

		ShowMainWindow(hInstance, nCmdShow);

		while (!mainWindowReady) {
			Sleep(10);
			MSG msg;
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		wchar_t join_msg[512];
		swprintf(join_msg, sizeof(join_msg) / sizeof(wchar_t), L"[CONNECT]: %ls connected from %ls.", peer_name, peer_ip);
		AddMessage(join_msg);
		LogMessage(join_msg);
	} else {
		client_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (client_socket == INVALID_SOCKET) {
			ShowError(L"Failed to create socket.", WSAGetLastError());
			WSACleanup();
			return false;
		}

		struct timeval timeout = {
			.tv_sec = 5,
			.tv_usec = 0
		};
		setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

		int active_port = xor_enabled ? PORT_QCS : PORT_QC;
		struct sockaddr_in server_addr = {0};
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(active_port);

		char server_ip_utf8[16];
		WideCharToMultiByte(CP_UTF8, 0, server_ip, -1, server_ip_utf8, sizeof(server_ip_utf8), NULL, NULL);
		server_addr.sin_addr.s_addr = inet_addr(server_ip_utf8);

		if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
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
			closesocket(client_socket);
			WSACleanup();
			return false;
		}

		timeout.tv_sec = 0;
		setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

		// Disabling "Weak Host Model" on pre-Vista versions (known problem on XP and 2000).
		// On Vista and newer - switching from "soft bind" to "hard bind".
		struct sockaddr_in server_info;
		int len = sizeof(server_info);
		getsockname(client_socket, (struct sockaddr*)&server_info, &len);
		wchar_t ip_w[16];
		DWORD ip_len = 16;
		WSAAddressToStringW((LPSOCKADDR)&server_info, sizeof(server_info), NULL, ip_w, &ip_len);
		wcscpy(peer_ip, ip_w);

		// Send handshake (Unicode -> UTF-8).
		char hs[256];
		int pos = snprintf(hs, sizeof(hs), "%s", QC_LABEL);
		WideCharToMultiByte(CP_UTF8, 0, computer_name, -1, hs + pos, sizeof(hs) - pos, NULL, NULL);
		int hs_len = strlen(hs);
		XorObf((unsigned char*)hs, hs_len);
		send(client_socket, hs, hs_len, 0);

		char hs_reply[256];
		int recv_len = recv(client_socket, hs_reply, sizeof(hs_reply) - 1, 0);
		if (recv_len <= 0) {
			ShowError(L"Failed to receive peer handshake.", WSAGetLastError());
			closesocket(client_socket);
			WSACleanup();
			return false;
		}

		XorObf((unsigned char*)hs_reply, recv_len);
		hs_reply[recv_len] = '\0';

		if (strncmp(hs_reply, QC_LABEL, strlen(QC_LABEL)) != 0) {
			MessageBoxW(NULL, L"Remote host sent an invalid handshake,", L"QuickChat", MB_OK | MB_ICONERROR);
			closesocket(client_socket);
			WSACleanup();
			return false;
		}

		const char* name_ptr = hs_reply + strlen(QC_LABEL);
		MultiByteToWideChar(CP_UTF8, 0, name_ptr, -1, peer_name, 256);
		if (peer_name[0] == L'\0') wcscpy(peer_name, L"<Unknown>");

		ShowMainWindow(hInstance, nCmdShow);

		while (!mainWindowReady) {
			Sleep(10);
			MSG msg;
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		wchar_t join_msg[512];
		swprintf(join_msg, sizeof(join_msg) / sizeof(wchar_t), L"[CONNECT]: Connected to %ls at %ls.", peer_name, server_ip);
		AddMessage(join_msg);
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
	if (!xor_enabled) return;

	unsigned char k[KEY_LEN];
	memcpy(k, key, KEY_LEN);

	for (int i = 0; i < len; i++) data[i] ^= k[i % KEY_LEN];

	memset(k, 0, KEY_LEN);
}

unsigned int __stdcall ReceiveMessages(void* arg) {
	(void)arg;

	char buffer[BUFFER_SIZE];

	while (is_running) {
		int bytes = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
		if (!is_running)
			break;

		if (bytes == SOCKET_ERROR) {
			int err_code = WSAGetLastError();
			if (err_code == WSAETIMEDOUT) continue; // Timeout is not an error in this case

			wchar_t connlost_err[512];
			swprintf(connlost_err, sizeof(connlost_err) / sizeof(wchar_t), L"[ERROR]: Connection with remote computer lost. (Error: %d)", err_code);
			AddMessage(connlost_err);
			if (is_server) LogMessage(connlost_err);
			DisableChatControls(TRUE);
			DragAcceptFiles(hWndGlobal, FALSE);

			is_running = 0;
			break;
		}

		if (bytes == 0) {
			wchar_t close_msg[512] = L"[DISCONNECT]: Remote computer has closed the connection.";
			AddMessage(close_msg);
			if (is_server) LogMessage(close_msg);
			DisableChatControls(TRUE);
			is_running = 0;
			break;
		}

		XorObf((unsigned char*)buffer, bytes);
		buffer[bytes] = '\0';

		// Custom ping
		if (strcmp(buffer, "QCPING") == 0) {
			char pong_msg[] = "QCPONG";
			int len = strlen(pong_msg);
			XorObf((unsigned char*)pong_msg, len);
			send(client_socket, pong_msg, len, 0);
			continue;
		}

		if (strcmp(buffer, "QCPONG") == 0) {
			wchar_t ping_msg[512] = L"[INFO]: Client responded to PING packet.";
			LogMessage(ping_msg);
			AddMessage(ping_msg);
			continue;
		}

		FlashMessageWindow(hWndGlobal);

		if (strncmp(buffer, "[DISCONNECT]", 12) == 0) {
			DisableChatControls(TRUE);
			PlayNotifySound(SOUND_LEAVE);
			DragAcceptFiles(hWndGlobal, FALSE);
		} else {
			PlayNotifySound(SOUND_MSG);
		}

		// Convert received buffer to Unicode for display.
		wchar_t wbuffer[BUFFER_SIZE];
		MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wbuffer, BUFFER_SIZE);
		AddMessage(wbuffer);
		if (is_server) LogMessage(wbuffer);
	}

	_endthread();
	return 0;
}

void SendCurrentMessage(HWND hWnd) {
	// Counting characters including formatting.
	int msglen = GetWindowTextLengthW(hEdit);
	int maxallowed = (BUFFER_SIZE - 1) - wcslen(computer_name) - 8;
	if (msglen > maxallowed) {
		wchar_t toolong_err[512] = L"[ERROR]: Message is too long to send.";
		AddMessage(toolong_err);
		if (is_server) LogMessage(toolong_err);
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
		swprintf(full_msg, BUFFER_SIZE + 128, L"[%ls]: %ls", computer_name, start);

		// Convert to UTF-8 for transmission.
		char utf8_buffer[BUFFER_SIZE + 128];
		WideCharToMultiByte(CP_UTF8, 0, full_msg, -1, utf8_buffer, sizeof(utf8_buffer), NULL, NULL);
		int msg_len = strlen(utf8_buffer);

		unsigned char encbuf[BUFFER_SIZE + 128];
		memcpy(encbuf, utf8_buffer, msg_len);
		XorObf(encbuf, msg_len);
		AddMessage(full_msg);
		if (is_server) LogMessage(full_msg);
		int send_result = send(client_socket, (char*)encbuf, msg_len, 0);

		if (send_result == SOCKET_ERROR) {
			int error_code = WSAGetLastError();
			const wchar_t* error_desc = L"Unknown socket error";

			switch(error_code) {
				case WSAECONNRESET:
					error_desc = L"Connection was reset by peer";
					break;
				case WSAENOTCONN:
					error_desc = L"Socket is not connected";
					break;
				case WSAETIMEDOUT:
					error_desc = L"Connection timed out";
					break;
				case WSAECONNABORTED:
					error_desc = L"Connection was aborted";
					break;
				default:
					error_desc = L"An unknown error has happened";
					break;
			}

			wchar_t send_err[512];
			swprintf(send_err, sizeof(send_err) / sizeof(wchar_t), L"[ERROR]: Failed to send message. Error: %ls (WSA error: %d)", error_desc, error_code);
			AddMessage(send_err);
			if (is_server) LogMessage(send_err);
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
				GetDlgItemTextW(hwnd, IDC_IP, ip, sizeof(ip) / sizeof(wchar_t));

				// Trim leading spaces.
				wchar_t *p = ip;
				while (*p == L' ') p++;

				// Trim trailing spaces.
				int len = wcslen(p);
				while (len > 0 && p[len-1] == L' ') {
					p[len-1] = L'\0';
					len--;
				}

				if (wcslen(p) == 0) {
					MessageBoxW(hwnd, L"Host IP is required for connection.", L"QuickChat", MB_OK | MB_ICONWARNING);
					SetFocus(GetDlgItem(hwnd, IDC_IP));
					return TRUE;
				}

				// Validation of IP address syntax.
				int octets[4];
				int valid = (swscanf(p, L"%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]) == 4);
				
				if (valid) {
					for (int i = 0; i < 4; i++) {
						if (octets[i] < 0 || octets[i] > 255) {
							valid = 0;
							break;
						}
					}
				}

				if (!valid) {
					MessageBoxW(hwnd, 
						L"Invalid IP address.\n"
						L"Example: 192.168.1.100 or 127.0.0.1.",
						L"QuickChat", 
						MB_OK | MB_ICONWARNING);
					SetFocus(GetDlgItem(hwnd, IDC_IP));
					return TRUE;
				}

				if (!IsValidTargetIP(p)) {
					MessageBoxW(hwnd, L"This IP address is valid, but cannot be used for connection.", L"QuickChat", MB_OK | MB_ICONWARNING);
					SetFocus(GetDlgItem(hwnd, IDC_IP));
					return TRUE;
				}
 
				wcscpy(server_ip, p);
				server_ip[sizeof(server_ip)/sizeof(wchar_t) - 1] = L'\0';
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

	wchar_t srv_info[512];
	const wchar_t* mode = xor_enabled ? L"QCS (QuickChat Obfuscated)" : L"QC (QuickChat, plain text)";
	swprintf(srv_info, sizeof(srv_info) / sizeof(wchar_t),
		L"Host IP: %ls\n"
		L"Protocol: %ls\n"
		L"Share with users to connect to host.",
		server_ip, mode);

	MessageBoxW(NULL, srv_info, L"QuickChat", MB_OK | MB_ICONINFORMATION);
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

	wchar_t title[512];
	const wchar_t* protocolLabel = xor_enabled ? L"QCS" : L"QC";
	swprintf(title, sizeof(title) / sizeof(wchar_t), L"QuickChat (%ls) - %ls", protocolLabel, peer_name);

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
			// Calculating DPI for UI fonts.
			// Note that controls are not scalable items.
			// Scaling controls to display DPI is quite hard.
			HDC hdc = GetDC(hWnd);
			int dpi = GetDeviceCaps(hdc, LOGPIXELSY); // 96 DPI
			int font8pt = -MulDiv(8, dpi, 72); // 8pt
			int font9pt = -MulDiv(9, dpi, 72); // 9pt
			
			hFontBold = CreateFontW(
				font8pt, 0, 0, 0, FW_BOLD,
				FALSE, FALSE, FALSE, DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
				L"Tahoma"
			);

			hFontMono = CreateFontW(
				font9pt, 0, 0, 0, FW_NORMAL,
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

			// Zero out error code to do not trigger false event.
			SetLastError(0);
			oldEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);
			if (!oldEditProc && GetLastError() != 0) {
				AddMessage(L"[WARNING]: Edit subclass setup failed. Enter and Ctrl+A may not work as expected.");
			}

			DragAcceptFiles(hWnd, TRUE);

			mainWindowReady = TRUE;
			return 0;
		}

		case WM_COMMAND: {
			if (LOWORD(wParam) == ID_SEND) {
				int textLen = GetWindowTextLengthW(hEdit);
				SendMessageW(hEdit, EM_SETSEL, textLen, textLen);
				SendCurrentMessage(hWnd);
				SetFocus(hEdit);
			} else if (LOWORD(wParam) == IDM_CLOSE) {
				if (is_server) {
					if (!is_running) {
						AddMessage(L"[ERROR]: Remote computer is not connected.");
						return 0L;
					}

					if (MessageBoxW(hWnd, L"Close current connection?", L"QuickChat", MB_ICONQUESTION | MB_YESNO) == IDYES) {
						CloseConnection();
					}
				}
			} else if (LOWORD(wParam) == IDM_LEAVE) {
				if (!is_running) CleanupAndExit();

				if (MessageBoxW(hWnd, L"Leave current chat?", L"QuickChat", MB_ICONQUESTION | MB_YESNO) == IDYES) {
					Disconnect();
				}
			} else if (LOWORD(wParam) == IDM_ABOUT) {
				wchar_t about_msg[512];
				swprintf(about_msg, sizeof(about_msg) / sizeof(wchar_t),
					L"QuickChat\n"
					L"Built on %s\n"
					L"Created by WinXP655\n"
					L"https://github.com/WinXP655/quickchat",
					__DATE__);
				MessageBoxW(hWnd, about_msg, L"About QuickChat", MB_OK | MB_ICONINFORMATION);
			} else if (LOWORD(wParam) == ID_SOUND_TOGGLE) {
				sound_enabled = !sound_enabled;
				CheckMenuItem(GetMenu(hWnd), ID_SOUND_TOGGLE, MF_BYCOMMAND | (sound_enabled ? MF_CHECKED : MF_UNCHECKED));
				return 0;
			} else if (LOWORD(wParam) == ID_FLASH_TOGGLE) {
				flash_enabled = !flash_enabled;
				CheckMenuItem(GetMenu(hWnd), ID_FLASH_TOGGLE, MF_BYCOMMAND | (flash_enabled ? MF_CHECKED : MF_UNCHECKED));
				return 0;
			} else if (LOWORD(wParam) == IDM_COMPUTER_INFO) {
				// Using existing variables to display data.
				const wchar_t* connected = is_running ? L"Yes" : L"No";
				const wchar_t* displayName = (peer_name[0] != L'\0') ? peer_name : L"N/A";
				const wchar_t* protocolLabel = xor_enabled ? L"QCS" : L"QC";

				// Problem before was that both server and client used same variable (peer_ip),
				// but they contained different IP addresses, meaning it would display
				// wrong IP for one of sides. Was affected only client side.
				const wchar_t* ip = is_server ? peer_ip : server_ip;

				wchar_t info_msg[512];
				swprintf(info_msg, 512,
					L"Remote Name: %ls\n"
					L"Remote IP: %ls\n"
					L"Protocol: %ls\n"
					L"Connected: %ls",
					displayName, ip, protocolLabel, connected);

				MessageBoxW(hWnd, info_msg, L"QuickChat", MB_OK | MB_ICONINFORMATION);
			} else if (LOWORD(wParam) == IDM_CLEAR_CHAT) {
				SetWindowTextW(hMsgDisplay, L"");
			} else if (LOWORD(wParam) == IDM_ALWAYS_ON_TOP) {
				always_on_top = !always_on_top;
				
				SetWindowPos(hWnd,
					always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
					0, 0, 0, 0,
					SWP_NOMOVE | SWP_NOSIZE);

				CheckMenuItem(GetMenu(hWnd), IDM_ALWAYS_ON_TOP, MF_BYCOMMAND | (always_on_top ? MF_CHECKED : MF_UNCHECKED));
				return 0;
			} else if (LOWORD(wParam) == IDM_PING_REMOTE) {
				if (!is_running || client_socket == INVALID_SOCKET) {
					AddMessage(L"[ERROR]: Remote computer is not connected.");
					return 0;
				}

				char ping_msg[] = "QCPING";
				int len = strlen(ping_msg);
				XorObf((unsigned char*)ping_msg, len);
				send(client_socket, ping_msg, len, 0);
				return 0;
			} else if (LOWORD(wParam) == IDM_SAVE) {
				wchar_t filename[MAX_PATH];
				time_t now = time(NULL);
				struct tm *tm_info = localtime(&now);
				wcsftime(filename, MAX_PATH, L"Chat-%Y%m%d-%H%M%S.txt", tm_info);

				int len = GetWindowTextLengthW(hMsgDisplay);
				wchar_t *chatText = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
				GetWindowTextW(hMsgDisplay, chatText, len + 1);

				FILE *f = _wfopen(filename, L"wb");
				if (f) {
					// Use only for UTF-8 with BOM.
					// fwrite("\xEF\xBB\xBF", 1, 3, f);

					char *utf8 = (char*)malloc(len * 3 + 1);
					WideCharToMultiByte(CP_UTF8, 0, chatText, -1, utf8, len * 3 + 1, NULL, NULL);
					fwrite(utf8, 1, strlen(utf8), f);
					free(utf8);
					fclose(f);

					wchar_t save_msg[512];
					swprintf(save_msg, 512, L"Chat saved to %ls", filename);
					MessageBoxW(hWnd, save_msg, L"QuickChat", MB_OK | MB_ICONINFORMATION);
				} else {
					wchar_t save_err[512];
					swprintf(save_err, 512, L"Failed to save chat history. Error: %d.", GetLastError());
					MessageBoxW(hWnd, save_err, L"QuickChat", MB_OK | MB_ICONERROR);
				}

				free(chatText);
			}
			return 0;
		}

		case WM_CLOSE: {
			if (!is_running) CleanupAndExit();

			if (MessageBoxW(hWnd, L"Leave current chat?", L"QuickChat", MB_ICONQUESTION | MB_YESNO) == IDYES) {
				Disconnect();
			}

			return 0;
		}

		case WM_DESTROY: {
			// Prevent GDI leaks
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
		
		case WM_DROPFILES: {
			HDROP hDrop = (HDROP)wParam;
			wchar_t path[MAX_PATH];
			DragQueryFileW(hDrop, 0, path, MAX_PATH);

			if (!IsValidTextExtension(path)) {
				MessageBoxW(hWnd,
					L"Format not supported.\n\n"
					L"Supported extensions:\n"
					L".txt, .log, .md, .c, .cpp, .h, .hpp,\n"
					L".py, .js, .sh, .bat, .cmd, .ps1,\n"
					L".json, .xml, .yaml, .yml, .toml,\n"
					L".ini, .cfg, .conf, .css, .html, .htm",
					L"QuickChat", MB_OK | MB_ICONWARNING);
				DragFinish(hDrop);
				return 0;
			}

			wchar_t *content = ReadTextFileContent(path, hWnd);
			if (!content) {
				DragFinish(hDrop);
				return 0;
			}

			int max_len = (BUFFER_SIZE - 1) - wcslen(computer_name) - 8;
			if ((int)wcslen(content) > max_len) {
				free(content);
				MessageBoxW(hWnd, L"File is too large for message field.", L"QuickChat", MB_OK | MB_ICONWARNING);
				DragFinish(hDrop);
				return 0;
			}

			InsertTextIntoEdit(content);
			free(content);

			DragFinish(hDrop);
			return 0;
		}
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void CreateMenuBar(HWND hWnd) {
	HMENU hMenu = CreateMenu();

	HMENU hConn = CreatePopupMenu();
	if (is_server) AppendMenuW(hConn, MF_STRING, IDM_CLOSE, L"Close Connection");
	AppendMenuW(hConn, MF_STRING, IDM_PING_REMOTE, L"Ping Remote");
	AppendMenuW(hConn, MF_STRING, IDM_COMPUTER_INFO, L"Computer Info");
	AppendMenuW(hConn, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hConn, MF_STRING, IDM_SAVE, L"Save Chat");
	AppendMenuW(hConn, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hConn, MF_STRING, IDM_LEAVE, L"Leave Chat");
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

	CheckMenuItem(hMenu, IDM_ALWAYS_ON_TOP, MF_BYCOMMAND | (always_on_top ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hMenu, ID_SOUND_TOGGLE, MF_BYCOMMAND | (sound_enabled ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hMenu, ID_FLASH_TOGGLE, MF_BYCOMMAND | (flash_enabled ? MF_CHECKED : MF_UNCHECKED));
}

LRESULT CALLBACK EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_GETDLGCODE) {
		return DLGC_WANTALLKEYS | CallWindowProcW(oldEditProc, hWnd, uMsg, wParam, lParam);
	} else if (uMsg == WM_KEYDOWN) {
		// Setting our own handlers

		// Ctrl+A
		// Microsoft never made it Select all out-of-box, so developers
		// have choice what they would do with this combination.
		if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
			SendMessageW(hWnd, EM_SETSEL, 0, -1);
			return 0;
		}

		// Enter
		// By default it inserts a new line, behavior since Windows 3.x.
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

// ======= 9. Drag-and-Drop Functions =======
static bool IsValidTextExtension(const wchar_t *path) {
	const wchar_t *ext = wcsrchr(path, L'.');
	if (!ext) return false;

	wchar_t ext_lower[16];
	wcsncpy(ext_lower, ext, 15);
	ext_lower[15] = L'\0';
	for (int i = 0; ext_lower[i]; i++) ext_lower[i] = towlower(ext_lower[i]);

	// Main
	if (wcscmp(ext_lower, L".txt") == 0 ||
		wcscmp(ext_lower, L".log") == 0 ||
		wcscmp(ext_lower, L".md") == 0) return true;

	// Code
	if (wcscmp(ext_lower, L".c") == 0 ||
		wcscmp(ext_lower, L".cpp") == 0 ||
		wcscmp(ext_lower, L".h") == 0 ||
		wcscmp(ext_lower, L".hpp") == 0) return true;

	// Scripts
	if (wcscmp(ext_lower, L".py") == 0 ||
		wcscmp(ext_lower, L".js") == 0 ||
		wcscmp(ext_lower, L".sh") == 0 ||
		wcscmp(ext_lower, L".bat") == 0 ||
		wcscmp(ext_lower, L".cmd") == 0 ||
		wcscmp(ext_lower, L".ps1") == 0) return true;

	// Configs
	if (wcscmp(ext_lower, L".json") == 0 ||
		wcscmp(ext_lower, L".xml") == 0 ||
		wcscmp(ext_lower, L".yaml") == 0 ||
		wcscmp(ext_lower, L".yml") == 0 ||
		wcscmp(ext_lower, L".toml") == 0 ||
		wcscmp(ext_lower, L".ini") == 0 ||
		wcscmp(ext_lower, L".cfg") == 0 ||
		wcscmp(ext_lower, L".conf") == 0) return true;

	// Web
	if (wcscmp(ext_lower, L".css") == 0 ||
		wcscmp(ext_lower, L".html") == 0 ||
		wcscmp(ext_lower, L".htm") == 0) return true;

	return false;
}

static wchar_t* ReadTextFileContent(const wchar_t *path, HWND hWnd) {
	HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
							   NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		MessageBoxW(hWnd, L"Cannot open file (locked or inaccessible).",
					L"QuickChat", MB_OK | MB_ICONWARNING);
		return NULL;
	}

	DWORD size = GetFileSize(hFile, NULL);
	if (size == 0 || size == INVALID_FILE_SIZE) {
		CloseHandle(hFile);
		MessageBoxW(hWnd, L"File is empty.", L"QuickChat",
					MB_OK | MB_ICONWARNING);
		return NULL;
	}

	char *ansi = (char*)malloc(size + 1);
	if (!ansi) {
		CloseHandle(hFile);
		MessageBoxW(hWnd, L"Memory allocation failed.", L"QuickChat",
					MB_OK | MB_ICONERROR);
		return NULL;
	}

	DWORD read;
	if (!ReadFile(hFile, ansi, size, &read, NULL)) {
		free(ansi);
		CloseHandle(hFile);
		MessageBoxW(hWnd, L"Failed to read file.", L"QuickChat",
					MB_OK | MB_ICONERROR);
		return NULL;
	}
	ansi[read] = '\0';
	CloseHandle(hFile);

	// Skip BOM
	int bom_offset = 0;
	if (read >= 3 && (unsigned char)ansi[0] == 0xEF &&
		(unsigned char)ansi[1] == 0xBB && (unsigned char)ansi[2] == 0xBF) {
		bom_offset = 3;
	}

	int wide_len = MultiByteToWideChar(CP_UTF8, 0, ansi + bom_offset, -1, NULL, 0);
	if (wide_len <= 0) {
		free(ansi);
		MessageBoxW(hWnd, L"File is not valid UTF-8 text.", L"QuickChat",
					MB_OK | MB_ICONWARNING);
		return NULL;
	}

	wchar_t *wide = (wchar_t*)malloc(wide_len * sizeof(wchar_t));
	if (!wide) {
		free(ansi);
		MessageBoxW(hWnd, L"Memory allocation failed.", L"QuickChat",
					MB_OK | MB_ICONERROR);
		return NULL;
	}

	MultiByteToWideChar(CP_UTF8, 0, ansi + bom_offset, -1, wide, wide_len);
	free(ansi);

	return wide;
}

static void InsertTextIntoEdit(const wchar_t *text) {
	SetWindowTextW(hEdit, text);

	int len = GetWindowTextLengthW(hEdit);
	SendMessageW(hEdit, EM_SETSEL, len, len);
	SetFocus(hEdit);
}