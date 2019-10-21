/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Test suite for ncbi_blowfish.[ch]
 *
 */

#include <connect/ncbi_blowfish.h>
#include <connect/ncbi_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_assert.h"  /* This header must go last */


#define SizeOf(a)  (sizeof(a) / sizeof((a)[0]))


static void selftest(void)
{
    /* Standard test vectors */
    static const struct {
        char key [17];
        char text[17];
        char data[17];
    } tests[] = {
        { "0000000000000000", "0000000000000000", "4EF997456198DD78" },
        { "FFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF", "51866FD5B85ECB8A" },
        { "3000000000000000", "1000000000000001", "7D856F9A613063F2" },
        { "1111111111111111", "1111111111111111", "2466DD878B963C9D" },
        { "0123456789ABCDEF", "1111111111111111", "61F9C3802281B096" },
        { "1111111111111111", "0123456789ABCDEF", "7D0CC630AFDA1EC7" },
        { "0000000000000000", "0000000000000000", "4EF997456198DD78" },
        { "FEDCBA9876543210", "0123456789ABCDEF", "0ACEAB0FC6A0A28D" },
        { "7CA110454A1A6E57", "01A1D6D039776742", "59C68245EB05282B" },
        { "0131D9619DC1376E", "5CD54CA83DEF57DA", "B1B8CC0B250F09A0" },
        { "07A1133E4A0B2686", "0248D43806F67172", "1730E5778BEA1DA4" },
        { "3849674C2602319E", "51454B582DDF440A", "A25E7856CF2651EB" },
        { "04B915BA43FEB5B6", "42FD443059577FA2", "353882B109CE8F1A" },
        { "0113B970FD34F2CE", "059B5E0851CF143A", "48F4D0884C379918" },
        { "0170F175468FB5E6", "0756D8E0774761D2", "432193B78951FC98" },
        { "43297FAD38E373FE", "762514B829BF486A", "13F04154D69D1AE5" },
        { "07A7137045DA2A16", "3BDD119049372802", "2EEDDA93FFD39C79" },
        { "04689104C2FD3B2F", "26955F6835AF609A", "D887E0393C2DA6E3" },
        { "37D06BB516CB7546", "164D5E404F275232", "5F99D04F5B163969" },
        { "1F08260D1AC2465E", "6B056E18759F5CCA", "4A057A3B24D3977B" },
        { "584023641ABA6176", "004BD6EF09176062", "452031C1E4FADA8E" },
        { "025816164629B007", "480D39006EE762F2", "7555AE39F59B87BD" },
        { "49793EBC79B3258F", "437540C8698F3CFA", "53C55F9CB49FC019" },
        { "4FB05E1515AB73A7", "072D43A077075292", "7A8E7BFA937E89A3" },
        { "49E95D6D4CA229BF", "02FE55778117F12A", "CF9C5D7A4986ADB5" },
        { "018310DC409B26D6", "1D9D5C5018F728C2", "D1ABB290658BC778" },
        { "1C587F1C13924FEF", "305532286D6F295A", "55CB3774D13EF201" },
        { "0101010101010101", "0123456789ABCDEF", "FA34EC4847B268B2" },
        { "1F1F1F1F0E0E0E0E", "0123456789ABCDEF", "A790795108EA3CAE" },
        { "E0FEE0FEF1FEF1FE", "0123456789ABCDEF", "C39E072D9FAC631D" },
        { "0000000000000000", "FFFFFFFFFFFFFFFF", "014933E0CDAFF6E4" },
        { "FFFFFFFFFFFFFFFF", "0000000000000000", "F21E9A77B71C49BC" },
        { "0123456789ABCDEF", "0000000000000000", "245946885754369A" },
        { "FEDCBA9876543210", "FFFFFFFFFFFFFFFF", "6B5C5A9C5D9E0A5A" }
    };
    size_t i, n;

    putchar('\n');
    for (n = 0;  n < SizeOf(tests);  ++n) {
        Uint8 text, data, temp;
        NCBI_BLOWFISH bf;
        char key[8];
        int d;
        temp = 0;
        for (i = 0;  i < 8;  ++i) {
            sscanf(tests[n].key + (i << 1), "%2x", &d);
            temp <<= 8;
            temp  |= (Uint1) d;
            key[i] = d;
        }
        text = 0;
        for (i = 0;  i < 8;  ++i) {
            sscanf(tests[n].text + (i << 1), "%2x", &d);
            text <<= 8;
            text  |= (Uint1) d;
        }
        data = 0;
        for (i = 0;  i < 8;  ++i) {
            sscanf(tests[n].data + (i << 1), "%2x", &d);
            data <<= 8;
            data  |= (Uint1) d;
        }
#if 0
        for (i = 0;  i < 132;  ++i)
            putchar('0' + i % 10);
        putchar('\n');
#endif
        printf("test vector %zu:\t"
			   "0x%016" NCBI_BIGCOUNT_FORMAT_SPEC_HEX_X "\t"
			   "0x%016" NCBI_BIGCOUNT_FORMAT_SPEC_HEX_X "\t"
			   "0x%016" NCBI_BIGCOUNT_FORMAT_SPEC_HEX_X "\n", n + 1,
               temp, text, data);
        bf = NcbiBlowfishInit(key, sizeof(key));
        assert(bf);
        temp = text;
        NcbiBlowfishEncrypt(bf, &temp);
        printf("\t\t\t\t\t\t\t\t"
               "0x%016" NCBI_BIGCOUNT_FORMAT_SPEC_HEX_X "\n", temp);
        assert(temp == data);
        NcbiBlowfishDecrypt(bf, &temp);
        assert(temp == text);
        NcbiBlowfishFini(bf);
        printf("\t\t\t\t\t\t\t\tPASSED\n\n");
    }
}


