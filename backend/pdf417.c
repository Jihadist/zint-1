/* pdf417.c - Handles PDF417 stacked symbology */

/*  Zint - A barcode generating program using libpng
    Copyright (C) 2008-2021 Robin Stuart <rstuart114@gmail.com>
    Portions Copyright (C) 2004 Grandzebu
    Bug Fixes thanks to KL Chin <klchin@users.sourceforge.net>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* vim: set ts=4 sw=4 et : */

/*  This code is adapted from "Code barre PDF 417 / PDF 417 barcode" v2.5.0
    which is Copyright (C) 2004 (Grandzebu).
    The original code can be downloaded from http://grandzebu.net/index.php */

/* NOTE: symbol->option_1 is used to specify the security level (i.e. control the
   number of check codewords)

   symbol->option_2 is used to adjust the width of the resulting symbol (i.e. the
   number of codeword columns not including row start and end data)

   symbol->option_3 is used to adjust the rows of the resulting symbol */

#include <stdio.h>
#include <math.h>
#ifndef _MSC_VER
#include <stdint.h>
#else
#include "ms_stdint.h"
#endif
#include <assert.h>
#include "common.h"
#include "pdf417.h"
#include "pdf417_tabs.h"

#define TEX 900
#define BYT 901
#define NUM 902

/*
   Three figure numbers in comments give the location of command equivalents in the
   original Visual Basic source code file pdf417.frm
   this code retains some original (French) procedure and variable names to ease conversion */

/* text mode processing tables */

static const char pdf_asciix[127] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 8, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    7, 8, 8, 4, 12, 4, 4, 8, 8, 8, 12, 4, 12, 12, 12, 12, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 12, 8, 8, 4, 8, 8, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 8, 8, 8, 4, 8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 8, 8, 8, 8
};

static const char pdf_asciiy[127] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 15, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    26, 10, 20, 15, 18, 21, 10, 28, 23, 24, 22, 20, 13, 16, 17, 19, 0, 1, 2, 3,
    4, 5, 6, 7, 8, 9, 14, 0, 1, 23, 2, 25, 3, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 4, 5, 6, 24, 7, 8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 21, 27, 9
};

/* Automatic sizing table */

static const char pdf_MicroAutosize[56] = {
    4, 6, 7, 8, 10, 12, 13, 14, 16, 18, 19, 20, 24, 29, 30, 33, 34, 37, 39, 46, 54, 58, 70, 72, 82, 90, 108, 126,
    1, 14, 2, 7, 3, 25, 8, 16, 5, 17, 9, 6, 10, 11, 28, 12, 19, 13, 29, 20, 30, 21, 22, 31, 23, 32, 33, 34
};

/* ISO/IEC 15438:2015 5.1.1 c) 3) Max possible number of characters at error correction level 0
   (Numeric Compaction mode) */
#define PDF_MAX_LEN         2710

/* ISO/IEC 24728:2006 5.1.1 c) 3) Max possible number of characters (Numeric Compaction mode) */
#define MICRO_PDF_MAX_LEN   366

/* 866 */

static int pdf_quelmode(const unsigned char codeascii) {
    if ((codeascii <= '9') && (codeascii >= '0')) {
        return NUM;
    }
    if (codeascii < 127 && pdf_asciix[codeascii]) {
        return TEX;
    }
    /* 876 */

    return BYT;
}

/* 844 */
static void pdf_regroupe(int liste[2][PDF_MAX_LEN], int *indexliste) {

    /* bring together same type blocks */
    if (*(indexliste) > 1) {
        int i = 1;
        while (i < *(indexliste)) {
            if (liste[1][i - 1] == liste[1][i]) {
                int j;
                /* bring together */
                liste[0][i - 1] = liste[0][i - 1] + liste[0][i];
                j = i + 1;

                /* decrease the list */
                while (j < *(indexliste)) {
                    liste[0][j - 1] = liste[0][j];
                    liste[1][j - 1] = liste[1][j];
                    j++;
                }
                *(indexliste) = *(indexliste) - 1;
                i--;
            }
            i++;
        }
    }
    /* 865 */
}

