# Security Report — web-server

- **Tarih:** 2026-10-14
- **Hedef:** `http://127.0.0.1:6868` — `web-server` v0.1.0 (C++20 HTTP/1.1 static file server)
- **Kapsam:** Black-box, yalnızca ağ üzerinden (kaynak kod / tüm depo incelenmedi).
- **Test eden:** Red Team (yetkili, sahibi tarafından talep edildi)
- **Genel sonuç:** **Kritik.** Sunucu, karşı tarafı doğrulama gerektirmeyen düşük maliyetli bir DoS ile tamamen çökertilebiliyor.

---

## Özet

| ID | Başlık | Önem |
|---|---|---|
| VULN-01 | Slowloris / worker-pool exhaustion DoS | **Kritik** |
| VULN-02 | Maksimum eşzamanlı bağlantı sınırı yok | Orta |
| VULN-03 | Duplicate / negatif / aşırı `Content-Length` kabulü | Orta |
| VULN-04 | `Transfer-Encoding` + `Content-Length` birlikte → hang | Orta |
| VULN-05 | HTTP versiyonu doğrulanmıyor | Orta |
| VULN-06 | HTTP/1.1'de `Host` header'ı zorunlu değil | Orta |
| VULN-07 | Bilgi sızıntısı (`/statistics`, `/docs`) | Orta |
| VULN-08 | Düz HTTP (TLS yok) + fingerprint | Orta |
| VULN-09 | 403 vs 404 traversal oracle'ı | Düşük |
| VULN-10 | Bloklayıcı thread-per-connection mimarisi | Düşük |

---

## VULN-01 — Slowloris / worker-pool exhaustion DoS (Kritik)

**Etki:** 4 soket ile sunucu **tamamen ve kalıcı olarak** kilitlenebiliyor; tüm meşru istekler timeout oluyor.

**Kök neden:** Bağlantı başına bir thread; thread havuzu yalnızca **4 worker**
(`/statistics` → `workers.total = 4`). Sonlandırıcı `\r\n\r\n` gönderilmeden açık
tutulan her bağlantı bir worker'ı meşgul ediyor. Bağlantı başına **toplam okuma
deadline'ı**, **minimum veri hızı** ve **eşzamanlı bağlantı limiti** yok.

**Kanıt:**

```
3 partial conns: still served
4 partial conns: BLOCKED (TimeoutError)   <-- tam outage
5 partial conns: BLOCKED (TimeoutError)
```

Tek başına header read timeout'u **~15 sn**, ancak her ~10 sn'de 1 byte drip
(örn. boşluk karakteri) gönderilerek süre sonsuz uzatılıyor:

```
6 drip bağlantısı -> sunucu 36+ sn boyunca %100 kilitli, meşru istekler timeout
```

**İstismar koşulu:** Worker sayısı kadar (>4) soket aç, hiç tamamlanmayan bir
istek başlığı gönder, periyodik tek byte ile timeout'u sıfırla.

**Düzeltme:**
- Bağlantı başına **toplam istek deadline'ı** (örn. 5–10 sn) ve **minimum okuma hızı** uygula.
- **Eşzamanlı bağlantı cap'i** (worker sayısından bağımsız) ve listen backlog sınırı ekle.
- İdeali: bloklayıcı thread-per-connection yerine **epoll / non-blocking async** mimari.

---

## VULN-02 — Maksimum eşzamanlı bağlantı sınırı yok (Orta)

Accept kuyruğu ve eşzamanlı bağlantı sayısında cap gözlenmedi. 200 eşzamanlı hızlı
istek sorunsuz servis edildi; bu, VULN-01 ile birleşince kaynak tüketimini hızlandırır.

**Kanıt:** `seq 1 200 | xargs -P200 curl ...` → 200/200 başarılı, sınır yok.

**Düzeltme:** `max_connections` limiti, bağlantı başına idle/okuma deadline'ı, kabul throttling.

---

## VULN-03 — Duplicate / negatif / aşırı Content-Length kabulü (Orta)

Sunucu geçersiz `Content-Length` değerlerini reddetmiyor; bu, arada bir proxy/LB
varsa **HTTP request smuggling** riskidir.

**Kanıt:**
```
Content-Length: 3 + Content-Length: 5  -> 200 (kabul)
Content-Length: -1                      -> 200 (kabul)
Content-Length: 99999999999999999999    -> 200 (kabul)
```

**Düzeltme:** Tam olarak bir `Content-Length`; pozitif, makul üst sınırlı tamsayı;
aksini **400** ile reddet. `Transfer-Encoding` varlığında `Content-Length`'i yok say/reddet.

---

## VULN-04 — Transfer-Encoding + Content-Length birlikte → hang (Orta)

