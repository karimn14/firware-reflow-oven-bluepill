# Rancangan menu OLED 128 × 64

**Status:** rancangan yang menjadi dasar implementasi menu firmware. Menu E2E tersedia sebagai **E2E Uji 5 Detik**, **E2E Profil 120C POC**, dan **E2E Sn63 Terkunci**. Yang terakhir belum bisa dijalankan karena batas aman termal alat belum diuji. **Target perangkat:** SSD1306 monokrom, font ASCII 5 × 7, empat tombol A/B/C/D. Isi contoh layar di bawah dibatasi 21 karakter per baris dan 8 baris. Angka pada contoh adalah ilustrasi tampilan, bukan pembacaan alat; tata letak firmware dapat sedikit berbeda agar status aktual muat.

## 1. Menu lama sebelum perubahan

Sebelum implementasi rancangan ini, firmware membuka layar **CONVEYOR** saat boot. Tidak ada menu utama: pengguna berpindah dalam rangkaian layar berikut.

```text
CONVEYOR -(D tahan)-> HEATER -(D)-> KALIBRASI
KALIBRASI -(D tahan)-> PID -(D)-> REFLOW
REFLOW -(D tahan)-> KARAKTERISASI
KARAKTERISASI -(D tahan)-> MOTOR TEST
MOTOR TEST -(D)-> CONVEYOR
```

Urutan lengkap dari CONVEYOR adalah: tahan D → HEATER → D → KALIBRASI → tahan D → PID → D → REFLOW → tahan D → KARAKTERISASI → tahan D → MOTOR TEST → D → CONVEYOR.

| Layar | Isi OLED sekarang | Tombol sekarang |
|---|---|---|
| Conveyor, layar awal | Status sequencer, motor, encoder, IR, interlock, servo, hasil inspeksi, jumlah pass/fail, koneksi vision | A mulai/abort/akui fault; B/C ubah kecepatan saat idle atau putuskan pass/fail saat inspeksi; D singkat pusatkan servo; D tahan pindah ke heater |
| Heater manual | SSR, duty, ADC, mV, suhu NTC, status kalibrasi | A hidup/mati; B/C duty ±25%; D ke kalibrasi |
| Kalibrasi thermistor | ADC, resistansi, suhu terukur, suhu referensi, P1/P2, heater | A ambil titik; B/C referensi ±0,1 °C; D singkat heater hidup/mati; D tahan simpan jika dua titik valid dan pindah ke PID |
| PID suhu | Suhu aktual, setpoint, ramp, heater/fan, status, komponen P/I/D, kalibrasi | A mulai/stop; B/C setpoint ±0,5 °C; D stop dan pindah ke reflow |
| Reflow | Tahap, suhu, waktu, target P/S/R, grafik, heater/fan | A mulai/stop; B/C target terpilih ±1 °C saat idle; D singkat pilih tahap; D tahan stop dan pindah ke karakterisasi |
| Karakterisasi heater | Duty, laju pemanasan, rata-rata, puncak, overshoot, waktu, fault | A mulai/stop; B/C duty ±25%; D tahan ke uji motor |
| Motor DC manual | Status motor, PWM, pulsa encoder | A mulai/stop; B/C duty ±10%; D ke conveyor |

**Temuan yang memengaruhi rancangan:**

