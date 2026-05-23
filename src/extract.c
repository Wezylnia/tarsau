#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include "../include/extract.h"

#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(path, mode) _mkdir(path)
#else
  #define MKDIR(path, mode) mkdir(path, mode)
#endif

#define MAX_FILES 32
#define MAX_ORG_SIZE (MAX_FILES * 512 + 1)
#define HEADER_SIZE 10
#define MAX_TOTAL_SIZE (200L * 1024 * 1024)

/* Arsivde saklanan her dosyanin bilgisi */
typedef struct {
    char filename[256];
    mode_t permissions;
    long size;
} FileRecord;

static int is_safe_filename(const char *name)
{
    size_t len;

    if (!name || name[0] == '\0' || strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0) {
        return 0;
    }
    len = strlen(name);
    return len < sizeof(((FileRecord *)0)->filename) &&
           !strchr(name, '/') && !strchr(name, '\\') &&
           !strchr(name, ',') && !strchr(name, '|');
}

static int parse_octal_permissions(const char *value, mode_t *permissions)
{
    char *end;
    long parsed;

    if (!value || value[0] == '\0') return -1;
    errno = 0;
    parsed = strtol(value, &end, 8);
    if (errno != 0 || *end != '\0' || parsed < 0 || parsed > 0777) return -1;
    *permissions = (mode_t)parsed;
    return 0;
}

static int parse_size(const char *value, long *size)
{
    char *end;
    long parsed;

    if (!value || value[0] == '\0') return -1;
    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || *end != '\0' || parsed < 0 ||
        parsed > MAX_TOTAL_SIZE) return -1;
    *size = parsed;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Organizasyon bolumunu ayristirarak FileRecord dizisine doldurur.   */
/*  Format: |dosya_adi,izinler,boyut|dosya_adi,izinler,boyut|         */
/* ------------------------------------------------------------------ */
static int parse_records(const char *org, FileRecord *records, int *count)
{
    const char *p = org;
    long total_size = 0;
    *count = 0;

    if (!org || *p != '|') return -1;
    p++;

    while (*p) {
        int i;

        if (*count >= MAX_FILES || *p == '|') return -1;

        /* Sonraki '|' isaretini bul */
        const char *end = strchr(p, '|');
        if (!end) return -1;

        /* Kaydi gecici tampon kopyala */
        int len = (int)(end - p);
        if (len <= 0 || len >= 512) return -1;

        char record[512];
        memcpy(record, p, len);
        record[len] = '\0';

        /* Alanlari ayristir: dosya_adi,izinler,boyut */
        char *comma1 = strchr(record, ',');
        if (!comma1) return -1;
        *comma1 = '\0';

        char *comma2 = strchr(comma1 + 1, ',');
        if (!comma2) return -1;
        *comma2 = '\0';
        if (strchr(comma2 + 1, ',')) return -1;

        if (!is_safe_filename(record) ||
            parse_octal_permissions(comma1 + 1, &records[*count].permissions) != 0 ||
            parse_size(comma2 + 1, &records[*count].size) != 0 ||
            records[*count].size > MAX_TOTAL_SIZE - total_size) {
            return -1;
        }

        for (i = 0; i < *count; i++) {
            if (strcmp(records[i].filename, record) == 0) return -1;
        }
        strcpy(records[*count].filename, record);
        total_size += records[*count].size;

        (*count)++;
        p = end + 1;
    }

    return (*count > 0) ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/*  Verilen yolu ozyinelemeli olarak olusturur (mkdir -p benzeri)      */
/* ------------------------------------------------------------------ */
static int ensure_directory(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
    if (MKDIR(path, 0755) == 0) return 0;
    return (errno == EEXIST && stat(path, &st) == 0 && S_ISDIR(st.st_mode))
               ? 0 : -1;
}

static int create_directory_recursive(const char *path)
{
    char tmp[512];
    char *p;
    size_t len;

    if (!path || path[0] == '\0' ||
        snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) {
        return -1;
    }

    for (p = tmp; *p; p++) {
#ifdef _WIN32
        if (*p == '\\') *p = '/';
#endif
    }

    len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        len--;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (!(strlen(tmp) == 2 && tmp[1] == ':') &&
                ensure_directory(tmp) != 0) {
                return -1;
            }
            *p = '/';
        }
    }
    return ensure_directory(tmp);
}

