# Firmware Reflow Oven STM32 Blue Pill

Firmware eksperimental untuk pengujian hardware dan kendali suhu *hot plate* berbasis **STM32F103C8T6 (Blue Pill)**. Proyek ini membaca thermistor NTC, menampilkan status pada OLED SSD1306, menerima input dari empat tombol, dan mengendalikan heater melalui SSR menggunakan PWM. Firmware berjalan di atas STM32 HAL dan FreeRTOS.

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
- Profil PCB reflow POC otomatis dengan tahap preheat, soaking, reflow, dan cooling; target puncak dibatasi 120 °C.
- Halaman OLED reflow dengan status tahap, target, output heater, waktu proses, dan grafik suhu bergulir.
- Mode karakterisasi heater otomatis dengan duty tetap, cutoff 110 °C, pengukuran °C/s, overshoot, cooling, dan log CSV USB.
- Sequencer conveyor otomatis dari LOAD → HEAT → INSPECT → SORT, dengan encoder, sensor IR, dan timeout fail-safe.
- Interlock mutex FreeRTOS yang mencegah motor conveyor dan profil heater menguasai plant pada saat bersamaan.
- Inspeksi PCB melalui USB CDC ke Raspberry Pi dan servo sorting PASS ke kanan atau FAIL/timeout ke kiri.
- Proteksi heater pada 150 °C untuk PID, 125 °C untuk reflow POC, dan 110 °C untuk karakterisasi.
- Antarmuka OLED 128×64 berbasis SSD1306 melalui I2C 400 kHz.
- Empat tombol aktif-low dengan pull-up internal, debounce, dan auto-repeat.
- USB Full Speed sebagai Virtual COM Port (CDC).
- Console interaktif USB CDC untuk PuTTY, `screen`, atau serial monitor lain.
- Masuk ke HID bootloader melalui reset yang dipicu oleh utilitas flashing pada port USB CDC.
- FreeRTOS dengan task statis untuk input/kendali, sensor/display, thermal, conveyor, inspeksi, dan console USB CDC.

## Hardware

Komponen utama yang digunakan:

- STM32 Blue Pill dengan MCU STM32F103C8T6 dan RAM 20 KiB. Part C8 secara resmi biasa diperlakukan sebagai flash 64 KiB, tetapi board yang digunakan project ini telah diuji memiliki flash fisik 128 KiB.
- Kristal HSE 8 MHz pada board Blue Pill; system clock dikonfigurasi ke 72 MHz.
- OLED SSD1306 128×64, I2C, alamat 7-bit `0x3C`.
- Thermistor NTC nominal 100 kΩ, Beta bawaan 3950 K.
- Resistor tetap 4,7 kΩ untuk pembagi tegangan thermistor.
- SSR atau driver heater dengan input logika yang sesuai untuk sinyal 3,3 V.
- Empat push button.
- Conveyor belt, motor DC, driver/H-bridge dengan input PWM, dan encoder optik satu kanal.
- Sensor IR aktif-low untuk mendeteksi PCB di posisi inspeksi.
- Servo 50 Hz untuk menyortir PCB ke sisi PASS atau FAIL.
- Catu daya motor dan servo yang terpisah dari regulator 3,3 V Blue Pill, dengan common ground.
- USB data cable untuk CDC dan HID bootloader.
- ST-Link atau programmer lain untuk pemasangan awal bootloader.

Nilai posisi encoder, deadband motor, duty conveyor, dan pulse servo pada firmware adalah nilai awal POC. Semuanya harus dikalibrasi terhadap mekanik dan catu daya yang sebenarnya sebelum conveyor dijalankan dengan PCB.

## Pin mapping

| Fungsi | Pin MCU | Peripheral | Konfigurasi/keterangan |
|---|---:|---|---|
| Thermistor | PA0 | ADC1 IN0 | Input analog, 12-bit |
| Servo sorter | PA1 | TIM2 CH2 | PWM 50 Hz, 1 µs/tick; 1000/1500/2000 µs |
| PWM heater / SSR | PA6 | TIM3 CH1 | PWM 1 Hz, aktif-high; digunakan aplikasi |
| PWM fan | PA7 | TIM3 CH2 | PWM 1 Hz; baru dikonfigurasi, belum dijalankan aplikasi |
| Encoder conveyor | PA8 | EXTI8 | Input rising-edge, pull-down, encoder satu kanal |
| Sensor IR PCB | PB1 | GPIO input | Aktif-low, pull-up, debounce 150 ms |
| OLED SCL | PB6 | I2C1 SCL | 400 kHz, open-drain |
| OLED SDA | PB7 | I2C1 SDA | 400 kHz, open-drain |
| PWM DC motor | PB8 | TIM4 CH3 | PWM 20 kHz, aktif-high, arah motor diatur hardware |
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

