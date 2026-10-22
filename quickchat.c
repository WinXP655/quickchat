// QuickChat by WinXP655.
// Repository: https://github.com/WinXP655/quickchat.
// Distributed under MIT License.

// ======= 1. Headers =======
#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <windows.h>

#include <commctrl.h>
#include <process.h>
#include <shellapi.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>

#include "key.h"

// ======= 2. Defines =======
// ----- Limits -----
#define BUFFER_SIZE 8192

// ----- Files -----
#define CHATLOG_FILE L"chatlog.txt"
#define CRASHLOG_FILE L"crashlog.txt"
#define INI_FILE L"quickchat.ini"

// ----- Network -----
#define PORT_XOR 65501
#define PORT_PLAIN 65502

// ----- Protocol -----
#define QC_LABEL "QC:"

// ----- Sounds -----
#define SOUND_JOIN 0
#define SOUND_LEAVE 1
#define SOUND_MSG 2

// ----- Window Messages -----
#define WM_APP_ADDMSG (WM_APP + 1)
#define WM_APP_DISABLE (WM_APP + 2)
#define WM_APP_FLASH (WM_APP + 3)

// ----- Control IDs -----
#define ID_EDIT 101
#define ID_SEND 102
#define ID_MSG_DISPLAY 103

// ----- Menu: Connection -----
#define IDM_COMPUTER_INFO 2001
#define IDM_LEAVE 2002
#define IDM_SAVE 2003

// ----- Menu: View -----
#define IDM_ALWAYS_ON_TOP 2101
#define IDM_CLEAR_CHAT 2102

// ----- Menu: Options -----
#define IDM_FLASH_TOGGLE 2201
#define IDM_SOUND_TOGGLE 2202
#define IDM_RESET_SETTINGS 2203
#define IDM_CONFIRM_TOGGLE 2204

// ----- Menu: Help -----
#define IDM_ABOUT 2301

// ======= 3. Global variables =======
// ----- Flags -----
BOOL is_server = FALSE;
BOOL xor_enabled = TRUE;
BOOL logging_enabled = FALSE;
BOOL sound_enabled = TRUE;
BOOL flash_enabled = TRUE;
BOOL always_on_top = FALSE;
BOOL confirm_enabled = TRUE;

// ----- Network -----
SOCKET client_socket = INVALID_SOCKET;
HANDLE hReceiveThread = NULL;
wchar_t local_ip[16] = L"";
wchar_t remote_ip[16] = L"";
wchar_t remote_name[256] = L"";
wchar_t computer_name[256] = L"";

// ----- Logging -----
HANDLE chat_log = NULL;

// ----- User Interface -----
HWND hWndGlobal = NULL;
HWND hHostInfoDlg = NULL;
HWND hEdit = NULL;
HWND hSendBtn = NULL;
HWND hMsgDisplay = NULL;
WNDPROC oldEditProc = NULL;
WNDPROC oldDisplayProc = NULL;
HFONT hFontBold = NULL;
HFONT hFont = NULL;
HINSTANCE hInstGlobal = NULL;

// ----- Thread sync -----
volatile LONG is_running = TRUE;

// ======= 4. Prototypes =======
// ----- Infrastructure -----
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ExceptionInfo);
BOOL InitializeLog(HANDLE hLogFile);

// ----- Settings -----
char* ReadIniValue(const char* buffer, const char* key, char* out_value, size_t out_size);

// ----- Utilities -----
void EnableVisualStyles(void);
void ShowError(const wchar_t* msg, DWORD err);

// ----- Network: Core -----
void CleanupAndExit(void);
void SendLeaveMessage(void);
unsigned int __stdcall ReceiveMessages(void*);
void ProcessIncomingMessage(char* buffer, int bytes);
BOOL InitializeNetwork(BOOL server_mode, HINSTANCE hInstance, int nCmdShow);
BOOL StartServer(HINSTANCE hInstance, int nCmdShow);
BOOL StartClient(HINSTANCE hInstance, int nCmdShow);