/* ------------------------------------------------------------------ */
/*  Ana cikarma fonksiyonu                                            */
/* ------------------------------------------------------------------ */
int extract_archive(const char *archive_file, const char *output_dir)
{
    int i;
    struct stat archive_stat;

    /* --- Uzanti kontrolu --- */
    const char *ext = strrchr(archive_file, '.');
    if (!ext || strcmp(ext, ".sau") != 0) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    FILE *fp = fopen(archive_file, "rb");
    if (!fp) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    /* --- Ilk 10 bayt: organizasyon bolumu boyutu --- */
    char size_str[11];
    if (fread(size_str, 1, HEADER_SIZE, fp) != HEADER_SIZE) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        fclose(fp);
        return 1;
    }
    size_str[10] = '\0';

    for (i = 0; i < HEADER_SIZE; i++) {
        if (!isdigit((unsigned char)size_str[i])) {
            fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
            fclose(fp);
            return 1;
        }
    }

    long org_long = strtol(size_str, NULL, 10);
    if (org_long <= 0 || org_long > MAX_ORG_SIZE) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        fclose(fp);
        return 1;
    }
    if (stat(archive_file, &archive_stat) != 0) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        fclose(fp);
        return 1;
    }
    int org_size = (int)org_long;

    /* --- Organizasyon bolumunu oku --- */
    char *org_section = (char *)malloc(org_size + 1);
    if (!org_section) {
        fprintf(stderr, "Hata: Bellek ayrilamadi.\n");
        fclose(fp);
        return 1;
    }

    if ((int)fread(org_section, 1, org_size, fp) != org_size) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        free(org_section);
        fclose(fp);
        return 1;
    }
    org_section[org_size] = '\0';

    /* --- Kayitlari ayristir --- */
    FileRecord records[MAX_FILES];
    int record_count = 0;

    if (parse_records(org_section, records, &record_count) != 0 ||
        record_count == 0) {
        fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        free(org_section);
        fclose(fp);
        return 1;
    }
    free(org_section);

    {
        long data_size = 0;
        long archive_size;
        long expected_size;

        for (i = 0; i < record_count; i++) data_size += records[i].size;
        if (fseek(fp, 0, SEEK_END) != 0 ||
            (archive_size = ftell(fp)) < 0) {
            fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
            fclose(fp);
            return 1;
        }
        expected_size = HEADER_SIZE + (long)org_size + data_size;
        if (archive_size != expected_size ||
            fseek(fp, HEADER_SIZE + org_size, SEEK_SET) != 0) {
            fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
            fclose(fp);
            return 1;
        }
    }

    /* --- Cikti dizinini olustur --- */
    if (output_dir && create_directory_recursive(output_dir) != 0) {
        fprintf(stderr, "Hata: '%s' dizini olusturulamadi.\n", output_dir);
        fclose(fp);
        return 1;
    }

    /* --- Dosyalari cikar --- */
    for (i = 0; i < record_count; i++) {
        char filepath[512];
        if (output_dir) {
            if (snprintf(filepath, sizeof(filepath), "%s/%s",
                         output_dir, records[i].filename) >= (int)sizeof(filepath)) {
                fprintf(stderr, "Hata: Cikti dosya yolu cok uzundur.\n");
                fclose(fp);
                return 1;
            }
        } else {
            strcpy(filepath, records[i].filename);
        }

        {
            struct stat output_stat;
            if (stat(filepath, &output_stat) == 0 &&
                output_stat.st_dev == archive_stat.st_dev &&
                output_stat.st_ino == archive_stat.st_ino) {
                fprintf(stderr, "Arsiv dosyasi uygunsuz veya bozuk!\n");
                fclose(fp);
                return 1;
            }
        }

        FILE *out = fopen(filepath, "wb");
        if (!out) {
            fprintf(stderr, "Hata: '%s' dosyasi olusturulamadi.\n", filepath);
            fclose(fp);
            return 1;
        }

        /* Dosya icerigini oku ve yaz */
        long remaining = records[i].size;
        char buffer[4096];
        while (remaining > 0) {
            size_t to_read = (remaining > (long)sizeof(buffer))
                                 ? sizeof(buffer)
                                 : (size_t)remaining;
            size_t bytes_read = fread(buffer, 1, to_read, fp);
            if (bytes_read == 0) {
                fprintf(stderr,
                        "Hata: Arsiv dosyasi bozuk, '%s' icerigi eksik.\n",
                        records[i].filename);
                fclose(out);
                fclose(fp);
                return 1;
            }
            if (fwrite(buffer, 1, bytes_read, out) != bytes_read) {
                fprintf(stderr, "Hata: '%s' dosyasina yazilamadi.\n", filepath);
                fclose(out);
                fclose(fp);
                remove(filepath);
                return 1;
            }
            remaining -= (long)bytes_read;
        }

        if (fclose(out) != 0) {
            fprintf(stderr, "Hata: '%s' dosyasina yazilamadi.\n", filepath);
            fclose(fp);
            remove(filepath);
            return 1;
        }

        /* Dosya izinlerini geri yukle */
        if (chmod(filepath, records[i].permissions) != 0) {
            fprintf(stderr, "Hata: '%s' dosya izinleri ayarlanamadi.\n", filepath);
            fclose(fp);
            return 1;
        }

        printf("Cikariliyor: %s\n", records[i].filename);
    }

    fclose(fp);
    printf("Arsiv basariyla acildi.\n");
    return 0;
}
