# 3DS Drop (`3ds_drop`)

> **Wireless SD File Server** — Nintendo 3DS Homebrew (`CTR-P-DROP`, UniqueId `0x0D3E3`) by [**lafiabrew**](https://github.com/orgs/lafiabrew/repositories)

Wireless LAN HTTP/JSON SD card scanner, crash log harvester, and atomic save/ROM file drop server (:8766) with live 3DS IP display.

## Build & Verify

```bash
# Run host C11 ASan/UBSan unit & UI simulation tests
make test

# Build ARM11 3DS .elf, .smdh, .3dsx, and .cia (via devkitARM or rootless Podman container)
make 3ds && make verify-3ds

# Serve FBI QR code or push .cia over LAN
make qr
```
