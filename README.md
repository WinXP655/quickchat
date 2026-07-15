# <img width="48" height="48" alt="icon48" src="https://github.com/user-attachments/assets/e01482e7-3821-4102-be18-e42e7553139b" /> QuickChat
QuickChat is a minimalistic LAN messenger, written on pure C with re Win32 API.
Does not require installation, leaves no traces, and works on any version starting from Windows 2000 up to 11.

<img width="586" height="388" alt="image" src="https://github.com/user-attachments/assets/5fae96ec-dd38-4159-b84c-e47dc066941e" />

Based on [MicroChat Framework](https://github.com/WinXP655/microchat).

## Features
1. **Portable** - just 1 .exe file
2. **Unicode Support** - any languages, any symbols.
3. **QC/QCS Protocol**
- QC (QuickChat) - plaintext.
- QCS (QuickChat Obfuscated) - XOR obfuscation.
4. **Logging** - save chat history and events. Only for Server, disabled by default.
5. **Tiny Size** - just 142 KB.

## Requirements
- **OS** - Windows 2000 and newer.

## How to use

### 1. Server
1. Run QuickChat.
2. Click Yes.
3. Select protocol.
4. Select if you want to enable logs.
5. Share displayed IP.
   Note that user should be in same network.

### 2. Client
1. Run QuickChat.
2. Click No.
3. Type the Server IP and click Connect.
4. Select protocol.

## Logging
Chat log stored only on server side and do not contain sensitive information. It only logs:
1. Session start and end.
2. User messages with timestamps.
3. System messages and events.
4. Where server set port binding.
5. Who connected (computer name and LAN IP).

## Protocol
It uses a very simple yet working custom protocol QuickChat/QuickChat Secure (QC/QCS):
1. Who starts first: Client. Server never sends anything until it will be QC handshake.
2. Client sent handshake in following format: QC:PCNAME\0.
   Replace PCNAME with your computer name or what you want remote side to see.
   "\0" is required - official backend written on C, meaning you have to follow C rules.
   If you are using QCS, you need to XOR everything before.
   First 3 bytes should be exactly "QC:" or XORed version of it. Server reject if it is non-QC or at least 1 byte is wrong.
3. Server send its name in same format.
4. Chat starts.

It also supports custom ping:
1. Send "QCPING" or its XORed version to remote side.
2. If you receive "QCPONG" or its XORed version, then remote side is active
3. Note that both sides should know both commands.
*Server side can delete logs at any time, it is stored only locally*.

## Changelog
Read CHANGELOG.md.

---

Enjoy! If you have questions, write me to my Discord - @pcsettings