- Petunjuk tombol hanya muncul di bagian bawah tiap layar; perpindahan layar melalui D tidak membentuk struktur menu yang mudah ditemukan.
- Pada layar conveyor, D tahan dapat pindah layar saat siklus masih aktif. Operator lalu kehilangan tampilan proses yang sedang berjalan. Rancangan baru menahan navigasi keluar selama proses aktif.
- “P/I/D” pada layar PID adalah **kontribusi kendali saat itu**, bukan nilai Kp/Ki/Kd. Nilai Kp=1,5, Ki=0,035, Kd=14,0 masih konstanta di kode dan belum dapat diubah melalui OLED.
- “End to end” saat ini ialah uji **conveyor → heater 25% selama 5 detik → inspeksi → sortir**, bukan reflow bertahap. Profil reflow terpisah memiliki target 60/90/120 °C dan durasi 90/75/75 detik, lalu cooling sampai ≤50 °C.
- Target profil dapat diubah hanya saat idle, mengikuti P < S < R dengan jarak minimum 5 °C, preheat minimum 40 °C, dan reflow maksimum 120 °C. Target kembali ke nilai bawaan setelah reset.
- Uji heater manual saat ini tidak memiliki cutoff suhu otomatis. Sebelum mode itu disajikan di menu diagnostik baru, implementasi perlu menambahkan batas suhu dan penanganan sensor gagal yang mematikan heater.
- Uji end to end dan reflow/PID memerlukan thermistor terkalibrasi serta sensor valid. Jika Raspberry Pi/vision tidak memberi hasil inspeksi, jalur saat ini berakhir sebagai reject/fail berdasarkan timeout.

## 2. Prinsip navigasi yang diusulkan

| Tombol | Di daftar/menu | Di layar nilai | Saat proses aktif |
|---|---|---|---|
| A singkat | Buka pilihan | Ambil/simpan nilai, sesuai label layar | Stop proses langsung; pada fault, akui fault |
| B singkat/tahan | Pilihan sebelumnya | Naikkan nilai; tahan mengulang | Hanya bila label layar memberi aksi, misalnya PASS saat inspeksi |
| C singkat/tahan | Pilihan berikutnya | Turunkan nilai; tahan mengulang | Hanya bila label layar memberi aksi, misalnya FAIL saat inspeksi |
| D singkat | Kembali satu tingkat | Kembali tanpa mengubah nilai yang belum dikonfirmasi | Ganti tampilan ringkas/detail; tidak meninggalkan monitor proses |
| D tahan ≥1,5 s | Kembali ke Beranda | Kembali ke Beranda setelah membatalkan edit | **Hentikan proses**, matikan output terkait, buka layar hasil “Dihentikan” |

Setiap layar memakai pola tetap: **judul/status** di atas, **informasi atau pilihan** di tengah, dan **petunjuk tombol yang benar untuk keadaan saat itu** pada baris terbawah. Pilihan aktif diberi `>`; nilai yang sedang diedit diberi `[ ]`. Layar daftar bergulir bila isi melebihi ruang. Saat tombol tidak berlaku, labelnya tidak ditampilkan. Batas nilai dan alasan proses gagal ditampilkan sebagai pesan, bukan perubahan yang diam-diam diabaikan.

## 3. Peta menu baru

```text
BERANDA
├─ E2E Uji 5 Detik
│  ├─ Persiapan dan cek syarat
│  ├─ Konfirmasi mulai
│  ├─ Monitor: muat → panas 5 s → inspeksi → sortir
│  └─ Hasil / fault
├─ E2E Profil 120C POC
│  ├─ Persiapan: target P/S/R dari menu Profil Suhu
│  ├─ Konfirmasi mulai
│  ├─ Monitor: conveyor → profil lengkap → inspeksi → sortir
│  └─ Hasil / fault
├─ E2E Sn63 Terkunci
│  ├─ Informasi alloy dan status uji termal
│  └─ Pesan: batas aman belum diuji; start ditolak
├─ Profil suhu
│  ├─ Atur target: preheat, soaking, reflow
│  ├─ Tinjau durasi dan batas suhu
│  ├─ Mulai profil
│  └─ Monitor: preheat → soaking → reflow → cooling
├─ Kalibrasi NTC
│  ├─ Titik 1: atur referensi, ambil data
│  ├─ Titik 2: atur referensi, ambil data
│  ├─ Pemanas bantu kalibrasi
│  └─ Simpan dua titik valid
├─ Kendali PID
│  ├─ Atur setpoint
│  ├─ Atur Kp, Ki, Kd
│  └─ Mulai / monitor PID
├─ Diagnostik
│  ├─ Karakterisasi heater
│  ├─ Uji heater manual
│  └─ Uji motor DC
└─ Status alat
   ├─ Sensor, kalibrasi, fan/heater
   └─ Vision, IR, interlock
```