Saat boot, output heater dan motor dipastikan 0%, ADC dikalibrasi, data kalibrasi thermistor dimuat dari flash, OLED diinisialisasi, lalu program membuka layar **CONVEYOR** untuk pengujian end-to-end. Tekan A untuk memulai; semua aktuator tetap mati sebelum tombol ditekan.

### Mode pengujian motor DC (layar awal)

Hubungkan input PWM driver motor ke **PB8 (TIM4 CH3)**. PWM berjalan pada 20 kHz. Motor wajib memakai driver/H-bridge dan catu daya terpisah; jangan menghubungkan motor langsung ke pin Blue Pill. Satukan ground driver dengan ground Blue Pill.

| Tombol | Fungsi |
|---|---|
| A | Menjalankan/menghentikan motor |
| B | Menaikkan duty cycle 10% |
| C | Menurunkan duty cycle 10% |
| D | Masuk ke halaman sequencer conveyor |

Duty awal adalah 40% dan dapat diatur pada rentang 30–100%. Perubahan duty langsung diterapkan ketika motor sedang berjalan. OLED juga menampilkan jumlah pulsa encoder yang terbaca pada PA8. Motor selalu tetap mati saat boot dan ketika keluar dari halaman pengujian.

### 1. Mode pengujian heater

| Tombol | Fungsi |
|---|---|
| A | Menghidupkan/mematikan heater |
| B | Menaikkan duty cycle sebesar 25% |
| C | Menurunkan duty cycle sebesar 25% |
| D | Masuk ke kalibrasi thermistor |

Duty cycle awal adalah 50%. Timer heater menggunakan periode 1 detik (PWM 1 Hz), sesuai untuk pengujian SSR zero-cross. Mode ini adalah mode manual dan tidak memiliki cutoff suhu otomatis; operator tetap bertanggung jawab mengawasi suhu dan sistem daya. Gunakan mode karakterisasi, bukan mode manual, untuk pengujian kemampuan heater dengan cutoff 110 °C.

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

Data yang lolos validasi disimpan pada halaman flash terakhir, `0x0801FC00–0x0801FFFF`. Firmware menghitung ulang nilai Beta dan resistansi nominal R25 dari dua titik tersebut. Kalibrasi yang belum lengkap tidak disimpan. Data valid dari layout lama pada `0x0800FC00` akan dimigrasikan otomatis satu kali ke alamat baru.

### 3. Mode kendali PID

| Tombol | Fungsi |
|---|---|
| A | Menjalankan/menghentikan PID |
| B | Menaikkan setpoint 0,5 °C; tahan untuk auto-repeat |
| C | Menurunkan setpoint 0,5 °C; tahan untuk auto-repeat |
| D | Mematikan PID dan membuka halaman PCB reflow |

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

### 4. Mode PCB reflow POC

Tekan D dari halaman PID untuk membuka halaman reflow. Task `reflowTask` menjalankan urutan tahap secara terpisah dari task input dan display, sedangkan pengaturan daya heater tetap menggunakan pengendali PID yang sama.

Profil bawaan sengaja diturunkan untuk demonstrasi dengan target puncak maksimum 120 °C:

| Tahap | Target bawaan | Durasi/kondisi selesai |
|---|---:|---|
| Idle | Heater mati | Menunggu tombol A |
| Preheat | 60 °C | 90 detik |
| Soaking | 90 °C | 75 detik |
| Reflow | 120 °C | 75 detik |
| Cooling | Heater mati | Hingga suhu ≤50 °C |

Durasi setiap tahap pemanasan masih tetap di dalam firmware. Target suhu dapat diatur ketika status **IDLE**:

| Tombol | Fungsi pada halaman reflow |
|---|---|
| A | Menjalankan profil; saat proses aktif, menghentikan profil dan mematikan heater |
| B | Menaikkan target tahap terpilih 1 °C; tahan untuk auto-repeat |
| C | Menurunkan target tahap terpilih 1 °C; tahan untuk auto-repeat |
| D singkat | Memilih target Preheat (`P`), Soaking (`S`), atau Reflow (`R`) |
| D ditahan ≥1,5 detik | Menghentikan profil, mematikan heater, dan membuka halaman karakterisasi heater |

Firmware menjaga urutan target `Preheat < Soaking < Reflow` dengan selisih minimum 5 °C. Target preheat tidak dapat diturunkan di bawah 40 °C dan target puncak reflow tidak dapat dinaikkan di atas 120 °C. Perubahan target dikunci selama profil berjalan. Pengaturan profil belum disimpan ke flash dan kembali ke nilai bawaan setelah reset.

OLED menampilkan tahap aktif (`IDLE`, `PREHEAT`, `SOAKING`, `REFLOW`, atau `COOLING`), suhu aktual, waktu total, ketiga target, output heater, dan target tahap aktif. Grafik menyimpan 128 sampel dengan interval 2 detik, sehingga menampilkan sekitar 256 detik riwayat suhu; garis titik-titik menunjukkan target aktif. Skala grafik adalah 20–120 °C.

Profil hanya dapat dimulai jika thermistor sudah dikalibrasi dan pembacaannya valid. Selama tahap pemanasan, suhu aktual ≥125 °C atau fault PID langsung mematikan heater dan memindahkan state ke cooling dengan indikator fault `!`. Kehilangan pembacaan sensor mematikan profil. Cooling saat ini bersifat pasif karena output fan belum digunakan.

> [!WARNING]
> Profil 120 °C ini hanya untuk proof-of-concept dan tidak cukup untuk proses solder reflow produksi. Batas 120 °C adalah batas **target**; inersia termal masih dapat menyebabkan overshoot, sehingga firmware menggunakan cutoff tambahan pada 125 °C. Tetap gunakan pengaman termal independen dan pengawasan operator.

### 5. Karakterisasi heater otomatis

Tahan tombol D minimal 1,5 detik pada halaman reflow untuk membuka **HEATER CHARACTERIZATION**. Mode ini memberikan duty PWM tetap agar kemampuan plant dapat diukur tanpa dipengaruhi ramp PID.

| Tombol | Fungsi pada halaman karakterisasi |
|---|---|
| A | Memulai pengujian; saat pengujian aktif, menghentikan pengujian dan mematikan heater |
| B | Menaikkan duty 25% |
| C | Menurunkan duty 25% |
| D ditahan ≥1,5 detik | Menghentikan pengujian dan membuka halaman uji motor DC |

Duty bawaan adalah 25% dan dapat dipilih menjadi 25%, 50%, 75%, atau 100% ketika pengujian tidak aktif. Pengujian hanya dapat dimulai jika thermistor sudah dikalibrasi, sensor valid, dan suhu awal ≤50 °C.

Urutan pengujian otomatis:

1. State **HEATING** menjalankan heater pada duty yang dipilih.
2. Firmware menghitung laju suhu setiap jendela 10 detik serta laju rata-rata sejak pengujian dimulai.
3. Pada suhu 110 °C, heater dimatikan dan state berpindah ke **COOLING**.
4. Firmware terus merekam suhu puncak dan overshoot setelah heater dimatikan.
5. State menjadi **COMPLETE** ketika suhu kembali hingga 3 °C di atas suhu awal.

Proteksi tambahan mematikan heater langsung ketika sensor menjadi invalid atau mencapai cutoff 110 °C. Pemanasan dibatasi maksimal 15 menit dan pencatatan cooling maksimal 30 menit. OLED menampilkan state, suhu, duty, laju saat ini, laju rata-rata, suhu puncak, overshoot, waktu, dan fault.

Saat terminal USB CDC terhubung, firmware otomatis mengirim sampel CSV setiap detik dengan header:

```text
elapsed_ms,state,duty_pct,temp_tenths_c,rate_milli_c_per_s,peak_tenths_c,overshoot_tenths_c,fault
```

