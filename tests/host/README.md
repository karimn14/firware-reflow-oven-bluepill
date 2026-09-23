# Uji host `inspection.c`

Menjalankan `Core/Src/inspection.c` dan `Core/Src/pi_protocol.c` yang asli di PC, tanpa board: jam 1 ms palsu, model conveyor kecil, dan Raspberry Pi tiruan. Header di `stub/` menggantikan FreeRTOS, HAL, dan conveyor. Enum di `stub/conveyor_app.h` harus tetap sama dengan `Core/Inc/conveyor_sequencer.h`.

```bash
# dari root firmware-bluepill, dengan GCC untuk PC (MinGW/MSYS2 di Windows, gcc di Linux)
gcc -std=c99 -Wall -Wextra -Werror -I tests/host/stub -I Core/Inc \
    tests/host/test_inspection.c Core/Src/inspection.c Core/Src/pi_protocol.c -o build/test_inspection
for s in pass fail silent lostack offline; do build/test_inspection $s | tail -1; done
```

| Skenario | Yang diperiksa |
|---|---|
| `pass` | Settle 300 ms → satu `$DET`; `$RES` PCB lain dan frame rusak diabaikan; `$RES` PASS → servo kanan, `$ACK`, `$SORT bin=PASS` |
| `fail` | `$RES` FAIL → servo kiri, `$SORT bin=REJECT` |
| `silent` | Pi diam: `$DET` 3 kali tiap 300 ms, 3 detik → kiri, `$SORT ... why=TIMEOUT`; jawaban terlambat dibalas `$ACK` tetapi diabaikan |
| `lostack` | `$ACK` Pi hilang: `$DET` dikirim ulang, jawaban yang datang masih dalam 3 detik tetap dipakai |
| `offline` | Port tidak dibuka (DTR rendah): tidak ada frame terkirim, PCB tetap berakhir REJECT lewat timeout |

Setiap baris `TX` dapat dicek dengan parser Raspberry Pi di repositori utama (`pi/station/protocol.py`, `decode(line, STM_TO_PI)`).

## Uji host watchdog

Uji ini memakai `watchdog.c` asli dan IWDG tiruan. Kasusnya mencakup heartbeat yang belum lengkap, heartbeat sehat, task yang melewati tenggat, kegagalan yang tetap terkunci, dan wrap counter tick 32 bit.

```bash
gcc -std=c99 -Wall -Wextra -Werror -I tests/host/stub -I Core/Inc \
    tests/host/test_watchdog.c Core/Src/watchdog.c -o build/test_watchdog
build/test_watchdog
```
