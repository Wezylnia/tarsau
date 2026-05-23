#include <stdio.h>
#include <string.h>
#include "../include/archive.h"
#include "../include/extract.h"

static void print_usage(void)
{
    fprintf(stderr, "Kullanim:\n");
    fprintf(stderr,
            "  tarsau -b <dosya1> <dosya2> ... [-o <arsiv.sau>]\n");
    fprintf(stderr,
            "  tarsau -a <arsiv.sau> [dizin]\n");
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        print_usage();
        return 1;
    }

    /* =================== ARSIV OLUSTURMA (-b) =================== */
    if (strcmp(argv[1], "-b") == 0) {

        char *output_file = "a.sau";    /* varsayilan cikti adi */
        char *input_files[32];
        int   input_count = 0;
        int   i;

        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) {
                if (i + 1 < argc) {
                    output_file = argv[++i];
                } else {
                    fprintf(stderr,
                            "Hata: -o parametresinden sonra dosya adi "
                            "belirtilmelidir.\n");
                    return 1;
                }
            } else {
                if (input_count >= 32) {
                    fprintf(stderr,
                            "Hata: En fazla 32 giris dosyasi "
                            "kabul edilir.\n");
                    return 1;
                }
                input_files[input_count++] = argv[i];
            }
        }

        if (input_count == 0) {
            fprintf(stderr,
                    "Hata: En az bir giris dosyasi belirtilmelidir.\n");
            return 1;
        }

        return create_archive(input_files, input_count, output_file);

    /* =================== ARSIV ACMA (-a) ======================== */
    } else if (strcmp(argv[1], "-a") == 0) {

        if (argc > 4) {
            fprintf(stderr,
                    "Hata: -a parametresinden sonra en fazla 2 parametre "
                    "alinmalidir.\n");
            return 1;
        }

        char *archive_file = argv[2];
        char *output_dir   = (argc == 4) ? argv[3] : NULL;

        return extract_archive(archive_file, output_dir);

    /* =================== HATALI KULLANIM ======================== */
    } else {
        print_usage();
        return 1;
    }
}
