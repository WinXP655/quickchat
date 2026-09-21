// QuickChat by WinXP655.
// Repository: https://github.com/WinXP655/quickchat.
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
#include "key.h"

// ======= 2. Defines =======
// ----- Static -----
#define CHATLOG_FILE L"chatlog.txt"
#define SOUND_JOIN 0
#define SOUND_LEAVE 1
#define SOUND_MSG 2
#define BUFFER_SIZE 8192
#define INI_FILE L"quickchat.ini"
#define CRASHLOG_FILE L"crashlog.txt"
#define PORT_XOR 65501
#define PORT_PLAIN 65502
#define QC_LABEL "QC:"

// ----- UI Controls -----
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
#define ID_FLASH_TOGGLE 2201
#define ID_SOUND_TOGGLE 2202
#define IDM_RESET_SETTINGS 2203
#define ID_CONFIRM_TOGGLE 2204

// ----- Menu: Help -----
#define IDM_ABOUT 2301

// ======= 3. Global variables =======
// ----- Control flags -----
bool is_server = false;
bool xor_enabled = true;
bool logging_enabled = false;
bool is_running = true;
bool sound_enabled = true;
bool flash_enabled = true;
bool always_on_top = false;
bool confirm_enabled = true;
int error_counter = 0;

// ----- Network state -----
SOCKET client_socket = INVALID_SOCKET;
HANDLE hReceiveThread = NULL;
wchar_t server_ip[16] = L"127.0.0.1";
wchar_t peer_ip[16] = L"";
wchar_t peer_name[256] = L"";
wchar_t computer_name[256] = L"";

// ----- Logging -----
HANDLE chat_log = NULL;

// ----- UI handles -----
HWND hWndGlobal = NULL;
HWND hEdit = NULL;
HWND hSendBtn = NULL;
HWND hMsgDisplay = NULL;

// ----- UI resources -----
WNDPROC oldEditProc = NULL;
HFONT hFontBold = NULL;
HFONT hFont = NULL;
HINSTANCE hInstGlobal = NULL;

// ----- Thread sync -----
volatile BOOL mainWindowReady = FALSE;

// ======= 4. Prototypes =======
// ----- Core Functions -----
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ExceptionInfo);
bool InitializeLog(HANDLE hLogFile);

// ----- Helper Functions -----
void GetLocalComputerName(void);
void LoadSettings(void);
void EnableVisualStyles(void);
void PlayNotifySound(int sound);
char* ReadIniValue(const char* buffer, const char* key, char* out_value, size_t out_size);
bool IsValidTargetIP(const wchar_t* ip_str);
void ShowError(const wchar_t* msg, DWORD err);
void CleanupAndExit(void);
void AddMessage(const wchar_t* msg);
void FlashMessageWindow(HWND hWnd);
bool GetDefaultIP(wchar_t *ip_buffer, size_t size);
void LogMessage(const wchar_t* message);
void DisableChatControls(BOOL disable);
void SaveSettings(void);
void CloseLog(void);
void Disconnect(void);
void SaveChatToFile(HWND hWnd);
void ResetSettings(HWND hWnd);

// ----- Network Core -----
bool InitializeNetwork(bool server_mode, HINSTANCE hInstance, int nCmdShow);
bool StartServer(HINSTANCE hInstance, int nCmdShow);
bool StartClient(HINSTANCE hInstance, int nCmdShow);
unsigned int __stdcall ReceiveMessages(void* arg);
void ProcessIncomingMessage(char* buffer, int bytes);
void XorObf(unsigned char *data, int len);
void Disconnect(void);