/* 478 */
static void pdf_smooth(int liste[2][PDF_MAX_LEN], int *indexliste) {
    int i, crnt, last, next, length;

    for (i = 0; i < *(indexliste); i++) {
        crnt = liste[1][i];
        length = liste[0][i];
        if (i != 0) {
            last = liste[1][i - 1];
        } else {
            last = FALSE;
        }
        if (i != *(indexliste) - 1) {
            next = liste[1][i + 1];
        } else {
            next = FALSE;
        }

        if (crnt == NUM) {
            if (i == 0) {
                /* first block */
                if (*(indexliste) > 1) {
                    /* and there are others */
                    if ((next == TEX) && (length < 8)) {
                        liste[1][i] = TEX;
                    } else if ((next == BYT) && (length == 1)) {
                        liste[1][i] = BYT;
                    }
                }
            } else {
                if (i == *(indexliste) - 1) {
                    /* last block */
                    if ((last == TEX) && (length < 7)) {
                        liste[1][i] = TEX;
                    } else if ((last == BYT) && (length == 1)) {
                        liste[1][i] = BYT;
                    }
                } else {
                    /* not first or last block */
                    if (((last == BYT) && (next == BYT)) && (length < 4)) {
                        liste[1][i] = BYT;
                    } else if (((last == BYT) && (next == TEX)) && (length < 4)) {
                        liste[1][i] = TEX;
                    } else if (((last == TEX) && (next == BYT)) && (length < 5)) {
                        liste[1][i] = TEX;
                    } else if (((last == TEX) && (next == TEX)) && (length < 8)) {
                        liste[1][i] = TEX;
                    }
                }
            }
        }
    }
    pdf_regroupe(liste, indexliste);
    /* 520 */
    for (i = 0; i < *(indexliste); i++) {
        crnt = liste[1][i];
        length = liste[0][i];
        if (i != 0) {
            last = liste[1][i - 1];
        } else {
            last = FALSE;
        }
        if (i != *(indexliste) - 1) {
            next = liste[1][i + 1];
        } else {
            next = FALSE;
        }

        if ((crnt == TEX) && (i > 0)) {
            /* not the first */
            if (i == *(indexliste) - 1) {
                /* the last one */
                if ((last == BYT) && (length == 1)) {
                    liste[1][i] = BYT;
                }
            } else {
                /* not the last one */
                if (((last == BYT) && (next == BYT)) && (length < 5)) {
                    liste[1][i] = BYT;
                } else if ((((last == BYT) && (next != BYT)) || ((last != BYT) && (next == BYT))) && (length < 3)) {
                    liste[1][i] = BYT;
                }
            }
        }
    }
    /* 540 */
    pdf_regroupe(liste, indexliste);
}

/* 547 */
static void pdf_textprocess(int *chainemc, int *mclength, const unsigned char chaine[], int start, const int length,
            const int is_micro) {
    int j, indexlistet, curtable, listet[2][PDF_MAX_LEN] = {{0}}, chainet[PDF_MAX_LEN], wnet;

    wnet = 0;

    /* listet will contain the table numbers and the value of each characters */
    for (indexlistet = 0; indexlistet < length; indexlistet++) {
        const int codeascii = chaine[start + indexlistet];
        listet[0][indexlistet] = pdf_asciix[codeascii];
        listet[1][indexlistet] = pdf_asciiy[codeascii];
    }

    /* 570 */
    curtable = 1; /* default table */
    for (j = 0; j < length; j++) {
        if (listet[0][j] & curtable) {
            /* The character is in the current table */
            chainet[wnet++] = listet[1][j];
        } else {
            /* Obliged to change table */
            int newtable;
            if (j == (length - 1) || !(listet[0][j] & listet[0][j + 1])) {
                /* we change only one character - look for temporary switch */
                if ((listet[0][j] & 1) && (curtable == 2)) { /* T_UPP */
                    chainet[wnet++] = 27;
                    chainet[wnet++] = listet[1][j];
                    continue;
                }
                if (listet[0][j] & 8) { /* T_PUN (T_PUN and T_UPP not both possible) */
                    chainet[wnet++] = 29;
                    chainet[wnet++] = listet[1][j];
                    continue;
                }
                /* No temporary switch available */
                newtable = listet[0][j];
            } else {
                newtable = listet[0][j] & listet[0][j + 1];
            }

            /* 599 */

            /* Maintain the first if several tables are possible */
            if (newtable == 7) {
                newtable = 1;
            } else if (newtable == 12) {
                newtable = 4;
            }

            /* 619 - select the switch */
            switch (curtable) {
                case 1:
                    switch (newtable) {
                        case 2: chainet[wnet++] = 27;
                            break;
                        case 4: chainet[wnet++] = 28;
                            break;
                        case 8: chainet[wnet++] = 28;
                            chainet[wnet++] = 25;
                            break;
                    }
                    break;
                case 2:
                    switch (newtable) {
                        case 1: chainet[wnet++] = 28;
                            chainet[wnet++] = 28;
                            break;
                        case 4: chainet[wnet++] = 28;
                            break;
                        case 8: chainet[wnet++] = 28;
                            chainet[wnet++] = 25;
                            break;
                    }
                    break;
                case 4:
                    switch (newtable) {
                        case 1: chainet[wnet++] = 28;
                            break;
                        case 2: chainet[wnet++] = 27;
                            break;
                        case 8: chainet[wnet++] = 25;
                            break;
                    }
                    break;
                case 8:
                    switch (newtable) {
                        case 1: chainet[wnet++] = 29;
                            break;
                        case 2: chainet[wnet++] = 29;
                            chainet[wnet++] = 27;
                            break;
                        case 4: chainet[wnet++] = 29;
                            chainet[wnet++] = 28;
                            break;
                    }
                    break;
            }
            curtable = newtable;
            /* 659 - at last we add the character */
            chainet[wnet++] = listet[1][j];
        }
    }

    /* 663 */
    if (wnet & 1) {
        chainet[wnet++] = 29;
    }
    /* Now translate the string chainet into codewords */

    /* Default mode for PDF417 is Text Compaction Alpha (ISO/IEC 1543:2015 5.4.2.1), and for MICROPDF417 is Byte
     * Compaction (ISO/IEC 24728:2006 5.4.3), so only add flag if not first codeword or is MICROPDF417 */
    if (*mclength > 1 || is_micro) {
        chainemc[(*mclength)++] = 900;
    }

    for (j = 0; j < wnet; j += 2) {
        const int cw_number = (30 * chainet[j]) + chainet[j + 1];
        chainemc[(*mclength)++] = cw_number;
    }
}