Her ikisi birlikte gönderildiğinde istek yanıtsız kalıyor (timeout), tutarlı biçimde
reddedilmiyor. RFC 7230 gereği bu kombinasyon 400 olmalıdır.

**Kanıt:** `POST / HTTP/1.1` + `Transfer-Encoding: chunked` + `Content-Length: 6` → yanıt yok.

**Düzeltme:** TE ve CL birlikte geldiğinde isteği 400 ile reddet veya TE'yi tek yetkili kabul et.

---

## VULN-05 — HTTP versiyonu doğrulanmıyor (Orta)

`GET / HTTP/9.9` isteği **200** döndürüyor. Protokol versiyonu doğrulanmıyor;
downgrade/smuggling yardımcısı olabilir.

**Düzeltme:** Yalnızca `HTTP/1.0` ve `HTTP/1.1` kabul et; diğerlerine 400/505.

---

## VULN-06 — HTTP/1.1'de Host header'ı zorunlu değil (Orta)

`Host` olmayan HTTP/1.1 isteği 200 dönüyor. RFC 7230 zorunlu kılar; sanal host
yönlendirmesi/absolute-URI çözümlemesi olan ortamlarda yanlış route riski.

**Düzeltme:** HTTP/1.1 isteklerinde `Host` yoksa 400; birden fazla `Host` varsa reddet.

---

## VULN-07 — Bilgi sızıntısı: `/statistics` ve `/docs` (Orta)

Kimlik doğrulaması olmadan açık uçlar:

- `/statistics`: process RSS/CPU, worker/queue sayıları, uptime, byte/istek sayaçları, cache istatistikleri → fingerprint ve kapasite planı için Keşif (recon).
- `/docs`: iç route tablosu; sayfalarda link verilmeyen route'u (`/merhaba` → `./demo/about.html`) ifşa ediyor.

**Kanıt:**
```json
// GET /docs
{ "routes": [ {"path":"/","target":"./demo","type":"directory"},
              {"path":"/merhaba","target":"./demo/about.html","type":"file"} ] }
```

**Düzeltme:** Bu uçları auth + rate-limit arkasına al veya prod'da kapat; worker/process
metriklerini dışa açma.

---

## VULN-08 — Düz HTTP (TLS yok) (Orta)

Trafik şifresiz; `Server: web-server` başlığı ile sürüm fingerprint'i kolay.

**Düzeltme:** TLS (reverse proxy arkasında), `Server` başlığını minimize et.

---

## VULN-09 — 403 vs 404 traversal oracle'ı (Düşük)

URL-encoded traversal denemeleri 403, diğerleri 404 dönüyor; bu, traversal tespit
mantığının varlığını/çalışmasını sızdırır.

**Kanıt:**
```
/..%2fconfig.toml        -> 403
/%2e%2e%2f%2e%2e%2fetc/passwd -> 403
/../config.toml          -> 404
/../../../../etc/passwd  -> 404
```

**Düzeltme:** Güvenlik nedeniyle reddedilen tüm isteklerde tek tip **404** döndür.

---

## VULN-10 — Bloklayıcı thread-per-connection mimarisi (Düşük)

Mimari, VULN-01 ve VULN-02'nin altında yatan nedendir. Küçük ve sabit worker havuzu,
yavaş istemci karşısında tüm servisi bloke eder.

**Düzeltme:** Non-blocking/async veya en azından bağlantı başına deadline + kaynak limitleri.

---

## Başarısız saldırı denemeleri (savunma iyi çalıştı)

- **Path traversal engellendi:** `/../config.toml`, `/%2e%2e/`, `/....//`, `/..;/`, `/etc/passwd` → 403/404. Kök dışına çıkılamadı.
- **Metot kısıtı:** GET/HEAD dışındakiler → 405.
- **Büyük istek/başlık:** aşırı URL/header → 431.
- **Bozuk istek:** boş/ayrılmış satır, eksik versiyon → 400.
- **Cache poisoning:** cache key query-string'i yok sayıyor; query ile şüpheli cache büyümesi yok.

---

## Öncelikli aksiyon listesi

1. **VULN-01 (Kritik):** Bağlantı başına toplam deadline + minimum veri hızı + eşzamanlı bağlantı cap'i. *(Bu yapılmadan sunucu 4 soketle düşürülebilir.)*
2. **VULN-03/04:** `Content-Length` / `Transfer-Encoding` doğrulaması.
3. **VULN-05/06:** HTTP versiyonu ve `Host` doğrulaması.
4. **VULN-07/08:** `/statistics` ve `/docs`'u kısıtla; TLS ve sertleştirilmiş reverse proxy.
5. **VULN-09:** Traversal'da tek tip 404.