// ----- User Interface -----
INT_PTR CALLBACK ModeSelectProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ConnectDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI ShowServerIPMessage(LPVOID lpParam);
void ShowMainWindow(HINSTANCE hInstance, int nCmdShow);
INT_PTR CALLBACK HostInfoProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CreateAppMenu(HWND hWnd);
void CreateAppFonts(HWND hWnd);
void CreateAppControls(HWND hWnd);
void HandleSendCommand(HWND hWnd);
void HandleMenuCommand(HWND hWnd, int id);
void CleanupGdiResources(void);
void ResizeMainWindow(HWND hWnd, int width, int height);
LRESULT CALLBACK EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void SendCurrentMessage(HWND hWnd);
INT_PTR CALLBACK AboutDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ComputerInfoProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ----- Drag and Drop -----
void ProcessDroppedFile(HWND hWnd, HDROP hDrop);
bool IsValidTextExtension(const wchar_t *path);
wchar_t* ReadTextFileContent(const wchar_t *path, HWND hWnd);
void InsertTextIntoEdit(const wchar_t *text);

// ======== 5. Core Functions =======
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	(void)hPrevInstance;
	(void)lpCmdLine;
	hInstGlobal = hInstance;

	SetUnhandledExceptionFilter(CrashHandler);
	GetLocalComputerName();
	LoadSettings();
	EnableVisualStyles();

	INT_PTR mode_result = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(2), NULL, ModeSelectProc, 0);
	if (mode_result < 0) {
		MessageBoxW(NULL, L"Could not load initial dialog.", L"QuickChat", MB_OK | MB_ICONERROR);
		return 0;
	}
	if (mode_result != IDOK) return 0;

	if (is_server && logging_enabled) {
		HANDLE hLogFile = CreateFileW(CHATLOG_FILE, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hLogFile != INVALID_HANDLE_VALUE) {
			if (!InitializeLog(hLogFile)) {
				wchar_t err_msg[512];
				swprintf(err_msg, sizeof(err_msg) / sizeof(wchar_t), L"Failed to write log header. Error: %lu.", GetLastError());
				MessageBoxW(NULL, err_msg, L"QuickChat", MB_OK | MB_ICONWARNING);
				CloseHandle(hLogFile);
				logging_enabled = false;
			} else {
				chat_log = hLogFile;
			}
		} else {
			wchar_t log_err[512];
			swprintf(log_err, sizeof(log_err) / sizeof(wchar_t), L"Failed to open log file. Logging disabled. Error: %lu.", GetLastError());
			MessageBoxW(NULL, log_err, L"QuickChat", MB_OK | MB_ICONWARNING);
			logging_enabled = false;
		}
	}

	if (!is_server) {
		while (1) {
			INT_PTR dlg = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(1), NULL, ConnectDialogProc, 0);
			if (dlg < 0) {
				MessageBoxW(NULL, L"Could not load connection dialog.", L"QuickChat", MB_OK | MB_ICONERROR);
				return 0;
			}
			if (dlg != IDOK) return 0;

			break;
		}
	}

	if (!InitializeNetwork(is_server, hInstance, nCmdShow)) return 0;

	PlayNotifySound(SOUND_JOIN);

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

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

bool InitializeLog(HANDLE hLogFile) {
	if (hLogFile == INVALID_HANDLE_VALUE) return FALSE;

	SetFilePointer(hLogFile, 0, NULL, FILE_END);

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	wchar_t header[256];
	wcsftime(header, sizeof(header) / sizeof(wchar_t), L"\r\n=== New session started at %H:%M:%S %d/%m/%Y ===\r\n", t);

	int utf8_len = WideCharToMultiByte(CP_UTF8, 0, header, -1, NULL, 0, NULL, NULL);
	if (utf8_len <= 0) return TRUE;

	char *utf8_buffer = (char*)malloc(utf8_len);
	if (!utf8_buffer) ExitProcess(1);

	WideCharToMultiByte(CP_UTF8, 0, header, -1, utf8_buffer, utf8_len, NULL, NULL);

	DWORD bytes_written;
	BOOL write_result = WriteFile(hLogFile, utf8_buffer, utf8_len - 1, &bytes_written, NULL);
	free(utf8_buffer);

	return write_result;
}

// ======= 6. Helper Functions =======
// ----- System -----
void GetLocalComputerName(void) {
	DWORD size = sizeof(computer_name) / sizeof(wchar_t);
	GetComputerNameW(computer_name, &size);
}

