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
- Dua mode end to end: uji heater 5 detik dan siklus conveyor dengan profil suhu lengkap sebelum inspeksi serta sortir.
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
| Servo sorter | PA2 | TIM2 CH3 | PWM 50 Hz, 1 µs/tick; 1000/1500/2000 µs |
| Heater / SSR | PA6 | GPIO output | Aktif-high; time-proportioning software dengan jendela 1 detik |
| PWM fan 4-wire | PA7 | TIM3 CH2 | PWM 25 kHz open-drain; active braking PID dan cooling reflow |
| Encoder conveyor | PA8 | TIM1 CH1 | External clock rising-edge, pull-down, filter digital IC1F=`0xF` |
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

### Uji servo dari firmware utama

Di OLED, buka **Beranda → Diagnostik → Uji Servo**. Pilih **kiri (1000 µs)**,
**tengah (1500 µs)**, atau **kanan (2000 µs)** dengan **B/C**. Tekan **A**
untuk mengaktifkan servo; B/C lalu langsung mengubah posisinya. Tekan **A**
lagi untuk berhenti, atau **D** untuk kembali. Servo kembali ke tengah saat
uji dihentikan atau menu ditutup. Uji ini hanya dapat dimulai ketika proses
conveyor, motor manual, dan pemanasan sedang tidak aktif. Sinyal servo sekarang
keluar dari **PA2 (TIM2 CH3)**. Multimeter DC biasanya membaca rata-rata hanya
sekitar 0,17–0,33 V untuk pulsa 3,3 V berdurasi 1–2 ms setiap 20 ms; gunakan
osiloskop atau logic analyzer untuk memeriksa lebar pulsa dan perubahan posisi.

### Konsumsi stack task

Buka **Beranda → Diagnostik → Task Stack Consumption** (label menu OLED:
`TASK STACK CONSUM.`). Gunakan **B/C** untuk menggulir ketujuh task dan **D**
untuk kembali. Setiap baris menampilkan **puncak terpakai / alokasi** dalam byte
sejak boot. Puncak dihitung dari `uxTaskGetStackHighWaterMark()`; angka ini dapat
bertambah selama sistem berjalan dan hanya mencakup pola eksekusi yang sudah
terjadi. Rincian alokasi dan ID watchdog ada di
[`LAPORAN_WATCHDOG_TASK.md`](LAPORAN_WATCHDOG_TASK.md).

Saat boot, OLED membuka **Beranda** dan semua aktuator mati. Pilih menu dengan **B** (naik) dan **C** (turun), lalu tekan **A** untuk membuka. **D singkat** kembali; **D tahan minimal 1,5 detik** kembali ke Beranda atau menghentikan proses aktif. Saat proses aktif, D singkat menukar tampilan ringkas/detail tanpa menyembunyikan monitor. Petunjuk yang berlaku selalu ditampilkan di bagian bawah OLED.

| Menu | Fungsi |
|---|---|
| E2E Uji 5 Detik | Conveyor → pemanasan tetap 25% selama 5 detik → inspeksi → sortir; untuk uji singkat aktuator dan alur. |
| E2E Profil 120C POC | Conveyor → profil uji preheat, soaking, pemanasan 120 °C, cooling → inspeksi → sortir. Targetnya mengikuti menu Profil Suhu. Ini bukan reflow solder. |
| E2E Sn63 Terkunci | Rute reflow Sn63/Pb37 yang direncanakan; tombol mulai ditolak karena batas aman termal alat belum diuji dan suhu PCB belum dapat diukur. |
| Profil suhu | Atur target preheat, soaking, dan reflow saat idle; tinjau durasi, lalu jalankan profil tanpa conveyor. |
| Kalibrasi NTC | Ambil dua titik dengan suhu referensi, gunakan pemanas bantu bila perlu, lalu simpan ke flash setelah keduanya valid. |
| Kendali PID | Atur setpoint, Kp, Ki, Kd saat idle; jalankan dan pantau kontrol suhu. |
| Diagnostik | Karakterisasi heater, uji heater manual dengan cutoff, uji motor DC, dan uji servo. |
| Status alat | Lihat sensor, kalibrasi, vision, heater, fan, motor, dan interlock. |

### End to end

Kedua menu E2E yang dapat dijalankan memakai sequencer conveyor dan memerlukan thermistor terkalibrasi serta pembacaan sensor valid. **E2E Uji 5 Detik** memakai heater 25% selama 5 detik, dengan timeout tahap heater 10 detik. **E2E Profil 120C POC** menjalankan seluruh profil uji suhu, termasuk cooling hingga ≤50 °C, dengan timeout tahap heater 30 menit. Conveyor baru bergerak ke posisi inspeksi setelah pemanasan pada mode yang dipilih selesai tanpa fault. **E2E Sn63** tetap terkunci dan tidak mengaktifkan motor atau heater.

