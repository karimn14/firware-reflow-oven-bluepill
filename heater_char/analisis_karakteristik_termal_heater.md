# Laporan Analisis Karakteristik & Laju Pemanasan Heater

## 1. Pendahuluan
Laporan ini berisi analisis komprehensif mengenai respons termal elemen pemanas (*heater*) berdasarkan pengujian dengan empat variasi *duty cycle* PWM: **25%**, **50%**, **75%**, dan **100%**. Parameter yang dievaluasi mencakup waktu respon pemanasan, laju kenaikan suhu ($dT/dt$), inersia termal (*lag time*), serta besarnya lonjakan suhu (*overshoot*) setelah *heater* dimatikan pada titik *cut-off* ($\approx 105 - 109^\circ\text{C}$).

---

## 2. Ringkasan Parameter Utama

Tabel berikut menyajikan perbandingan metrik termal utama untuk setiap variasi *duty cycle*:

| Parameter Metrik | Duty Cycle 25% | Duty Cycle 50% | Duty Cycle 75% | Duty Cycle 100% |
| :--- | :---: | :---: | :---: | :---: |
| **Suhu Awal ($T_0$)** | $27.8^\circ\text{C}$ | $28.2^\circ\text{C}$ | $29.1^\circ\text{C}$ | $30.9^\circ\text{C}$ |
| **Suhu Saat Cut-off (Heater OFF)** | $109.2^\circ\text{C}$ | $106.6^\circ\text{C}$ | $105.2^\circ\text{C}$ | $106.9^\circ\text{C}$ |
| **Durasi Pemanasan ($t_{\text{heating}}$)** | $140.0\text{ s}$ | $58.0\text{ s}$ | $36.0\text{ s}$ | $30.0\text{ s}$ |
| **Suhu Puncak Maksimum ($T_{\text{max}}$)** | $120.2^\circ\text{C}$ | $138.6^\circ\text{C}$ | $149.2^\circ\text{C}$ | $149.4^\circ\text{C}$ |
| **Waktu Mencapai Puncak ($t_{\text{peak}}$)** | $156.7\text{ s}$ | $76.0\text{ s}$ | $57.0\text{ s}$ | $50.7\text{ s}$ |
| **Laju Pemanasan Rata-Rata** | **$0.58^\circ\text{C/s}$** | **$1.35^\circ\text{C/s}$** | **$2.11^\circ\text{C/s}$** | **$2.53^\circ\text{C/s}$** |
| **Laju Linier Fase Stabil** | **$0.55^\circ\text{C/s}$** | **$1.23^\circ\text{C/s}$** | **$1.86^\circ\text{C/s}$** | **$2.39^\circ\text{C/s}$** |
| **Laju Pemanasan Maksimum Terukur** | $1.11^\circ\text{C/s}$ | $2.09^\circ\text{C/s}$ | $3.17^\circ\text{C/s}$ | $5.00^\circ\text{C/s}$ |
| **Besar Overshoot Termal ($\Delta T_{\text{overshoot}}$)** | **$11.0^\circ\text{C}$** | **$32.0^\circ\text{C}$** | **$44.0^\circ\text{C}$** | **$42.5^\circ\text{C}$** |
| **Inersia Termal / Lag Time ($t_{\text{lag}}$)** | $16.7\text{ s}$ | $18.0\text{ s}$ | $21.0\text{ s}$ | $20.7\text{ s}$ |

---

## 3. Analisis Per Duty Cycle

### 3.1 Duty Cycle 25%
* **Kecepatan Respon:** Membutuhkan waktu terlama ($140.0\text{ s}$) untuk menyentuh batas *cut-off*.
* **Laju Pemanasan:** Menghasilkan laju pemanasan terendah sebesar $0.58^\circ\text{C/s}$ ($\approx 34.8^\circ\text{C/menit}$). Kenaikan suhu berlangsung secara gradual dan lebih stabil.
* **Karakteristik Overshoot:** Menunjukkan *overshoot* terkecil yaitu $11.0^\circ\text{C}$ dengan puncak suhu di $120.2^\circ\text{C}$.

### 3.2 Duty Cycle 50%
* **Kecepatan Respon:** Waktu pemanasan berkurang secara drastis menjadi $58.0\text{ s}$ ($2.41\times$ lebih cepat dibanding DC 25%).
* **Laju Pemanasan:** Laju pemanasan rata-rata naik ke $1.35^\circ\text{C/s}$ ($\approx 81.1^\circ\text{C/menit}$).
* **Karakteristik Overshoot:** Energi tersimpan mulai berdampak signifikan, menghasilkan lonjakan suhu sebesar $32.0^\circ\text{C}$ hingga mencapai puncak $138.6^\circ\text{C}$.

### 3.3 Duty Cycle 75%
* **Kecepatan Respon:** Sangat cepat, mencapai batas *cut-off* hanya dalam $36.0\text{ s}$.
* **Laju Pemanasan:** Rata-rata laju pemanasan berada di $2.11^\circ\text{C/s}$ ($\approx 126.8^\circ\text{C/menit}$).
* **Karakteristik Overshoot:** Lonjakan suhu sangat tinggi ($44.0^\circ\text{C}$), mendorong suhu puncak hingga $149.2^\circ\text{C}$.