/* 671 */
INTERNAL void pdf_byteprocess(int *chainemc, int *mclength, const unsigned char chaine[], int start, const int length,
                const int debug_print) {

    if (debug_print) printf("\nEntering byte mode at position %d\n", start);

    if (length == 1) {
        chainemc[(*mclength)++] = 913;
        chainemc[(*mclength)++] = chaine[start];
        if (debug_print) {
            printf("913 %d\n", chainemc[*mclength - 1]);
        }
    } else {
        int len;
        /* select the switch for multiple of 6 bytes */
        if (length % 6 == 0) {
            chainemc[(*mclength)++] = 924;
            if (debug_print) printf("924 ");
        } else {
            /* Default mode for MICROPDF417 is Byte Compaction (ISO/IEC 24728:2006 5.4.3), but not emitting it
             * depends on whether an ECI has been emitted previously (or not) it appears, so simpler and safer
             * to always emit it. */
            chainemc[(*mclength)++] = 901;
            if (debug_print) printf("901 ");
        }

        len = 0;

        while (len < length) {
            uint64_t total;
            unsigned int chunkLen = length - len;
            if (6 <= chunkLen) { /* Take groups of 6 */
                chunkLen = 6;
                len += chunkLen;
                total = 0;

                while (chunkLen--) {
                    const uint64_t mantisa = chaine[start++];
                    total |= mantisa << (chunkLen * 8);
                }

                chunkLen = 5;

                while (chunkLen--) {
                    chainemc[*mclength + chunkLen] = (int) (total % 900);
                    total /= 900;
                }
                *mclength += 5;
            } else { /* If there remains a group of less than 6 bytes */
                len += chunkLen;
                while (chunkLen--) {
                    chainemc[(*mclength)++] = chaine[start++];
                }
            }
        }
    }
}

/* 712 */
static void pdf_numbprocess(int *chainemc, int *mclength, const unsigned char chaine[], int start, const int length) {
    int j, loop, dummy[50] = {0}, diviseur, nombre;
    char chainemod[45];

    chainemc[(*mclength)++] = 902;

    j = 0;
    while (j < length) {
        int longueur;
        int dumlength = 0;
        int p, len;
        longueur = length - j;
        if (longueur > 44) {
            longueur = 44;
        }
        len = longueur + 1;
        chainemod[0] = 1;
        for (loop = 1; loop < len; loop++) {
            chainemod[loop] = ctoi(chaine[start + loop + j - 1]);
        }
        do {
            diviseur = 900;

            /* 877 - gosub Modulo */
            p = 0;
            nombre = 0;
            for (loop = 0; loop < len; loop++) {
                nombre *= 10;
                nombre += chainemod[loop];
                if (nombre < diviseur) {
                    if (p) {
                        chainemod[p++] = 0;
                    }
                } else {
                    chainemod[p++] = (nombre / diviseur);
                    nombre = nombre % diviseur;
                }
            }
            diviseur = nombre;
            /* return to 723 */

            dummy[dumlength++] = diviseur;
            len = p;
        } while (p);
        for (loop = dumlength - 1; loop >= 0; loop--) {
            chainemc[(*mclength)++] = dummy[loop];
        }
        j += longueur;
    }
}

