#ifndef EXTRACT_H
#define EXTRACT_H

/* Bir .sau arsiv dosyasini acar ve dosyalari cikarir.
   archive_file: arsiv dosyasi adi
   output_dir: cikti dizini (NULL ise mevcut dizine cikarir)
   Basarili ise 0, hata durumunda 1 dondurur. */
int extract_archive(const char *archive_file, const char *output_dir);

#endif
