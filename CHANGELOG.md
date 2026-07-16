## QuickChat Modern Family

**QuickChat (Current, 16 July 2026)**:
- QuickChat became open-source.
- XOR is generated seprately.
- Better build system.
- Added 7z.exe to build package.

**QuickChat (12 July 2026)**:

**Version Note**: Windows 9x (98, ME) support is discontinued since this version. If you want to continue using QuickChat, please upgrade your Windows version.

- First public release.
- Final code refactor.
- Renamed variables to a single style.
- Removed Windows 9x compatibility code.
- Simplified XOR - single array instead 4, wiping using memset.
- Replaced all ANSI functions with Unicode. - done
- Removed legacy gethostbyname fallback.
- isRunning now bool instead int.
- Removed dublicating code.
- Removed unused arg hWndGlobal.
- Renamed chatlog.log to chatlog.txt.
- Buffer is 8192 characters because of Unicode.

*Everything goes below was stage when project was private*.

**QuickChat (30 June 2026)**:
- General code readability and structure refactor (naming, function order, warning icons).
- Error handling: added length checks for send/display, active connection check before close.
- Merged dublicating sound functions into PlayNotifySound(sound_type).
- Fixed: Computer Info IP (role‑based), menu toggle states, message logging order.
- Cleaned globals: removed dead g_hInstance, explicit NULL init for UI handles.
- Threading: replaced _beginthread with _beginthreadex; ReceiveMessages now __stdcall, proper thread termination in CleanupAndExit.
- Updated XOR key.
- Buffer limit now uses BUFFER_SIZE - 1.
Note: This is last version for Windows 9x

**QuickChat (27 June 2026)**:
- Removed log write error code (breaks Common Controls v6).

**QuickChat (25 June 2026)**:
- Reverted to 17 June 2026 code base.
- Added check for non-reachable IP addresses.
- Restored error code displaying when failed to start logging.
- Added WM_SETFOCUS to set focus on Message Edit regardless of previously selected control.
- Common Controls v6 broken again (I hate this thing).

**QuickChat (22 June 2026)**:
- Added error code displaying when failed to start logging.
- Added new key combinations: Ctrl+W - Terminate Connection, Ctrl+L - Clear Chat.
- Tab key now switch control instead adds Tab as character.
- Added key combinations to menu items text.

**QuickChat (17 June 2026)**:
- Fixed Common Controls v6 specifically for Windows XP (added comctl32.dll dependency, used InitCommonControlsEx).
- Fixed Windows 9x compatibility (dummy thread ID, platform ID chack).
- Small client dialog refactor.
- Made Send button font smaller.

**QuickChat (15 June 2026)**:
- Replaced explicit A (GetComputerNameA, MessageBoxA) variations to generic (GetComputerName, MessageBox).
- Removed unused pragma comments (compiler ignores them because of -Wunknown-pragmas).
- Replaced some %d with %ld.
- Cast unused parameters into void (lpCmdLine, hWnd in some cases, lpParam, hPrevInstance).
- Replaced some %lu with %u.
- Version Info metadata converted from UTF-8 to ANSI.
- Remove unused "void* arg" from ReceiveMessages.

**QuickChat (29 May 2026)**:
- Removed text output for Sound, Window Flashing, Always On Top, Ping (except response from remote side).
- Remove WSAEWOULDBLOCK and WSAESHUTDOWN from errors list.
- Remove "Remote computer not connected" logging from Ping Remote.

**QuickChat (25 May 2026)**:
- Fixed typo MicroChat -> QuickChat in receive thread error handling.

**QuickChat (14 May 2026)**:
- Removed unused global flag.
- Added special error text for "Port in use" error.
- Removed unused condition "if (!g_manual_disconnect) ...".

**QuickChat (2 May 2026)**:
- Removed that annoying dot in end of error texts.
- Added showing user message before send.

**QuickChat (27 April 2026)**:
- Changed getsockname() after connect() comment.

**QuickChat (21 April 2026)**:
- Turned InitializeNetwork() from void to bool.
- Added error handling messages in GetDefaultIP().
- Added error handling when main window failed to create.
- Added error handling when subclassing failed.
- ExitProcess() replaced with returning false.
- Added error handling when message receive thread failed to start.
- Added error handling when InitializeNetwork() returned false.

**QuickChat (25 May 2026)**:
- Fixed typo MicroChat -> QuickChat in receive thread error handling.

**QuickChat (24 March 2026 19:44)**:
- Added skipping leave confirmation when is_running is 0.
- Added send error code to readable text.

**QuickChat (24 March 2026 18:03)**:
- Restored getsockname after connect (known problem of Weak Host Model on pre-Vista systems).
- Remove unused "if (g_xor_enabled) ..." from ReceiveMessages.