**Penamaan proses:** “E2E Uji 5 Detik” menjalankan uji singkat 5 detik. “E2E Profil 120C POC” menjalankan seluruh profil uji suhu lalu melanjutkan ke inspeksi dan sortir. “Profil Suhu” menjalankan profil yang sama secara terpisah tanpa conveyor. “E2E Sn63 Terkunci” menampilkan alasan start belum diizinkan. Profil bawaan berpuncak 120 °C dan masih POC, bukan profil solder produksi.

## 4. Mockup layar ASCII

Setiap blok mewakili **8 baris OLED**, tanpa garis bingkai agar ruang layar tidak terbuang. Label `A:`, `B:`, `C:`, `D:` adalah teks yang benar-benar diusulkan tampil. `D!` berarti tahan D ≥1,5 detik. Satuan `C` pada layar berarti °C karena font sekarang hanya mendukung ASCII.

### 4.1 Beranda dan daftar

```text
REFLOW CTRL     READY
PEL 28.4C  CAL:OK
> E2E UJI 5 DETIK
  E2E PROFIL 120C POC
  E2E SN63 TERKUNCI
  PROFIL SUHU
  KALIBRASI NTC  v
B:^ C:v A:BUKA
```

Baris kedua berubah menjadi `SENSOR ERROR`, `CAL:PERLU`, atau status fault bila syarat belum terpenuhi. Daftar digulir untuk membuka **Diagnostik** dan **Status alat**. Seleksi terakhir dapat dipertahankan selama alat menyala; setelah boot fokus pertama adalah “E2E Uji 5 Detik”.

### 4.2 End to end: persiapan, monitor, hasil

```text
END TO END  1/2
BELT>HEAT>INSP>SORT
HEAT 25% / 5 DETIK
SENSOR:OK  CAL:OK
VISION:OK  IR:SIAP
SPEED:70%  B/C UBAH
A:LANJUT D:KEMBALI
D!:BERANDA
```

```text
END TO END  2/2
SPEED 70%  HEAT 5S
HASIL INSPEKSI:
VISION ATAU TOMBOL
JIKA TIMEOUT: FAIL
OUTPUT AWAL: MATI
A:MULAI D:KEMBALI
D!:BERANDA
```

```text
END TO END   RUN
TAHAP 2/4: PANAS
SUHU 64.2C H:25%
WAKTU PANAS 03/05S
PCB #12  MOTOR:OFF
BERIKUT: INSPEKSI
A:STOP D:DETAIL
D!:HENTIKAN
```

Saat tahap inspeksi, baris tindakan berubah menjadi `B:PASS C:FAIL`, dan status vision ditampilkan. Bila vision menjawab lebih dahulu, hasilnya ditampilkan tanpa memerlukan tombol. Bila timeout, layar hasil menunjukkan `FAIL/TIMEOUT` secara jelas. Setelah sortir, layar hasil menunjukkan PASS/FAIL dan `A:ULANG D:BERANDA`. Pada fault tampil penyebab, output mati, lalu `A:AKUI D:BERANDA`.

**Mode E2E Profil 120C POC** menampilkan target P/S/R pada persiapan. Saat conveyor berhenti di heater, baris tengah monitor menampilkan tahap aktif (`PREHEAT`, `SOAKING`, `REFLOW`, `COOLING`), target, suhu aktual, dan waktu. Conveyor menunggu cooling selesai sebelum bergerak ke inspeksi. Targetnya diubah lewat menu **Profil Suhu** sebelum mulai. **E2E Sn63 Terkunci** menampilkan NTC di pelat, suhu PCB yang belum diukur, serta batas termal yang belum diuji. Tombol `A:ALASAN` membuka keterangan; start tidak tersedia.