void CleanupAndExit(void) {
	SaveSettings();
	is_running = 0;

	if (client_socket != INVALID_SOCKET) {
		shutdown(client_socket, SD_BOTH);
		closesocket(client_socket);
		client_socket = INVALID_SOCKET;
	}

	if (hReceiveThread != NULL) {
		CloseHandle(hReceiveThread);
		hReceiveThread = NULL;
	}

	CloseLog();

	WSACleanup();
	PostQuitMessage(0);
}

// ----- Settings -----
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

		always_on_top = false;
		flash_enabled = true;
		sound_enabled = true;

		CheckMenuItem(GetMenu(hWnd), IDM_ALWAYS_ON_TOP, MF_BYCOMMAND | MF_UNCHECKED);
		CheckMenuItem(GetMenu(hWnd), ID_FLASH_TOGGLE, MF_BYCOMMAND | MF_CHECKED);
		CheckMenuItem(GetMenu(hWnd), ID_SOUND_TOGGLE, MF_BYCOMMAND | MF_CHECKED);

		SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

		MessageBoxW(hWnd, L"Settings have been reset to default values.", L"QuickChat", MB_OK | MB_ICONINFORMATION);
	}
}

// ----- User Interface -----
void EnableVisualStyles(void) {
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icex);
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

void ShowError(const wchar_t* msg, DWORD err) {
	wchar_t buffer[512];
	swprintf(buffer, sizeof(buffer) / sizeof(wchar_t), L"%ls. Error: %lu", msg, err);
	MessageBoxW(NULL, buffer, L"QuickChat", MB_OK | MB_ICONERROR);
}

void DisableChatControls(BOOL disable) {
	SendMessageW(hEdit, EM_SETREADONLY, TRUE, 0);
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

void AddMessage(const wchar_t* msg) {
	if (!hMsgDisplay || !msg || !*msg) return;

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

// ----- Network -----
bool IsValidTargetIP(const wchar_t* ip_str) {
	int o1, o2, o3, o4;
	if (swscanf(ip_str, L"%d.%d.%d.%d", &o1, &o2, &o3, &o4) != 4) return false;

	if (o1 == 0) return false;
	if (o4 == 0) return false;
	if (o4 == 255) return false;
	if (o1 >= 224 && o1 <= 239) return false;
	if (o1 >= 240) return false;

	return true;
}

bool GetDefaultIP(wchar_t *ip_buffer, size_t size) {
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return false;

	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET) {
		WSACleanup();
		return false;
	}

	struct sockaddr_in remote = {0};
	remote.sin_family = AF_INET;
	remote.sin_port = htons(53);
	remote.sin_addr.s_addr = inet_addr("8.8.8.8");

	if (connect(s, (struct sockaddr*)&remote, sizeof(remote)) != 0) {
		closesocket(s);
		WSACleanup();
		return false;
	}

	struct sockaddr_in local;
	int len = sizeof(local);
	if (getsockname(s, (struct sockaddr*)&local, &len) != 0) {
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

void Disconnect(void) {
	wchar_t leave_msg[512];
	swprintf(leave_msg, sizeof(leave_msg) / sizeof(wchar_t), L"[DISCONNECT]: %ls left the chat.", computer_name);
	AddMessage(leave_msg);
	if (is_server) LogMessage(leave_msg);

	if (client_socket != INVALID_SOCKET && is_running) {
		char utf8_msg[256];
		WideCharToMultiByte(CP_UTF8, 0, leave_msg, -1, utf8_msg, sizeof(utf8_msg), NULL, NULL);
		int msg_len = strlen(utf8_msg);

		unsigned char encbuf[256];
		memcpy(encbuf, utf8_msg, msg_len);
		XorObf(encbuf, msg_len);
		send(client_socket, (char*)encbuf, msg_len, 0);
	}

	CleanupAndExit();
}

// ----- Logging -----
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
		error_counter++;
		DWORD err = GetLastError();

		wchar_t err_msg[512];
		swprintf(err_msg, sizeof(err_msg) / sizeof(wchar_t), L"[ERROR]: Failed to write to log. Error: %lu. Error count: %d.", err, error_counter);
		AddMessage(err_msg);

		if (error_counter >= 3) {
			AddMessage(L"[ERROR]: Logging was disabled for this session.");
			logging_enabled = false;
			CloseHandle(chat_log);
			chat_log = NULL;
		}
		return;
	}

	FlushFileBuffers(chat_log);
}

void CloseLog(void) {
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	wchar_t timestamp[64];
	wcsftime(timestamp, sizeof(timestamp) / sizeof(wchar_t), L"%H:%M:%S %d/%m/%Y", t);

	wchar_t wbuffer[512];
	swprintf(wbuffer, sizeof(wbuffer) / sizeof(wchar_t), L"=== Session ended at %ls ===\r\n", timestamp);

	int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, NULL, 0, NULL, NULL);
	if (utf8_len > 0) {
		char* utf8_buffer = (char*)malloc(utf8_len);
		if (utf8_buffer) {
			WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, utf8_buffer, utf8_len, NULL, NULL);
			DWORD bytes_written;
			WriteFile(chat_log, utf8_buffer, utf8_len - 1, &bytes_written, NULL);
			free(utf8_buffer);
		} else {
			ExitProcess(1);
		}
	}

	FlushFileBuffers(chat_log);
	CloseHandle(chat_log);
	chat_log = NULL;
}

