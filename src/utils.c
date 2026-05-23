#include <stdio.h>
#include "../include/utils.h"

int is_text_file(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return 0;

    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        /* Gecerli ASCII metin karakterleri:
           - Yazdirilabilir karakterler: 32-126
           - Tab: 9, Yeni satir: 10, Satir basi: 13 */
        if (ch != 9 && ch != 10 && ch != 13 && (ch < 32 || ch > 126)) {
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return 1;
}