/* Initial processing of data, shared by `pdf417()` and `micropdf417()` */
static int pdf_initial(struct zint_symbol *symbol, unsigned char chaine[], const int length, const int is_micro,
            int chainemc[PDF_MAX_LEN], int *p_mclength, int structapp_cws[18], int *p_structapp_cp) {
    int i, indexchaine, indexliste, mode;
    int liste[2][PDF_MAX_LEN] = {{0}};
    int mclength, structapp_cp = 0;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    /* 456 */
    indexliste = 0;
    indexchaine = 0;

    mode = pdf_quelmode(chaine[indexchaine]);

    /* 463 */
    do {
        liste[1][indexliste] = mode;
        while ((liste[1][indexliste] == mode) && (indexchaine < length)) {
            liste[0][indexliste]++;
            indexchaine++;
            mode = pdf_quelmode(chaine[indexchaine]);
        }
        indexliste++;
    } while (indexchaine < length);

    /* 474 */
    pdf_smooth(liste, &indexliste);

    if (debug_print) {
        printf("Initial block pattern:\n");
        for (i = 0; i < indexliste; i++) {
            printf("Len: %d  Type: ", liste[0][i]);
            switch (liste[1][i]) {
                case TEX: printf("Text\n");
                    break;
                case BYT: printf("Byte\n");
                    break;
                case NUM: printf("Number\n");
                    break;
                default: printf("ERROR\n"); /* Should never happen */ /* Not reached */
                    break;
            }
        }
    }

    /* 541 - now compress the data */
    indexchaine = 0;
    mclength = is_micro ? 0 : 1; /* Allow for length descriptor for full symbol */

    if (symbol->structapp.count) {
        int id_cnt = 0, ids[10];

        if (symbol->structapp.count < 2 || symbol->structapp.count > 99999) {
            strcpy(symbol->errtxt, "740: Structured Append count out of range (2-99999)");
            return ZINT_ERROR_INVALID_OPTION;
        }
        if (symbol->structapp.index < 1 || symbol->structapp.index > symbol->structapp.count) {
            sprintf(symbol->errtxt, "741: Structured Append index out of range (1-%d)", symbol->structapp.count);
            return ZINT_ERROR_INVALID_OPTION;
        }
        if (symbol->structapp.id[0]) {
            int id_len;

            for (id_len = 0; id_len < 32 && symbol->structapp.id[id_len]; id_len++);

            if (id_len > 30) { /* 10 triplets */
                strcpy(symbol->errtxt, "742: Structured Append ID too long (30 digit maximum)");
                return ZINT_ERROR_INVALID_OPTION;
            }

            for (i = 0; i < id_len; i += 3, id_cnt++) {
                const int triplet_len = i + 3 < id_len ? 3 : id_len - i;
                ids[id_cnt] = to_int((const unsigned char *) (symbol->structapp.id + i), triplet_len);
                if (ids[id_cnt] == -1) {
                    strcpy(symbol->errtxt, "743: Invalid Structured Append ID (digits only)");
                    return ZINT_ERROR_INVALID_OPTION;
                }
                if (ids[id_cnt] > 899) {
                    sprintf(symbol->errtxt, "744: Structured Append ID triplet %d '%03d' out of range (000-899)",
                            id_cnt + 1, ids[id_cnt]);
                    return ZINT_ERROR_INVALID_OPTION;
                }
            }
        }
        structapp_cws[structapp_cp++] = 928; /* Macro marker */
        structapp_cws[structapp_cp++] = (100000 + symbol->structapp.index - 1) / 900; /* Segment index 1 */
        structapp_cws[structapp_cp++] = (100000 + symbol->structapp.index - 1) % 900; /* Segment index 2 */
        for (i = 0; i < id_cnt; i++) {
            structapp_cws[structapp_cp++] = ids[i];
        }
        structapp_cws[structapp_cp++] = 923; /* Optional field */
        structapp_cws[structapp_cp++] = 1; /* Segment count tag */
        structapp_cws[structapp_cp++] = (100000 + symbol->structapp.count) / 900; /* Segment count 1 */
        structapp_cws[structapp_cp++] = (100000 + symbol->structapp.count) % 900; /* Segment count 2 */
        if (symbol->structapp.index == symbol->structapp.count) {
            structapp_cws[structapp_cp++] = 922; /* Special last segment terminator */
        }
    }

    if (symbol->output_options & READER_INIT) {
        chainemc[mclength++] = 921; /* Reader Initialisation */
    }

    if (symbol->eci != 0) {
        if (symbol->eci > 811799) {
            strcpy(symbol->errtxt, "472: Invalid ECI");
            return ZINT_ERROR_INVALID_OPTION;
        }
        /* Encoding ECI assignment number, according to Table 8 */
        if (symbol->eci <= 899) {
            chainemc[mclength++] = 927; /* ECI */
            chainemc[mclength++] = symbol->eci;
        } else if (symbol->eci <= 810899) {
            chainemc[mclength++] = 926; /* ECI */
            chainemc[mclength++] = (symbol->eci / 900) - 1;
            chainemc[mclength++] = symbol->eci % 900;
        } else {
            chainemc[mclength++] = 925; /* ECI */
            chainemc[mclength++] = symbol->eci - 810900;
        }
    }

    for (i = 0; i < indexliste; i++) {
        switch (liste[1][i]) {
            case TEX: /* 547 - text mode */
                pdf_textprocess(chainemc, &mclength, chaine, indexchaine, liste[0][i], is_micro);
                break;
            case BYT: /* 670 - octet stream mode */
                pdf_byteprocess(chainemc, &mclength, chaine, indexchaine, liste[0][i], debug_print);
                break;
            case NUM: /* 712 - numeric mode */
                pdf_numbprocess(chainemc, &mclength, chaine, indexchaine, liste[0][i]);
                break;
        }
        indexchaine = indexchaine + liste[0][i];
    }

    *p_mclength = mclength;
    *p_structapp_cp = structapp_cp;

    return 0;
}