**QuickChat (24 March 2026 15:32)**:
- Removed old 49152 port mentioning.
- Removed unused width and height from globals.
- Removed unused hStatusBar.
- Removed unused margin value from resizing logic.
- Removed dead code.
- Removied getsockname after connect.
- Added 5 second timeout on client side recv.

**QuickChat (18 February 2026)**:
- Added handler when failed to load a connection dialog.
- General compacting code (compressing several lines of same function into single without losing readability where possible).
- Receiving thread now shows exact error instead generic connection lose.
- Server now logs binding.

**QuickChat (8 February 2026)**:
- Added custom handler for Ctrl+A.
- Updated "copyright" from 2025 to 2026.
- Now Send also set focus back to message edit.
- Ping changed to QCPING/QCPONG.
- P2PMSGR easter egg removed.
- Server now logs all pings.

**QuickChat (7 February 2026)**:
- Added custom ping command to QC/QCS protocol: PING/PONG.
- Added cleanup after exit to prevent GDI leak.
- Title bar now have icon.

**QuickChat (31 January 2026)**:
- Source code have funny comment:

// Rust devs, if you are reading this,

// try not to have a panic attack rewriting this :D

- Always On Top, Sound and Window Flashing converted into checkboxes.
- Removed all comments aka code cleanup.

**QuickChat (30 January 2026 13:26)**:
- Clear Chat is back.
- Added Always-On-Top.
- Menu items are ungrouped to 4 menus - Connection, View, Options, Help.
- Removed "You're not connected to server" in Computer Info (it now indicates in a field "Connected: Yes/No").

**QuickChat (30 January 2026 12:42)**:
- Added Computer Info feature.

**QuickChat (29 January 2026)**:
- Added pragma comments for ws2_32, winmm and gdi32.
- Added window flash toggle.
- Added P2P Messenger related easter egg when computer name is set to P2PMSGR.
- Window flashing is back.

**QuickChat (23 January 2026)**:
- Added sound toggle.
- Added separator between sound toggle, leave chat and about.