Saat inspeksi, tombol **B** dapat memberi hasil PASS dan **C** hasil FAIL; Raspberry Pi juga dapat mengirim keputusan lewat USB CDC. Tanpa keputusan yang valid hingga timeout, PCB disortir sebagai FAIL/REJECT. **A** menghentikan proses; **D tahan** juga menghentikan dan membuka pesan status. Fault motor, IR/PCB, atau heater ditampilkan sebelum pengguna melanjutkan.

Kecepatan conveyor bawaan 70%, dapat diubah 5% per langkah pada layar persiapan, dalam rentang 30–100%. Posisi heater 50 pulsa encoder serta nilai duty, timeout, dan servo pada `Core/Inc/conveyor_config.h` masih nilai awal POC yang harus disesuaikan dengan mekanik sebenarnya.

### Profil suhu

Target bawaan: preheat **60 °C**, soaking **90 °C**, reflow **120 °C**. Durasi pemanasan berturut-turut **90, 75, dan 75 detik**, lalu cooling hingga suhu **≤50 °C**. Editor target tersedia hanya saat idle; firmware menjaga `preheat < soaking < reflow` dengan jarak minimal 5 °C, batas preheat minimal 40 °C, dan puncak reflow maksimal 120 °C. Grafik suhu dan target tampil selama proses. Nilai target kembali ke bawaan setelah reset.

Target pada menu ini juga dipakai **E2E Profil 120C POC**. Profil 120 °C adalah profil POC untuk alat saat ini, **belum profil solder produksi**; batas keselamatan profil 125 °C tetap berlaku.
Perpindahan tahap POC masih berdasarkan waktu; firmware belum mensyaratkan suhu target benar-benar tercapai sebelum lanjut. Karena itu hasil E2E POC tidak menyatakan solder pada PCB sudah meleleh.

### Kandidat profil solder Sn63/Pb37

Sn63/Pb37 melebur pada **183 °C**. Sebagai contoh untuk **pasta AIM NC293+** (belum dipastikan sebagai pasta yang digunakan), datasheet menyebut ramp awal 1,4–1,8 °C/detik menuju 150 °C, soak **150–170 °C selama 30–60 detik**, puncak **215 ± 5 °C**, waktu PCB di atas 183 °C **45 ± 15 detik**, dan pendinginan maksimal 4 °C/detik. Nilai ini adalah acuan pengembangan, **bukan setelan aktif firmware**. Resep akhir harus mengikuti datasheet pasta yang benar-benar dipakai dan batas suhu komponen PCB.

**Status alat: belum diuji.** Suhu maksimum aman heater, sensor, PCB, dan pelindung termal belum diverifikasi. **NTC PID terpasang pada pelat; tidak ada termokopel PCB.** Maka pembacaan NTC bukan suhu solder/PCB dan tidak dapat dipakai untuk menyatakan berapa lama sambungan solder berada di atas 183 °C. Catatan karakterisasi yang tersedia hanya menunjukkan puncak pelat sekitar 149 °C; itu tidak membuktikan alat bisa mencapai atau mempertahankan 215 °C, maupun membuktikan suhu PCB mengikuti pelat. Karena itu menu E2E Sn63 menolak start.

Sebelum profil solder produksi dapat diaktifkan, ukur suhu PCB pada papan uji dengan termokopel yang terpasang di titik representatif sambil mencatat suhu pelat; tentukan selisih dan keterlambatan panas pada beberapa kondisi. Verifikasi batas aman seluruh komponen, kemampuan ramp/peak/cooling, serta proteksi termal independen. Setelah itu implementasikan dan uji logika tahap terhadap pengukuran suhu PCB dan waktu di atas liquidus, sesuai datasheet pasta yang benar-benar digunakan. Termokopel eksternal dapat dipakai untuk validasi resep; bila alat produksi tetap hanya memiliki NTC pelat, firmware tidak dapat mengetahui suhu PCB setiap siklus secara langsung. Menaikkan setpoint dan cutoff pelat saja tidak memenuhi syarat tersebut.