// ----- User Interface: Window -----
void ShowMainWindow(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CreateAppMenu(HWND hWnd);
void CreateAppFonts(HWND hWnd);
void CreateAppControls(HWND hWnd);
void ResizeMainWindow(HWND hWnd, int width, int height);
void CleanupGdiResources(void);
void HandleSendCommand(void);
void HandleMenuCommand(HWND hWnd, int id);
LRESULT CALLBACK EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AddMessage(const wchar_t* msg);
void DisableChatControls(BOOL disable);
void FlashMessageWindow(HWND hWnd);
void PlayNotifySound(int sound);

// ----- User Interface: Dialogs -----
DWORD WINAPI ShowServerIPMessage(LPVOID lpParam);
INT_PTR CALLBACK ModeSelectProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ConnectDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK HostInfoProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ComputerInfoProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ----- Files -----
void SaveChatToFile(HWND hWnd);
void ProcessDroppedFile(HWND hWnd, HDROP hDrop);
BOOL IsValidTextExtension(const wchar_t *path);
wchar_t* ReadTextFileContent(const wchar_t *path, HWND hWnd);
void InsertTextIntoEdit(const wchar_t *text);

// ======= 5. Entry Point =======
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	(void)hPrevInstance;
	(void)lpCmdLine;
	hInstGlobal = hInstance;

	SetUnhandledExceptionFilter(CrashHandler);
	EnableVisualStyles();

	INT_PTR mode_result = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(2), NULL, ModeSelectProc, 0);
	if (mode_result == -1 || mode_result == 0) {
		MessageBoxW(NULL, L"Could not load initial dialog.", L"QuickChat", MB_OK | MB_ICONERROR);
		return 0;
	}
	if (mode_result != IDOK) return 0;

	if (is_server && logging_enabled) {
		HANDLE hLogFile = CreateFileW(CHATLOG_FILE, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hLogFile != INVALID_HANDLE_VALUE) {
			if (!InitializeLog(hLogFile)) {
				ShowError(L"Failed to write log header", GetLastError());
				CloseHandle(hLogFile);
				logging_enabled = FALSE;
			} else {
				chat_log = hLogFile;
			}
		} else {
			ShowError(L"Failed to open log file", GetLastError());
			logging_enabled = FALSE;
		}
	}

	if (!is_server) {
		INT_PTR dlg = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(1), NULL, ConnectDialogProc, 0);
		if (dlg < 0) {
			MessageBoxW(NULL, L"Could not load connection dialog.", L"QuickChat", MB_OK | MB_ICONERROR);
			return 0;
		}

		if (dlg != IDOK) return 0;
	}

	if (!InitializeNetwork(is_server, hInstance, nCmdShow)) return 0;

	PlayNotifySound(SOUND_JOIN);

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0)) {
		if (!IsDialogMessageW(hWndGlobal, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}

// ======= 6. Infrastructure =======
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ExceptionInfo) {
	DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
	void* address = ExceptionInfo->ExceptionRecord->ExceptionAddress;

	wchar_t user_msg[512];
	swprintf(user_msg, sizeof(user_msg) / sizeof(wchar_t),
		L"A critical error has occurred.\n"
		L"Error code: 0x%08lX\n"
		L"Address: %p\n\n"
		L"Click OK to exit QuickChat",
		code, address);
	MessageBoxW(NULL, user_msg, L"QuickChat", MB_OK | MB_ICONERROR);

	HANDLE hCrashLog = CreateFileW(CRASHLOG_FILE, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hCrashLog != INVALID_HANDLE_VALUE) {
		time_t current = time(NULL);
		struct tm* time_info = localtime(&current);
		char timestamp[64];
		strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);

		char buffer[512];
		int len = snprintf(buffer, sizeof(buffer),
			"--- Crash Report ---\r\n"
			"Time: %s\r\n"
			"Error code: 0x%08lX\r\n"
			"Address: %p\r\n",
			timestamp, code, address);

		DWORD bytes_written;
		WriteFile(hCrashLog, buffer, len, &bytes_written, NULL);
		CloseHandle(hCrashLog);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

BOOL InitializeLog(HANDLE hLogFile) {
	if (hLogFile == INVALID_HANDLE_VALUE) return FALSE;

	SetFilePointer(hLogFile, 0, NULL, FILE_END);

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	wchar_t header[256];
	wcsftime(header, sizeof(header) / sizeof(wchar_t), L"\r\n========== New session started at %H:%M:%S %d/%m/%Y ==========\r\n", t);

	int utf8_len = WideCharToMultiByte(CP_UTF8, 0, header, -1, NULL, 0, NULL, NULL);
	if (utf8_len <= 0) return FALSE;

	char *utf8_buffer = (char*)malloc(utf8_len);
	if (!utf8_buffer) return FALSE;

	WideCharToMultiByte(CP_UTF8, 0, header, -1, utf8_buffer, utf8_len, NULL, NULL);

	DWORD bytes_written;
	BOOL write_result = WriteFile(hLogFile, utf8_buffer, utf8_len - 1, &bytes_written, NULL);
	free(utf8_buffer);

	return write_result;
}

void LogMessage(const wchar_t* message) {
	if (!logging_enabled) return;
	if (chat_log == NULL || chat_log == INVALID_HANDLE_VALUE) return;

	SYSTEMTIME st;
	GetLocalTime(&st);

	char timestamp[64];
	snprintf(timestamp, sizeof(timestamp),
		"%04d-%02d-%02d %02d:%02d:%02d.%03d",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	char msg_utf8[1024];
	WideCharToMultiByte(CP_UTF8, 0, message, -1, msg_utf8, sizeof(msg_utf8), NULL, NULL);

	char buffer[2048];
	int len = snprintf(buffer, sizeof(buffer), "[%s] %s\r\n", timestamp, msg_utf8);

	DWORD bytes_written;
	if (!WriteFile(chat_log, buffer, len, &bytes_written, NULL)) {
		DWORD err = GetLastError();

		wchar_t err_msg[512];
		swprintf(err_msg, sizeof(err_msg) / sizeof(wchar_t), L"[ERROR]: Failed to write to log. Error: %lu. Logging will be disabled for this session.", err);
		AddMessage(err_msg);

		logging_enabled = FALSE;
		CloseHandle(chat_log);
		chat_log = NULL;
		return;
	}

	FlushFileBuffers(chat_log);
}

void CloseLog(void) {
	if (chat_log == NULL || chat_log == INVALID_HANDLE_VALUE) {
		return;
	}

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	wchar_t timestamp[64];
	wcsftime(timestamp, sizeof(timestamp) / sizeof(wchar_t), L"%H:%M:%S %d/%m/%Y", t);

	wchar_t wbuffer[512];
	swprintf(wbuffer, sizeof(wbuffer) / sizeof(wchar_t), L"========== Session ended at %ls ==========\r\n", timestamp);

	int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, NULL, 0, NULL, NULL);
	if (utf8_len > 0) {
		char* utf8_buffer = (char*)malloc(utf8_len);
		if (utf8_buffer) {
			WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, utf8_buffer, utf8_len, NULL, NULL);
			DWORD bytes_written;
			WriteFile(chat_log, utf8_buffer, utf8_len - 1, &bytes_written, NULL);
			free(utf8_buffer);
		}
	}

	FlushFileBuffers(chat_log);
	CloseHandle(chat_log);
	chat_log = NULL;
}

// ======= 7. Settings =======
void LoadSettings(void) {
	HANDLE hSettingsFile = CreateFileW(INI_FILE, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hSettingsFile != INVALID_HANDLE_VALUE) {
		char buffer[4096];
		DWORD bytes_read;
		if (ReadFile(hSettingsFile, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0) {
			buffer[bytes_read] = '\0';

			char val[64];
			if (ReadIniValue(buffer, "always_on_top", val, sizeof(val))) {
				always_on_top = (val[0] == '1');
			}
			if (ReadIniValue(buffer, "flash", val, sizeof(val))) {
				flash_enabled = (val[0] == '1');
			}
			if (ReadIniValue(buffer, "sound", val, sizeof(val))) {
				sound_enabled = (val[0] == '1');
			}
			if (ReadIniValue(buffer, "leave_confirm", val, sizeof(val))) {
				confirm_enabled = (val[0] == '1');
			}
		}
		CloseHandle(hSettingsFile);
	}
}

char* ReadIniValue(const char* buffer, const char* key, char* out_value, size_t out_size) {
	if (!buffer || !key || !out_value || out_size == 0) return NULL;

	const char* p = buffer;
	size_t key_len = strlen(key);

	while (*p) {
		if (*p == '\r' || *p == '\n' || *p == ';' || *p == '#') {
			while (*p && *p != '\n') p++;
			if (*p == '\n') p++;
			continue;
		}

		if (*p == '[') {
			while (*p && *p != '\n') p++;
			if (*p == '\n') p++;
			continue;
		}

		const char* eq = strchr(p, '=');
		if (!eq) {
			while (*p && *p != '\n') p++;
			if (*p == '\n') p++;
			continue;
		}

		if (eq == p) {
			p = eq + 1;
			while (*p && *p != '\n') p++;
			if (*p =='\n') p++;
			continue;
		}

		const char* key_start = p;
		const char* key_end = eq - 1;
		while (key_end > key_start && (*key_end == ' ' || *key_end == '\t')) key_end--;

		size_t found_key_len = key_end - key_start + 1;
		if (found_key_len == key_len && strncmp(key_start, key, key_len) == 0) {
			const char* val_start = eq + 1;
			while (*val_start == ' ' || *val_start == '\t') val_start++;

			const char* val_end = val_start + strlen(val_start) - 1;
			while (val_end > val_start && (*val_end == ' ' || *val_end == '\t' || *val_end == '\r' || *val_end == '\n')) {
				val_end--;
			}

			size_t val_len = val_end - val_start + 1;
			if (val_len >= out_size) val_len = out_size - 1;

			memcpy(out_value, val_start, val_len);
			out_value[val_len] = '\0';
			return out_value;
		}

		p = eq + 1;
		while (*p && *p != '\n') p++;
		if (*p == '\n') p++;
	}

	return NULL;
}

void SaveSettings(void) {
	HANDLE hSettingsFile = CreateFileW(INI_FILE, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hSettingsFile != INVALID_HANDLE_VALUE) {
		char buffer[256];
		int len = snprintf(buffer, sizeof(buffer),
			"[QuickChat]\r\n"
			"always_on_top=%d\r\n"
			"flash=%d\r\n"
			"sound=%d\r\n"
			"leave_confirm=%d\r\n",
			always_on_top ? 1 : 0,
			flash_enabled ? 1 : 0,
			sound_enabled ? 1 : 0,
			confirm_enabled ? 1 : 0);

		DWORD bytes_written;
		WriteFile(hSettingsFile, buffer, len, &bytes_written, NULL);
		CloseHandle(hSettingsFile);
	}
}

void ResetSettings(HWND hWnd) {
	int result = MessageBoxW(hWnd, L"Are you sure you want to reset all settings?", L"QuickChat", MB_YESNO | MB_ICONWARNING);

	if (result == IDYES) {
		DeleteFileW(INI_FILE);

		always_on_top = FALSE;
		flash_enabled = TRUE;
		sound_enabled = TRUE;
		confirm_enabled = TRUE;

		CheckMenuItem(GetMenu(hWnd), IDM_ALWAYS_ON_TOP, MF_BYCOMMAND | MF_UNCHECKED);
		CheckMenuItem(GetMenu(hWnd), IDM_FLASH_TOGGLE, MF_BYCOMMAND | MF_CHECKED);
		CheckMenuItem(GetMenu(hWnd), IDM_SOUND_TOGGLE, MF_BYCOMMAND | MF_CHECKED);
		CheckMenuItem(GetMenu(hWnd), IDM_CONFIRM_TOGGLE, MF_BYCOMMAND | MF_CHECKED);

		SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

		AddMessage(L"[INFO]: Settings have been reset to default values.");
	}
}

// ======= 8. Utilities =======
void EnableVisualStyles(void) {
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icex);
}

void GetLocalComputerName(void) {
	DWORD size = sizeof(computer_name) / sizeof(wchar_t);
	GetComputerNameW(computer_name, &size);
}

void ShowError(const wchar_t* msg, DWORD err) {
	wchar_t buffer[512];
	swprintf(buffer, sizeof(buffer) / sizeof(wchar_t), L"%ls. Error: %lu", msg, err);
	MessageBoxW(NULL, buffer, L"QuickChat", MB_OK | MB_ICONERROR);
}

BOOL IsValidTargetIP(const wchar_t* ip_str) {
	int o1, o2, o3, o4;
	if (swscanf(ip_str, L"%d.%d.%d.%d", &o1, &o2, &o3, &o4) != 4) return FALSE;

	if (o1 == 0) return FALSE;
	if (o4 == 0) return FALSE;
	if (o4 == 255) return FALSE;
	if (o1 >= 224 && o1 <= 239) return FALSE;
	if (o1 >= 240) return FALSE;

	return TRUE;
}

BOOL GetDefaultIP(wchar_t *ip_buffer, size_t size) {
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return FALSE;

	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET) {
		WSACleanup();
		return FALSE;
	}

	struct sockaddr_in remote = {0};
	remote.sin_family = AF_INET;
	remote.sin_port = htons(53);
	remote.sin_addr.s_addr = inet_addr("8.8.8.8");

	if (connect(s, (struct sockaddr*)&remote, sizeof(remote)) != 0) {
		closesocket(s);
		WSACleanup();
		return FALSE;
	}

	struct sockaddr_in local;
	int len = sizeof(local);
	if (getsockname(s, (struct sockaddr*)&local, &len) != 0) {
		closesocket(s);
		WSACleanup();
		return FALSE;
	}

	char ip_utf8[16];
	strncpy(ip_utf8, inet_ntoa(local.sin_addr), 15);
	ip_utf8[15] = '\0';
	MultiByteToWideChar(CP_UTF8, 0, ip_utf8, -1, ip_buffer, size);

	closesocket(s);
	WSACleanup();
	return TRUE;
}

