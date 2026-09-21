# <img width="48" height="48" alt="icon-48x48x32" src="https://github.com/user-attachments/assets/5d840285-ea25-4967-b2ad-bc79b684905f" /> QuickChat
QuickChat is a minimalistic LAN messenger, written in pure C with the Win32 API.
Does not require installation, leaves no traces, and works on any version starting from Windows 2000 up to 11.

<img width="586" height="388" alt="image" src="https://github.com/user-attachments/assets/ce0089cd-bc3b-4a9b-8f83-8d4a2015994b" />

*Based on [MicroChat Framework](https://github.com/WinXP655/microchat).*

## Project Status
Active development is now focuses on maintenance, bug fixes and security patches.\
No major features are planned.

## Features
1. **Portable** - just 1 .exe file
2. **Unicode Support** - any languages, any symbols.
3. **QC (QuickChat)** protocol with optional XOR.
4. **Logging** - save chat history and events. Only for Server, disabled by default.
5. **Tiny Size** - just 150 KB.
6. **Drag-and-Drop** - drag-and-drop any compatible text file and it will extract text instantly.

## Requirements
- **OS** - Windows Vista and newer.

## How to use

### 1. Host
1. Run QuickChat.
2. In the startup dialog, click Host.
3. Configure options: check XOR Obfuscation and/or Enable Logging if needed.
4. Share the displayed IP address with peers (must be on the same network).

### 2. Client
1. Run QuickChat.
2. In the startup dialog, click Join.
3. Enter the Host IP address in the connection window and click Connect.
4. Verify protocol settings (XOR must match the host's configuration).

## Logging
Full chat log stored only on host side and do not contain sensitive information. It only logs:
1. Session start and end.
2. User messages with timestamps.
3. System messages and events.
4. Where server set port binding.
5. Who connected (computer name and LAN IP).

For client, added option to save just chat history:
1. Connection
2. Save Chat

## Protocol
It uses a very simple custom protocol. The base protocol is QuickChat Plaintext (QC).\
1. Who starts first: Client. Host never sends anything until it will be QC handshake.
2. Client sends handshake in following format: QC:PCNAME\0.
   Replace PCNAME with your computer name or what you want remote side to see.\
   "\0" is required - official backend written on C, meaning you have to follow C rules.\
   If you are using XOR, you need to XOR everything before.\
   First 3 bytes should be exactly "QC:" or XORed version of it. Host reject if it is non-QC or at least 1 byte is wrong.
3. Host send its name in same format.
4. Chat starts.

> Host and client side can delete logs/history at any time, it is stored only locally.

## Use сases
- **Quick 1-to-1 chat in a local network**: No server, no accounts, no setup.\
- **Private chat**: Optional XOR layer hides traffic from casual inspectors.\
- **Portable chat**: Single .exe file, no registry, no install, no traces.\
- **One-time sessions with no traces**: Logs off by default. Close the app and nothing remains.

## Changelog
Read CHANGELOG.md.

## Building from source

### Requirements
- **MinGW-w64** compiler
- **Python** any version
- **Windows** (7/10/11 recommended for build tools)

### Included tools
- 7z.exe - packaging.
- keygen.py - XOR key generator.
- keygen_fixed.py - XOR key generator, but with your own key.
- build.bat - build script.

### Steps
1. Clone or download a repository
2. Make sure `gcc`, `windres`, and `python` are available in PATH.
3. Open a command prompt in the project folder.
4. If you want to define your own key before, run:\
   `python keygen_fixed.py <your own key>`\
   If you want to make your build compatible with official host, copy QCS key string from release.
6. Run depending on what you need:\
   `build.bat` - Compile QuickChat without changing key and packing.\
   `build.bat /rekey` - Compile QuickChat and regenerate key without packing.\
   `build.bat /pack` - Compile QuickChat and pack without regenerating key.\
   `build.bat /rekey /pack` - Compile QuickChat, regenerate key and pack.\
   `build.bat /minbuild` - Compile QuickChat with most minimal configuration.\
   `build.bat /clean` - Delete existing compiled files.\
   `build.bat /clean /nocleankey` - Delete existing compiled files, but do not delete key file.

## Credits
This project uses [7-Zip](https://www.7-zip.org/) for archiving.  
`7z.exe` is included for convenience and is used under the terms of the GNU LGPL license.

---

Enjoy! If you have questions, write me to my Discord - @pcsettings