**QuickChat (21 January 2026)**:
- QC and QCS finally got its protocol marker - QC:PCNAME\0.
- XOR encryption became a bit more complex: 4 arrays x 8 bytes each in mixed order (but didn't helped much lol).
- Timeout increased from 1 second to 5 seconds.
- Blacklist replaced with Whitelist.

**QuickChat (20 January 2026)**:
- sprintf replaced with snprintf.
- strcpy replaced with strncpy.
- Added check for recv length.
- Messages longer than 4095 no longer displayed.
- Dynamic buffer for computer name replaced with static 256 bytes buffer.
- Almost all buffers became 256 bytes.
- Mostly security update.

**QuickChat (19 January 2026)**:
- Limited message edit to 4095 characters to fit buffer size.
- Leave Session renamed to Leave Chat.
- Make code style more unified.

**QuickChat (7 January 2026)**:
- Moved protocol ports. QCS: 65501, QC: 65502.
- Server IP dialog now DWORD WINAPI instead VOID.
- CleanupAndExit now do not supports Restart option.
- Dublicating code of graceful leave now moved to separate function.
- Terminate Connection renamed to Close Connection.
- Added a huge black list for filters (SMTP, Telnet, SSH and etc).
- Finally fixed logging back to milliseconds.

**QuickChat (6 January 2026 22:43)**:
- Now XOR encrypted and Plain text are in separate ports. QCS (QuickChat Secure): 49152, QC (QuickChat): 49153.
- Now QC/QCS protocol shown in title bar.

## QuickChat Legacy Family
**QuickChat (6 January 2026 21:10)**:
- XOR crypt became optional (enabled by default).
- Connect confirmation replaced with QC/QCS protocol select (ports wasn't separated at this moment).

**QuickChat (6 January 2026 16:27)**:
- Accidentally reverted back to seconds precision.
- Removed buffering (broke XOR).

**QuickChat (29 December 2026 13:40)**:
- Logging now happens with milliseconds precision.

**QuickChat (29 December 2025 12:31)**:
- Added message buffering.

**QuickChat (26 December 2025)**:
- Added confirmation message box before client connection.
- End Session renamed to Leave Session.
- Added XOR key wiping.
- Added filtering for "PUT /" and ClientHello.

**QuickChat (17 December 2025)**:
- Added Terminate Connection option. Enabled only for server.
- Added chat controls disable function.
- Logging became optional and disabled by default.

**QuickChat (16 December 2025)**:
- Message buffer extended to 4096 chars instead 1024.
- XOR key moved to code stack.
- Added leave message encryption.
- Server side calls ExitProcess when network error happens.
- "Connection with server or client" replaced with neutral "Connection with remote computer lost".
- Now XOR also encrypt leave message.
- Added ExitProcess when critical error occurs (bind, listen, connect error and etc).

**QuickChat (18 November 2025)**:
- Added XOR crypt with 16 byte key: (quite funny was that key was put into global storage).
  XOR is mandatory.
  XOR was added only for incoming and outcomong connections, but leave message turned into garbage (forgot to add XOR to leave message, fixed later).

**QuickChat (8 November 2025 12:42)**:
- Removed more dead code (Save and Clear Chat leftovers).
- Combined some multiline functions into a singleline.

**QuickChat (3 November 2025 15:27)**:
- WinChat renamed to QuickChat (now it will be permanent name).
- Added window resizing.
- Send Message now again Enter.

**WinChat (3 November 2025 15:09)**:
- Removed single instance mutex leftovers.
- Removed SYSTEM user. Now system messages sent with tags depending on contents.
- Fonts are Tahoma and Lucida Console again.
- Made main window slightly smaller by height.
- Removed system sound.

**WinChat (3 November 14:29)**:
- Removed users list and related functionality.
- Removed last comments.
- Removed single instance mutex.
- Menu reduced to just End Session and About.
- Restart removed completely.
- Removed private IP validation.
- Send Message changed to Shift+Enter.
- WinP2P System renamed back to SYSTEM.
- ShowErrorMessage completely deleted, its code integrated directly.
- Removed window flashing.
- Added discarding empty connections.
- All menu items grouped into a single drop down menu Menu.
- GetDefaultIP simplified:
  getsockname -> inet_ntoa replaced with just getsockname.
- Port changed to 49152.
BUG: Common Controls v6 manifest crashes any future versions. Affected: only Windows XP.


**WinP2P (15 September 2025)**:
- Removed unused pragma comments for ws2_32 and winmm.
- Removed unused "char msg[256]".
- Removed 0x00-0x1F range filtering.

Extra info
1. Recovered from archives.
2. Uses Windows 7 sounds and Chat2 in connection dialog title.
3. Do not have app icon (do not exists in saved assets).


**WinP2P (9 September 2025)**:
- Removed pragma comment for user32 and advapi32.
- Title now shows remote computer name instead this computer.
- Custom colors code completely deleted.

**WinP2P (3 September 2025)**:
- Added mutex for single instance.
- Added error if failed to send leave message.
- SYSTEM renamed to WinP2P System.
- Added window synchronization to allow window be fully initialized.
- Added error handler when send failed.
- Added logging error when failed to add message to control.
- Removed annoying message boxes before client and server start.

**WinP2P (14 August 2025 18:52)**:
- Now GUI called from main thread.
- GetDefaultIP now runs WSACleanup before returning false.
- Replaced inet_ntoa instead inet_ntop for pre-Vista compatibility.
- Added message boxes before staring server and client (quite annoying).
- GetDefaultIP now used.
- Better trim method - now trims tab, caret, newline, space.
- Using SetWindowTextA instead SetWindowText.

**WinP2P (14 August 2025 17:50)**:
- Added GetDefaultIP function (UDP hack for getting local IP but currently unused).

**WinP2P (14 August 2025 16:05)**:
- Deleted dead code.
- Deleted tray notification support (was buggy).
- APIPA (169.254.x.x) no longer allowed.
- Deleted unsafe method for ending dialog (dialog will stay after connection).

**WinP2P (14 August 2025 14:34)**:
- Added tray notification support prototype.
- Removed forced custom colors.

**WinP2P (13 April 2025)**:
- Added ending server IP dialog using unsafe method.
- Removed last comments.

**WinP2P (9 August 2025)**:
- Changed port from 8888 to 50000
- Removed Edit menu and Help Contents leftovers.
- Added ShowErrorMessage function. First implementation of at least basic error handling.
- Added IP address checking (restricted to only private IP).
- Added deleting user from users list on leave.
- Added filtering control characters.
- Fixed inconsistent user and system message styling.
- Finally switched to WinSock 2.0.

**WinP2P (1 July 2025)**:
- Removed almost every comment.
- Replaced fonts with MS Shell Dlg.
- Removed Help Contents menu (but leftover code still present).
- Removed system.wav and error.wav sounds.

**WinP2P (11 June 2025)**:
- Added Edit and Help Contents menu.
- Initial dialog text shortened.
- Removed incoming/outcoming connection warning dialog.
- Removed more comments.
- Added HTTP filtering.
- Removed installer package (exists only as portable).
- Icon reverted to Windows XP style.

Older history here: https://github.com/WinXP655/p2pmsgr
