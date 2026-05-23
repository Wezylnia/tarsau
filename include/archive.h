#ifndef ARCHIVE_H
#define ARCHIVE_H

/* Verilen giris dosyalarini tek bir .sau arsiv dosyasina birlestirir.
   input_files: dosya adlari dizisi
   count: dosya sayisi
   output_file: cikti arsiv dosyasi adi
   Basarili ise 0, hata durumunda 1 dondurur. */
int create_archive(char *input_files[], int count, const char *output_file);

#endif