Nilai suhu CSV menggunakan satuan sepersepuluh derajat Celsius; misalnya `875` berarti 87,5 °C. Laju menggunakan mili-°C/detik; `583` berarti 0,583 °C/detik. Kode fault: `0` normal, `1` sensor invalid, `2` belum dikalibrasi, `3` suhu awal terlalu tinggi, `4` timeout pemanasan, `5` timeout cooling, dan `6` dibatalkan operator.

> [!IMPORTANT]
> Mode karakterisasi mengurangi risiko kesalahan pencatatan, tetapi bukan pengaman kelistrikan atau termal independen. Gunakan thermal fuse/thermostat, sekering, isolasi, dan pemutus daya fisik yang sesuai.

### 6. Uji end-to-end conveyor, pemanasan, inspeksi, dan servo

Firmware conveyor diadaptasi dari `../sunda_reflow_oven/firmware/conveyor`. Versi sumber menargetkan STM32F411/CMSIS-RTOS; integrasi ini menggunakan STM32F103, native FreeRTOS API, task statis, encoder EXTI, dan timer yang tidak berbenturan dengan heater maupun OLED.

Halaman **CONVEYOR** dibuka otomatis saat boot. Halaman ini juga dapat dicapai dengan menahan D minimal 1,5 detik dari halaman karakterisasi untuk membuka **DC MOTOR TEST**, lalu menekan D sekali. OLED menampilkan state, duty motor, target speed, posisi/target encoder, sensor IR, pemilik mutex, pulse servo, nomor PCB, status inspeksi, serta penghitung PASS/FAIL.

| Tombol | Fungsi pada halaman conveyor |
|---|---|
| A pada `IDLE` | Memulai satu siklus otomatis |
| A saat siklus aktif | Abort: heater dan motor dimatikan, state menjadi `ESTOP` |
| A pada state fault | Acknowledge fault dan kembali ke `IDLE` |
| B/C pada `IDLE` | Menaikkan/menurunkan duty motor 5% |
| B/C saat `INSPECT` | Memasukkan hasil PASS/FAIL secara manual sebelum hasil otomatis dijalankan |
| D singkat pada `IDLE` | Mengembalikan servo ke posisi tengah |
| D ditahan ≥1,5 detik | Kembali ke halaman heater tanpa menghentikan siklus yang sedang berjalan |

Urutan uji end-to-end satu siklus:

1. `TO-MID`: conveyor mengambil mutex plant, menjalankan motor, lalu berhenti di tengah setelah target encoder 50 pulsa tercapai.
2. `HEAT-5S`: conveyor melepas mutex. Task thermal mengambil mutex heater dan menyalakan heater pada duty 25% selama 5 detik.
3. Setelah 5 detik, heater dimatikan dan task thermal melepas mutex.
4. `TO-END`: conveyor mengambil mutex lagi dan bergerak sampai sensor IR di ujung mendeteksi PCB. Motor kemudian berhenti.
5. `INSPECT`: task inspeksi dan jalur request USB CDC dijalankan. Untuk pengujian mandiri, hasil PASS otomatis diberikan setelah 500 ms jika belum ada hasil eksternal.
6. `SWIPE-R`: servo menyapu PCB ke kanan selama 500 ms, kembali ke posisi tengah, lalu state kembali `IDLE`.

Mutex plant hanya mempunyai satu pemilik pada satu waktu: `BELT`, `HEAT`, atau `FREE`. Output heater dipaksa mati ketika mutex sedang dimiliki conveyor. Uji pemanasan hanya dapat dimulai jika thermistor valid dan sudah dikalibrasi; sensor invalid, suhu mencapai 110 °C, atau timeout menghasilkan `HEAT-ERR` dan alur dihentikan.

Konfigurasi awal conveyor berada di `Core/Inc/conveyor_config.h`:

| Parameter | Nilai awal |
|---|---:|
| Duty motor | 70% |
| Deadband minimum | 30% |
| Target posisi heater | 50 pulse |
| Duty/durasi uji heater | 25% / 5 detik |
| Timeout gerak ke heater | 15 detik |
| Timeout mencari sensor IR | 30 detik |
| Hasil inspeksi otomatis | PASS setelah 500 ms |
| Servo kiri/tengah/kanan | 1000/1500/2000 µs |

#### Protokol inspeksi USB CDC

