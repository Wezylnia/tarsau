# tarsau - Arsivleme Programi

## Aciklama

`tarsau`, tar/rar/zip benzeri ancak sikistirma yapmayan bir C11 komut satiri
arsivleme programidir. Birden fazla ASCII metin dosyasini tek bir `.sau`
dosyasinda birlestirir ve daha sonra geri cikartir.

GitHub deposu: https://github.com/Wezylnia/tarsau

## Kullanim

Arsiv olusturma:

```bash
./tarsau -b dosya1.txt dosya2.txt dosya3.txt -o arsivim.sau
```

- `-o` belirtilmezse cikti adi `a.sau` olur.
- En fazla 32 giris dosyasi ve toplam 200 MB veri kabul edilir.
- Yalnizca ASCII metin dosyalari kabul edilir.
- Arsiv icinde cakisan veya formatta guvenle saklanamayan dosya adlari reddedilir.

Arsiv acma:

```bash
./tarsau -a arsivim.sau cikti_dizini
```

- Dizin verilmezse dosyalar mevcut dizine cikartilir.
- Dizin yoksa olusturulur.
- Kaydedilen dosya izinleri geri yuklenir.
- Bozuk ustbilgi/metadata, eksik ya da fazla veri, yinelenen kayitlar ve
  dizin disina cikmaya calisan dosya adlari reddedilir.

## Derleme

Linux / WSL:

```bash
make
make test
```

Windows (MSYS2 / MinGW):

```cmd
mingw32-make
mingw32-make test
```

Manuel derleme:

```bash
gcc -Wall -Wextra -std=c11 -Iinclude -o tarsau src/main.c src/archive.c src/extract.c src/utils.c
```

Temizlik:

```bash
make clean
```

## `.sau` Dosya Formati

```text
[10 bayt: organizasyon boyutu][|dosya1,izin1,boyut1|dosya2,izin2,boyut2|][icerik1][icerik2]...
```

1. Ilk 10 bayt, organizasyon bolumunun sola sifir dolgulu ASCII boyutudur.
2. Organizasyon bolumundeki her kayit `dosya_adi,izinler(octal),boyut` bicimindedir.
3. Veri bolumunde metin dosyalarinin icerikleri ayiricisiz art arda yer alir.

## Unit Testler

`test/test_tarsau.c`, her kosumda bir seed ile rastgele ASCII icerikler,
dosya sayilari ve boyutlari uretir. Bir hata halinde ayni vaka asagidaki gibi
tekrarlanabilir:

```bash
TARSAU_TEST_SEED=123456 ./test_tarsau
```

Paket su davranislari denetler:

- Rastgele coklu dosya round-trip icerik dogrulamasi.
- Linux'ta dosya izinlerinin geri yuklenmesi.
- Binary girdi, metadata ayracli ad ve cakisan basename reddi.
- Bozuk baslik, gecersiz izin, eksik/fazla veri ve path traversal reddi.
- Arsivin giris veya cikti olarak kendi uzerine yazilmasinin engellenmesi.
- 32 dosya siniri ve bos girdi reddi.

23 Mayis 2026 tarihinde dogrulanan sonuc:

| Platform | Komut | Rastgele kosum | Sonuc |
| --- | --- | ---: | --- |
| Windows (MSYS2 / MinGW) | `mingw32-make test` + tekrar | 250 | 250/250 basarili |
| Linux (WSL Debian) | `make test` + tekrar | 250 | 250/250 basarili |

Toplam `500/500` rastgele kosum basarili tamamlandi.

## Hazir Derlenmis Surumler

Derleme yapmadan calistirmak icin:

```text
HAZIR-DERLENMIS/
|-- windows/
|   |-- tarsau.exe
|   |-- test1.txt
|   |-- test2.txt
|   `-- test3.txt
`-- linux/
    |-- tarsau
    |-- test1.txt
    |-- test2.txt
    `-- test3.txt
```

Windows:

```cmd
cd HAZIR-DERLENMIS\windows
tarsau.exe -b test1.txt test2.txt test3.txt -o arsiv.sau
tarsau.exe -a arsiv.sau cikti
```

Linux:

```bash
cd HAZIR-DERLENMIS/linux
chmod +x tarsau
./tarsau -b test1.txt test2.txt test3.txt -o arsiv.sau
./tarsau -a arsiv.sau cikti
```

## Proje Yapisi

```text
tarsau/
|-- HAZIR-DERLENMIS/       # Windows ve Linux calistirilabilir dosyalari
|-- include/                # Header dosyalari
|-- src/                    # Uygulama kaynaklari
|-- test/test_tarsau.c      # Rastgele girdili unit/regresyon testleri
|-- Makefile
|-- rapor.docx
`-- README.md
```
