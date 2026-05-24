#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/archive.h"
#include "../include/utils.h"

/* Platform-bagimsiz basename: son '/' veya '\' sonrasini dondurur */
static const char *get_basename(const char *path)
{
    const char *p = strrchr(path, '/');
    const char *q = strrchr(path, '\\');
    if (q && (!p || q > p)) p = q;
    return p ? p + 1 : path;
}

#define MAX_TOTAL_SIZE (200L * 1024 * 1024)  /* 200 MB */
#define MAX_FILES      32
#define HEADER_SIZE    10

static int is_safe_archive_name(const char *name)
{
    size_t len;

    if (!name || name[0] == '\0' || strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0) {
        return 0;
    }

    len = strlen(name);
    if (len >= 256 || strchr(name, ',') || strchr(name, '|') ||
        strchr(name, '/') || strchr(name, '\\')) {
        return 0;
    }

    return 1;
}

int create_archive(char *input_files[], int count, const char *output_file)
{
    struct stat file_stats[MAX_FILES];
    struct stat output_stat;
    const char *base_names[MAX_FILES];
    long total_size = 0;
    int i;

    if (!input_files || !output_file || count <= 0) {
        fprintf(stderr, "Hata: En az bir giris dosyasi belirtilmelidir.\n");
        return 1;
    }

    if (count > MAX_FILES) {
        fprintf(stderr, "Hata: En fazla %d giris dosyasi kabul edilir.\n", MAX_FILES);
        return 1;
    }

    /* --- 1. Giris dosyalarini dogrula --- */
    for (i = 0; i < count; i++) {
        if (stat(input_files[i], &file_stats[i]) != 0) {
            fprintf(stderr, "Hata: '%s' dosyasi bulunamadi.\n", input_files[i]);
            return 1;
        }

        if (!S_ISREG(file_stats[i].st_mode)) {
            fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", input_files[i]);
            return 1;
        }

        if (!is_text_file(input_files[i])) {
            fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", input_files[i]);
            return 1;
        }

        base_names[i] = get_basename(input_files[i]);
        if (!is_safe_archive_name(base_names[i])) {
            fprintf(stderr, "%s giris dosyasinin formati uyumsuzdur!\n", input_files[i]);
            return 1;
        }

        {
            int j;
            for (j = 0; j < i; j++) {
                if (strcmp(base_names[i], base_names[j]) == 0) {
                    fprintf(stderr,
                            "Hata: Arsivde ayni ada sahip birden fazla dosya olamaz: %s\n",
                            base_names[i]);
                    return 1;
                }
            }
        }

        if ((long)file_stats[i].st_size > MAX_TOTAL_SIZE - total_size) {
            fprintf(stderr, "Hata: Toplam dosya boyutu 200 MB'i asamaz.\n");
            return 1;
        }
        total_size += file_stats[i].st_size;
    }

    if (stat(output_file, &output_stat) == 0) {
        for (i = 0; i < count; i++) {
            if (output_stat.st_dev == file_stats[i].st_dev &&
                output_stat.st_ino == file_stats[i].st_ino) {
                fprintf(stderr, "Hata: Arsiv dosyasi giris dosyasi ile ayni olamaz.\n");
                return 1;
            }
        }
    }

    /* --- 2. Organizasyon bolumunu olustur --- */
    /* Format: |dosya_adi,izinler,boyut|dosya_adi,izinler,boyut| */
    size_t org_capacity = (size_t)count * 512 + 2;
    char *org_section = (char *)malloc(org_capacity);
    if (!org_section) {
        fprintf(stderr, "Hata: Bellek ayrilamadi.\n");
        return 1;
    }
    org_section[0] = '\0';

    for (i = 0; i < count; i++) {
        char record[512];
        int written;

        written = snprintf(record, sizeof(record), "|%s,%o,%ld",
                           base_names[i],
                           file_stats[i].st_mode & 0777,
                           (long)file_stats[i].st_size);
        if (written < 0 || (size_t)written >= sizeof(record) ||
            strlen(org_section) + (size_t)written + 2 > org_capacity) {
            fprintf(stderr, "Hata: Organizasyon bilgisi olusturulamadi.\n");
            free(org_section);
            return 1;
        }
        strcat(org_section, record);
    }
    strcat(org_section, "|");

    int org_size = (int)strlen(org_section);

    /* --- 3. Arsiv dosyasini yaz --- */
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        fprintf(stderr, "Hata: '%s' dosyasi olusturulamadi.\n", output_file);
        free(org_section);
        return 1;
    }

    /* Ilk 10 bayt: organizasyon bolumunun boyutu (sola sifir dolgulu) */
    if (fprintf(out, "%010d", org_size) != HEADER_SIZE ||
        fwrite(org_section, 1, (size_t)org_size, out) != (size_t)org_size) {
        fprintf(stderr, "Hata: '%s' dosyasina yazilamadi.\n", output_file);
        free(org_section);
        fclose(out);
        remove(output_file);
        return 1;
    }
    free(org_section);

    /* --- 4. Dosya iceriklerini art arda yaz --- */
    for (i = 0; i < count; i++) {
        FILE *in = fopen(input_files[i], "rb");
        if (!in) {
            fprintf(stderr, "Hata: '%s' dosyasi okunamadi.\n", input_files[i]);
            fclose(out);
            remove(output_file);
            return 1;
        }

        char buffer[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), in)) > 0) {
            if (fwrite(buffer, 1, bytes_read, out) != bytes_read) {
                fprintf(stderr, "Hata: '%s' dosyasina yazilamadi.\n", output_file);
                fclose(in);
                fclose(out);
                remove(output_file);
                return 1;
            }
        }

        if (ferror(in)) {
            fprintf(stderr, "Hata: '%s' dosyasi okunamadi.\n", input_files[i]);
            fclose(in);
            fclose(out);
            remove(output_file);
            return 1;
        }
        fclose(in);
    }

    if (fclose(out) != 0) {
        fprintf(stderr, "Hata: '%s' dosyasina yazilamadi: %s.\n",
                output_file, strerror(errno));
        remove(output_file);
        return 1;
    }
    printf("Dosyalar birlestirildi.\n");
    return 0;
}