Frame menggunakan ASCII satu baris. Checksum adalah XOR semua karakter di antara `$` dan `*`, ditulis sebagai dua digit heksadesimal. Raspberry Pi berperan sebagai USB host; konektor USB Blue Pill hanya dapat terhubung ke satu host pada satu waktu, jadi PC dan Raspberry Pi tidak dapat memakai link CDC yang sama secara bersamaan.

STM32 meminta inspeksi:

```text
$INSPECT,id=1*7B
```

Raspberry Pi membalas salah satu:

```text
$RESULT,id=1,PASS*19
$RESULT,id=1,FAIL*0A
$RESULT,id=1,PASS,conf=0.94*1F
```

`id` hasil wajib sama dengan PCB yang sedang menunggu. Firmware menerima field tambahan setelah PASS/FAIL, sehingga confidence atau kode inspeksi dapat disertakan. Checksum balasan disarankan dan diverifikasi bila ada; frame tanpa checksum juga diterima untuk bring-up. Frame dengan checksum salah atau ID lama diabaikan. Pada mode uji end-to-end ini, respons valid yang tiba dalam 500 ms tetap digunakan; jika tidak ada respons, firmware membuat hasil PASS otomatis agar pengujian servo dapat selesai tanpa Raspberry Pi.

> [!WARNING]
> Motor dan servo tidak boleh disuplai dari pin 3,3 V Blue Pill. Gunakan driver dan supply terpisah dengan common ground, level logika yang aman, sekering, serta emergency stop fisik. Pastikan arah H-bridge benar sebelum menjalankan siklus karena firmware ini hanya mengatur duty, bukan arah.

## Optimasi RAM

Firmware menggunakan alokasi task FreeRTOS statis dan tidak menggunakan heap FreeRTOS dinamis. Software timer dan counting semaphore tetap dinonaktifkan; hanya satu mutex statis yang diaktifkan untuk interlock heater–conveyor. Jumlah level prioritas disesuaikan menjadi empat sambil mempertahankan urutan prioritas task. Pemeriksaan stack overflow level 2 tetap aktif dan memaksa output heater dan motor menjadi 0 jika overflow terdeteksi.

Buffer RX/TX USB CDC disesuaikan dengan ukuran maksimum paket Full Speed 64 byte, sedangkan ring buffer console 256 byte tetap dipertahankan. Riwayat grafik reflow tetap 128 sampel tetapi disimpan sebagai derajat terkuantisasi satu byte karena resolusi vertikal OLED hanya 32 piksel. Formatter teks ringan menggantikan `snprintf` pada UI dan console.

Hasil build Release setelah conveyor dan inspeksi ditambahkan:

| Kondisi | RAM | Persentase RAM 20 KiB |
|---|---:|---:|
| Sebelum optimasi | 17.304 byte | 84,49% |
| Setelah optimasi karakterisasi | 9.480 byte | 46,29% |
| Setelah conveyor + inspeksi | 10.840 byte | 52,93% |

Walaupun dua task statis, mutex, state machine conveyor, dan protokol inspeksi ditambahkan, penggunaan RAM masih 6.464 byte lebih rendah daripada kondisi awal 84,49%. Konfigurasi RAM ini berada pada file generated seperti `freertos.c`, `FreeRTOSConfig.h`, dan konfigurasi USB; periksa kembali perubahan tersebut jika project diregenerasi dengan STM32CubeMX.

## STM32 HID bootloader

Proyek menggunakan **STM32 HID Bootloader** dari Serasidis, bukan bootloader USB DFU bawaan ROM STM32. Bootloader tampil sebagai perangkat USB HID sehingga proses upload aplikasi tidak memerlukan driver USB khusus untuk HID pada sistem operasi modern.

Pembagian flash proyek:

| Alamat | Ukuran | Isi |
|---|---:|---|
| `0x08000000–0x080007FF` | 2 KiB | STM32 HID Bootloader |
| `0x08000800–0x0801FBFF` | 125 KiB | Firmware aplikasi |
| `0x0801FC00–0x0801FFFF` | 1 KiB | Data kalibrasi thermistor |

### Hasil pengujian kapasitas flash board

Board yang digunakan saat ini telah diuji langsung dan terbukti mempunyai **flash fisik 128 KiB**:

- Flash-size register pada `0x1FFFF7E0` melaporkan `128 KiB`.
- Halaman uji `0x0801F800` berhasil dihapus, ditulis dengan marker, dan diverifikasi.
- Halaman `0x0800F800` tidak berubah, sehingga area di atas 64 KiB bukan alias dari area bawah.
- Isi kedua halaman dicadangkan sebelum pengujian dan berhasil dipulihkan byte-for-byte setelah pengujian.
- Data kalibrasi lama pada `0x0800FC00` tetap utuh selama pengujian.

Linker project telah dikonfigurasi untuk menggunakan kapasitas fisik 128 KiB board ini. Karena konfigurasi tersebut melebihi kapasitas resmi sebagian STM32F103C8T6, firmware hasil build ini hanya boleh digunakan pada board yang telah dipastikan mempunyai flash fisik 128 KiB.

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

Command console tetap tidak dapat menyalakan heater atau mengubah PID. Frame `$RESULT` dengan ID PCB aktif hanya diterima ketika state conveyor sedang `INSPECTION`; efeknya terbatas pada pilihan arah servo sebelum fallback PASS otomatis dijalankan.

Data USB diterima oleh callback CDC dan dimasukkan ke ring buffer 256 byte. Task `cdcTask` memproses command tanpa melakukan pekerjaan berat di dalam interrupt USB. Panjang maksimum satu command adalah 95 karakter.

Selama karakterisasi heater aktif, task console juga mengirim satu baris CSV setiap detik. Saat inspeksi aktif, task yang sama mengirim frame `$INSPECT`. Program serial Raspberry Pi harus membuka DTR—perilaku bawaan `pyserial`—agar firmware menandai host terhubung dan mengirim request.

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
| `Core/Src/freertos.c` | Pembuatan task statis input, display, thermal, conveyor, inspeksi, dan console CDC |
| `Core/Src/cdc_console.c` | Ring buffer RX, console, parser command/frame inspeksi, dan transmisi USB CDC |
| `Core/Src/hardware_test.c` | State layar, tombol, ADC, heater, PID, serta UI reflow/karakterisasi/uji motor/conveyor |
| `Core/Src/conveyor*.c` | Motion primitive, sequencer LOAD→HEAT→INSPECT→SORT, dan integrasi task |
| `Core/Src/{encoder,motor,pcb_sensor,servo}.c` | Driver perangkat conveyor yang diadaptasi untuk STM32F103 |
| `Core/Src/inspection.c` | Task request/retry/timeout dan parser hasil inspeksi Raspberry Pi |
| `Core/Src/process_interlock.c` | Mutex statis pemisah ownership conveyor dan heater |
| `Core/Src/heater_characterization.c` | State machine karakterisasi, cutoff, perhitungan laju, overshoot, dan sampel CSV |
| `Core/Src/reflow.c` | State machine dan task profil preheat, soaking, reflow, serta cooling |
| `Core/Src/thermistor.c` | Model NTC, kalibrasi dua titik, validasi, dan penyimpanan flash |
| `Core/Src/ssd1306.c` | Driver OLED SSD1306 |
| `Core/Src/text_format.c` | Formatter teks ringan tanpa overhead `snprintf` |
| `USB_DEVICE/App/usbd_cdc_if.c` | USB CDC serta mekanisme reset menuju HID bootloader |
| `Core/Src/{adc,i2c,tim,usart,gpio}.c` | Konfigurasi peripheral hasil STM32CubeMX |
| `STM32F103xx_FLASH.ld` | Layout RAM/flash aplikasi dan reservasi kalibrasi |
| `test-bluepill-1.ioc` | Konfigurasi STM32CubeMX |
| `.zed/` | Task build, flash HID, size report, dan debug ST-Link |

Catatan: file `iwdg.c` tersedia dari konfigurasi generated code, tetapi `MX_IWDG_Init()` belum dipanggil oleh aplikasi sehingga watchdog belum aktif.

## Peringatan keselamatan

Proyek ini mengendalikan heater dan berpotensi berhubungan dengan tegangan listrik serta suhu tinggi. Gunakan SSR yang benar, sekering, grounding, isolasi, enclosure, emergency cutoff, dan proteksi termal independen. Jangan mengandalkan firmware sebagai satu-satunya lapisan keselamatan. Uji pertama kali dengan beban bertegangan rendah atau beban simulasi dan awasi output heater secara langsung.