```text
E2E SN63 2/2
SN63/PB37 CAIR 183C
NTC DI PELAT
SUHU PCB:TAK DIUKUR
BATAS TERMAL:BELUM UJI
START TERKUNCI
A:ALASAN
D:KEMBALI
```

### 4.3 Profil suhu: setelan, editor, monitor

```text
PROFIL SUHU  IDLE
> PREHEAT     60C
  SOAKING     90C
  REFLOW     120C
  TINJAU & MULAI
BATAS: P<S<R
B:^ C:v A:UBAH
D:BERANDA
```

```text
ATUR PREHEAT
TARGET [ 60 C ]
BATAS 40..85C
SOAKING 90C
LANGKAH 1C
HANYA SAAT IDLE
B:+ C:- A:SIMPAN
D:BATAL
```

```text
PROFIL   PREHEAT
SUHU 54.3C  SP 60C
WAKTU 01:12/01:30
HEATER 32% FAN 0%
P60 S90 R120
GRAFIK: ~~~/----
A:STOP D:DETAIL
D!:HENTIKAN
```

Di implementasi, area `GRAFIK` memakai garis piksel seperti layar reflow saat ini; teks ASCII hanya menunjukkan letaknya. Pada SOAKING/REFLOW target dan penghitung waktu berubah mengikuti tahap. Pada COOLING tampil `HEATER OFF`, `FAN 100%`, serta target akhir `<=50C`. B/C tidak mengubah suhu ketika profil berjalan. Durasi 90/75/75 detik ditampilkan pada layar “Tinjau & mulai”; editor durasi belum ada pada firmware dan tidak dicakup rancangan ini.

### 4.4 Kalibrasi NTC: langkah dan penyimpanan

```text
KALIBRASI NTC
> TITIK 1   BELUM
  TITIK 2   BELUM
  PEMANAS BANTU
  SIMPAN KALIBRASI
P1/P2 HARUS VALID
B:^ C:v A:BUKA
D:BERANDA
```

```text
KAL NTC  TITIK 1
TERUKUR 28.4C
REF    [ 28.4C ]
ADC 1842  STABIL?
P1:--.- P2:--.-
RUJUK TERMOMETER
B:+.1 C:-.1
A:AMBIL D:KEMBALI
```

```text
KAL NTC   SIAP
P1 30.0C  P2 90.0C
SELISIH: VALID
HEATER: OFF
DATA AKAN DISIMPAN
KE FLASH
A:SIMPAN
D:KEMBALI
```

Pemanas bantu menjadi layar tersendiri dengan status suhu, duty, dan `A:ON/OFF B:+ C:- D:KEMBALI`; kembali dari layar itu selalu mematikan heater. Batas kalibrasi saat ini 150 °C tetap berlaku. Tombol “Simpan kalibrasi” aktif hanya jika dua titik diterima oleh validasi thermistor; keberhasilan atau kegagalan penulisan flash wajib diberi pesan. Keluar dengan data belum lengkap tidak menyimpan perubahan.

### 4.5 PID: setpoint, parameter, monitor

```text
KENDALI PID  IDLE
> SETPOINT   70.0C
  KP          1.50
  KI         0.035
  KD         14.00
  MULAI PID
B:^ C:v A:UBAH
D:BERANDA
```

```text
ATUR KI
NILAI [ 0.035 ]
LANGKAH 0.001
BAWAAN 0.035
SESI INI SAJA
PID HARUS IDLE
B:+ C:- A:SIMPAN
D:BATAL
```

```text
PID SUHU     RUN
AKTUAL 66.2C
TARGET 70.0C
RAMP   68.0C
HEATER 28% FAN 0%
SENSOR OK  <150C
A:STOP D:DETAIL
D!:HENTIKAN
```