/*
 * Without command line arguments, runs a standard test.
 *
 * With the arguments, performs an encryption / decryption cycle of an
 * arbitrary text string with an arbitrary text key, like so:
 *
 * ./test_ncbi_blowfish "THE QUICK BROWN FOX JUMPED OVER THE LAZY DOG'S BACK 1234567890" "Secret Key"
 *
 */
int main(int argc, char* argv[])
{
    const char* str = argc > 1 ? argv[1] : 0;
    const char* key = argc > 2 ? argv[2] : 0;
    size_t slen = str ? strlen(str) : 0;
    size_t klen = key ? strlen(key) : 0;
    NCBI_BLOWFISH bf;
    size_t n, k, i;
    Uint8* data;

    if (!str  ||  !key) {
        selftest();
        return 0;
    }

    printf("STR=\"%.*s\"\tKEY=\"%.*s\"\n\n", (int) slen, str, (int) klen, key);
    if (!(bf = NcbiBlowfishInit(key, klen))) {
        perror("Cannot init cipher");
        return 1;
    }
    if (!(klen = (slen + sizeof(*data) - 1) / sizeof(*data))) {
        printf("Nothing to do!\n");
        return 0;
    }
    if (!(data = (Uint8*) malloc(klen * sizeof(*data)))) {
        perror("Cannot allocate memory");
        return 1;
    }

    i = n = 0;
    while (n < slen) {
        Uint8 X = 0;
        for (k = 0;  k < sizeof(X);  ++k) {
            X <<= 8;
            X  |= (Uint1) str[n++];
            if (n >= slen)
                break;
        }
        NcbiBlowfishEncrypt(bf, &X);
        data[i++] = X;
    }
    assert(i == klen);
    for (i = 0;  i < klen;  ++i)
        printf("\t0x%016" NCBI_BIGCOUNT_FORMAT_SPEC_HEX_X "\n", data[i]);
    putchar('\n');
    
    for (k = i = 0;  i < klen;  ++i) {
        Uint8 X = data[i];
        char s[sizeof(X)];
        NcbiBlowfishDecrypt(bf, &X);
        while (k < sizeof(X)) {
            char c = (char) X;
            if (!(s[k] = c))
                break;
            X >>= 8;
            ++k;
        }
        while (k > 0)
            putchar(s[--k]);
    }
    putchar('\n');

    free(data);
    NcbiBlowfishFini(bf);

    return 0;    
}
