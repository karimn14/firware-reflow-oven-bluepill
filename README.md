# Firmware Reflow Oven STM32 Blue Pill

Firmware eksperimental untuk pengujian hardware dan kendali suhu *hot plate* berbasis **STM32F103C8T6 (Blue Pill)**. Proyek ini membaca thermistor NTC, menampilkan status pada OLED SSD1306, menerima input dari empat tombol, dan mengendalikan heater melalui SSR menggunakan PWM. Firmware berjalan di atas STM32 HAL dan FreeRTOS/CMSIS-RTOS v2.

> [!IMPORTANT]
> Firmware ini dibuat untuk berjalan bersama [Serasidis STM32 HID Bootloader](https://github.com/Serasidis/STM32_HID_Bootloader). Aplikasi **bukan** ditempatkan di awal flash: vector table aplikasi dimulai di `0x08000800`, setelah area bootloader 2 KiB.

### Inisialisasi project untuk VS Code atau Zed

> [!NOTE]
> Pastikan STM32 Blue Pill telah diinstal bootloader stm32-hid-bootloader dari Serasidis.
> Sudah Terinstal STM32CubeClt (Toolchain + CMake + Ninja)

#### Panduan Development
- Install STM32CubeClt
- Clone Project ini ke komputer Anda.
- Clone repository [saufik-ramadhan/stm32-vscode-init](https://github.com/saufik-ramadhan/stm32-vscode-init) ke komputer Anda.
- Buka terminal dan arahkan ke direktori `stm32-vscode-init`.
- Jalankan `./init.sh` untuk menginstal dependensi dan konfigurasi awal berikan argumen `--editor Zed` atau `--editor VSCode` untuk IDE yang akan digunakan lalu berikan path ke project STM32 Blue Pill Anda, script ini akan mengkonfigurasi environment sesuai kebutuhan.
- Untuk Windows
```bash
.\init.ps1 --editor zed --flash-method hid --hid-flash tools\hid-flash.exe --hid-port COM7 C:\path\to\project
```
- Untuk Linux
```bash
./init.sh --editor zed --flash-method hid --hid-flash tools/hid-flash --hid-port /dev/ttyUSB0 /path/to/project
```
- Buka project STM32 Blue Pill Anda di VS Code atau Zed.
- Ctrl + Shift + P untuk membuka Command Palette dan menjalankan task configure (Release/Debug).
- Lalu Jalankan task build dan flash.

Konfigurasi development environment untuk **Visual Studio Code** atau **Zed** dapat dibuat menggunakan repository [saufik-ramadhan/stm32-vscode-init](https://github.com/saufik-ramadhan/stm32-vscode-init).

Tool tersebut dapat digunakan untuk membantu menyiapkan integrasi project STM32 berbasis CMake, termasuk task build, flash, dan debug editor. Project ini sudah memiliki konfigurasi Zed pada direktori `.zed/`, termasuk task build dan flash melalui STM32 HID Bootloader. Ikuti petunjuk instalasi dan penggunaan terbaru pada README repository `stm32-vscode-init` apabila konfigurasi perlu dibuat ulang atau project dibuka pada komputer lain.

## Fitur saat ini

- Pengujian manual SSR/heater dengan duty cycle 25%, 50%, 75%, atau 100%.
- Pembacaan ADC thermistor dengan rata-rata 32 sampel; sampel minimum dan maksimum dibuang.
- Kalibrasi thermistor dua titik dan penyimpanan hasil kalibrasi di flash.
- Kendali suhu PID dengan *setpoint ramp*, filter derivatif, pembatas integral, dan pembatas daya ketika mendekati target.
- Proteksi heater ketika sensor tidak valid atau suhu mencapai 150 °C.
- Antarmuka OLED 128×64 berbasis SSD1306 melalui I2C 400 kHz.
- Empat tombol aktif-low dengan pull-up internal, debounce, dan auto-repeat.
- USB Full Speed sebagai Virtual COM Port (CDC).
- Console interaktif USB CDC untuk PuTTY, `screen`, atau serial monitor lain.
- Masuk ke HID bootloader melalui reset yang dipicu oleh utilitas flashing pada port USB CDC.
- FreeRTOS dengan task terpisah untuk pembacaan input/kendali, pembaruan sensor/display, dan console USB CDC.

## Hardware

Komponen utama yang digunakan:

- STM32 Blue Pill dengan MCU STM32F103C8T6 dan RAM 20 KiB. Part C8 secara resmi biasa diperlakukan sebagai flash 64 KiB, tetapi board yang digunakan project ini telah diuji memiliki flash fisik 128 KiB.
- Kristal HSE 8 MHz pada board Blue Pill; system clock dikonfigurasi ke 72 MHz.
- OLED SSD1306 128×64, I2C, alamat 7-bit `0x3C`.
- Thermistor NTC nominal 100 kΩ, Beta bawaan 3950 K.
- Resistor tetap 4,7 kΩ untuk pembagi tegangan thermistor.
- SSR atau driver heater dengan input logika yang sesuai untuk sinyal 3,3 V.
- Empat push button.
- USB data cable untuk CDC dan HID bootloader.
- ST-Link atau programmer lain untuk pemasangan awal bootloader.

Output fan dan DC motor telah dialokasikan pada pin dan kanal timer, tetapi program aplikasi saat ini hanya memulai serta mengubah PWM heater pada TIM3 channel 1.

## Pin mapping

| Fungsi | Pin MCU | Peripheral | Konfigurasi/keterangan |
|---|---:|---|---|
| Thermistor | PA0 | ADC1 IN0 | Input analog, 12-bit |
| PWM heater / SSR | PA6 | TIM3 CH1 | PWM 1 Hz, aktif-high; digunakan aplikasi |
| PWM fan | PA7 | TIM3 CH2 | PWM 1 Hz; baru dikonfigurasi, belum dijalankan aplikasi |
| PWM DC motor | PB0 | TIM3 CH3 | PWM 1 Hz; baru dikonfigurasi, belum dijalankan aplikasi |
| OLED SCL | PB6 | I2C1 SCL | 400 kHz, open-drain |
| OLED SDA | PB7 | I2C1 SDA | 400 kHz, open-drain |
| UART TX | PB10 | USART3 TX | 115200, 8-N-1; belum digunakan logika aplikasi |
| UART RX | PB11 | USART3 RX | 115200, 8-N-1; belum digunakan logika aplikasi |
| Tombol A | PB12 | GPIO/EXTI12 | Aktif-low, pull-up internal |
| Tombol B | PB13 | GPIO/EXTI13 | Aktif-low, pull-up internal |
| Tombol C | PB14 | GPIO/EXTI14 | Aktif-low, pull-up internal |
| Tombol D | PB15 | GPIO/EXTI15 | Aktif-low, pull-up internal |
| USB D− | PA11 | USB FS DM | USB CDC saat aplikasi berjalan; USB HID saat bootloader aktif |
| USB D+ | PA12 | USB FS DP | USB CDC saat aplikasi berjalan; USB HID saat bootloader aktif |
| SWDIO | PA13 | SWD | Pemrograman/debug dengan ST-Link |
| SWCLK | PA14 | SWD | Pemrograman/debug dengan ST-Link |
| HSE IN/OUT | PD0/PD1 | RCC OSC | Kristal eksternal 8 MHz pada board |

Semua ground modul, catu daya logika, sensor, dan Blue Pill harus mempunyai referensi ground yang sama. Jangan menghubungkan heater berdaya tinggi langsung ke pin MCU; gunakan SSR/driver serta isolasi dan proteksi yang sesuai.

### Rangkaian thermistor

Perhitungan bawaan menggunakan thermistor 100 kΩ dan resistor tetap 4,7 kΩ. Firmware dapat mendeteksi arah pembagi tegangan dari dua titik kalibrasi:

```text
3,3 V --- resistor 4,7 kΩ ---+--- NTC --- GND
                             |
                            PA0
```

Topologi kebalikannya juga dapat dikenali setelah dua titik kalibrasi yang valid. Nilai ADC sangat dekat `0` atau `4095` dianggap sebagai kondisi sensor putus atau korslet.

## Cara menggunakan program

Saat boot, output heater dipastikan 0%, ADC dikalibrasi, data kalibrasi thermistor dimuat dari flash, OLED diinisialisasi, lalu program membuka layar **SSR HEATER TEST**.

### 1. Mode pengujian heater

| Tombol | Fungsi |
|---|---|
| A | Menghidupkan/mematikan heater |
| B | Menaikkan duty cycle sebesar 25% |
| C | Menurunkan duty cycle sebesar 25% |
| D | Masuk ke kalibrasi thermistor |

Duty cycle awal adalah 50%. Timer heater menggunakan periode 1 detik (PWM 1 Hz), sesuai untuk pengujian SSR zero-cross. Mode ini adalah mode manual; operator tetap bertanggung jawab mengawasi suhu dan sistem daya.

### 2. Kalibrasi thermistor dua titik

Gunakan referensi suhu yang tepercaya dan ambil dua titik yang berjauhan agar model thermistor lebih akurat.

| Tombol | Fungsi |
|---|---|
| A | Menangkap ADC dan suhu referensi untuk titik aktif |
| B | Menambah suhu referensi 0,1 °C; tahan untuk auto-repeat |
| C | Mengurangi suhu referensi 0,1 °C; tahan untuk auto-repeat |
| D singkat | Menghidupkan/mematikan heater kalibrasi |
| D ditahan ≥1,5 detik | Keluar ke mode PID; simpan apabila kedua titik valid |

Alur kalibrasi:

1. Masuk ke layar kalibrasi dengan tombol D dari layar pengujian heater.
2. Stabilkan sistem pada suhu pertama.
3. Sesuaikan `REF` dengan tombol B/C, lalu tekan A untuk mengambil titik P1.
4. Stabilkan sistem pada suhu kedua yang berbeda cukup jauh.
5. Sesuaikan `REF`, lalu tekan A untuk mengambil titik P2.
6. Tahan D minimal 1,5 detik untuk menyimpan dan masuk ke layar PID.

Data yang lolos validasi disimpan pada halaman flash terakhir, `0x0800FC00–0x0800FFFF`. Firmware menghitung ulang nilai Beta dan resistansi nominal R25 dari dua titik tersebut. Kalibrasi yang belum lengkap tidak disimpan.

### 3. Mode kendali PID

| Tombol | Fungsi |
|---|---|
| A | Menjalankan/menghentikan PID |
| B | Menaikkan setpoint 0,5 °C; tahan untuk auto-repeat |
| C | Menurunkan setpoint 0,5 °C; tahan untuk auto-repeat |
| D | Mematikan PID dan kembali ke mode pengujian heater |

Setpoint awal adalah 70 °C dan dapat diatur pada rentang 20–140 °C. PID hanya dapat dijalankan jika thermistor sudah dikalibrasi dan pembacaan sensor valid. Konfigurasi kontrol saat ini:

| Parameter | Nilai |
|---|---:|
| Kp | 3,0 |
| Ki | 0,05 |
| Kd | 15,0 |
| Interval kontrol | 250 ms |
| Laju ramp setpoint | 0,5 °C/s |
| Konstanta filter derivatif | 1,0 s |
| Batas keselamatan | heater off pada ≥150 °C |

Nilai tersebut masih bersifat parameter awal untuk plant hot plate dan perlu divalidasi/tuning pada hardware sebenarnya.

## STM32 HID bootloader

Proyek menggunakan **STM32 HID Bootloader** dari Serasidis, bukan bootloader USB DFU bawaan ROM STM32. Bootloader tampil sebagai perangkat USB HID sehingga proses upload aplikasi tidak memerlukan driver USB khusus untuk HID pada sistem operasi modern.

Pembagian flash proyek:

| Alamat | Ukuran | Isi |
|---|---:|---|
| `0x08000000–0x080007FF` | 2 KiB | STM32 HID Bootloader |
| `0x08000800–0x0800FBFF` | 61 KiB | Firmware aplikasi |
| `0x0800FC00–0x0800FFFF` | 1 KiB | Data kalibrasi thermistor |

### Hasil pengujian kapasitas flash board

Board yang digunakan saat ini telah diuji langsung dan terbukti mempunyai **flash fisik 128 KiB**:

- Flash-size register pada `0x1FFFF7E0` melaporkan `128 KiB`.
- Halaman uji `0x0801F800` berhasil dihapus, ditulis dengan marker, dan diverifikasi.
- Halaman `0x0800F800` tidak berubah, sehingga area di atas 64 KiB bukan alias dari area bawah.
- Isi kedua halaman dicadangkan sebelum pengujian dan berhasil dipulihkan byte-for-byte setelah pengujian.
- Data kalibrasi pada `0x0800FC00` tetap utuh.

Walaupun hardware ini mempunyai 128 KiB, linker project masih sengaja membatasi aplikasi pada layout 64 KiB agar kompatibel dengan STM32F103C8T6 lain yang mungkin benar-benar hanya menyediakan 64 KiB. Area tambahan belum digunakan oleh firmware. Menggunakan seluruh 128 KiB memerlukan perubahan linker script dan pemindahan halaman kalibrasi, serta akan mengurangi portabilitas firmware ke board C8 lain.

Konfigurasi tersebut diterapkan di dua tempat:

- `STM32F103xx_FLASH.ld` mengatur origin aplikasi ke `0x08000800` dan menyisakan halaman terakhir untuk kalibrasi.
- `USER_VECT_TAB_ADDRESS` dan `VECT_TAB_OFFSET = 0x800` memindahkan vector table ke alamat aplikasi.

Jangan mengubah offset menjadi `0x08000000` ketika firmware akan diunggah melalui bootloader ini karena image aplikasi dapat menimpa bootloader atau gagal berjalan.

### Pemasangan bootloader pertama kali

Bootloader harus dipasang satu kali menggunakan ST-Link/SWD atau metode pemrograman lain. Pilih binary untuk **STM32F103 low/medium density** yang sesuai dengan cara aktivasi bootloader pada board, lalu tulis ke alamat `0x08000000`. Ikuti dokumentasi dan binary rilis pada repositori [STM32_HID_Bootloader](https://github.com/Serasidis/STM32_HID_Bootloader).

Menghapus seluruh chip dengan perintah *full chip erase* juga akan menghapus bootloader dan data kalibrasi. Setelah itu bootloader perlu dipasang kembali.

### Masuk bootloader dari aplikasi

Saat aplikasi berjalan, USB tampil sebagai `STM32 Virtual ComPort`. Integrasi flashing proyek melakukan urutan berikut:

1. Membuka/men-toggle control line USB CDC beberapa kali.
2. Mengirim magic sequence ASCII `1EAF`.
3. Firmware menulis magic `0x424C` ke backup register `BKP_DR4`.
4. Firmware melakukan system reset dan bootloader mengambil alih sebagai perangkat HID.
5. `hid-flash` mengirim firmware baru dan board kembali menjalankan aplikasi.

Mekanisme ini sudah ditangani oleh task **Build + Flash via HID** pada konfigurasi Zed proyek. Port bawaan yang digunakan adalah `ttyACM0`; sesuaikan file `.zed/flash-*.cmake` apabila device muncul dengan nama lain.

## Console USB CDC

Ketika firmware aplikasi berjalan, konektor USB Blue Pill tampil sebagai Virtual COM Port. Di Linux/Raspberry Pi perangkat biasanya muncul sebagai `/dev/ttyACM0`; nomor port dapat berubah apabila ada perangkat serial USB lain.

Contoh membuka console di Raspberry Pi/Linux:

```bash
screen /dev/ttyACM0 115200
```

Atau gunakan PuTTY dengan pengaturan:

- Connection type: **Serial**
- Serial line: `/dev/ttyACM0` pada Linux/Raspberry Pi, atau port `COMx` pada Windows
- Speed: `115200`
- Data bits: 8
- Stop bits: 1
- Parity: None
- Flow control: None
- Local echo: Auto atau Off; firmware melakukan echo karakter

Nilai baud rate tidak mengubah kecepatan fisik USB CDC, tetapi `115200 8-N-1` disarankan agar konfigurasinya konsisten dengan USART3 proyek.

Saat terminal membuka DTR, firmware menampilkan prompt:

```text
STM32 Blue Pill USB CDC console
Type 'help' to list commands.
bluepill>
```

Perintah yang tersedia:

| Perintah | Fungsi |
|---|---|
| `help` | Menampilkan daftar perintah |
| `ping` | Menguji komunikasi; perangkat membalas `pong` |
| `info` | Menampilkan board, nama firmware, transport, dan bootloader |
| `status` | Membaca layar aktif, heater, duty, ADC, suhu, resistansi, kalibrasi, PID, dan setpoint |
| `echo <teks>` | Mengirim kembali teks ke host |

Console saat ini bersifat read-only terhadap sistem kontrol: tidak tersedia perintah untuk menyalakan heater atau mengubah PID. Pembatasan ini disengaja agar membuka terminal tidak dapat mengaktifkan beban panas secara tidak sengaja.

Data USB diterima oleh callback CDC dan dimasukkan ke ring buffer 256 byte. Task `cdcTask` memproses command tanpa melakukan pekerjaan berat di dalam interrupt USB. Panjang maksimum satu command adalah 95 karakter.

## Build firmware

### Kebutuhan software

- CMake 3.22 atau lebih baru.
- Ninja.
- GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, dan utilitas terkait).
- `hid-flash` dari STM32 HID Bootloader untuk upload melalui USB.
- Opsional: STM32CubeMX 6.18.1 dan STM32CubeF1 HAL v1.8.7 untuk membuka atau meregenerasi file `.ioc`.

### Build dengan preset CMake portabel

```bash
cmake --preset Release
cmake --build --preset Release
arm-none-eabi-objcopy -O binary \
  build/Release/test-bluepill-1.elf \
  build/Release/test-bluepill-1.bin
```

Untuk build debug, ganti `Release` dengan `Debug`.

### Build dengan STM32CubeCLT yang dikonfigurasi di proyek

`CMakeUserPresets.json` menyediakan preset lokal `clt-Debug` dan `clt-Release`. Contoh:

```bash
cmake --preset clt-Release
cmake --build --preset clt-Release
```

Preset ini mengacu ke instalasi STM32CubeCLT pada `/opt/st/stm32cubeclt_1.22.0`. Ubah konfigurasi jika lokasi instalasi berbeda.

## Upload melalui HID

Setelah bootloader terpasang dan file `.bin` selesai dibuat:

```bash
hid-flash build/Release/test-bluepill-1.bin ttyACM0
```

Gunakan nama port tanpa awalan `/dev/`, sesuai argumen yang dipakai konfigurasi flash proyek. Pada Zed, task berikut sudah tersedia:

- `STM32: Build + Flash via HID (Debug)`
- `STM32: Build + Flash via HID (Release)`
- `STM32: Flash via HID (Debug)`
- `STM32: Flash via HID (Release)`

Task flash mengubah ELF menjadi BIN terlebih dahulu, kemudian menjalankan `hid-flash`.

## Struktur source code

| Lokasi | Isi utama |
|---|---|
| `Core/Src/main.c` | Inisialisasi clock/peripheral, USB CDC awal, dan scheduler |
| `Core/Src/freertos.c` | Pembuatan task input/kendali, hardware/display, dan console CDC |
| `Core/Src/cdc_console.c` | Ring buffer RX, task console, parser command, dan respons USB CDC |
| `Core/Src/hardware_test.c` | State layar, tombol, pembacaan ADC, heater manual, kalibrasi, dan PID |
| `Core/Src/thermistor.c` | Model NTC, kalibrasi dua titik, validasi, dan penyimpanan flash |
| `Core/Src/ssd1306.c` | Driver OLED SSD1306 |
| `USB_DEVICE/App/usbd_cdc_if.c` | USB CDC serta mekanisme reset menuju HID bootloader |
| `Core/Src/{adc,i2c,tim,usart,gpio}.c` | Konfigurasi peripheral hasil STM32CubeMX |
| `STM32F103xx_FLASH.ld` | Layout RAM/flash aplikasi dan reservasi kalibrasi |
| `test-bluepill-1.ioc` | Konfigurasi STM32CubeMX |
| `.zed/` | Task build, flash HID, size report, dan debug ST-Link |

Catatan: file `iwdg.c` tersedia dari konfigurasi generated code, tetapi `MX_IWDG_Init()` belum dipanggil oleh aplikasi sehingga watchdog belum aktif.

## Peringatan keselamatan

Proyek ini mengendalikan heater dan berpotensi berhubungan dengan tegangan listrik serta suhu tinggi. Gunakan SSR yang benar, sekering, grounding, isolasi, enclosure, emergency cutoff, dan proteksi termal independen. Jangan mengandalkan firmware sebagai satu-satunya lapisan keselamatan. Uji pertama kali dengan beban bertegangan rendah atau beban simulasi dan awasi output heater secara langsung.