// ----- File Operations -----
void SaveChatToFile(HWND hWnd) {
	wchar_t filename[MAX_PATH];
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	wcsftime(filename, MAX_PATH, L"Chat-%Y%m%d-%H%M%S.txt", tm_info);

	int len = GetWindowTextLengthW(hMsgDisplay);
	wchar_t *chat_text = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	if (!chat_text) ExitProcess(1);
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
					swprintf(save_msg, sizeof(save_msg) / sizeof(wchar_t), L"Chat saved to %ls", filename);
					MessageBoxW(hWnd, save_msg, L"QuickChat", MB_OK | MB_ICONINFORMATION);
				} else {
					wchar_t save_err[512];
					swprintf(save_err, sizeof(save_err) / sizeof(wchar_t), L"Failed to save chat history. Error: %lu.", GetLastError());
					MessageBoxW(hWnd, save_err, L"QuickChat", MB_OK | MB_ICONERROR);
				}

				free(utf8);
			} else {
				ExitProcess(1);
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

// ======= 7. Network Core =======
bool InitializeNetwork(bool server_mode, HINSTANCE hInstance, int nCmdShow) {
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		ShowError(L"WSAStartup failed.", WSAGetLastError());
		return false;
	}

	bool result = server_mode ? StartServer(hInstance, nCmdShow) : StartClient(hInstance, nCmdShow);

	if (!result) {
		WSACleanup();
		return false;
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

bool StartServer(HINSTANCE hInstance, int nCmdShow) {
	SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == INVALID_SOCKET) {
		ShowError(L"Failed to create socket.", WSAGetLastError());
		WSACleanup();
		return false;
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
		return false;
	}

	GetDefaultIP(server_ip, sizeof(server_ip) / sizeof(wchar_t));

	wchar_t bind_msg[512];
	const wchar_t* mode_str = xor_enabled ? L"QC with XOR" : L"QC";
	swprintf(bind_msg, sizeof(bind_msg) / sizeof(wchar_t), L"Host started: %ls on address %ls port %d.", mode_str, server_ip, active_port);
	LogMessage(bind_msg);

	if (listen(server_fd, 1) == SOCKET_ERROR) {
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
				send(temp_client, "QCERR: No data", 24, 0);
			}

			closesocket(temp_client);
			continue;
		}

		if (recv_len >= (int)sizeof(hs) - 1) {
			LogMessage(L"[SECURITY]: Handshake is too large. Connection closed.");

			if (!xor_enabled) {
				send(temp_client, "QCERR: Packet is too large", 24, 0);
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
		MultiByteToWideChar(CP_UTF8, 0, ip_utf8, -1, peer_ip, sizeof(peer_ip) / sizeof(wchar_t));
		MultiByteToWideChar(CP_UTF8, 0, name_ptr, -1, peer_name, sizeof(peer_name) / sizeof(wchar_t));
		break;
	}

	closesocket(server_fd);

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
	
	return true;
}

bool StartClient(HINSTANCE hInstance, int nCmdShow) {
	client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket == INVALID_SOCKET) {
		ShowError(L"Failed to create socket.", WSAGetLastError());
		WSACleanup();
		return false;
	}

	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

	int active_port = xor_enabled ? PORT_XOR : PORT_PLAIN;
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

	struct sockaddr_in server_info;
	int len = sizeof(server_info);
	getsockname(client_socket, (struct sockaddr*)&server_info, &len);
	wchar_t ip_w[16];
	DWORD ip_len = 16;
	WSAAddressToStringW((LPSOCKADDR)&server_info, sizeof(server_info), NULL, ip_w, &ip_len);
	wcscpy(peer_ip, ip_w);

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
		MessageBoxW(NULL, L"Invalid handshake from remote host.", L"QuickChat", MB_OK | MB_ICONERROR);
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
	
	return true;
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
			if (err_code == WSAETIMEDOUT) continue;

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

		ProcessIncomingMessage(buffer, bytes);
	}

	_endthread();
	return 0;
}