### 3.4 Duty Cycle 100% (Full Power)
* **Kecepatan Respon:** Pemanasan maksimum, menyentuh *cut-off* dalam $30.0\text{ s}$.
* **Laju Pemanasan:** Mencapai laju pemanasan tertinggi sebesar $2.53^\circ\text{C/s}$ ($\approx 152.0^\circ\text{C/menit}$) dengan laju puncak sesaat mencapai $5.00^\circ\text{C/s}$.
* **Saturasi Termal:** Profil *overshoot* ($42.5^\circ\text{C}$) dan puncak suhu ($149.4^\circ\text{C}$) hampir sama dengan DC 75%. Hal ini mengindikasikan bahwa transfer panas sistem telah mendekati kapasitas konduksi maksimumnya.

---

## 4. Temuan Utama & Pola Termal

1. **Hubungan Linier Daya PWM Terhadap Laju Pemanasan:**
   Peningkatan daya pemanas berbanding lurus dengan peningkatan laju pemanasan rata-rata:
   $$\text{Duty Cycle } 25\% \rightarrow 50\% \rightarrow 75\% \rightarrow 100\%$$
   $$0.58^\circ\text{C/s} \rightarrow 1.35^\circ\text{C/s} \rightarrow 2.11^\circ\text{C/s} \rightarrow 2.53^\circ\text{C/s}$$

2. **Konsistensi Inersia Termal (Lag Time):**
   Waktu tunda antara daya *OFF* hingga suhu mencapai puncak stabil pada rentang **$16.7 - 21.0\text{ detik}$** untuk semua variasi daya. Karakteristik ini ditentukan oleh resistansi dan kapasitas termal fisik material (bukan oleh besarnya daya PWM).

3. **Risiko Overshoot pada Daya Tinggi:**
   Semakin tinggi daya pemanasan, semakin besar akumulasi panas pada inti elemen yang belum sempat berpindah ke sensor saat *cut-off*. Pada daya $\ge 75\%$, lonjakan suhu setelah *cut-off* mencapai lebih dari $40^\circ\text{C}$.

---

## 5. Rekomendasi Strategi Kontrol Reflow

Regresi linier terhadap laju rata-rata keempat pengujian menghasilkan pendekatan:

$$\frac{dT}{dt} \approx 0{,}02647 \times \text{duty} - 0{,}0094\quad[{}^\circ\text{C}/\text{s}]$$

Profil POC menaikkan target sekitar 30 °C dalam 75 detik, setara 0,40 °C/s. Karena pengujian 25% sudah menghasilkan rata-rata 0,58 °C/s, duty 75–100% tidak diperlukan untuk mengikuti profil tersebut. Daya tinggi hanya memperbesar energi tersimpan dan overshoot.

Parameter awal firmware hasil tuning:

| Parameter | Nilai | Dasar pemilihan |
| :--- | :---: | :--- |
| Ramp setpoint | 0,45 °C/s | Sedikit lebih cepat dari kebutuhan tahap 0,40 °C/s agar tersedia waktu settling |
| $K_p$ | 1,5 | Koreksi proporsional dibuat moderat untuk plant dengan delay panjang |
| $K_i$ | 0,035 | Mengoreksi rugi panas tanpa windup cepat |
| $K_d$ | 14,0 | Meredam laju kenaikan suhu; derivatif diterapkan pada measurement |
| Gain feed-forward | 0,0265 °C/s per 1% duty | Hasil regresi laju rata-rata |
| Horizon prediksi | 12 detik | Sebagian dari lag terukur 16,7–21 detik agar cutoff tidak terlalu dini |
| Duty maksimum PID | 50% | Masih menyediakan kemampuan mengejar profil tanpa wilayah overshoot 75–100% |

Strategi kontrol yang diterapkan:

1. Feed-forward menghasilkan duty dasar dari laju ramp yang diminta.
2. PID memperbaiki selisih terhadap ramped setpoint dan menggunakan anti-windup.
3. Suhu prediksi dihitung dari suhu terfilter ditambah laju kenaikan selama horizon 12 detik.
4. Batas daya diturunkan bertahap menjadi 45%, 30%, 15%, lalu 0% ketika suhu prediksi mendekati target.
5. Fan diaktifkan pada 50%, 70%, atau 100% ketika suhu prediksi menunjukkan overshoot. Heater dan fan tidak aktif bersamaan.
6. Pada fase cooling, fan dijalankan 100% hingga suhu turun ke batas akhir profil.

Fan yang digunakan adalah tipe 4-wire. Sinyal kontrol PA7 dibangkitkan oleh TIM3 CH2 sebagai PWM 25 kHz open-drain, sementara tacho tidak dihubungkan. Karena itu kontrol fan masih open-loop dan tidak dapat memastikan RPM aktual atau mendeteksi fan macet. Perilaku 0% perlu diperiksa pada datasheet fan karena sebagian tipe tetap berputar pada kecepatan minimum.

Gain pendinginan fan belum tersedia dalam data karakterisasi ini. Ambang dan duty fan merupakan nilai awal konservatif dan perlu divalidasi dengan satu pengujian profil lengkap yang mencatat suhu, duty heater, serta duty fan.
