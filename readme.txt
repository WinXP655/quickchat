# QuickChat - Minimalistic LAN Messenger

QuickChat is a minimalistic LAN messenger, written in pure C with the Win32 API.
Does not require installation, leaves no traces, and works on any version starting
from Windows 2000 up to 11.

Based on MicroChat Framework (https://github.com/WinXP655/microchat).

- Project Status
Active development is now focuses on maintenance, bug fixes and security patches.
No major features are planned.


- Features
1. Portable - just 1 .exe file
2. Unicode Support - any languages, any symbols.
3. QC/QCS Protocol
   - QC (QuickChat) - plaintext.
   - QCS (QuickChat Obfuscated) - XOR obfuscation.
4. Logging - save chat history and events. Only for Server, disabled by default.
5. Tiny Size - just 142 KB.
6. Extracting text from text files by just drag-and-drop.

- Requirements
OS - Windows 2000 and newer.

- How to use

Host:
  1. Run QuickChat.
  2. Click Yes.
  3. Select protocol.
  4. Select if you want to enable logs.
  5. Share displayed IP.
     Note that user should be in same network.

Client:
  1. Run QuickChat.
  2. Click No.
  3. Type the Hosy IP and click Connect.
  4. Select protocol.


- Logging
Full chat log stored only on host side and do not contain sensitive information.
It only logs:
  1. Session start and end.
  2. User messages with timestamps.
  3. System messages and events.
  4. Where server set port binding.
  5. Who connected (computer name and LAN IP).

For client, added option to save just chat history:
 - Connection
 - Save Chat


Protocol

It uses a very simple yet working custom protocol QuickChat/QuickChat Secure (QC/QCS):

  1. Who starts first: Client. Host never sends anything until it will be QC handshake.
  2. Client sends handshake in following format: QC:PCNAME\0.
     Replace PCNAME with your computer name or what you want remote side to see.
     "\0" is required - official backend written on C, meaning you have to follow C rules.
     If you are using QCS, you need to XOR everything before.
     First 3 bytes should be exactly "QC:" or XORed version of it. Host reject if
     it is non-QC or at least 1 byte is wrong.
  3. Host send its name in same format.
  4. Chat starts.

It also supports custom ping:
  1. Send "QCPING" or its XORed version to remote side.
  2. If you receive "QCPONG" or its XORed version, then remote side is active.
  3. Note that both sides should know both commands.

Host/client side can delete logs/history at any time, it is stored only locally.


- Changelog
Read CHANGELOG.md in the official repository.

- Quirks
- Adding error code to log write fail breaks theming.
  Adding showing error code in logging start failure breaks Common Controls v6
  (disabling theming).
  Fixed: Yes

- Client and Server show same IP address, when no route but computers connected anyways.
  Sometimes IP address displayed incorrectly when routing table is wrong.
  Fix: Reset routing table completely.

- Tab inserted as a character instead switching controls.
  This is a known limitation of multi-line EDIT control, no known fix exist except
  subclassing, but it will be handled only for specific control. Default behavior
  since Windows 3.x.
  Fix: Not available.


- How to activate high-DPI fonts
By default, manifest provide only Common Controls v6.
DPI-aware manifest wasn't added because Windows XP compatibility breaks with SxS error.

Steps to activate high DPI (Windows 10/11):
  1. Open quickchat.exe properties.
  2. Select "Compatibility" tab.
  3. Click "Change high DPI settings".
  4. Check "Override high DPI scaling behavior" and select "Application" from list.
  5. Click OK and then Apply.

Steps to activate high DPI (Windows 7/8):
  1. Open quickchat.exe properties.
  2. Select "Compatibility" tab.
  3. Check "Disable display scaling on high DPI settings".
  4. Click Apply.

Note that only fonts are scaled - controls are fixed in size.

---

Enjoy! If you have questions, write me to my Discord - @pcsettings

---

Error codes for extra info.

Winsock errors:
10060|Connection timed out (Server unreachable).
10061|Connection refused (Server not running or blocked by firewall).
10054|Connection reset by peer.
10053|Connection aborted.
10065|Host unreachable.
10051|Network unreachable.
10048|Port already in use.
10057|Socket is not connected.
10058|Cannot send after socket shutdown.
10035|Connection busy (Try again later).

System errors (Windows):
2|File not found.
3|Path not found.
4|Too many open files.
5|Access denied.
6|Invalid handle.
8|Not enough memory.
32|Sharing violation (file locked by another process).
33|Process lock violation.
87|Invalid parameter.
130|Sharing buffer exceeded.
183|Cannot create file (already exists).
206|Filename too long.
267|Directory name invalid.
123|Invalid name in path.
1392|Disk or file system corrupted.