// ======= 9. Network: Low-Level =======
void XorObf(unsigned char *data, int len) {
	if (!xor_enabled) return;

	unsigned char k[KEY_LEN];
	memcpy(k, key, KEY_LEN);

	for (int i = 0; i < len; i++) data[i] ^= k[i % KEY_LEN];

	memset(k, 0, KEY_LEN);
}

// ======= 10. UI Proxy =======
static void UIAddMessage(const wchar_t* msg) {
	if (!hWndGlobal || !msg) return;
	wchar_t* copy = _wcsdup(msg);
	if (!copy) return;
	if (!PostMessageW(hWndGlobal, WM_APP_ADDMSG, 0, (LPARAM)copy)) free(copy);
}

static void UIDisableControls(void) {
	if (hWndGlobal) PostMessageW(hWndGlobal, WM_APP_DISABLE, 0, 0);
}

static void UIFlash(void) {
	if (hWndGlobal) PostMessageW(hWndGlobal, WM_APP_FLASH, 0, 0);
}

// ======= 11. Network: Core =======
void Disconnect(void) {
	SendLeaveMessage();
	CleanupAndExit();
}

void CleanupAndExit(void) {
	static volatile LONG already_exiting = 0;
	if (InterlockedExchange(&already_exiting, 1)) return;

	SaveSettings();
	InterlockedExchange(&is_running, 0);

	if (client_socket != INVALID_SOCKET) {
		shutdown(client_socket, SD_BOTH);
	}

	if (hReceiveThread != NULL) {
		CloseHandle(hReceiveThread);
		hReceiveThread = NULL;
	}

	if (client_socket != INVALID_SOCKET) {
		closesocket(client_socket);
		client_socket = INVALID_SOCKET;
	}

	CloseLog();
	WSACleanup();
	CleanupGdiResources();
	PostQuitMessage(0);
}

void SendLeaveMessage(void) {
	wchar_t leave_msg[512];
	swprintf(leave_msg, sizeof(leave_msg) / sizeof(wchar_t), L"[DISCONNECT]: %ls left the chat.", computer_name);
	AddMessage(leave_msg);
	if (is_server) LogMessage(leave_msg);

	if (client_socket != INVALID_SOCKET && InterlockedCompareExchange(&is_running, 1, 1)) {
		char utf8_msg[2048];
		WideCharToMultiByte(CP_UTF8, 0, leave_msg, -1, utf8_msg, sizeof(utf8_msg), NULL, NULL);
		int msg_len = strlen(utf8_msg);

		unsigned char encbuf[2048];
		memcpy(encbuf, utf8_msg, msg_len);
		XorObf(encbuf, msg_len);
		send(client_socket, (char*)encbuf, msg_len, 0);
	}
}

void SendCurrentMessage(void) {
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
		swprintf(full_msg, sizeof(full_msg) / sizeof(wchar_t), L"[%ls]: %ls", computer_name, start);
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
}

BOOL InitializeNetwork(BOOL server_mode, HINSTANCE hInstance, int nCmdShow) {
	GetLocalComputerName();

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		ShowError(L"WSAStartup failed.", WSAGetLastError());
		return FALSE;
	}

	BOOL result = server_mode ? StartServer(hInstance, nCmdShow) : StartClient(hInstance, nCmdShow);

	if (!result) {
		WSACleanup();
		return FALSE;
	}

	unsigned int threadID;
	hReceiveThread = (HANDLE)_beginthreadex(NULL, 0, ReceiveMessages, NULL, 0, &threadID);

	if (hReceiveThread == NULL) {
		ShowError(L"Failed to start receive thread.", GetLastError());
		CleanupAndExit();
		return FALSE;
	}

	return TRUE;
}