void XorObf(unsigned char *data, int len) {
	if (!xor_enabled) return;

	unsigned char k[KEY_LEN];
	memcpy(k, key, KEY_LEN);

	for (int i = 0; i < len; i++) data[i] ^= k[i % KEY_LEN];

	memset(k, 0, KEY_LEN);
}

void ProcessIncomingMessage(char* buffer, int bytes) {
	XorObf((unsigned char*)buffer, bytes);
	buffer[bytes] = '\0';

	FlashMessageWindow(hWndGlobal);

	if (strncmp(buffer, "[DISCONNECT]", 12) == 0) {
		DisableChatControls(TRUE);
		PlayNotifySound(SOUND_LEAVE);
		DragAcceptFiles(hWndGlobal, FALSE);
	} else {
		PlayNotifySound(SOUND_MSG);
	}

	wchar_t wide_buffer[BUFFER_SIZE];
	MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wide_buffer, BUFFER_SIZE);
	AddMessage(wide_buffer);
	if (is_server) LogMessage(wide_buffer);
}

void SendCurrentMessage(HWND hWnd) {
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

	MSG nextMsg;
	while (PeekMessage(&nextMsg, hWnd, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {
		if (nextMsg.message == WM_KEYDOWN && nextMsg.wParam == VK_RETURN) continue;
		DispatchMessage(&nextMsg);
	}
}

// ======= 8. User Interface =======
INT_PTR CALLBACK ModeSelectProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	(void) lParam;

	switch (msg) {
		case WM_INITDIALOG:
			CheckDlgButton(hwnd, 201, xor_enabled ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwnd, 202, logging_enabled ? BST_CHECKED : BST_UNCHECKED);
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == 101) {
				is_server = true;
				EndDialog(hwnd, IDOK);
				return TRUE;
			}
			if (LOWORD(wParam) == 102) {
				is_server = false;
				EndDialog(hwnd, IDOK);
				return TRUE;
			}
			if (LOWORD(wParam) == 103 || LOWORD(wParam) == IDCANCEL) {
				EndDialog(hwnd, IDCANCEL);
				return TRUE;
			}
			if (LOWORD(wParam) == 201)
				xor_enabled = IsDlgButtonChecked(hwnd, 201) == BST_CHECKED;
			if (LOWORD(wParam) == 202)
				logging_enabled = IsDlgButtonChecked(hwnd, 202) == BST_CHECKED;
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
					SetFocus(GetDlgItem(hwnd, 1001));
					return TRUE;
				}

				if (!IsValidTargetIP(p)) {
					MessageBoxW(hwnd, L"This IP address is valid, but cannot be used for connection.", L"QuickChat", MB_OK | MB_ICONWARNING);
					SetFocus(GetDlgItem(hwnd, 1001));
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
	DialogBoxParamW(hInstGlobal, MAKEINTRESOURCEW(5), NULL, HostInfoProc, 0);
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
	swprintf(title, sizeof(title) / sizeof(wchar_t), L"QuickChat - %ls", peer_name);

	HWND hWnd = CreateWindowW(L"QuickChatWndClass", title,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 600, 395,
		NULL, NULL, hInstance, NULL);

	if (!hWnd) {
		MessageBoxW(NULL, L"Failed to create main window.", L"QuickChat", MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}

	hWndGlobal = hWnd;
	CreateAppMenu(hWnd);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
}

INT_PTR CALLBACK HostInfoProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	(void)lParam;

	switch (msg) {
		case WM_INITDIALOG:
			SetDlgItemTextW(hwnd, 301, server_ip);
			SetDlgItemTextW(hwnd, 302, xor_enabled ? L"Yes" : L"No");
			return TRUE;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
				EndDialog(hwnd, LOWORD(wParam));
				return TRUE;
			}
			break;
		case WM_CLOSE:
			EndDialog(hwnd, IDCANCEL);
			return TRUE;
	}
	return FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			CreateAppFonts(hWnd);
			CreateAppControls(hWnd);
			mainWindowReady = TRUE;
			return 0;
		}

		case WM_COMMAND: {
			int id = LOWORD(wParam);
			if (LOWORD(wParam) == ID_SEND) {
				HandleSendCommand(hWnd);
			} else {
				HandleMenuCommand(hWnd, id);
			}
			return 0;
		}

		case WM_CLOSE: {
			HandleMenuCommand(hWnd, IDM_LEAVE);
			return 0;
		}

		case WM_DESTROY: {
			CleanupGdiResources();
			CleanupAndExit();
			return 0;
		}

		case WM_SIZE: {
			ResizeMainWindow(hWnd, LOWORD(lParam), HIWORD(lParam));
			return 0;
		}

		case WM_SETFOCUS: {
			SetFocus(hEdit);
			break;
		}

		case WM_DROPFILES: {
			HDROP hDrop = (HDROP)wParam;
			ProcessDroppedFile(hWnd, hDrop);
			DragFinish(hDrop);
			return 0;
		}
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
	AppendMenuW(hOpts, MF_STRING | MF_UNCHECKED, ID_CONFIRM_TOGGLE, L"Leave Confirmation");
	AppendMenuW(hOpts, MF_STRING | MF_UNCHECKED, ID_FLASH_TOGGLE, L"Window Flash");
	AppendMenuW(hOpts, MF_STRING | MF_UNCHECKED, ID_SOUND_TOGGLE, L"Sound");
	AppendMenuW(hOpts, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hOpts, MF_STRING, IDM_RESET_SETTINGS, L"Reset Settings");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hOpts, L"Options");

	HMENU hHelp = CreatePopupMenu();
	AppendMenuW(hHelp, MF_STRING, IDM_ABOUT, L"About");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"Help");

	SetMenu(hWnd, hMenu);

	CheckMenuItem(hMenu, IDM_ALWAYS_ON_TOP, MF_BYCOMMAND | (always_on_top ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hMenu, ID_CONFIRM_TOGGLE, MF_BYCOMMAND | (confirm_enabled ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hMenu, ID_SOUND_TOGGLE, MF_BYCOMMAND | (sound_enabled ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hMenu, ID_FLASH_TOGGLE, MF_BYCOMMAND | (flash_enabled ? MF_CHECKED : MF_UNCHECKED));
}

void CreateAppFonts(HWND hWnd) {
	HDC hdc = GetDC(hWnd);
	int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(hWnd, hdc);

	LOGFONTW lf = {0};
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfQuality = DEFAULT_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	wcscpy(lf.lfFaceName, L"Tahoma");

	lf.lfHeight = -MulDiv(9, dpi, 72);
	lf.lfWeight = FW_NORMAL;
	hFont = CreateFontIndirectW(&lf);

	lf.lfHeight = -MulDiv(8, dpi, 72);
	lf.lfWeight = FW_BOLD;
	hFontBold = CreateFontIndirectW(&lf);
}

void CreateAppControls(HWND hWnd) {
	hMsgDisplay = CreateWindowW(L"EDIT", L"", 
		WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | 
		ES_READONLY | ES_AUTOVSCROLL,
		0, 0, 594, 275, hWnd, (HMENU)ID_MSG_DISPLAY, NULL, NULL);

	hEdit = CreateWindowW(L"EDIT", L"", 
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL,
		0, 278, 510, 46, hWnd, (HMENU)ID_EDIT, NULL, NULL);

	hSendBtn = CreateWindowW(L"BUTTON", L"Send", 
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		510, 278, 85, 46, hWnd, (HMENU)ID_SEND, NULL, NULL);

	SetWindowPos(hWnd, always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SendMessageW(hMsgDisplay, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessageW(hSendBtn, WM_SETFONT, (WPARAM)hFontBold, TRUE);
	SendMessageW(hEdit, EM_SETLIMITTEXT, BUFFER_SIZE - 1, 0);

	oldEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);
	DragAcceptFiles(hWnd, TRUE);
}

void HandleSendCommand(HWND hWnd) {
	int textLen = GetWindowTextLengthW(hEdit);
	SendMessageW(hEdit, EM_SETSEL, textLen, textLen);
	SendCurrentMessage(hWnd);
	SetFocus(hEdit);
}

void HandleMenuCommand(HWND hWnd, int id) {
	switch (id) {
		case IDM_LEAVE:
			if (!is_running) CleanupAndExit();
			if (!confirm_enabled) Disconnect();

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
		
		case ID_SOUND_TOGGLE:
			sound_enabled = !sound_enabled;
			CheckMenuItem(GetMenu(hWnd), ID_SOUND_TOGGLE, MF_BYCOMMAND | (sound_enabled ? MF_CHECKED : MF_UNCHECKED));
			break;
		
		case ID_FLASH_TOGGLE:
			flash_enabled = !flash_enabled;
			CheckMenuItem(GetMenu(hWnd), ID_FLASH_TOGGLE, MF_BYCOMMAND | (flash_enabled ? MF_CHECKED : MF_UNCHECKED));
			break;
		
		case IDM_SAVE:
			SaveChatToFile(hWnd);
			break;
		
		case IDM_RESET_SETTINGS:
			ResetSettings(hWnd);
			break;
		
		case ID_CONFIRM_TOGGLE:
			confirm_enabled = !confirm_enabled;
			CheckMenuItem(GetMenu(hWnd), ID_CONFIRM_TOGGLE, MF_BYCOMMAND | (confirm_enabled ? MF_CHECKED : MF_UNCHECKED));
			break;
	}
}

void CleanupGdiResources(void) {
	if (oldEditProc) {
		SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)oldEditProc);
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

void ResizeMainWindow(HWND hWnd, int width, int height) {
	(void)hWnd;

	int edit_height = 46;
	int send_width = 85;

	SetWindowPos(hMsgDisplay, NULL, 0, 0, width, height - edit_height, SWP_NOZORDER);
	SetWindowPos(hEdit, NULL, 0, height - edit_height, width - send_width, edit_height, SWP_NOZORDER);
	SetWindowPos(hSendBtn, NULL, width - send_width, height - edit_height, send_width, edit_height, SWP_NOZORDER);
}

LRESULT CALLBACK EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_GETDLGCODE) {
		return DLGC_WANTALLKEYS | CallWindowProcW(oldEditProc, hWnd, uMsg, wParam, lParam);
	} else if (uMsg == WM_KEYDOWN) {
		if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
			SendMessageW(hWnd, EM_SETSEL, 0, -1);
			return 0;
		}

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

INT_PTR CALLBACK AboutDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	(void)lParam;
	
	switch (msg) {
		case WM_INITDIALOG: {
			wchar_t version[64];
			swprintf(version, sizeof(version) / sizeof(wchar_t), L"QuickChat (%s)", __DATE__);
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
			const wchar_t* name = (peer_name[0] != L'\0') ? peer_name : L"N/A";
			SetDlgItemTextW(hWnd, 101, name);

			const wchar_t* ip = is_server ? peer_ip : server_ip;
			SetDlgItemTextW(hWnd, 102, ip);
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

// ======= 9. Drag-and-Drop Functions =======
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

bool IsValidTextExtension(const wchar_t *path) {
	const wchar_t *ext = wcsrchr(path, L'.');
	if (!ext) return false;

	wchar_t ext_lower[16];
	wcsncpy(ext_lower, ext, 15);
	ext_lower[15] = L'\0';
	for (int i = 0; ext_lower[i]; i++) ext_lower[i] = towlower(ext_lower[i]);

	if (wcscmp(ext_lower, L".txt") == 0 ||
		wcscmp(ext_lower, L".log") == 0 ||
		wcscmp(ext_lower, L".md") == 0) return true;

	if (wcscmp(ext_lower, L".c") == 0 ||
		wcscmp(ext_lower, L".cpp") == 0 ||
		wcscmp(ext_lower, L".h") == 0 ||
		wcscmp(ext_lower, L".hpp") == 0) return true;

	if (wcscmp(ext_lower, L".py") == 0 ||
		wcscmp(ext_lower, L".js") == 0 ||
		wcscmp(ext_lower, L".sh") == 0 ||
		wcscmp(ext_lower, L".bat") == 0 ||
		wcscmp(ext_lower, L".cmd") == 0 ||
		wcscmp(ext_lower, L".ps1") == 0) return true;

	if (wcscmp(ext_lower, L".json") == 0 ||
		wcscmp(ext_lower, L".xml") == 0 ||
		wcscmp(ext_lower, L".yaml") == 0 ||
		wcscmp(ext_lower, L".yml") == 0 ||
		wcscmp(ext_lower, L".toml") == 0 ||
		wcscmp(ext_lower, L".ini") == 0 ||
		wcscmp(ext_lower, L".cfg") == 0 ||
		wcscmp(ext_lower, L".conf") == 0) return true;

	if (wcscmp(ext_lower, L".css") == 0 ||
		wcscmp(ext_lower, L".html") == 0 ||
		wcscmp(ext_lower, L".htm") == 0) return true;

	return false;
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
		MessageBoxW(hWnd, L"This file is empty.", L"QuickChat", MB_OK | MB_ICONWARNING);
		return NULL;
	}

	char *ansi = (char*)malloc(size + 1);
	if (!ansi) {
		ExitProcess(1);
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

	int wide_len = MultiByteToWideChar(CP_UTF8, 0, ansi + bom_offset, -1, NULL, 0);
	if (wide_len <= 0) {
		free(ansi);
		MessageBoxW(hWnd, L"This file is not valid UTF-8 text.", L"QuickChat", MB_OK | MB_ICONWARNING);
		return NULL;
	}

	wchar_t *wide = (wchar_t*)malloc(wide_len * sizeof(wchar_t));
	if (!wide) {
		ExitProcess(1);
		return NULL;
	}

	MultiByteToWideChar(CP_UTF8, 0, ansi + bom_offset, -1, wide, wide_len);
	free(ansi);

	return wide;
}

void InsertTextIntoEdit(const wchar_t *text) {
	SetWindowTextW(hEdit, text);

	int len = GetWindowTextLengthW(hEdit);
	SendMessageW(hEdit, EM_SETSEL, len, len);
	SetFocus(hEdit);
}