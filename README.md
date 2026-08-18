# <img width="48" height="48" alt="icon-48x48x32" src="https://github.com/user-attachments/assets/5d840285-ea25-4967-b2ad-bc79b684905f" /> QuickChat
QuickChat is a minimalistic LAN messenger, written in pure C with the Win32 API.
Does not require installation, leaves no traces, and works on any version starting from Windows 2000 up to 11.

<img width="586" height="388" alt="image" src="https://github.com/user-attachments/assets/5fae96ec-dd38-4159-b84c-e47dc066941e" />

Based on [MicroChat Framework](https://github.com/WinXP655/microchat).

## Project Status
Active development is now focuses on maintenance, bug fixes and security patches.\
No major features are planned.

## Features
1. **Portable** - just 1 .exe file
2. **Unicode Support** - any languages, any symbols.
3. **QC/QCS Protocol**
  - QC (QuickChat) - plaintext.
  - QCS (QuickChat Obfuscated) - XOR obfuscation.
4. **Logging** - save chat history and events. Only for Server, disabled by default.
5. **Tiny Size** - just 142 KB.
6. **Drag-and-Drop** - drag-and-drop any compatible text file and it will extract text instantly.

## Requirements
- **OS** - Windows 2000 and newer.

## How to use

### 1. Host
1. Run QuickChat.
2. Click Yes.
3. Select protocol.
4. Select if you want to enable logs.
5. Share displayed IP.
   Note that user should be in same network.

### 2. Client
1. Run QuickChat.
2. Click No.
3. Type the Host IP and click Connect.
4. Select protocol.

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
It uses a very simple yet working custom protocol QuickChat/QuickChat Onfuscated (QC/QCS):
1. Who starts first: Client. Host never sends anything until it will be QC handshake.
2. Client sends handshake in following format: QC:PCNAME\0.
   Replace PCNAME with your computer name or what you want remote side to see.\
   "\0" is required - official backend written on C, meaning you have to follow C rules.\
   If you are using QCS, you need to XOR everything before.\
   First 3 bytes should be exactly "QC:" or XORed version of it. Host reject if it is non-QC or at least 1 byte is wrong.\
3. Host send its name in same format.
4. Chat starts.

It also supports custom ping:
1. Send "QCPING" or its XORed version to remote side.
2. If you receive "QCPONG" or its XORed version, then remote side is active
3. Note that both sides should know both commands.
*Host and client side can delete logs/history at any time, it is stored only locally*.

## Changelog
Read CHANGELOG.md.

## Building from source

### Requirements
- **MinGW-w64** compiler
- **Windows** (7/10/11 recommended for build tools)

### Included tools
- 7z.exe - packaging.
- keygen.py - XOR key generator.
- build.bat - build script.
- resource.rc - resources.
- quickchat.manifest - Common Controls v6 manifest. Mostly for visual styles.

### Steps
1. Clone or download a repository
2. Make sure `gcc`, `windres`, and `python` are available in PATH.
3. Open a command prompt in the project folder.
4. Run depending on what you need:\
   `build.bat` - Compile QuickChat without changing key and packing.\
   `build.bat /rekey` - Compile QuickChat and regenerate key without packing.\
   `build.bat /pack` - Compile QuickChat and pack without regenerating key.\
   `build.bat /rekey /pack` - Compile QuickChat, regenerate key and pack.\
   `build.bat /minbuild` - Compile QuickChat with most minimal configuration.\
   `build.bat /clean` - Delete existing compiled files.

## Quirks
- Adding error code to log write fail breaks themeing.\
Adding showing error code in logging start failure breaks Common Controls v6 (disabling themeing).\
Fixed: Yes.

- Client and Server show same IP address, when no route but computers connected anyways.\
Sometimes IP address displayed incorrectly when routing table is wrong.\
Fix: Reset routing table completely.

- Tab inserted as a character instead switching controls.\
This is a known limitation of multi-line EDIT control, no known fix exist except subclassing, but it will be handled only for specific control. Default behavior since Windows 3.x.\
Fix: Not available.

## How to activate high-DPI fonts
By default, manifest provide only Common Controls v6.
DPI-aware manifest wasn't added because Windows XP compatibility breaks with SxS error.

Steps to activate high DPI (Windows 10/11):
1. Open `quickchat.exe` properties.
2. Select "Compatiblity" tab.
3. Click "Change high DPI settings".
4. Check "Override high DPI scaling behavior" and select "Application" from list.
5. Click OK and then Apply.

Steps to activate high DPI (Windows 7/8):
1. Open `quickchat.exe` properties.
2. Select "Compatiblity" tab.
3. Check "Disable display scaling on high DPI settings".
4. Click Apply.

Note that only fonts are scaled - controls are fixed in size

<img width="584" height="387" alt="image" src="https://github.com/user-attachments/assets/2acadc53-e092-416e-98aa-5a82ce0661b4" />\
*QuickChat in 125% scaling on Windows 10*

## Credits
This project uses [7-Zip](https://www.7-zip.org/) for archiving.  
`7z.exe` is included for convenience and is used under the terms of the GNU LGPL license.

---

Enjoy! If you have questions, write me to my Discord - @pcsettings