/* 366 */
static int pdf_enc(struct zint_symbol *symbol, unsigned char chaine[], const int length) {
    int i, j, longueur, loop, mccorrection[520] = {0}, offset;
    int total, chainemc[PDF_MAX_LEN], mclength, c1, c2, c3, dummy[35];
    int rows, cols, ecc, ecc_cws, padding;
    char pattern[580];
    int bp = 0;
    int structapp_cws[18] = {0}; /* 3 (Index) + 10 (ID) + 4 (Count) + 1 (Last) */
    int structapp_cp = 0;
    int error_number;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    static const int ecc_num_cws[] = { 2, 4, 8, 16, 32, 64, 128, 256, 512 };

    if (length > PDF_MAX_LEN) {
        strcpy(symbol->errtxt, "463: Input string too long");
        return ZINT_ERROR_TOO_LONG;
    }

    error_number = pdf_initial(symbol, chaine, length, 0 /*is_micro*/, chainemc, &mclength, structapp_cws,
                    &structapp_cp);
    if (error_number) { /* Only errors return >= ZINT_ERROR */
        return error_number;
    }

    if (debug_print) {
        printf("\nCompressed data stream (%d):\n", mclength - 1);
        for (i = 1; i < mclength; i++) { /* Skip unset length descriptor */
            printf("%d ", chainemc[i]);
        }
        printf("\n\n");
    }

    /* 752 - Now take care of the number of CWs per row */

    /* ECC */
    ecc = symbol->option_1;
    if (ecc < 0) { /* If not specified, set ECC depending on no. of codewords */
        const int data_cws = mclength - 1 + structapp_cp; /* -1 for length descriptor */
        /* ISO/IEC 15438:2015 Annex E Table E.1 Recommended minima */
        if (data_cws <= 40) {
            ecc = 2;
        } else if (data_cws <= 160) {
            ecc = 3;
        } else if (data_cws <= 320) {
            ecc = 4;
        } else if (data_cws <= 863) {
            ecc = 5;
        } else {
            ecc = 6; /* Not mentioned in Table E.1 */
        }
    }
    ecc_cws = ecc_num_cws[ecc];

    longueur = mclength + structapp_cp + ecc_cws;

    if (debug_print) printf("Total No. of Codewords: %d, ECC %d, No. of ECC Codewords: %d\n", longueur, ecc, ecc_cws);

    if (longueur > 928) {
        /* Enforce maximum codeword limit */
        strcpy(symbol->errtxt, "464: Input string too long");
        return ZINT_ERROR_TOO_LONG;
    }

    cols = symbol->option_2;
    rows = symbol->option_3;
    if (rows) { /* Rows given */
        if (cols < 1) { /* Cols automatic */
            cols = (longueur + rows - 1) / rows;
            if (cols <= 1) {
                cols = 1;
            } else {
                /* Increase rows if would need > 30 columns */
                for (; cols > 30 && rows < 90; rows++, cols = (longueur + rows - 1) / rows);
                assert(cols <= 30);
                /* Increase rows if multiple too big */
                for (; cols >= 1 && rows < 90 && rows * cols > 928; rows++, cols = (longueur + rows - 1) / rows);
                if (rows * cols > 928) {
                    strcpy(symbol->errtxt, "465: Data too long for specified number of rows");
                    return ZINT_ERROR_TOO_LONG;
                }
            }
        } else { /* Cols given */
            /* Increase rows if multiple too big */
            for (; rows <= 90 && rows * cols < longueur; rows++);
            if (rows > 90 || rows * cols > 928) {
                strcpy(symbol->errtxt, "745: Data too long for specified number of columns");
                return ZINT_ERROR_TOO_LONG;
            }
        }
        if (rows != symbol->option_3) {
            sprintf(symbol->errtxt, "746: Rows increased from %d to %d", symbol->option_3, rows);
            error_number = ZINT_WARN_INVALID_OPTION;
        }
    } else { /* Rows automatic, cols automatic or given */
        if (cols < 1) { /* Cols automatic */
            cols = (int) round(sqrt((longueur - 1) / 3.0)); /* -1 (length descriptor) for back-compatibility */
        }
        rows = (longueur + cols - 1) / cols;
        if (rows <= 3) {
            rows = 3;
        } else {
            /* Increase cols if would need > 90 rows - do this even if cols specified for better back-compatibility
               (though previously only increased once) */
            for (; rows > 90 && cols < 30; cols++, rows = (longueur + cols - 1) / cols);
            assert(rows <= 90);
            /* Increase cols if multiple too big */
            for (; rows >= 3 && cols < 30 && rows * cols > 928; cols++, rows = (longueur + cols - 1) / cols);
            if (rows * cols > 928) {
                strcpy(symbol->errtxt, "747: Data too long for specified number of columns");
                return ZINT_ERROR_TOO_LONG;
            }
            if (symbol->option_2 && cols != symbol->option_2) { /* Note previously did not warn if cols auto-upped */
                sprintf(symbol->errtxt, "748: Columns increased from %d to %d", symbol->option_2, cols);
                error_number = ZINT_WARN_INVALID_OPTION;
            }
        }
    }
    assert(rows * cols >= longueur);

    /* 781 - Padding calculation */
    padding = rows * cols - longueur;

    /* We add the padding */
    for (i = 0; i < padding; i++) {
        chainemc[mclength++] = 900;
    }
    if (debug_print) printf("Padding: %d\n", padding);

    /* We add the Structured Append Macro Control Block if any */
    if (structapp_cp) {
        for (i = 0; i < structapp_cp; i++) {
            chainemc[mclength++] = structapp_cws[i];
        }
    }

    /* Set the length descriptor */
    chainemc[0] = mclength;

    /* 796 - we now take care of the Reed Solomon codes */
    switch (ecc) {
        case 1: offset = 2;
            break;
        case 2: offset = 6;
            break;
        case 3: offset = 14;
            break;
        case 4: offset = 30;
            break;
        case 5: offset = 62;
            break;
        case 6: offset = 126;
            break;
        case 7: offset = 254;
            break;
        case 8: offset = 510;
            break;
        default: offset = 0;
            break;
    }

    for (i = 0; i < mclength; i++) {
        total = (chainemc[i] + mccorrection[ecc_cws - 1]) % 929;
        for (j = ecc_cws - 1; j > 0; j--) {
            mccorrection[j] = (mccorrection[j - 1] + 929 - (total * pdf_coefrs[offset + j]) % 929) % 929;
        }
        mccorrection[0] = (929 - (total * pdf_coefrs[offset]) % 929) % 929;
    }

    /* we add these codes to the string */
    for (i = ecc_cws - 1; i >= 0; i--) {
        chainemc[mclength++] = mccorrection[i] ? 929 - mccorrection[i] : 0;
    }

    if (debug_print) {
        printf("Complete CW string (%d):\n", mclength);
        for (i = 0; i < mclength; i++) {
            printf("%d ", chainemc[i]);
        }
        printf("\n");
    }
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) {
        debug_test_codeword_dump_int(symbol, chainemc, mclength);
    }
