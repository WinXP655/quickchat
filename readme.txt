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
3. QC (QuickChat) protocol with optional XOR.
4. Logging - save chat history and events. Only for Server, disabled by default.
5. Tiny Size - just 142 KB.
6. Extracting text from text files by just drag-and-drop.

- Requirements
OS - Windows 2000 and newer.

- How to use

Host:
  1. Run QuickChat.
  2. In the startup dialog, click Host.
  3. Configure options: check XOR Obfuscation and/or Enable Logging if needed.
  4. Share the displayed IP address with peers (must be on the same network).

Client:
  1. Run QuickChat.
  2. In the startup dialog, click Join.
  3. Enter the Host IP address in the connection window and click Connect.
  4. Verify protocol settings (XOR must match the host's configuration).

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

- Changelog
Read CHANGELOG.md in the official repository.

---

Enjoy! If you have questions, write me to my Discord - @pcsettings