BOOL StartServer(HINSTANCE hInstance, int nCmdShow) {
	SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == INVALID_SOCKET) {
		ShowError(L"Failed to create socket.", WSAGetLastError());
		WSACleanup();
		return FALSE;
	}

	int active_port = xor_enabled ? PORT_XOR : PORT_PLAIN;
	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(active_port);

	if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		DWORD err = WSAGetLastError();
		if (err == WSAEADDRINUSE) {
			MessageBoxW(NULL, L"Port is already in use. Another QuickChat host may be running.", L"QuickChat", MB_OK | MB_ICONWARNING);
		} else {
			ShowError(L"Bind failed.", err);
		}
		closesocket(server_fd);
		WSACleanup();
		return FALSE;
	}

	if (!GetDefaultIP(local_ip, sizeof(local_ip) / sizeof(wchar_t))) {
		wcscpy(local_ip, L"<No LAN route>");
		LogMessage(L"[WARNING]: Could not detect LAN IP; using placeholder.");
	}

	wchar_t bind_msg[512];
	const wchar_t* mode_str = xor_enabled ? L"QC with XOR" : L"QC";
	swprintf(bind_msg, sizeof(bind_msg) / sizeof(wchar_t), L"Host started: %ls on address %ls port %d.", mode_str, local_ip, active_port);
	LogMessage(bind_msg);

	if (listen(server_fd, 1) == SOCKET_ERROR) {
		closesocket(server_fd);
		WSACleanup();
		return FALSE;
	}

	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ShowServerIPMessage, NULL, 0, NULL);
	if (hThread) CloseHandle(hThread);

	while (1) {
		struct sockaddr_in client_addr;
		int addr_len = sizeof(client_addr);

		SOCKET temp_client = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
		if (temp_client == INVALID_SOCKET) {
			DWORD err = WSAGetLastError();
			if (err == WSAEINTR || err == WSAENOTSOCK) {
				break;
			}
			Sleep(100);
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

			if (!xor_enabled) {
				send(temp_client, "QCERR: No data", 14, 0);
			}

			closesocket(temp_client);
			continue;
		}

		if (recv_len >= (int)sizeof(hs) - 1) {
			LogMessage(L"[SECURITY]: Handshake is too large. Connection closed.");

			if (!xor_enabled) {
				send(temp_client, "QCERR: Packet is too large", 26, 0);
			}

			closesocket(temp_client);
			continue;
		}

		XorObf((unsigned char*)hs, recv_len);
		hs[recv_len] = '\0';

		if (strncmp(hs, QC_LABEL, strlen(QC_LABEL)) != 0) {
			LogMessage(L"[SECURITY]: Invalid handshake. Connection closed.");

			if (!xor_enabled) {
				send(temp_client, "QCERR: Invalid handshake", 24, 0);
			}

			closesocket(temp_client);
			continue;
		}

		const char* name_ptr = hs + strlen(QC_LABEL);
		if (*name_ptr == '\0') {
			LogMessage(L"[SECURITY]: Empty name in handshake. Connection closed.");

			if (!xor_enabled) {
				send(temp_client, "QCERR: Invalid handshake", 24, 0);
			}

			closesocket(temp_client);
			continue;
		}

		client_socket = temp_client;

		char ip_utf8[16];
		strncpy(ip_utf8, inet_ntoa(client_addr.sin_addr), 15);
		ip_utf8[15] = '\0';
		MultiByteToWideChar(CP_UTF8, 0, ip_utf8, -1, remote_ip, sizeof(remote_ip) / sizeof(wchar_t));

		int wlen = MultiByteToWideChar(CP_UTF8, 0, name_ptr, -1, remote_name, sizeof(remote_name) / sizeof(wchar_t));
		if (wlen <= 0 || remote_name[0] == L'\0') {
			wcscpy(remote_name, L"<Unknown>");
			LogMessage(L"[SECURITY]: Invalid peer name. Using placeholder.");
		}
		break;
	}

	closesocket(server_fd);

	char hs_reply[256];
	int pos = snprintf(hs_reply, sizeof(hs_reply), "%s", QC_LABEL);
	WideCharToMultiByte(CP_UTF8, 0, computer_name, -1, hs_reply + pos, sizeof(hs_reply) - pos, NULL, NULL);

	int hs_r_len = strlen(hs_reply);
	XorObf((unsigned char*)hs_reply, hs_r_len);
	send(client_socket, hs_reply, hs_r_len, 0);
	
	if (hHostInfoDlg) {
		PostMessageW(hHostInfoDlg, WM_CLOSE, 0, 0);
		hHostInfoDlg = NULL;
	}

	ShowMainWindow(hInstance, nCmdShow);

	wchar_t join_msg[512];
	swprintf(join_msg, sizeof(join_msg) / sizeof(wchar_t), L"[CONNECT]: %ls connected from %ls.", remote_name, remote_ip);
	AddMessage(join_msg);
	LogMessage(join_msg);
	
	return TRUE;
}

BOOL StartClient(HINSTANCE hInstance, int nCmdShow) {
	client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket == INVALID_SOCKET) {
		ShowError(L"Failed to create socket.", WSAGetLastError());
		WSACleanup();
		return FALSE;
	}

	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

	int active_port = xor_enabled ? PORT_XOR : PORT_PLAIN;
	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(active_port);

	char remote_ip_utf8[16];
	WideCharToMultiByte(CP_UTF8, 0, remote_ip, -1, remote_ip_utf8, sizeof(remote_ip_utf8), NULL, NULL);
	server_addr.sin_addr.s_addr = inet_addr(remote_ip_utf8);

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
				ShowError(L"Connection failed", err);
		}
		closesocket(client_socket);
		WSACleanup();
		return FALSE;
	}

	timeout.tv_sec = 0;
	setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

	struct sockaddr_in server_info;
	int len = sizeof(server_info);
	if (getsockname(client_socket, (struct sockaddr*)&server_info, &len) != 0) {
		wcscpy(local_ip, L"<Unknown>");
	} else {
		wchar_t ip_w[64];
		DWORD ip_len = 64;
		WSAAddressToStringW((LPSOCKADDR)&server_info, sizeof(server_info), NULL, ip_w, &ip_len);

		wchar_t* colon = wcschr(ip_w, L':');
		if (colon) *colon = L'\0';

		wcsncpy(local_ip, ip_w, 15);
		local_ip[15] = L'\0';
	}

	char hs[256];
	int pos = snprintf(hs, sizeof(hs), "%s", QC_LABEL);
	WideCharToMultiByte(CP_UTF8, 0, computer_name, -1, hs + pos, sizeof(hs) - pos, NULL, NULL);
	int hs_len = strlen(hs);
	XorObf((unsigned char*)hs, hs_len);
	send(client_socket, hs, hs_len, 0);

	char hs_reply[256];
	int recv_len = recv(client_socket, hs_reply, sizeof(hs_reply) - 1, 0);
	if (recv_len <= 0) {
		ShowError(L"Failed to receive peer handshake", WSAGetLastError());
		closesocket(client_socket);
		WSACleanup();
		return FALSE;
	}

	XorObf((unsigned char*)hs_reply, recv_len);
	hs_reply[recv_len] = '\0';

	if (strncmp(hs_reply, QC_LABEL, strlen(QC_LABEL)) != 0) {
		MessageBoxW(NULL, L"Invalid handshake from remote host.", L"QuickChat", MB_OK | MB_ICONERROR);
		closesocket(client_socket);
		WSACleanup();
		return FALSE;
	}

	const char* name_ptr = hs_reply + strlen(QC_LABEL);
	MultiByteToWideChar(CP_UTF8, 0, name_ptr, -1, remote_name, 256);
	if (remote_name[0] == L'\0') wcscpy(remote_name, L"<Unknown>");

	ShowMainWindow(hInstance, nCmdShow);

	wchar_t join_msg[512];
	swprintf(join_msg, sizeof(join_msg) / sizeof(wchar_t), L"[CONNECT]: Connected to %ls at %ls.", remote_name, remote_ip);
	AddMessage(join_msg);
	
	return TRUE;
}