#endif

    if (debug_print) printf("\nSymbol size:\n%d columns x %d rows\n", cols, rows);

    /* 818 - The CW string is finished */
    c1 = (rows - 1) / 3;
    c2 = ecc * 3 + (rows - 1) % 3;
    c3 = cols - 1;

    /* we now encode each row */
    for (i = 0; i < rows; i++) {
        const int k = (i / 3) * 30;
        bp = 0;
        for (j = 0; j < cols; j++) {
            dummy[j + 1] = chainemc[i * cols + j];
        }
        switch (i % 3) {
            case 0:
                dummy[0] = k + c1;
                dummy[cols + 1] = k + c3;
                offset = 0; /* cluster(0) */
                break;
            case 1:
                dummy[0] = k + c2;
                dummy[cols + 1] = k + c1;
                offset = 929; /* cluster(3) */
                break;
            case 2:
                dummy[0] = k + c3;
                dummy[cols + 1] = k + c2;
                offset = 1858; /* cluster(6) */
                break;
        }
        bp = bin_append_posn(0x1FEA8, 17, pattern, bp); /* Row start */

        for (j = 0; j <= cols; j++) {
            bp = bin_append_posn(pdf_bitpattern[offset + dummy[j]], 16, pattern, bp);
            pattern[bp++] = '0';
        }

        if (symbol->symbology != BARCODE_PDF417COMP) {
            bp = bin_append_posn(pdf_bitpattern[offset + dummy[j]], 16, pattern, bp);
            pattern[bp++] = '0';
            bp = bin_append_posn(0x3FA29, 18, pattern, bp); /* Row Stop */
        } else {
            pattern[bp++] = '1'; /* Compact PDF417 Stop pattern */
        }

        for (loop = 0; loop < bp; loop++) {
            if (pattern[loop] == '1') {
                set_module(symbol, i, loop);
            }
        }
    }
    symbol->width = bp;
    symbol->rows = rows;

    /* ISO/IEC 15438:2015 Section 5.8.2 3X minimum row height */
    if (error_number) {
        (void) set_height(symbol, 3.0f, 0.0f, 0.0f, 1 /*no_errtxt*/);
    } else {
        error_number = set_height(symbol, 3.0f, 0.0f, 0.0f, 0 /*no_errtxt*/);
    }

    /* 843 */
    return error_number;
}

/* 345 */
INTERNAL int pdf417(struct zint_symbol *symbol, unsigned char source[], int length) {
    int codeerr, error_number;

    error_number = 0;

    if ((symbol->option_1 < -1) || (symbol->option_1 > 8)) {
        strcpy(symbol->errtxt, "460: Security value out of range");
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return ZINT_ERROR_INVALID_OPTION;
        }
        symbol->option_1 = -1;
        error_number = ZINT_WARN_INVALID_OPTION;
    }
    if ((symbol->option_2 < 0) || (symbol->option_2 > 30)) {
        strcpy(symbol->errtxt, "461: Number of columns out of range (1 to 30)");
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return ZINT_ERROR_INVALID_OPTION;
        }
        symbol->option_2 = 0;
        error_number = ZINT_WARN_INVALID_OPTION;
    }
    if (symbol->option_3 && (symbol->option_3 < 3 || symbol->option_3 > 90)) {
        strcpy(symbol->errtxt, "466: Number of rows out of range (3 to 90)");
        return ZINT_ERROR_INVALID_OPTION;
    }
    if (symbol->option_2 && symbol->option_3 && symbol->option_2 * symbol->option_3 > 928) {
        strcpy(symbol->errtxt, "475: Columns x rows out of range (1 to 928)");
        return ZINT_ERROR_INVALID_OPTION;
    }

    /* 349 */
    codeerr = pdf_enc(symbol, source, length);

    /* 352 */
    if (codeerr != 0) {
        error_number = codeerr;
    }

    /* 364 */
    return error_number;
}