Nilai Kp/Ki/Kd **belum dapat diubah saat ini**; ini adalah perubahan firmware yang diperlukan saat implementasi. Untuk rancangan awal, nilai berlaku selama sesi menyala dan kembali ke bawaan setelah reset, sama seperti target profil. Editor hanya tersedia ketika PID dan profil suhu idle. Rentang aman dan langkah tiap gain perlu ditetapkan saat implementasi bersama uji kendali; tampilan contoh Ki di atas hanya menunjukkan format. Layar detail menampilkan komponen P/I/D aktual dan status fan, terpisah dari editor gain.

### 4.6 Diagnostik, status, dan fault

```text
DIAGNOSTIK
> KARAKTER HEATER
  UJI HEATER MANUAL
  UJI MOTOR DC
AKSI DAPAT GERAKKAN
MOTOR/PEMANAS
B:^ C:v A:BUKA
D:BERANDA
```

```text
STATUS ALAT
NTC 28.4C  CAL:OK
ADC 1842   IR:NO
VISION:OK  USB:OK
HEATER:OFF FAN:0%
MOTOR:OFF LOCK:FREE
D:BERANDA

```

```text
! TIDAK BISA MULAI
SENSOR NTC TIDAK
VALID / TERPUTUS
HEATER & MOTOR OFF
PERIKSA SENSOR
LALU COBA LAGI
A:AKUI
D:BERANDA
```

Mode heater manual, karakterisasi, dan uji motor mempertahankan fungsi dasarnya, tetapi mengikuti tata letak yang sama: status besar, nilai utama, batas keselamatan, serta aksi tombol yang jelas. Contoh fault harus berubah sesuai sebab sebenarnya: belum kalibrasi, sensor rusak, interlock sibuk, suhu terlalu tinggi, motor/encoder timeout, atau inspeksi timeout. Jangan tampilkan `VISION:OK` sebagai syarat wajib bila inspeksi manual B/C atau fallback timeout memang diizinkan.

### 4.7 Animasi kecil yang membantu operator

OLED ini monokrom 128 × 64 dan layar sekarang diperbarui setiap 50 ms. Animasi yang cocok adalah **gerak 2–4 frame di area kecil**, bukan pergantian seluruh layar. Judul, angka suhu, dan petunjuk tombol tetap diam agar mudah dibaca. Bentuk akhir dapat digambar dengan piksel; ASCII berikut menunjukkan urutan geraknya.

| Tempat | Gerakan yang diusulkan | Makna dan waktu |
|---|---|---|
| Beranda/daftar | Kursor `>` bergeser satu baris; pilihan aktif diberi garis tipis | Sekitar 120 ms setelah B/C, lalu berhenti. Saat tombol ditahan, kursor langsung mengikuti pilihan agar tetap responsif. |
| End to end | Empat segmen `BELT > HEAT > INSP > SORT`; segmen aktif berkedip halus atau terisi bergantian | Menunjukkan posisi siklus, perubahan hanya saat tahap berganti; tiga titik kecil bergerak saat menunggu hasil vision. |
| Profil suhu | Garis suhu aktual terus bertambah seperti grafik saat ini; garis target putus-putus tetap | Informasi proses yang paling berguna. Di samping nama tahap, indikator kecil 3 frame bergerak saat pemanasan/cooling. |
| Penyunting nilai | Kurung `[ ]` pada angka yang sedang diedit menyala redup/normal bergantian | Sekitar 500 ms per keadaan, hanya saat edit aktif. Nilai tidak pernah disembunyikan. |
| Kalibrasi | Setelah titik diambil, kotak `P1`/`P2` terisi singkat lalu tanda centang piksel muncul | Umpan balik 300 ms, disusul status `TERAMBIL`; penyimpanan flash baru diikuti `TERSIMPAN` setelah berhasil. |
| Hasil dan fault | Hasil PASS/FAIL muncul lewat sapuan 2–3 langkah; fault muncul langsung dan tetap | Animasi hasil maksimal 300 ms. Fault dan perintah `A:AKUI` tidak berkedip atau tertunda. |

**Storyboard end to end** — hanya bagian tengah layar yang berubah; angka dan footer tetap:

```text
BELT>HEAT>INSP>SORT
[##] [  ] [  ] [  ]
```