unsigned int __stdcall ReceiveMessages(void*) {
	char buffer[BUFFER_SIZE];

	while (InterlockedCompareExchange(&is_running, 1, 1) != 0) {
		int bytes = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
		if (InterlockedCompareExchange(&is_running, 1, 1) == 0) break;

		if (bytes == SOCKET_ERROR) {
			int err_code = WSAGetLastError();
			if (err_code == WSAETIMEDOUT) continue;

			wchar_t connlost_err[512];
			swprintf(connlost_err, sizeof(connlost_err) / sizeof(wchar_t), L"[ERROR]: Connection with remote computer lost. (Error: %d)", err_code);
			UIAddMessage(connlost_err);
			if (is_server) LogMessage(connlost_err);

			UIDisableControls();
			InterlockedExchange(&is_running, 0);
			break;
		}

		if (bytes == 0) {
			wchar_t close_msg[512] = L"[DISCONNECT]: Remote computer has closed the connection.";
			UIAddMessage(close_msg);
			if (is_server) LogMessage(close_msg);

			UIDisableControls();

			InterlockedExchange(&is_running, 0);
			break;
		}

		ProcessIncomingMessage(buffer, bytes);
	}

	return 0;
}

void ProcessIncomingMessage(char* buffer, int bytes) {
	XorObf((unsigned char*)buffer, bytes);

	int b = bytes;
	int pos = bytes;
	while (pos > 0 && ((unsigned char)buffer[pos - 1] & 0xC0) == 0x80) pos--;
	if (pos == 0) {
		b = 0;
	} else {
		unsigned char lead = (unsigned char)buffer[pos - 1];
		int need = 0;
		if ((lead & 0x80) == 0x00) need = 0;
		else if ((lead & 0xE0) == 0xC0) need = 1;
		else if ((lead & 0xF0) == 0xE0) need = 2;
		else if ((lead & 0xF8) == 0xF0) need = 3;
		else need = -1;

		int have = bytes - pos;
		if (need == -1 || have < need) {
			b = pos - 1;
		} else {
			b = bytes;
		}
	}
	buffer[b] = '\0';

	UIFlash();

	if (b >= 12 && memcmp(buffer, "[DISCONNECT]", 12) == 0) {
		UIDisableControls();
		PlayNotifySound(SOUND_LEAVE);
	} else {
		PlayNotifySound(SOUND_MSG);
	}

	wchar_t wide_buffer[BUFFER_SIZE];
	if (MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wide_buffer, BUFFER_SIZE) <= 0) return;
	UIAddMessage(wide_buffer);
	if (is_server) LogMessage(wide_buffer);
}

