# 17511 sürümünün durumu

Hedef: Jasper RGH/JTAG, kernel 2.0.17511.0, Xbox 360 XDK / VS2010 araç zinciri.
Temel proje: EinTim23/hiddriver360. Bu çalışma 17511 uyarlamasıdır.

## Konsolda test edilip çalıştığı doğrulananlar
- PlayStation Classic USB kolu.
- Denenen 0810:0003 USB çevirici üzerinden PS1/PS2 kolları.
- Hori Fighting Stick mini 4 PS4-043(U/E). HID modundan 00ED Xbox tipi arayüze
  geçiş yakalandı; yalnızca bu kimlik/arayüze özel paket işleme eklendi.
  V22 logu: 9.838 geçerli paket, 0 bozuk paket, 0 USB aktarım hatası.
- Orijinal DualShock 3 CECHZC2E: USB kabloyla temel oyun kontrolü çalışıyor.

## Mevcut kontroller
- PS Classic ve denenen PS1/PS2 çeviricide Start + R1 + R2, R3 üretir.
- DS3 PS tuşu Xbox Guide'a eşlenmiş durumda. Fiziksel PS tuşu PS3'te de
  zor bastığından tamirden sonra yeniden denenecek.
- Forza Motorsport 4 varsayılan kamera değiştirme: Xbox Back / DS3 Select.

## Sonra eklenecek / doğrulanacak
- **Titreşim:** Henüz desteklenmiyor. Motorlu kollara oyun titreşim komutlarını
  gönderecek USB çıkış desteği eklenecek; bu sürümde çalıştığı iddia edilmiyor.
- **Nintendo SNES kolu + USB çevirici:** Çevirici edinildiğinde cihaz kimliği ve
  raporları incelenip destek eklenecek. Henüz test edilmedi.
- Açılışta takılı kolları sök-tak olmadan devralma henüz doğrulanmış değil.
- PS1/PS2 çeviricide fiziksel R3 için takip gerekiyor; mevcut kombinasyon çalışıyor.

Bu push kaynak kodu ve durum notlarını içerir. Yeni XEX release değildir.
Tanılama uygulamasındaki sürüm v22, sürücü işareti horixinput1'dir.