/* like PDF417 only much smaller! */
INTERNAL int micropdf417(struct zint_symbol *symbol, unsigned char chaine[], int length) {
    int i, k, j, longueur, mccorrection[50] = {0}, offset;
    int total, chainemc[PDF_MAX_LEN], mclength, error_number = 0;
    char pattern[580];
    int bp = 0;
    int structapp_cws[18] = {0}; /* 3 (Index) + 10 (ID) + 4 (Count) + 1 (Last) */
    int structapp_cp = 0;
    int variant, LeftRAPStart, CentreRAPStart, RightRAPStart, StartCluster;
    int LeftRAP, CentreRAP, RightRAP, Cluster, loop;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    if (length > MICRO_PDF_MAX_LEN) {
        strcpy(symbol->errtxt, "474: Input data too long");
        return ZINT_ERROR_TOO_LONG;
    }
    if (symbol->option_3) {
        strcpy(symbol->errtxt, "476: Cannot specify rows for MicroPDF417");
        return ZINT_ERROR_INVALID_OPTION;
    }

    /* Encoding starts out the same as PDF417, so use the same code */

    error_number = pdf_initial(symbol, chaine, length, 1 /*is_micro*/, chainemc, &mclength, structapp_cws,
                    &structapp_cp);
    if (error_number) { /* Only errors return >= ZINT_ERROR */
        return error_number;
    }

    /* This is where it all changes! */

    if (mclength + structapp_cp > 126) {
        strcpy(symbol->errtxt, "467: Input data too long");
        return ZINT_ERROR_TOO_LONG;
    }
    if (symbol->option_2 > 4) {
        strcpy(symbol->errtxt, "468: Specified width out of range");
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return ZINT_ERROR_INVALID_OPTION;
        }
        symbol->option_2 = 0;
        error_number = ZINT_WARN_INVALID_OPTION;
    }

    if (debug_print) {
        printf("\nEncoded Data Stream (%d):\n", mclength);
        for (i = 0; i < mclength; i++) {
            printf("%3d ", chainemc[i]);
        }
        printf("\n");
    }

    /* Now figure out which variant of the symbol to use and load values accordingly */

    variant = 0;

    if ((symbol->option_2 == 1) && (mclength + structapp_cp > 20)) {
        /* the user specified 1 column but the data doesn't fit - go to automatic */
        strcpy(symbol->errtxt, "469: Specified symbol size too small for data");
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return ZINT_ERROR_INVALID_OPTION;
        }
        symbol->option_2 = 0;
        error_number = ZINT_WARN_INVALID_OPTION;
    }

    if ((symbol->option_2 == 2) && (mclength + structapp_cp > 37)) {
        /* the user specified 2 columns but the data doesn't fit - go to automatic */
        strcpy(symbol->errtxt, "470: Specified symbol size too small for data");
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return ZINT_ERROR_INVALID_OPTION;
        }
        symbol->option_2 = 0;
        error_number = ZINT_WARN_INVALID_OPTION;
    }

    if ((symbol->option_2 == 3) && (mclength + structapp_cp > 82)) {
        /* the user specified 3 columns but the data doesn't fit - go to automatic */
        strcpy(symbol->errtxt, "471: Specified symbol size too small for data");
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return ZINT_ERROR_INVALID_OPTION;
        }
        symbol->option_2 = 0;
        error_number = ZINT_WARN_INVALID_OPTION;
    }

    if (symbol->option_2 == 1) {
        /* the user specified 1 column and the data does fit */
        if (mclength + structapp_cp <= 4) {
            variant = 1;
        } else if (mclength + structapp_cp <= 7) {
            variant = 2;
        } else if (mclength + structapp_cp <= 10) {
            variant = 3;
        } else if (mclength + structapp_cp <= 12) {
            variant = 4;
        } else if (mclength + structapp_cp <= 16) {
            variant = 5;
        } else {
            variant = 6;
        }
    } else if (symbol->option_2 == 2) {
        /* the user specified 2 columns and the data does fit */
        if (mclength + structapp_cp <= 8) {
            variant = 7;
        } else if (mclength + structapp_cp <= 13) {
            variant = 8;
        } else if (mclength + structapp_cp <= 19) {
            variant = 9;
        } else if (mclength + structapp_cp <= 24) {
            variant = 10;
        } else if (mclength + structapp_cp <= 29) {
            variant = 11;
        } else if (mclength + structapp_cp <= 33) {
            variant = 12;
        } else {
            variant = 13;
        }
    } else if (symbol->option_2 == 3) {
        /* the user specified 3 columns and the data does fit */
        if (mclength + structapp_cp <= 6) {
            variant = 14;
        } else if (mclength + structapp_cp <= 10) {
            variant = 15;
        } else if (mclength + structapp_cp <= 14) {
            variant = 16;
        } else if (mclength + structapp_cp <= 18) {
            variant = 17;
        } else if (mclength + structapp_cp <= 24) {
            variant = 18;
        } else if (mclength + structapp_cp <= 34) {
            variant = 19;
        } else if (mclength + structapp_cp <= 46) {
            variant = 20;
        } else if (mclength + structapp_cp <= 58) {
            variant = 21;
        } else if (mclength + structapp_cp <= 70) {
            variant = 22;
        } else {
            variant = 23;
        }
    } else if (symbol->option_2 == 4) {
        /* the user specified 4 columns and the data does fit */
        if (mclength + structapp_cp <= 8) {
            variant = 24;
        } else if (mclength + structapp_cp <= 12) {
            variant = 25;
        } else if (mclength + structapp_cp <= 18) {
            variant = 26;
        } else if (mclength + structapp_cp <= 24) {
            variant = 27;
        } else if (mclength + structapp_cp <= 30) {
            variant = 28;
        } else if (mclength + structapp_cp <= 39) {
            variant = 29;
        } else if (mclength + structapp_cp <= 54) {
            variant = 30;
        } else if (mclength + structapp_cp <= 72) {
            variant = 31;
        } else if (mclength + structapp_cp <= 90) {
            variant = 32;
        } else if (mclength + structapp_cp <= 108) {
            variant = 33;
        } else {
            variant = 34;
        }
    } else {
        /* Zint can choose automatically from all available variations */
        for (i = 27; i >= 0; i--) {
            /* Note mclength + structapp_cp <= 126 and pdf_MicroAutosize[27] == 126 so variant will be set */
            if (pdf_MicroAutosize[i] >= mclength + structapp_cp) {
                variant = pdf_MicroAutosize[i + 28];
            } else {
                break;
            }
        }
    }
    assert(variant > 0); /* Suppress clang-tidy clang-analyzer-core.uninitialized.Assign */

    /* Now we have the variant we can load the data */
    variant--;
    symbol->option_2 = pdf_MicroVariants[variant]; /* columns */
    symbol->rows = pdf_MicroVariants[variant + 34]; /* rows */
    k = pdf_MicroVariants[variant + 68]; /* number of EC CWs */
    longueur = (symbol->option_2 * symbol->rows) - k; /* number of non-EC CWs */
    i = longueur - (mclength + structapp_cp); /* amount of padding required */
    offset = pdf_MicroVariants[variant + 102]; /* coefficient offset */

    if (debug_print) {
        printf("\nChoose symbol size:\n");
        printf("%d columns x %d rows, variant %d\n", symbol->option_2, symbol->rows, variant + 1);
        printf("%d data codewords (including %d pads), %d ecc codewords\n", longueur, i, k);
        printf("\n");
    }

    /* We add the padding */
    while (i > 0) {
        chainemc[mclength++] = 900;
        i--;
    }

    /* We add the Structured Append Macro Control Block if any */
    if (structapp_cp) {
        for (i = 0; i < structapp_cp; i++) {
            chainemc[mclength++] = structapp_cws[i];
        }
    }

    /* Reed-Solomon error correction */
    longueur = mclength;
    for (i = 0; i < longueur; i++) {
        total = (chainemc[i] + mccorrection[k - 1]) % 929;
        for (j = k - 1; j >= 0; j--) {
            if (j == 0) {
                mccorrection[j] = (929 - (total * pdf_Microcoeffs[offset + j]) % 929) % 929;
            } else {
                mccorrection[j] = (mccorrection[j - 1] + 929 - (total * pdf_Microcoeffs[offset + j]) % 929) % 929;
            }
        }
    }

    for (j = 0; j < k; j++) {
        if (mccorrection[j] != 0) {
            mccorrection[j] = 929 - mccorrection[j];
        }
    }
    /* we add these codes to the string */
    for (i = k - 1; i >= 0; i--) {
        chainemc[mclength++] = mccorrection[i];
    }

    if (debug_print) {
        printf("Encoded Data Stream with ECC (%d):\n", mclength);
        for (i = 0; i < mclength; i++) {
            printf("%3d ", chainemc[i]);
        }
        printf("\n");
    }
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) {
        debug_test_codeword_dump_int(symbol, chainemc, mclength);
    }