// ======= 12. User Interface: Window =======
void ShowMainWindow(HINSTANCE hInstance, int nCmdShow) {
	WNDCLASSW wc = {0};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"QuickChatWndClass";
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.hIcon = LoadIconW(hInstance, L"ICON");
	wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
	RegisterClassW(&wc);

	wchar_t title[512];
	if (remote_name[0] != L'\0') {
		swprintf(title, sizeof(title) / sizeof(wchar_t), L"QuickChat - %ls", remote_name);
	} else {
		wcscpy(title, L"QuickChat - <No name>");
	}

	HDC hdc = GetDC(NULL);
	int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(NULL, hdc);

	int width = MulDiv(600, dpi, 96);
	int height = MulDiv(395, dpi, 96);

	HWND hWnd = CreateWindowW(L"QuickChatWndClass", title,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height,
		NULL, NULL, hInstance, NULL);

	if (!hWnd) {
		MessageBoxW(NULL, L"Failed to create main window.", L"QuickChat", MB_OK | MB_ICONERROR);
		return;
	}

	hWndGlobal = hWnd;
	LoadSettings();
	CreateAppMenu(hWnd);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE:
			CreateAppFonts(hWnd);
			CreateAppControls(hWnd);
			return 0;

		case WM_COMMAND: {
			int id = LOWORD(wParam);
			if (LOWORD(wParam) == ID_SEND) {
				HandleSendCommand();
			} else {
				HandleMenuCommand(hWnd, id);
			}
			return 0;
		}

		case WM_CLOSE:
			HandleMenuCommand(hWnd, IDM_LEAVE);
			return 0;

		case WM_DESTROY:
			CleanupAndExit();
			return 0;

		case WM_SIZE:
			ResizeMainWindow(hWnd, LOWORD(lParam), HIWORD(lParam));
			return 0;

		case WM_SETFOCUS:
			SetFocus(hEdit);
			break;

		case WM_DROPFILES: {
			HDROP hDrop = (HDROP)wParam;
			ProcessDroppedFile(hWnd, hDrop);
			DragFinish(hDrop);
			return 0;
		}

		case WM_APP_ADDMSG: {
			wchar_t* msg = (wchar_t*)lParam;
			if (msg) { AddMessage(msg); free(msg); }
			return 0;
		}

		case WM_APP_DISABLE:
			DisableChatControls(TRUE);
			DragAcceptFiles(hWnd, FALSE);
			return 0;

		case WM_APP_FLASH:
			FlashMessageWindow(hWnd);
			return 0;
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void CreateAppMenu(HWND hWnd) {
	HMENU hMenu = CreateMenu();

	HMENU hConn = CreatePopupMenu();
	AppendMenuW(hConn, MF_STRING, IDM_COMPUTER_INFO, L"Computer Info");
	AppendMenuW(hConn, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hConn, MF_STRING, IDM_SAVE, L"Save Chat");
	AppendMenuW(hConn, MF_STRING, IDM_LEAVE, L"Leave Chat");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hConn, L"Connection");

	HMENU hView = CreatePopupMenu();
	AppendMenuW(hView, MF_STRING | MF_UNCHECKED, IDM_ALWAYS_ON_TOP, L"Always On Top");
	AppendMenuW(hView, MF_STRING, IDM_CLEAR_CHAT, L"Clear Chat");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hView, L"View");

	HMENU hOpts = CreatePopupMenu();
	AppendMenuW(hOpts, MF_STRING | MF_UNCHECKED, IDM_CONFIRM_TOGGLE, L"Leave Confirmation");
	AppendMenuW(hOpts, MF_STRING | MF_UNCHECKED, IDM_FLASH_TOGGLE, L"Window Flash");
	AppendMenuW(hOpts, MF_STRING | MF_UNCHECKED, IDM_SOUND_TOGGLE, L"Sound");
	AppendMenuW(hOpts, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hOpts, MF_STRING, IDM_RESET_SETTINGS, L"Reset Settings");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hOpts, L"Options");

	HMENU hHelp = CreatePopupMenu();
	AppendMenuW(hHelp, MF_STRING, IDM_ABOUT, L"About");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"Help");

	SetMenu(hWnd, hMenu);

	CheckMenuItem(hMenu, IDM_ALWAYS_ON_TOP, MF_BYCOMMAND | (always_on_top ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hMenu, IDM_CONFIRM_TOGGLE, MF_BYCOMMAND | (confirm_enabled ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hMenu, IDM_SOUND_TOGGLE, MF_BYCOMMAND | (sound_enabled ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hMenu, IDM_FLASH_TOGGLE, MF_BYCOMMAND | (flash_enabled ? MF_CHECKED : MF_UNCHECKED));
}

void CreateAppFonts(HWND hWnd) {
	HDC hdc = GetDC(hWnd);
	int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(hWnd, hdc);

	LOGFONTW lf = {0};
	lf.lfHeight = -MulDiv(9, dpi, 72);
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfQuality = DEFAULT_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	wcscpy(lf.lfFaceName, L"Tahoma");

	lf.lfWeight = FW_NORMAL;
	hFont = CreateFontIndirectW(&lf);

	lf.lfWeight = FW_BOLD;
	hFontBold = CreateFontIndirectW(&lf);
}

void CreateAppControls(HWND hWnd) {
	hMsgDisplay = CreateWindowW(L"EDIT", L"", 
		WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_VSCROLL | 
		ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
		0, 0, 594, 275, hWnd, (HMENU)ID_MSG_DISPLAY, NULL, NULL);

	hEdit = CreateWindowW(L"EDIT", L"", 
		WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_VSCROLL | 
		ES_MULTILINE | ES_WANTRETURN | ES_NOHIDESEL,
		0, 278, 510, 46, hWnd, (HMENU)ID_EDIT, NULL, NULL);

	hSendBtn = CreateWindowW(L"BUTTON", L"Send", 
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		510, 278, 85, 46, hWnd, (HMENU)ID_SEND, NULL, NULL);

	SetWindowPos(hWnd, always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SendMessageW(hMsgDisplay, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessageW(hSendBtn, WM_SETFONT, (WPARAM)hFontBold, TRUE);

	SendMessageW(hEdit, EM_SETLIMITTEXT, BUFFER_SIZE - 1, 0);
	oldEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);
	oldDisplayProc = (WNDPROC)SetWindowLongPtrW(hMsgDisplay, GWLP_WNDPROC, (LONG_PTR)EditProc);
	DragAcceptFiles(hWnd, TRUE);
}

void ResizeMainWindow(HWND hWnd, int width, int height) {
	(void)hWnd;
	
	if (!hMsgDisplay || !hEdit || !hSendBtn) return;

	HDC hdc = GetDC(hWnd);
	int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(hWnd, hdc);

	const int base_edit_height = 46;
	const int base_send_width = 85;

	int edit_height = MulDiv(base_edit_height, dpi, 96);
	int send_width = MulDiv(base_send_width, dpi, 96);

	if (edit_height < 20) edit_height = 20;
	if (send_width < 40) send_width = 40;

	SetWindowPos(hMsgDisplay, NULL, 0, 0, width, height - edit_height, SWP_NOZORDER);
	SetWindowPos(hEdit, NULL, 0, height - edit_height, width - send_width, edit_height, SWP_NOZORDER);
	SetWindowPos(hSendBtn, NULL, width - send_width, height - edit_height, send_width, edit_height, SWP_NOZORDER);
}

void CleanupGdiResources(void) {
	if (oldEditProc) {
		SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)oldEditProc);
	}

	if (oldDisplayProc) {
		SetWindowLongPtrW(hMsgDisplay, GWLP_WNDPROC, (LONG_PTR)oldDisplayProc);
	}

	if (hFont) {
		DeleteObject(hFont);
		hFont = NULL;
	}


	if (hFontBold) {
		DeleteObject(hFontBold);
		hFontBold = NULL;
	}
}

void HandleSendCommand(void) {
	int textLen = GetWindowTextLengthW(hEdit);
	SendMessageW(hEdit, EM_SETSEL, textLen, textLen);
	SendCurrentMessage();
	SetFocus(hEdit);
}

void HandleMenuCommand(HWND hWnd, int id) {
	switch (id) {
		case IDM_LEAVE:
			if (!is_running) {
				CleanupAndExit();
				return;
			}

			if (!confirm_enabled) {
				Disconnect();
				return;
			}

			if (MessageBoxW(hWnd, L"Leave current chat?", L"QuickChat", MB_ICONQUESTION | MB_YESNO) == IDYES) {
				Disconnect();
			}
			break;
		
		case IDM_ABOUT:
			DialogBoxParamW(hInstGlobal, MAKEINTRESOURCEW(3), hWnd, AboutDialogProc, 0);
			break;
		
		case IDM_COMPUTER_INFO:
			DialogBoxParamW(hInstGlobal, MAKEINTRESOURCEW(4), hWnd, ComputerInfoProc, 0);
			break;
		
		case IDM_CLEAR_CHAT:
			if (MessageBoxW(hWnd, L"Clear chat window? This action cannot be undone.", L"QuickChat", MB_ICONQUESTION | MB_YESNO) == IDYES) {
				SetWindowTextW(hMsgDisplay, L"");
			}
			break;
		
		case IDM_ALWAYS_ON_TOP:
			always_on_top = !always_on_top;
			SetWindowPos(hWnd, always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			CheckMenuItem(GetMenu(hWnd), IDM_ALWAYS_ON_TOP, MF_BYCOMMAND | (always_on_top ? MF_CHECKED : MF_UNCHECKED));
			break;
		
		case IDM_SOUND_TOGGLE:
			sound_enabled = !sound_enabled;
			CheckMenuItem(GetMenu(hWnd), IDM_SOUND_TOGGLE, MF_BYCOMMAND | (sound_enabled ? MF_CHECKED : MF_UNCHECKED));
			break;
		
		case IDM_FLASH_TOGGLE:
			flash_enabled = !flash_enabled;
			CheckMenuItem(GetMenu(hWnd), IDM_FLASH_TOGGLE, MF_BYCOMMAND | (flash_enabled ? MF_CHECKED : MF_UNCHECKED));
			break;
		
		case IDM_SAVE:
			SaveChatToFile(hWnd);
			break;
		
		case IDM_RESET_SETTINGS:
			ResetSettings(hWnd);
			break;
		
		case IDM_CONFIRM_TOGGLE:
			confirm_enabled = !confirm_enabled;
			CheckMenuItem(GetMenu(hWnd), IDM_CONFIRM_TOGGLE, MF_BYCOMMAND | (confirm_enabled ? MF_CHECKED : MF_UNCHECKED));
			break;
	}
}

LRESULT CALLBACK EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	WNDPROC oldProc = (hWnd == hEdit) ? oldEditProc : oldDisplayProc;
	
	if (uMsg == WM_CHAR) {
		if (wParam == VK_TAB) return 0;
		if (wParam == VK_RETURN) {
			if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
				return 0;
			}
		}
	}
	
	if (uMsg == WM_GETDLGCODE) {
		return DLGC_WANTCHARS | DLGC_WANTARROWS | DLGC_WANTMESSAGE;
	}

	if (uMsg == WM_KEYDOWN) {
		if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
			SendMessageW(hWnd, EM_SETSEL, 0, -1);
			return 0;
		}

		if (wParam == VK_TAB) {
			HWND hNext = GetNextDlgTabItem(GetParent(hWnd), hWnd, GetKeyState(VK_SHIFT) & 0x8000);
			if (hNext) SetFocus(hNext);
			return 0;
		}
		
		if (wParam == VK_RETURN) {
			if (hWnd == hEdit) {
				if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
					return CallWindowProcW(oldProc, hWnd, uMsg, wParam, lParam);
				} else {
					PostMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(ID_SEND, 0), 0);
					return 0;
				}
			} else {
				return 0;
			}
		}
	}

	return CallWindowProcW(oldProc, hWnd, uMsg, wParam, lParam);
}

