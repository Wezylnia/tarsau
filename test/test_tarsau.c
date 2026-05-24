/* ============================================================
 * tarsau - randomized unit and regression tests
 * ============================================================ */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
  #include <direct.h>
  #include <process.h>
  #define getpid _getpid
  #define MKDIR(path) _mkdir(path)
  #define RMDIR(path) _rmdir(path)
#else
  #include <unistd.h>
  #define MKDIR(path) mkdir(path, 0755)
  #define RMDIR(path) rmdir(path)
#endif

#include "../include/archive.h"
#include "../include/extract.h"
#include "../include/utils.h"

#define MAX_CASE_FILES 8
#define PATH_SIZE 256

static int tests_run;
static int tests_passed;
static int tests_failed;
static unsigned long rng_state;
static char prefix[80];

#define ASSERT(cond, msg) do {                                      \
    tests_run++;                                                    \
    if (cond) {                                                     \
        tests_passed++;                                             \
    } else {                                                        \
        tests_failed++;                                             \
        fprintf(stderr, "  FAIL [%d]: %s (seed=%lu)\n",            \
                tests_run, msg, rng_state);                         \
    }                                                               \
} while (0)

static unsigned long next_random(void)
{
    unsigned long x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x ? x : 1UL;
    return rng_state;
}

static size_t random_size(size_t maximum)
{
    return (size_t)(next_random() % (unsigned long)(maximum + 1));
}

static void make_path(char *out, size_t out_size, const char *suffix)
{
    snprintf(out, out_size, "%s%s", prefix, suffix);
}

