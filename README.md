# <img width="48" height="48" alt="icon-48x48x32" src="https://github.com/user-attachments/assets/9e3ee00a-aaf8-4c38-b40f-438c3bb999c9" /> QuickChat
QuickChat is a minimalistic LAN messenger, written in pure C with the Win32 API.
Does not require installation, leaves no traces, and works on any version starting from Windows Vista up to 11.

<img width="586" height="388" alt="image" src="https://github.com/user-attachments/assets/ce0089cd-bc3b-4a9b-8f83-8d4a2015994b" />

*Based on [MicroChat Framework](https://github.com/WinXP655/microchat).*

## Project Status
Active development is now focuses on maintenance, bug fixes and security patches.\
No major features are planned.

## Features
1. **Portable** - just 1 .exe file
2. **Unicode Support** - any languages, any symbols.
3. **QC (QuickChat)** protocol with optional XOR.
4. **Logging** - save chat history and events. Only for host, disabled by default.
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
Logging is disabled by default. 
- **Host:** Can save the full session history (messages, timestamps, connection IPs) to a local file.
- **Client:** Can dump the current chat history to a file at any time.

Nothing is sent to the cloud. Everything is stored locally.

## Protocol
QuickChat uses a custom TCP-based protocol. The base protocol is **QC (QuickChat Plaintext)**. 

### Handshake Flow
1. **Client initiates:** The host stays silent until it receives a valid handshake.
2. **Client sends:** `QC:PCNAME\0`
   - `PCNAME` is the sender's computer name (or any custom name).
   - `\0` is the null-terminator, required for C-string compatibility.
3. **Host responds:** `QC:HOSTNAME\0` (in the same format).
4. **Chat starts:** Both sides can now exchange text messages.

### XOR Obfuscation (Optional)
If XOR is enabled, **all** transmitted data (including the handshake) is XORed before sending and decoded upon receipt. 
- The first 3 bytes must always be `QC:` (or its XORed equivalent). The host drops any connection that fails this check.
- **Error messages:** If a handshake fails (e.g., malformed packet or invalid prefix), the host closes the connection. Error responses (`QCERR: ...`) are sent in plain text **only when XOR is disabled**. If XOR is enabled, the server drops the connection silently to prevent sending XORed garbage that the client cannot parse.

> **Note:** All logs and history are stored locally. Neither side uploads or transmits chat history anywhere.

## Use сases
- **Quick 1-to-1 chat in a local network**: No server, no accounts, no setup.
- **Private chat**: Optional XOR layer hides traffic from casual inspectors.
- **Portable chat**: Single .exe file, no registry, no install, no traces.
- **One-time sessions with no traces**: Logs off by default. Close the app and nothing remains.

## Changelog
Read CHANGELOG.md.

---

## Building from source

### Requirements
- **MinGW-w64** compiler
- **Python** any version
- **Windows** (7/10/11 recommended for build tools)

### Included tools
- `7z.exe` - packaging.
- `keygen.py` - XOR key generator.
- `keygen_fixed.py` - XOR key generator, but with your own key.
- `build.bat` - build script.

### Build types
Build script provide 2 types of compiled binary:

- **Release**:
  - **Purpose**: Ready for distribution application.
  - **Includes**: All user interface dialogs, manifest, resources, icon, full optimization.
  - **Size**: 150 KB/
  - **Usage**: `build.bat` without args or `/pack` to create ZIP distribution.
- **MinBuild**:
  - **Purpose**: Quick and dirty compilation for fast test of features or code.
  - **Includes**: Only core resources. About, Computer Info dialogs, icons, manifest are removed.
  - **Optimization**: Disabled.
  - **Size**: 180-200 KB (larger because of debug symbols and no optimization).
  - **Usage**: `build.bat /minbuild`\
  *Note: It produce a binary with different name to do not overwrite release binary*

### Steps
1. Clone or download a repository
2. Make sure `gcc`, `windres`, and `python` are available in system `PATH`.
3. Open a command prompt in the project folder.
4. (Optional) If you want to define your own key before, run:\
   `python keygen_fixed.py <your own key>`\
   *Note: To ensure compatibility with official releases, use key specified in release you are targeting for.*
6. Run depending on what you need:\
   `build.bat` - Compile QuickChat without changing key and packing.\
   `build.bat /rekey` - Compile QuickChat and regenerate key without packing.\
   `build.bat /pack` - Compile QuickChat and pack without regenerating key.\
   `build.bat /rekey /pack` - Compile QuickChat, regenerate key and pack.\
   `build.bat /minbuild` - Compile QuickChat with most minimal configuration.\
   `build.bat /clean` - Delete existing compiled files.\
   `build.bat /clean /nocleankey` - Delete existing compiled files, but do not delete key file.
   `build.bat /minbuild /rekey` - Compile QuickChat with most minimal configuration and regerenate key

## Credits
This project uses [7-Zip](https://www.7-zip.org/) for archiving.  
`7z.exe` is included for convenience and is used under the terms of the GNU LGPL license.

---

Enjoy! If you have questions, write me to my Discord - @pcsettings