void AddMessage(const wchar_t* msg) {
	if (!hMsgDisplay || !msg || !*msg) return;

	wchar_t truncated[BUFFER_SIZE];
	const wchar_t* to_display = msg;

	if (wcslen(msg) >= BUFFER_SIZE) {
		wcsncpy(truncated, msg, BUFFER_SIZE - 20);
		truncated[BUFFER_SIZE - 20] = L'\0';
		wcscat(truncated, L"... [truncated]");
		to_display = truncated;

		wchar_t log_msg[512];
		swprintf(log_msg, sizeof(log_msg) / sizeof(wchar_t), L"[WARNING] Message is too long (%zu chars), truncated to %d.", wcslen(msg), BUFFER_SIZE - 1);
		if (is_server) LogMessage(log_msg);
	}

	int len = GetWindowTextLengthW(hMsgDisplay);
	SendMessageW(hMsgDisplay, EM_SETSEL, len, len);
	if (len > 0) SendMessageW(hMsgDisplay, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");

	SendMessageW(hMsgDisplay, EM_REPLACESEL, FALSE, (LPARAM)to_display);
	SendMessageW(hMsgDisplay, WM_VSCROLL, SB_BOTTOM, 0);
}

void DisableChatControls(BOOL disable) {
	SendMessageW(hEdit, EM_SETREADONLY, disable ? TRUE : FALSE, 0);
	EnableWindow(hSendBtn, !disable);
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

// ======= 13. User Interface: Dialogs =======
DWORD WINAPI ShowServerIPMessage(LPVOID lpParam) {
	(void)lpParam;
	DialogBoxParamW(hInstGlobal, MAKEINTRESOURCEW(5), NULL, HostInfoProc, 0);
	return 0;
}

INT_PTR CALLBACK ModeSelectProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	(void) lParam;

	switch (msg) {
		case WM_INITDIALOG:
			CheckDlgButton(hwnd, 201, xor_enabled ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwnd, 202, logging_enabled ? BST_CHECKED : BST_UNCHECKED);
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == 101) {
				is_server = TRUE;
				EndDialog(hwnd, IDOK);
				return TRUE;
			}

			if (LOWORD(wParam) == 102) {
				is_server = FALSE;
				EndDialog(hwnd, IDOK);
				return TRUE;
			}

			if (LOWORD(wParam) == 103 || LOWORD(wParam) == IDCANCEL) {
				EndDialog(hwnd, IDCANCEL);
				return TRUE;
			}

			if (LOWORD(wParam) == 201) {
				xor_enabled = IsDlgButtonChecked(hwnd, 201) == BST_CHECKED;
			}

			if (LOWORD(wParam) == 202) {
				logging_enabled = IsDlgButtonChecked(hwnd, 202) == BST_CHECKED;
			}
			break;

		case WM_CLOSE:
			EndDialog(hwnd, IDCANCEL);
			return TRUE;
	}
	return FALSE;
}

INT_PTR CALLBACK ConnectDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	(void)lParam;
	
	switch (msg) {
		case WM_INITDIALOG:
			SetDlgItemTextW(hwnd, 1001, L"127.0.0.1");
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK) {
				wchar_t ip[16];
				GetDlgItemTextW(hwnd, 1001, ip, sizeof(ip) / sizeof(wchar_t));

				wchar_t *p = ip;
				while (*p == L' ') p++;

				int len = wcslen(p);
				while (len > 0 && p[len-1] == L' ') {
					p[len-1] = L'\0';
					len--;
				}

				if (wcslen(p) == 0) {
					MessageBoxW(hwnd, L"Host IP is required for connection.", L"QuickChat", MB_OK | MB_ICONWARNING);
					SetFocus(GetDlgItem(hwnd, 1001));
					return TRUE;
				}

				int octets[4];
				wchar_t tail = 0;
				int parsed = swscanf(p, L"%d.%d.%d.%d%lc", &octets[0], &octets[1], &octets[2], &octets[3], &tail);
				int valid = (parsed == 4);

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
					SetFocus(GetDlgItem(hwnd, 1001));
					return TRUE;
				}

				if (!IsValidTargetIP(p)) {
					MessageBoxW(hwnd, L"This IP address is valid, but cannot be used for connection.", L"QuickChat", MB_OK | MB_ICONWARNING);
					SetFocus(GetDlgItem(hwnd, 1001));
					return TRUE;
				}
 
				wcscpy(remote_ip, p);
				remote_ip[sizeof(remote_ip) / sizeof(wchar_t) - 1] = L'\0';
				EndDialog(hwnd, IDOK);
			} else if (LOWORD(wParam) == IDCANCEL) {
				EndDialog(hwnd, IDCANCEL);
			}
			return TRUE;
	}
	return FALSE;
}

INT_PTR CALLBACK HostInfoProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	(void)lParam;

	switch (msg) {
		case WM_INITDIALOG:
			hHostInfoDlg = hwnd;
			SetDlgItemTextW(hwnd, 301, local_ip);
			SetDlgItemTextW(hwnd, 302, xor_enabled ? L"Yes" : L"No");
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
				EndDialog(hwnd, LOWORD(wParam));
				return TRUE;
			}
			break;

		case WM_CLOSE:
			hHostInfoDlg = NULL;
			EndDialog(hwnd, IDCANCEL);
			return TRUE;
	}
	return FALSE;
}

INT_PTR CALLBACK AboutDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	(void)lParam;
	
	switch (msg) {
		case WM_INITDIALOG: {
			wchar_t version[64];
			swprintf(version, sizeof(version) / sizeof(wchar_t), L"QuickChat (%hs)", __DATE__);
			SetDlgItemTextW(hWnd, 301, version);
			return TRUE;
		}

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
				EndDialog(hWnd, LOWORD(wParam));
				return TRUE;
			}
			break;

		case WM_CLOSE:
			EndDialog(hWnd, IDCANCEL);
			return TRUE;
	}
	return FALSE;
}

INT_PTR CALLBACK ComputerInfoProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	(void)lParam;

	switch (msg) {
		case WM_INITDIALOG: {
			SetDlgItemTextW(hWnd, 105, computer_name);
			SetDlgItemTextW(hWnd, 106, local_ip);

			const wchar_t* remote_name_display = (remote_name[0] != L'\0') ? remote_name : L"N/A";
			SetDlgItemTextW(hWnd, 101, remote_name_display);

			SetDlgItemTextW(hWnd, 102, remote_ip);
			SetDlgItemTextW(hWnd, 103, xor_enabled ? L"Yes" : L"No");
			SetDlgItemTextW(hWnd, 104, is_running ? L"Yes" : L"No");
			return TRUE;
		}

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
				EndDialog(hWnd, LOWORD(wParam));
				return TRUE;
			}
			break;

		case WM_CLOSE:
			EndDialog(hWnd, IDCANCEL);
			return TRUE;
	}
	return FALSE;
}

// ======= 14. Files  =======
void SaveChatToFile(HWND hWnd) {
	wchar_t filename[MAX_PATH];
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	wcsftime(filename, MAX_PATH, L"Chat-%Y%m%d-%H%M%S.txt", tm_info);

	int len = GetWindowTextLengthW(hMsgDisplay);
	wchar_t *chat_text = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	if (!chat_text) {
		MessageBoxW(hWnd, L"Failed to save chat history: Out of memory.", L"QuickChat", MB_OK | MB_ICONERROR);
		return;
	}

	GetWindowTextW(hMsgDisplay, chat_text, len + 1);

	HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		int utf8_len = WideCharToMultiByte(CP_UTF8, 0, chat_text, -1, NULL, 0, NULL, NULL);
		if (utf8_len > 0) {
			char *utf8 = (char*)malloc(utf8_len);
			if (utf8) {
				WideCharToMultiByte(CP_UTF8, 0, chat_text, -1, utf8, utf8_len, NULL, NULL);

				DWORD bytes_written;
				if (WriteFile(hFile, utf8, utf8_len - 1, &bytes_written, NULL)) {
					wchar_t save_msg[512];
					swprintf(save_msg, sizeof(save_msg) / sizeof(wchar_t), L"[INFO]: Chat saved to %ls.", filename);
					AddMessage(save_msg);
				} else {
					ShowError(L"Failed to save chat history", GetLastError());
				}

				free(utf8);
			} else {
				MessageBoxW(hWnd, L"Failed to save chat history: Out of memory.", L"QuickChat", MB_OK | MB_ICONERROR);
			}
		}

		CloseHandle(hFile);
	} else {
		wchar_t save_err[512];
		swprintf(save_err, sizeof(save_err) / sizeof(wchar_t), L"Failed to save chat history. Error: %lu.", GetLastError());
		MessageBoxW(hWnd, save_err, L"QuickChat", MB_OK | MB_ICONERROR);
	}

	free(chat_text);
}