Rujukan profil: [AIM Sn63/Pb37](https://www.aimsolder.com/products/sn63-pb37-leaded-solder-alloy/), [datasheet AIM NC293+](https://www.aimsolder.com/wp-content/uploads/legacy-files/nc293_sn_pb_solder_paste_tds.pdf), dan [panduan profil PCB AIM](https://www.aimsolder.com/white-paper/reflow-profiling-in-soldering-and-pcb-assembly/).

### Merekam pelat untuk uji dengan termokopel PCB eksternal

Firmware sudah mengirim frame `$STAT` lewat USB CDC setiap 500 ms. Kolom `pv` adalah **suhu NTC pelat**, `up` adalah waktu STM32 dalam milidetik, dan `zone` menunjukkan tahap termal. Skrip baca saja berikut menyimpan frame valid beserta waktu komputer; skrip tidak mengirim perintah ke firmware:

```bash
python3 -m pip install pyserial
python3 tools/log_plate.py /dev/ttyACM0 plate.csv
# Windows: python tools/log_plate.py COM3 plate.csv
```

Tekan `Ctrl-C` untuk mengakhiri rekaman. Simpan log termokopel PCB dari instrumen eksternal secara terpisah, dengan waktu mulai atau penanda yang dapat dicocokkan ke `host_utc`/`stm_up_ms`. Port USB hanya dapat dipakai satu host pada satu waktu; bila PC merekam profil, inspeksi Raspberry Pi tidak tersambung dan hasil inspeksi bisa dimasukkan manual lewat tombol. Perekam ini juga dapat dipakai saat **Profil Suhu POC** tanpa conveyor.

Untuk pengambilan data awal, gunakan hanya mode dan batas suhu firmware saat ini. Pasang termokopel pada titik PCB yang mewakili sambungan solder; bila mungkin ukur juga titik yang diperkirakan paling panas dan paling dingin. Catat tipe PCB, posisi sensor, suhu pelat dan PCB terhadap waktu, selisih suhu, serta keterlambatan pemanasan/pendinginan. Data pada rentang POC **tidak boleh diekstrapolasi sebagai bukti aman pada 215 °C**. Pengujian suhu tinggi menunggu verifikasi batas termal alat dan komponen serta profil pasta yang benar.

### Kalibrasi NTC

Gunakan termometer referensi yang tepercaya dan ambil dua titik yang berjauhan. Pilih Titik 1/Titik 2, sesuaikan suhu referensi dengan **B/C** sebesar 0,1 °C per tekan, lalu **A** untuk mengambil data ADC. Pemanas bantu mempunyai layar terpisah; heater mati saat layar itu ditinggalkan. Menyimpan memerlukan dua titik yang valid, dan OLED menampilkan sukses hanya setelah penulisan flash berhasil. Data kalibrasi disimpan pada halaman flash terakhir `0x0801FC00–0x0801FFFF`.

### PID dan diagnostik

Setpoint PID bawaan **70 °C**, rentang **20–140 °C**, berubah 0,5 °C per langkah. Gain bawaan adalah Kp **1,50**, Ki **0,035**, Kd **14,00**. Gain dapat diedit saat idle dan berlaku untuk sesi saat ini; reset mengembalikannya ke bawaan. PID memerlukan kalibrasi dan sensor valid, dengan batas keselamatan heater **150 °C**. Kontrol memiliki ramp setpoint, filter derivatif, pembatas integral, feed-forward, dan pengereman fan.

Uji heater manual di Diagnostik kini memakai cutoff **110 °C** dan mematikan heater bila sensor gagal. Duty dapat dipilih 25–100% saat heater mati. Karakterisasi heater memakai cutoff 110 °C dan menghasilkan log CSV melalui USB CDC. Uji motor DC mempunyai duty awal 40%, rentang 30–100%, serta menampilkan pulsa encoder; keluar dari layar uji menghentikan motor.

Rancangan alur layar dan animasi status ada di [DESAIN_MENU_OLED.md](DESAIN_MENU_OLED.md).

## Optimasi RAM

Firmware menggunakan alokasi task FreeRTOS statis dan tidak menggunakan heap FreeRTOS dinamis. Software timer dan counting semaphore tetap dinonaktifkan; hanya satu mutex statis yang diaktifkan untuk interlock heater–conveyor. Jumlah level prioritas disesuaikan menjadi empat sambil mempertahankan urutan prioritas task. Pemeriksaan stack overflow level 2 tetap aktif dan mematikan heater serta motor sambil menjalankan fan 100% jika overflow terdeteksi.

Buffer RX/TX USB CDC disesuaikan dengan ukuran maksimum paket Full Speed 64 byte, sedangkan ring buffer console 256 byte tetap dipertahankan. Riwayat grafik reflow tetap 128 sampel tetapi disimpan sebagai derajat terkuantisasi satu byte karena resolusi vertikal OLED hanya 32 piksel. Formatter teks ringan menggantikan `snprintf` pada UI dan console.

Hasil build Release setelah conveyor dan inspeksi ditambahkan:

| Kondisi | RAM | Persentase RAM 20 KiB |
|---|---:|---:|
| Sebelum optimasi | 17.304 byte | 84,49% |
| Setelah optimasi karakterisasi | 9.480 byte | 46,29% |
| Setelah protokol Pi + PID/fan | 11.600 byte | 56,64% |

Walaupun dua task statis, mutex, state machine conveyor, protokol inspeksi, dan buffer frame 160 byte ditambahkan, penggunaan RAM masih 5.704 byte lebih rendah daripada kondisi awal 84,49%. Konfigurasi RAM ini berada pada file generated seperti `freertos.c`, `FreeRTOSConfig.h`, dan konfigurasi USB; periksa kembali perubahan tersebut jika project diregenerasi dengan STM32CubeMX.

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
| `status` | Membaca layar aktif, heater, duty heater/fan, ADC, suhu, resistansi, kalibrasi, PID, dan setpoint |
| `echo <teks>` | Mengirim kembali teks ke host |

Command console tetap tidak dapat menyalakan heater atau mengubah PID. Frame `$RES` dengan ID PCB aktif hanya diterima ketika state conveyor sedang `INSPECTION`; efeknya terbatas pada pilihan arah servo.

Data USB diterima oleh callback CDC dan dimasukkan ke ring buffer 256 byte. Task `cdcTask` memproses command tanpa melakukan pekerjaan berat di dalam interrupt USB. Buffer input satu baris berukuran 160 byte agar frame protokol penuh dapat diterima.

Selama karakterisasi heater aktif, task console juga mengirim satu baris CSV setiap detik. Task yang sama adalah satu-satunya penulis ke USB: ia juga mengirim frame protokol Raspberry Pi (`$HELLO`, `$STAT`, `$DET`, `$ACK`, `$SORT`, `$EVT`). Program serial Raspberry Pi harus membuka DTR—perilaku bawaan `pyserial`—agar firmware menandai host terhubung dan mulai mengirim.

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
| `Core/Src/inspection.c` | Link inspeksi Raspberry Pi: settle, `$DET`/retry, timeout → REJECT, `$SORT`/`$EVT`/`$STAT`, parser `$RES`/`$ACK`/`$HB` |
| `Core/Src/pi_protocol.c` | Build/parse frame `$TYPE,key=value,...*CS` (C murni, dapat diuji di PC) |
| `tests/host/` | Uji host `inspection.c` dengan jam palsu dan Raspberry Pi tiruan |
| `Core/Src/process_interlock.c` | Mutex statis pemisah ownership conveyor dan heater |
| `Core/Src/heater_characterization.c` | State machine karakterisasi, cutoff, perhitungan laju, overshoot, dan sampel CSV |
| `Core/Src/reflow.c` | State machine dan task profil preheat, soaking, reflow, serta cooling |
| `Core/Src/thermistor.c` | Model NTC, kalibrasi dua titik, validasi, dan penyimpanan flash |
| `Core/Src/ssd1306.c` | Driver OLED SSD1306 |
| `Core/Src/text_format.c` | Formatter teks ringan tanpa overhead `snprintf` |
| `tools/log_plate.py` | Pencatat `$STAT` valid ke CSV dengan waktu STM32 dan waktu komputer untuk dibandingkan dengan termokopel PCB eksternal |
| `USB_DEVICE/App/usbd_cdc_if.c` | USB CDC serta mekanisme reset menuju HID bootloader |
| `Core/Src/{adc,i2c,tim,usart,gpio}.c` | Konfigurasi peripheral hasil STM32CubeMX |
| `STM32F103xx_FLASH.ld` | Layout RAM/flash aplikasi dan reservasi kalibrasi |
| `test-bluepill-1.ioc` | Konfigurasi STM32CubeMX |
| `.zed/` | Task build, flash HID, size report, dan debug ST-Link |

IWDG aktif pada firmware normal. Idle task hanya me-refresh IWDG bila heartbeat keenam task aplikasi dan idle task masih tepat waktu. Rincian ID, periode, batas heartbeat, prioritas, dan alokasi stack tersedia di [`LAPORAN_WATCHDOG_TASK.md`](LAPORAN_WATCHDOG_TASK.md).

## Peringatan keselamatan

Proyek ini mengendalikan heater dan berpotensi berhubungan dengan tegangan listrik serta suhu tinggi. Gunakan SSR yang benar, sekering, grounding, isolasi, enclosure, emergency cutoff, dan proteksi termal independen. Jangan mengandalkan firmware sebagai satu-satunya lapisan keselamatan. Uji pertama kali dengan beban bertegangan rendah atau beban simulasi dan awasi output heater secara langsung.