static int write_bytes(const char *path, const unsigned char *bytes, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    if (size > 0 && fwrite(bytes, 1, size, fp) != size) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static unsigned char *random_ascii(size_t size)
{
    unsigned char *data = (unsigned char *)malloc(size ? size : 1);
    size_t i;

    if (!data) return NULL;
    for (i = 0; i < size; i++) {
        unsigned long pick = next_random() % 20UL;
        if (pick == 0) data[i] = '\n';
        else if (pick == 1) data[i] = '\t';
        else if (pick == 2) data[i] = '\r';
        else data[i] = (unsigned char)(32 + (next_random() % 95UL));
    }
    return data;
}

static unsigned char *read_bytes(const char *path, long *out_size)
{
    FILE *fp = fopen(path, "rb");
    unsigned char *data;
    long size;

    if (!fp || fseek(fp, 0, SEEK_END) != 0) {
        if (fp) fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    data = (unsigned char *)malloc((size_t)size + 1);
    if (!data) {
        fclose(fp);
        return NULL;
    }
    if (size > 0 && fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    if (out_size) *out_size = size;
    return data;
}

static int files_equal(const char *first, const char *second)
{
    unsigned char *a;
    unsigned char *b;
    long a_size;
    long b_size;
    int equal;

    a = read_bytes(first, &a_size);
    b = read_bytes(second, &b_size);
    equal = a && b && a_size == b_size &&
            memcmp(a, b, (size_t)a_size) == 0;
    free(a);
    free(b);
    return equal;
}

static long file_size(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (long)st.st_size : -1;
}

static int make_dir(const char *path)
{
    if (MKDIR(path) == 0) return 1;
    return errno == EEXIST;
}

static int write_raw_archive(const char *path, const char *org,
                             const unsigned char *data, size_t data_size)
{
    FILE *fp = fopen(path, "wb");
    size_t org_size = strlen(org);

    if (!fp) return 0;
    if (fprintf(fp, "%010lu", (unsigned long)org_size) != 10 ||
        fwrite(org, 1, org_size, fp) != org_size ||
        (data_size > 0 && fwrite(data, 1, data_size, fp) != data_size)) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static void test_text_validation(void)
{
    char valid[PATH_SIZE];
    char invalid[PATH_SIZE];
    size_t size = random_size(4096);
    unsigned char *data = random_ascii(size);

    make_path(valid, sizeof(valid), "valid.txt");
    make_path(invalid, sizeof(invalid), "invalid.dat");

    ASSERT(data != NULL, "Rastgele metin bellegi ayrilabilmeli");
    if (!data) return;
    ASSERT(write_bytes(valid, data, size), "Rastgele ASCII dosyasi yazilabilmeli");
    ASSERT(is_text_file(valid) == 1, "Rastgele ASCII metin kabul edilmeli");

    if (size == 0) {
        free(data);
        size = 1;
        data = (unsigned char *)malloc(1);
        if (!data) return;
    }
    data[next_random() % size] = (next_random() & 1UL) ? 0U : 200U;
    ASSERT(write_bytes(invalid, data, size), "Rastgele bozuk dosya yazilabilmeli");
    ASSERT(is_text_file(invalid) == 0, "Metin disi bayt reddedilmeli");
    free(data);
}

static void test_random_roundtrip(void)
{
    char archive[PATH_SIZE];
    char output[PATH_SIZE];
    char inputs[MAX_CASE_FILES][PATH_SIZE];
    char extracted[PATH_SIZE];
    char *files[MAX_CASE_FILES];
    int count = 1 + (int)(next_random() % MAX_CASE_FILES);
    int i;

    make_path(archive, sizeof(archive), "roundtrip.sau");
    make_path(output, sizeof(output), "out");

    for (i = 0; i < count; i++) {
        unsigned char *data;
        size_t size = random_size(32768);
        snprintf(inputs[i], sizeof(inputs[i]), "%sf%d.txt", prefix, i);
        files[i] = inputs[i];
        data = random_ascii(size);
        ASSERT(data != NULL, "Rastgele round-trip icerigi ayrilabilmeli");
        if (!data) return;
        ASSERT(write_bytes(inputs[i], data, size), "Giris dosyasi yazilabilmeli");
        free(data);
#ifndef _WIN32
        chmod(inputs[i], (i & 1) ? 0640 : 0754);
#endif
    }

    ASSERT(create_archive(files, count, archive) == 0,
           "Rastgele dosyalar arsivlenebilmeli");
    ASSERT(extract_archive(archive, output) == 0,
           "Rastgele arsiv geri acilabilmeli");

    for (i = 0; i < count; i++) {
#ifndef _WIN32
        struct stat original;
        struct stat restored;
#endif
        snprintf(extracted, sizeof(extracted), "%sout/%sf%d.txt",
                 prefix, prefix, i);
        ASSERT(files_equal(inputs[i], extracted),
               "Rastgele icerik round-trip sonunda ayni olmali");
#ifndef _WIN32
        ASSERT(stat(inputs[i], &original) == 0 &&
               stat(extracted, &restored) == 0 &&
               (original.st_mode & 0777) == (restored.st_mode & 0777),
               "Linux izinleri round-trip sonunda korunmali");
#endif
    }
}

static void test_archive_rejections(void)
{
    char source[PATH_SIZE];
    char binary[PATH_SIZE];
    char unsafe[PATH_SIZE];
    char archive[PATH_SIZE];
    char first_dir[PATH_SIZE];
    char second_dir[PATH_SIZE];
    char first_same[PATH_SIZE];
    char second_same[PATH_SIZE];
    char same_output[PATH_SIZE];
    unsigned char *data = random_ascii(32 + random_size(128));
    unsigned char binary_data[3] = {'x', 0, 'y'};
    char *files[2];

    make_path(source, sizeof(source), "source.txt");
    make_path(binary, sizeof(binary), "binary.dat");
    make_path(unsafe, sizeof(unsafe), "bad,name.txt");
    make_path(archive, sizeof(archive), "reject.sau");
    make_path(same_output, sizeof(same_output), "same_output.sau");
    make_path(first_dir, sizeof(first_dir), "dir_a");
    make_path(second_dir, sizeof(second_dir), "dir_b");
    snprintf(first_same, sizeof(first_same), "%sdir_a/same.txt", prefix);
    snprintf(second_same, sizeof(second_same), "%sdir_b/same.txt", prefix);

    ASSERT(data != NULL, "Red senaryosu verisi ayrilabilmeli");
    if (!data) return;
    ASSERT(write_bytes(source, data, 32), "Gecerli kaynak yazilabilmeli");
    ASSERT(write_bytes(binary, binary_data, sizeof(binary_data)),
           "Binary kaynak yazilabilmeli");
    ASSERT(write_bytes(unsafe, data, 32), "Virgullu dosya yazilabilmeli");
    files[0] = binary;
    ASSERT(create_archive(files, 1, archive) == 1,
           "Binary dosya arsive alinmamali");
    files[0] = unsafe;
    ASSERT(create_archive(files, 1, archive) == 1,
           "Metadata ayraci iceren ad reddedilmeli");
    ASSERT(write_bytes(same_output, data, 32), "Ayni cikti kaynagi yazilabilmeli");
    files[0] = same_output;
    ASSERT(create_archive(files, 1, same_output) == 1,
           "Arsiv kaynaginin uzerine yazilmamali");

    ASSERT(make_dir(first_dir) && make_dir(second_dir),
           "Ayni basename dizinleri olusturulabilmeli");
    ASSERT(write_bytes(first_same, data, 32) &&
           write_bytes(second_same, data, 32),
           "Ayni basename kaynaklari yazilabilmeli");
    files[0] = first_same;
    files[1] = second_same;
    ASSERT(create_archive(files, 2, archive) == 1,
           "Ayni basename ile veri kaybi engellenmeli");
    free(data);
}

static void test_corrupt_archives(void)
{
    char archive[PATH_SIZE];
    char output[PATH_SIZE];
    char outside[PATH_SIZE];
    char self_archive[PATH_SIZE];
    char self_org[PATH_SIZE];
    unsigned char payload[2] = {'o', 'k'};

    make_path(archive, sizeof(archive), "bad.sau");
    make_path(output, sizeof(output), "bad_out");
    make_path(outside, sizeof(outside), "escape.txt");
    make_path(self_archive, sizeof(self_archive), "self.sau");

    ASSERT(write_bytes(archive, (const unsigned char *)"abc0000000|x,644,0|", 20),
           "Bozuk baslik yazilabilmeli");
    ASSERT(extract_archive(archive, output) == 1,
           "Sayisal olmayan baslik reddedilmeli");

    ASSERT(write_raw_archive(archive, "|../escape.txt,644,2|", payload, 2),
           "Traversal arsivi yazilabilmeli");
    ASSERT(extract_archive(archive, output) == 1 && file_size(outside) < 0,
           "Dizin disina cikarma reddedilmeli");

    ASSERT(write_raw_archive(archive, "|a.txt,644,1|a.txt,644,1|", payload, 2),
           "Mumkun veri kaybi arsivi yazilabilmeli");
    ASSERT(extract_archive(archive, output) == 1,
           "Tekrarlanan kayit adi reddedilmeli");

    ASSERT(write_raw_archive(archive, "|a.txt,99x,2|", payload, 2),
           "Gecersiz izin arsivi yazilabilmeli");
    ASSERT(extract_archive(archive, output) == 1,
           "Gecersiz izin alani reddedilmeli");

    ASSERT(write_raw_archive(archive, "|a.txt,644,1|", payload, 2),
           "Fazla veri iceren arsiv yazilabilmeli");
    ASSERT(extract_archive(archive, output) == 1,
           "Fazladan veri iceren arsiv reddedilmeli");

    ASSERT(write_raw_archive(archive, "|a.txt,644,3|", payload, 2),
           "Eksik veri iceren arsiv yazilabilmeli");
    ASSERT(extract_archive(archive, output) == 1,
           "Eksik veri iceren arsiv reddedilmeli");

    snprintf(self_org, sizeof(self_org), "|%sself.sau,644,2|", prefix);
    ASSERT(write_raw_archive(self_archive, self_org, payload, 2),
           "Kendi uzerine acilan arsiv yazilabilmeli");
    ASSERT(extract_archive(self_archive, NULL) == 1,
           "Arsiv dosyasinin uzerine cikartma yapilmamali");
}

static void test_limits(void)
{
    char input[PATH_SIZE];
    char archive[PATH_SIZE];
    char *files[33];
    unsigned char *data = random_ascii(1 + random_size(64));
    int i;

    make_path(input, sizeof(input), "limit.txt");
    make_path(archive, sizeof(archive), "limit.sau");
    ASSERT(data != NULL, "Limit testi verisi ayrilabilmeli");
    if (!data) return;
    ASSERT(write_bytes(input, data, 1), "Limit dosyasi yazilabilmeli");
    free(data);
    for (i = 0; i < 33; i++) files[i] = input;
    ASSERT(create_archive(files, 33, archive) == 1,
           "32 dosya siniri asildiginda hata donmeli");
    ASSERT(create_archive(NULL, 0, archive) == 1,
           "Bos girdi listesi reddedilmeli");
}

static void cleanup(void)
{
    char path[PATH_SIZE];
    char output[PATH_SIZE];
    int i;
    const char *suffixes[] = {
        "valid.txt", "invalid.dat", "roundtrip.sau", "source.txt",
        "binary.dat", "bad,name.txt", "reject.sau", "bad.sau",
        "same_output.sau", "escape.txt", "self.sau", "limit.txt", "limit.sau"
    };

    for (i = 0; i < (int)(sizeof(suffixes) / sizeof(suffixes[0])); i++) {
        make_path(path, sizeof(path), suffixes[i]);
        remove(path);
    }
    make_path(output, sizeof(output), "out");
    for (i = 0; i < MAX_CASE_FILES; i++) {
        snprintf(path, sizeof(path), "%sf%d.txt", prefix, i);
        remove(path);
        snprintf(path, sizeof(path), "%sout/%sf%d.txt", prefix, prefix, i);
        remove(path);
    }
    RMDIR(output);

    make_path(output, sizeof(output), "bad_out");
    snprintf(path, sizeof(path), "%sbad_out/a.txt", prefix);
    remove(path);
    RMDIR(output);

    make_path(output, sizeof(output), "dir_a");
    snprintf(path, sizeof(path), "%sdir_a/same.txt", prefix);
    remove(path);
    RMDIR(output);
    make_path(output, sizeof(output), "dir_b");
    snprintf(path, sizeof(path), "%sdir_b/same.txt", prefix);
    remove(path);
    RMDIR(output);
}

int main(void)
{
    const char *seed_text = getenv("TARSAU_TEST_SEED");
    unsigned long seed;

    if (seed_text && seed_text[0] != '\0') {
        seed = strtoul(seed_text, NULL, 10);
    } else {
        seed = (unsigned long)time(NULL) ^ (unsigned long)clock() ^
               ((unsigned long)getpid() << 16);
    }
    rng_state = seed ? seed : 1UL;
    snprintf(prefix, sizeof(prefix), "_ts_%lu_%lu_", seed, (unsigned long)getpid());

    printf("tarsau randomized tests (seed=%lu)\n", seed);
    test_text_validation();
    test_random_roundtrip();
    test_archive_rejections();
    test_corrupt_archives();
    test_limits();
    cleanup();

    printf("Sonuc: %d/%d test BASARILI", tests_passed, tests_run);
    if (tests_failed) printf(", %d BASARISIZ", tests_failed);
    printf("\n");
    return tests_failed ? 1 : 0;
}
