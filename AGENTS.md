# AGENTS.md

Panduan kerja untuk agent/collaborator yang mengubah firmware ESP32 pada repository ini.

## Prinsip Utama

- Battery-first: setiap perubahan harus mempertimbangkan konsumsi daya lebih dulu.
- Non-blocking: gunakan `millis()` dan state machine; hindari `delay()` panjang atau loop tunggu aktif.
- Modular services: pertahankan pemisahan tanggung jawab antar service.
- `DeviceController` adalah orchestrator utama flow device.
- Hindari rewrite besar tanpa alasan yang jelas dan manfaat yang terukur.

## Arsitektur

Service yang ada:

- `SensorService`
- `DisplayService`
- `TimeService`
- `MqttService`
- `WiFiService`
- `OfflineLoggerService`
- `PowerService`
- `ConfigService`

Aturan arsitektur:

- `main.cpp` hanya berperan sebagai composition root.
- Flow utama harus berada di `DeviceController`.
- Service sebaiknya tidak saling mengambil alih tanggung jawab service lain.
- Dependency antar service harus tetap satu arah dan sederhana.
- Jika butuh koordinasi lintas service, lakukan di `DeviceController`, bukan dengan coupling langsung.

## Battery-First Rules

- WiFi hanya aktif saat dibutuhkan untuk:
  - publish MQTT
  - sync NTP
  - flush backlog offline
- Setelah pekerjaan jaringan selesai, lepaskan WiFi agar bisa idle/disconnect.
- OLED harus default `off`.
- Hindari menambah task periodik yang berjalan terus tanpa kebutuhan nyata.
- Saat menambah retry logic, gunakan backoff dan jangan retry agresif setiap loop.

## Non-Blocking Rules

- Gunakan `millis()` untuk timer, timeout, interval, dan cooldown.
- Jangan menambahkan blocking wait untuk WiFi, MQTT, sensor, atau display update.
- Jika library memaksa operasi yang agak blocking, batasi frekuensi pemanggilan dan bungkus dengan guard kondisi.
- Backlog flush harus dilakukan bertahap, bukan satu batch besar dalam satu loop.

## DeviceController Rules

- `DeviceController` mengatur:
  - kapan sensor dibaca
  - kapan waktu di-sync
  - kapan MQTT connect/publish
  - kapan offline logger append/flush
  - kapan WiFi diminta/dilepas
  - kapan OLED aktif/nonaktif
- State machine device harus tetap jelas dan event-driven.
- Jangan memindahkan orchestration kembali ke `main.cpp` kecuali ada alasan arsitektural kuat.

## MQTT Rules

- MQTT publish harus rate-limited.
- Jangan publish dari banyak tempat dengan aturan interval yang berbeda-beda.
- Live telemetry dan backlog replay harus dibedakan dengan jelas.
- Hindari spam publish saat koneksi broker sedang buruk.

## Offline Logger Rules

- Jika publish live gagal, telemetry boleh masuk logger lokal.
- Flush backlog harus bertahap dan aman terhadap restart.
- Jangan kirim seluruh backlog dalam satu loop.
- Retention harus tetap dijaga:
  - batas jumlah record
  - batas ukuran file
  - hapus record tertua lebih dulu jika penuh

## Display Rules

- OLED default `off`.
- Display hanya aktif saat ada event tombol atau kebutuhan eksplisit.
- Render display jangan dijalankan setiap loop; gunakan interval yang masuk akal.

## Logging Rules

- Gunakan log serial event-based, bukan per-loop spam.
- Prefix yang dipakai:
  - `[BOOT]`
  - `[WIFI]`
  - `[MQTT]`
  - `[SENSOR]`
  - `[RTC]`
  - `[DISPLAY]`
  - `[LOGGER]`
  - `[POWER]`
  - `[STATE]`

## Change Policy

- Pilih patch minimal lebih dulu.
- Jangan rename, pindah, atau gabung service besar-besaran tanpa kebutuhan jelas.
- Jika ingin mengubah kontrak public method, cek semua header dan implementasi agar tetap sinkron.
- Jika perubahan menyentuh retry, power, atau backlog behavior, review dampaknya ke baterai dan state machine.