void ProcessDroppedFile(HWND hWnd, HDROP hDrop) {
	UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);

	if (fileCount > 1) {
		MessageBoxW(hWnd, L"Only one file can be dropped at a time.", L"QuickChat", MB_OK | MB_ICONWARNING);
		return;
	}

	wchar_t path[MAX_PATH];
	if (DragQueryFileW(hDrop, 0, path, MAX_PATH) == 0) {
		return;
	}

	if (!IsValidTextExtension(path)) {
		MessageBoxW(hWnd,
			L"Format not supported.\n\n"
			L"Supported extensions:\n"
			L".txt, .log, .md, .c, .cpp, .h, .hpp,\n"
			L".py, .js, .sh, .bat, .cmd, .ps1,\n"
			L".json, .xml, .yaml, .yml, .toml,\n"
			L".ini, .cfg, .conf, .css, .html, .htm",
			L"QuickChat", MB_OK | MB_ICONWARNING);
		return;
	}

	int existingLen = GetWindowTextLengthW(hEdit);
	if (existingLen > 0) {
		int result = MessageBoxW(hWnd, L"The current text will be replaced. Do you want to continue?", L"QuickChat", MB_YESNO | MB_ICONQUESTION);
		if (result != IDYES) {
			return;
		}
	}

	wchar_t *content = ReadTextFileContent(path, hWnd);
	if (!content) {
		return;
	}

	int max_len = (BUFFER_SIZE - 1) - (int)wcslen(computer_name) - 8;
	if ((int)wcslen(content) > max_len) {
		free(content);
		MessageBoxW(hWnd, L"File is too large for message field.", L"QuickChat", MB_OK | MB_ICONWARNING);
		return;
	}

	InsertTextIntoEdit(content);
	free(content);
}

BOOL IsValidTextExtension(const wchar_t *path) {
	const wchar_t *ext = wcsrchr(path, L'.');
	if (!ext) return FALSE;

	wchar_t ext_lower[16];
	wcsncpy(ext_lower, ext, 15);
	ext_lower[15] = L'\0';
	for (int i = 0; ext_lower[i]; i++) ext_lower[i] = towlower(ext_lower[i]);

	if (wcscmp(ext_lower, L".txt") == 0 ||
		wcscmp(ext_lower, L".log") == 0 ||
		wcscmp(ext_lower, L".md") == 0) return TRUE;

	if (wcscmp(ext_lower, L".c") == 0 ||
		wcscmp(ext_lower, L".cpp") == 0 ||
		wcscmp(ext_lower, L".h") == 0 ||
		wcscmp(ext_lower, L".hpp") == 0) return TRUE;

	if (wcscmp(ext_lower, L".py") == 0 ||
		wcscmp(ext_lower, L".js") == 0 ||
		wcscmp(ext_lower, L".sh") == 0 ||
		wcscmp(ext_lower, L".bat") == 0 ||
		wcscmp(ext_lower, L".cmd") == 0 ||
		wcscmp(ext_lower, L".ps1") == 0) return TRUE;

	if (wcscmp(ext_lower, L".json") == 0 ||
		wcscmp(ext_lower, L".xml") == 0 ||
		wcscmp(ext_lower, L".yaml") == 0 ||
		wcscmp(ext_lower, L".yml") == 0 ||
		wcscmp(ext_lower, L".toml") == 0 ||
		wcscmp(ext_lower, L".ini") == 0 ||
		wcscmp(ext_lower, L".cfg") == 0 ||
		wcscmp(ext_lower, L".conf") == 0) return TRUE;

	if (wcscmp(ext_lower, L".css") == 0 ||
		wcscmp(ext_lower, L".html") == 0 ||
		wcscmp(ext_lower, L".htm") == 0) return TRUE;

	return FALSE;
}

wchar_t* ReadTextFileContent(const wchar_t *path, HWND hWnd) {
	HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		wchar_t error_msg[512];

		if (err == ERROR_SHARING_VIOLATION) {
			swprintf(error_msg, sizeof(error_msg) / sizeof(wchar_t), L"Cannot open this file because it is being used by another process.");
		} else {
			swprintf(error_msg, sizeof(error_msg) / sizeof(wchar_t), L"Cannot open this file. Error: %lu", err);
		}

		MessageBoxW(hWnd, error_msg, L"QuickChat", MB_OK | MB_ICONERROR);
		return NULL;
	}

	DWORD size = GetFileSize(hFile, NULL);
	if (size == 0 || size == INVALID_FILE_SIZE) {
		CloseHandle(hFile);
		MessageBoxW(hWnd, L"Empty files cannot be dropped.", L"QuickChat", MB_OK | MB_ICONWARNING);
		return NULL;
	}

	char *ansi = (char*)malloc(size + 1);
	if (!ansi) {
		CloseHandle(hFile);
		MessageBoxW(hWnd, L"Failed to read file: Out of memory.", L"QuickChat", MB_OK | MB_ICONERROR);
		return NULL;
	}

	DWORD read;
	if (!ReadFile(hFile, ansi, size, &read, NULL)) {
		free(ansi);
		CloseHandle(hFile);
		wchar_t error_msg[512];
		swprintf(error_msg, sizeof(error_msg) / sizeof(wchar_t), L"Failed to read file. Error: %d", GetLastError());
		MessageBoxW(hWnd, error_msg, L"QuickChat", MB_OK | MB_ICONERROR);
		return NULL;
	}
	ansi[read] = '\0';
	CloseHandle(hFile);

	int bom_offset = 0;
	if (read >= 3 && (unsigned char)ansi[0] == 0xEF && (unsigned char)ansi[1] == 0xBB && (unsigned char)ansi[2] == 0xBF) {
		bom_offset = 3;
	}

	UINT cp = CP_UTF8;
	int offset = bom_offset;
	int wide_len = MultiByteToWideChar(CP_UTF8, 0, ansi + bom_offset, -1, NULL, 0);
	if (wide_len <= 0) {
		cp = CP_ACP;
		offset = 0;
		wide_len = MultiByteToWideChar(cp, 0, ansi, -1, NULL, 0);
		if (wide_len <= 0) {
			free(ansi);
			MessageBoxW(hWnd, L"Failed to decode the file.", L"QuickChat", MB_OK | MB_ICONWARNING);
			return NULL;
		}
	}

	wchar_t *wide = (wchar_t*)malloc(wide_len * sizeof(wchar_t));
	if (!wide) {
		free(ansi);
		MessageBoxW(hWnd, L"Failed to read file: Out of memory.", L"QuickChat", MB_OK | MB_ICONWARNING);
		return NULL;
	}

	MultiByteToWideChar(cp, 0, ansi + offset, -1, wide, wide_len);
	free(ansi);

	return wide;
}

void InsertTextIntoEdit(const wchar_t *text) {
	SetWindowTextW(hEdit, text);

	int len = GetWindowTextLengthW(hEdit);
	SendMessageW(hEdit, EM_SETSEL, len, len);
	SetFocus(hEdit);
}