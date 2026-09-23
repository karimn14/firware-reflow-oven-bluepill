# Laporan task dan watchdog multiplexing

## Cakupan

Firmware membuat enam task secara statis; kernel FreeRTOS membuat satu idle task. Tick FreeRTOS adalah 1 ms. Satu `StackType_t` adalah satu word 32 bit (4 byte). Pengujian servo tersedia melalui **Beranda → Diagnostik → Uji Servo** pada firmware yang sama.

## Inventaris task

| ID watchdog | Task | Prioritas | Stack dialokasikan | Interval loop | Periode heartbeat yang diharapkan | Batas usia heartbeat |
|---:|---|---:|---:|---|---|---:|
| 0 | `defaultTask` | 2 | 256 word = 1.024 byte | `vTaskDelay(10 ms)`; pembaruan OLED paling cepat tiap 50 ms | Setelah setiap iterasi, sekitar 10 ms ditambah waktu kerja | 1.500 ms |
| 1 | `inputTask` | 3 | 192 word = 768 byte | `vTaskDelay(10 ms)` | Setelah setiap iterasi, sekitar 10 ms ditambah waktu kerja | 100 ms |
| 2 | `cdcTask` | 1 | 224 word = 896 byte | `vTaskDelay(5 ms)` | Setelah setiap iterasi, sekitar 5 ms ditambah waktu kerja USB dan perintah | 3.000 ms |
| 3 | `thermalTask` | 2 | 96 word = 384 byte | `vTaskDelay(100 ms)` | Setelah setiap iterasi, sekitar 100 ms ditambah waktu kerja | 500 ms |
| 4 | `conveyorTask` | 2 | 96 word = 384 byte | `vTaskDelayUntil(10 ms)` | Setelah setiap iterasi, target 10 ms | 100 ms |
| 5 | `inspectionTask` | 1 | 96 word = 384 byte | `vTaskDelayUntil(20 ms)` | Setelah setiap iterasi, target 20 ms | 1.000 ms |
| 6 | Idle task | 0 | 96 word = 384 byte | Berjalan saat tidak ada task lain yang siap | Setiap 100 ms **jika idle mendapat CPU** | 500 ms |

**Total stack statis:** 960 word = 3.840 byte untuk enam task aplikasi; 1.056 word = 4.224 byte termasuk idle task. Angka pada tabel adalah kapasitas yang dialokasikan. Konsumsi puncak yang teramati tersedia langsung di OLED melalui **Beranda → Diagnostik → Task Stack Consumption**. Halaman itu menghitung `(alokasi word − uxTaskGetStackHighWaterMark()) × 4 byte` untuk setiap task dan memperbaruinya saat ditampilkan. Nilainya berlaku sejak boot dan dapat meningkat ketika jalur kerja baru dijalankan. `configCHECK_FOR_STACK_OVERFLOW=2` juga aktif. Prioritas FreeRTOS lebih tinggi bila angkanya lebih besar. TCB dan variabel statis lain tidak termasuk total stack ini.

`vTaskDelay()` menambahkan waktu tunda **setelah** kerja selesai. `vTaskDelayUntil()` menargetkan periode tetap, tetapi dapat terlambat jika eksekusi atau penjadwalan melewati periodenya. Batas heartbeat sengaja lebih longgar daripada interval loop agar transaksi OLED, antrean USB, dan penjadwalan task prioritas rendah tidak langsung menyebabkan reset. Batas tersebut perlu divalidasi di perangkat dengan beban terburuk.

## Fungsi tiap task

- **ID 0 — `defaultTask`:** menginisialisasi USB dan perangkat melalui `HardwareTest_Init()`, lalu membaca ADC/suhu dan memperbarui tampilan OLED melalui `HardwareTest_Run()`. Tampilan dijadwalkan paling cepat setiap 50 ms, sedangkan loop diperiksa setiap sekitar 10 ms.
- **ID 1 — `inputTask`:** memindai tombol dan UI, memperbarui kontrol/PID, memeriksa keselamatan heater, dan menerapkan output heater melalui `HardwareTest_InputRun()`. Ini memiliki prioritas aplikasi tertinggi.
- **ID 2 — `cdcTask`:** mengolah perintah CDC dari USB, protokol Raspberry Pi, serta mengirim status/log. Penulisan USB dapat menunggu hingga 500 ms per panggilan; karena itu tenggatnya paling longgar.
- **ID 3 — `thermalTask`:** menjalankan state machine profil reflow, memeriksa suhu dan fault, serta memproses karakterisasi heater.
- **ID 4 — `conveyorTask`:** memperbarui motor dan encoder setiap 10 ms; state machine urutan conveyor diperbarui setiap 50 ms saat mode manual tidak aktif.
- **ID 5 — `inspectionTask`:** mengelola permintaan inspeksi ke Raspberry Pi, hasil inspeksi, timeout, dan peristiwa penyortiran.
- **ID 6 — Idle task:** menjalankan supervisor watchdog. Bila task lain terus memakai CPU sehingga idle tidak mendapat giliran, IWDG juga tidak di-refresh.

## Cara kerja watchdog

IWDG diaktifkan tepat sebelum scheduler berjalan dengan prescaler 64 dan reload 2499. Dengan LSI nominal 40 kHz, timeout perangkat kerasnya sekitar **4 detik**; frekuensi LSI sesungguhnya mengubah angka ini. Setiap task aplikasi memperbarui timestamp heartbeat **setelah** pekerjaan satu iterasi selesai. Idle hook memeriksa seluruh ID tiap 100 ms dan menjadi satu-satunya tempat yang memanggil `HAL_IWDG_Refresh()`.

Refresh hanya terjadi bila **ketujuh ID sudah pernah melapor** dan seluruh timestamp masih berada dalam batas usia pada tabel. Bila satu task belum pernah melapor, refresh tidak dimulai. Jika satu task melewati batas setelah pernah melapor, kegagalan dikunci sampai MCU reset; heartbeat yang pulih belakangan tidak menghidupkan refresh lagi. Pemantauan ini membuktikan kemajuan loop; heartbeat saja tidak menjamin setiap hasil sensor, aktuator, atau komunikasi benar secara semantik. Jalur stack overflow yang ada mematikan output lalu berhenti; IWDG selanjutnya mereset MCU.

## Dasar konfigurasi dan verifikasi

- Alokasi stack dan prioritas: `Core/Src/freertos.c`.
- Interval loop: `Core/Src/freertos.c`, `Core/Src/cdc_console.c`, `Core/Src/reflow.c`, `Core/Src/conveyor_app.c`, `Core/Src/inspection.c`.
- ID, batas heartbeat, dan syarat refresh: `Core/Inc/watchdog.h` dan `Core/Src/watchdog.c`.
- Timeout IWDG: `Core/Src/iwdg.c`; waktu tick: `Core/Inc/FreeRTOSConfig.h`.
- Build Debug dan Release serta uji host `test_watchdog` dan `test_inspection` berhasil. Uji fault pada perangkat, misalnya menghentikan satu heartbeat sementara, masih diperlukan untuk membuktikan waktu reset fisik serta mengecek batas heartbeat terhadap beban nyata.
