
peki birisi surekli 20 farkli bilgisayardan bize istek gonderirse ve hepsinden art arda gondermeye devam ederse ve keep_alive suresinin icinde kalarak ve baskalarinin hizmet gormesini engellerse ne olacak?
+ Thought: 13.5s
Çok yerinde bir soru — bu, thread pool + blocking I/O + keep-alive modelinin bilinen zayıf noktası. Dürüst cevap: evet, bu senaryo pool'u tıkayabilir. Ama iyi haber: savunması var, ve asıl çözüm mimaride.
Neden tıkanır
"20 bilgisayar" önemli değil; önemli olan eşzamanlı tutulan bağlantı sayısı vs. pool boyu (N). Şöyle:
- Worker bir bağlantıyı işlerken bloklar; keep-alive'de istekler arası beklerken de o worker o bağlantıya bağlı kalır.
- Saldırgan, her isteği idle timeout'un hemen öncesinde gönderir → bağlantı hiç boşa düşmez, worker hiç serbest kalmaz.
- 20 makine × birkaç bağlantı > N ise → tüm worker'lar meşgul. Yeni istemciler kernel'in accept backlog'unda (veya bizim kuyrukta) bekler; meşru kullanıcılar hizmet alamaz. Bu tam olarak "keep-alive slot exhaustion" / Slowloris sınıfı bir DoS.
Yani 1000 istek hızlı biter senaryosundan farkı: orada bağlantılar kısa; burada uzun süre açık tutuluyor, o da worker'ı rehin alıyor.
Neden nginx bu yüzden var
nginx'te boş bir keep-alive bağlantı bir thread tutmaz — sadece bir fd + küçük state; epoll hepsini tek/az thread'de bekletir. Sadece işlenecek (tam istek gelmiş) bağlantı worker'a verilir. 10.000 boş bağlantı neredeyse bedava. Bizim blocking modelinde her boş bağlantı bir thread pahasına. İşte bu yüzden nginx event-driven.
İlk sürüm için savunmalar (pool'u koruyarak)
Savunma
Toplam bağlantı cap'i (max_connections)
IP başına bağlantı cap'i (max_connections_per_ip)
Bağlantı başına istek cap'i (max_requests_per_connection)
Kısa keep_alive_timeout
Rate limit (requests_per_second_per_ip → 429)
read_timeout (zaten var)
Bunlar saldırıyı zorlaştırır ama yapısal olarak çözmez (dağıtık + timeout'a uyan saldırgan yine pool'u doldurabilir).
Yapısal çözüm (sonra): hibrit model
- Idle bekleme katmanı: non-blocking soketler + epoll. Bağlantı boşken worker tutmaz.
- İşleme katmanı: tam istek gelince (veya dosya okuma gibi bloklayan iş gerekince) thread pool'a devret.
- Yani "bekleme" epoll'da, "bloklayan iş" pool'da. net::Socket soyutlaması sayesinde bu geçişi sonra yapabiliriz.
Karar
Önerim: ilk sürümde küçük savunmaları koyalım — max_connections, max_connections_per_ip, max_requests_per_connection, kısa keep_alive_timeout (zaten var), ve read_timeout (var). Event-driven hibrit'i sonraya bırakalım; mimariyi ona hazır kuralım (accept/queue katmanını ayrı tutalım ki epoll'a geçiş kolay olsun).
Sana sormak istediğim: bu savunmaları ilk sürüme dahil edeyim mi (config'e max_connections + max_connections_per_ip + max_requests_per_connection ekleyip), yoksa önce sade çalışan bir Server yapıp savunmaları bir sonraki adıma mı bırakalım? (Şu an net timeout/local_port yardımcılarını ekliyordum; ona göre devam ederim.)