```text
BELT>HEAT>INSP>SORT
[##] [.:] [  ] [  ]
```

```text
BELT>HEAT>INSP>SORT
[##] [##] [.:] [  ]
```

Isian `#` menunjukkan tahap selesai; `.:` bergantian menjadi `:.` untuk menunjukkan tahap aktif/menunggu. Setelah SORT, layar hasil menampilkan keputusan akhir. Saat suhu/profil berjalan, indikator kecil dapat memakai tiga frame piksel `.` → `:` → `*` di samping judul; bukan animasi api yang bisa disalahartikan sebagai heater aktif ketika output sebenarnya nol.

**Aturan gerak:** animasi berhenti saat pengguna menekan tombol agar respons terasa langsung; tidak boleh menghalangi pembacaan suhu atau aksi STOP. Jangan animasikan seluruh framebuffer tiap 50 ms semata-mata untuk dekorasi. Layar statis tetap lengkap bila satu atau beberapa frame terlewat. Jika pengiriman OLED melalui I2C membuat task sibuk, kurangi animasi menjadi 5–10 frame per detik tanpa mengurangi frekuensi pembacaan atau kendali suhu.

## 5. Perilaku yang harus dipenuhi saat implementasi

1. Setelah boot tampil Beranda dengan semua aktuator mati. Tidak ada proses otomatis dimulai hanya karena layar dipilih.
2. Sebelum A benar-benar memulai end to end, profil suhu, PID, atau diagnostik yang menggerakkan aktuator, tampilkan ringkasan tindakan dan status syaratnya. Kegagalan memulai menampilkan alasan spesifik.
3. Hanya satu proses pemilik plant yang boleh aktif. Semua layar operasi harus membaca state proses yang nyata, bukan hanya state layar. Saat proses aktif, D singkat tidak boleh menyembunyikan monitor; D tahan menghentikan aktuator melalui jalur abort yang benar.
4. Perubahan suhu profil dan gain PID hanya saat idle. Validasi target P/S/R tetap mengikuti batas dan jarak tahap yang sudah ada. Tidak ada perubahan diam-diam ketika tombol ditekan pada batas nilai.
5. Kalibrasi dua titik hanya disimpan setelah keduanya valid. Saat keluar, berhenti, fault sensor, atau suhu melampaui batas, pemanas bantu harus mati. Pesan sukses baru muncul setelah penulisan flash berhasil.
6. Footer dan istilah mengikuti konteks: `MULAI`, `STOP`, `AKUI`, `AMBIL`, `SIMPAN`, `BATAL`, `KEMBALI`. Hindari satu tombol yang diberi label `NEXT` padahal sedang mengganti target atau berpindah mode.
7. Teks layar maksimal 21 karakter per baris dan 8 baris dengan font saat ini. Elemen visual seperti highlight pilihan dan grafik boleh digambar memakai piksel, tetapi petunjuk tombol tetap terbaca.
8. Uji heater manual di Diagnostik memerlukan cutoff otomatis dan pemadaman ketika sensor tidak valid. Batas tersebut harus ditegakkan oleh logika kendali, bukan hanya ditulis di layar.
9. Animasi hanya menghias status yang sudah benar. Fault, STOP, nilai suhu, dan petunjuk tombol harus terlihat tanpa menunggu frame animasi berikutnya.

## 6. Catatan implementasi

Firmware telah memakai state menu bertingkat, pemetaan tombol baru, layar persiapan/hasil/fault, editor parameter, gain PID runtime, indikator tahap bergerak, dan penjagaan navigasi selama proses aktif. Mesin proses conveyor, reflow, karakterisasi, dan kalibrasi yang ada tetap menjadi dasar. Perintah **A:STOP** menghentikan proses langsung; tidak ada dialog konfirmasi tambahan agar respons berhenti cepat. Penyimpanan permanen gain PID atau target profil belum dimasukkan karena perlu rancangan format dan lokasi flash yang aman terpisah dari data kalibrasi NTC.