#endif

    /* Now get the RAP (Row Address Pattern) start values */
    LeftRAPStart = pdf_RAPTable[variant];
    CentreRAPStart = pdf_RAPTable[variant + 34];
    RightRAPStart = pdf_RAPTable[variant + 68];
    StartCluster = pdf_RAPTable[variant + 102] / 3;

    /* That's all values loaded, get on with the encoding */

    LeftRAP = LeftRAPStart;
    CentreRAP = CentreRAPStart;
    RightRAP = RightRAPStart;
    Cluster = StartCluster;
    /* Cluster can be 0, 1 or 2 for Cluster(0), Cluster(3) and Cluster(6) */

    if (debug_print) printf("\nInternal row representation:\n");
    for (i = 0; i < symbol->rows; i++) {
        if (debug_print) printf("row %d: ", i);
        bp = 0;
        offset = 929 * Cluster;
        k = i * symbol->option_2;

        /* Copy the data into codebarre */
        bp = bin_append_posn(pdf_rap_side[LeftRAP - 1], 10, pattern, bp);
        bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k]], 16, pattern, bp);
        pattern[bp++] = '0';
        if (symbol->option_2 >= 2) {
            if (symbol->option_2 == 3) {
                bp = bin_append_posn(pdf_rap_centre[CentreRAP - 1], 10, pattern, bp);
            }
            bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k + 1]], 16, pattern, bp);
            pattern[bp++] = '0';
            if (symbol->option_2 >= 3) {
                if (symbol->option_2 == 4) {
                    bp = bin_append_posn(pdf_rap_centre[CentreRAP - 1], 10, pattern, bp);
                }
                bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k + 2]], 16, pattern, bp);
                pattern[bp++] = '0';
                if (symbol->option_2 == 4) {
                    bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k + 3]], 16, pattern, bp);
                    pattern[bp++] = '0';
                }
            }
        }
        bp = bin_append_posn(pdf_rap_side[RightRAP - 1], 10, pattern, bp);
        pattern[bp++] = '1'; /* stop */
        if (debug_print) printf("%.*s\n", bp, pattern);

        /* so now pattern[] holds the string of '1's and '0's. - copy this to the symbol */
        for (loop = 0; loop < bp; loop++) {
            if (pattern[loop] == '1') {
                set_module(symbol, i, loop);
            }
        }

        /* Set up RAPs and Cluster for next row */
        LeftRAP++;
        CentreRAP++;
        RightRAP++;
        Cluster++;

        if (LeftRAP == 53) {
            LeftRAP = 1;
        }
        if (CentreRAP == 53) {
            CentreRAP = 1;
        }
        if (RightRAP == 53) {
            RightRAP = 1;
        }
        if (Cluster == 3) {
            Cluster = 0;
        }
    }
    symbol->width = bp;

    /* ISO/IEC 24728:2006 Section 5.8.2 2X minimum row height */
    if (error_number) {
        (void) set_height(symbol, 2.0f, 0.0f, 0.0f, 1 /*no_errtxt*/);
    } else {
        error_number = set_height(symbol, 2.0f, 0.0f, 0.0f, 0 /*no_errtxt*/);
    }

    return error_number;
}

#undef TEX
#undef BYT
#undef NUM
