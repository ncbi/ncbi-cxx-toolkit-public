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
 *          Dmitry Kazimirov (base64url variant)
 *
 * File Description:
 *   BASE-64 Encoding/Decoding
 *
 */

#include "ncbi_base64.h"


extern void BASE64_Encode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written,
 size_t*     line_len)
{
    static const char syms[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ" /*26*/
        "abcdefghijklmnopqrstuvwxyz" /*52*/
        "0123456789+/";              /*64*/
    const size_t max_len = line_len ? *line_len : 76;
    const size_t max_src =
        ((dst_size - (max_len ? dst_size/(max_len + 1) : 0)) >> 2) * 3;
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;
    size_t len = 0, i = 0, j = 0;
    unsigned char temp = 0, c;
    unsigned char shift = 2;
    if (!max_src  ||  !src_size) {
        *src_read    = 0;
        *dst_written = 0;
        if (dst_size > 0) {
            *dst = '\0';
        }
        return;
    }
    if (src_size > max_src) {
        src_size = max_src;
    }
    c = src[0];
    for (;;) {
        unsigned char bits = (c >> shift) & 0x3F;
        if (max_len  &&  len >= max_len) {
            dst[j++] = '\n';
            len = 0;
        }
        _ASSERT((size_t)(temp | bits) < sizeof(syms) - 1);
        dst[j++] = syms[temp | bits];
        len++;
        if (i >= src_size) {
            break;
        }
        shift += 2;
        shift &= 7;
        temp = (c << (8 - shift)) & 0x3F;
        if (shift) {
            c = ++i < src_size ? src[i] : 0;
        } else if (i + 1 == src_size) {
            i++;
        }
    }
    _ASSERT(j <= dst_size);
    *src_read = i;
    for (i = 0; i < (3 - src_size % 3) % 3; i++) {
        if (max_len  &&  len >= max_len) {
            dst[j++] = '\n';
            len = 0;
        }
        dst[j++] = '=';
        len++;
    }
    _ASSERT(j <= dst_size);
    *dst_written = j;
    if (j < dst_size) {
        dst[j] = '\0';
    }
}


extern int/*bool*/ BASE64_Decode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written)
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;
    size_t i = 0, j = 0, k = 0, l;
    unsigned int temp = 0;
    if (src_size < 4  ||  dst_size < 3) {
        *src_read    = 0;
        *dst_written = 0;
        return 0/*false*/;
    }
    for (;;) {
        int/*bool*/  ok = i < src_size ? 1/*true*/ : 0/*false*/;
        unsigned char c = ok ? src[i++] : '=';
        if (c == '=') {
            c  = 64; /*end*/
        } else if (c >= 'A'  &&  c <= 'Z') {
            c -= 'A';
        } else if (c >= 'a'  &&  c <= 'z') {
            c -= 'a' - 26;
        } else if (c >= '0'  &&  c <= '9') {
            c -= '0' - 52;
        } else if (c == '+') {
            c  = 62;
        } else if (c == '/') {
            c  = 63;
        } else {
            continue;
        }
        temp <<= 6;
        temp  |= c & 0x3F;
        if (!(++k & 3)  ||  c == 64) {
            if (c == 64) {
                if (k < 2) {
                    if (ok) {
                        /* pushback leading '=' */
                        --i;
                    }
                    break;
                }
                switch (k) {
                case 2:
                    temp >>= 4;
                    break;
                case 3:
                    temp >>= 10;
                    break;
                case 4:
                    temp >>= 8;
                    break;
                default:
                    _ASSERT(0);
                    break;
                }
                l = 4 - k;
                while (l > 0) {
                    /* eat up '='-padding */
                    if (i >= src_size)
                        break;
                    if (src[i] == '=')
                        l--;
                    else if (src[i] != '\r'  &&  src[i] != '\n')
                        break;
                    i++;
                }
            } else {
                k = 0;
            }
            switch (k) {
            case 0:
                dst[j++] = (temp & 0xFF0000) >> 16;
                /*FALLTHRU*/;
            case 4:
                dst[j++] = (temp & 0xFF00) >> 8;
                /*FALLTHRU*/
            case 3:
                dst[j++] = (temp & 0xFF);
                break;
            default:
                break;
            }
            if (j + 3 >= dst_size  ||  c == 64) {
                break;
            }
            temp = 0;
        }
    }
    *src_read    = i;
    *dst_written = j;
    return i  &&  j ? 1/*true*/ : 0/*false*/;
}


#ifdef NCBI_CXX_TOOLKIT

static const unsigned char xlat_bytes1and4[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static const unsigned char xlat_bytes2and3[] =
    "AAAEAIAMAQAUAYAcAgAkAoAsAwA0A4A8BABEBIBMBQBUBYBcBgBkBoBsBwB0B4B8"
    "CACECICMCQCUCYCcCgCkCoCsCwC0C4C8DADEDIDMDQDUDYDcDgDkDoDsDwD0D4D8"
    "EAEEEIEMEQEUEYEcEgEkEoEsEwE0E4E8FAFEFIFMFQFUFYFcFgFkFoFsFwF0F4F8"
    "GAGEGIGMGQGUGYGcGgGkGoGsGwG0G4G8HAHEHIHMHQHUHYHcHgHkHoHsHwH0H4H8"
    "IAIEIIIMIQIUIYIcIgIkIoIsIwI0I4I8JAJEJIJMJQJUJYJcJgJkJoJsJwJ0J4J8"
    "KAKEKIKMKQKUKYKcKgKkKoKsKwK0K4K8LALELILMLQLULYLcLgLkLoLsLwL0L4L8"
    "MAMEMIMMMQMUMYMcMgMkMoMsMwM0M4M8NANENINMNQNUNYNcNgNkNoNsNwN0N4N8"
    "OAOEOIOMOQOUOYOcOgOkOoOsOwO0O4O8PAPEPIPMPQPUPYPcPgPkPoPsPwP0P4P8"
    "QAQEQIQMQQQUQYQcQgQkQoQsQwQ0Q4Q8RARERIRMRQRURYRcRgRkRoRsRwR0R4R8"
    "SASESISMSQSUSYScSgSkSoSsSwS0S4S8TATETITMTQTUTYTcTgTkToTsTwT0T4T8"
    "UAUEUIUMUQUUUYUcUgUkUoUsUwU0U4U8VAVEVIVMVQVUVYVcVgVkVoVsVwV0V4V8"
    "WAWEWIWMWQWUWYWcWgWkWoWsWwW0W4W8XAXEXIXMXQXUXYXcXgXkXoXsXwX0X4X8"
    "YAYEYIYMYQYUYYYcYgYkYoYsYwY0Y4Y8ZAZEZIZMZQZUZYZcZgZkZoZsZwZ0Z4Z8"
    "aAaEaIaMaQaUaYacagakaoasawa0a4a8bAbEbIbMbQbUbYbcbgbkbobsbwb0b4b8"
    "cAcEcIcMcQcUcYcccgckcocscwc0c4c8dAdEdIdMdQdUdYdcdgdkdodsdwd0d4d8"
    "eAeEeIeMeQeUeYecegekeoesewe0e4e8fAfEfIfMfQfUfYfcfgfkfofsfwf0f4f8"
    "gAgEgIgMgQgUgYgcgggkgogsgwg0g4g8hAhEhIhMhQhUhYhchghkhohshwh0h4h8"
    "iAiEiIiMiQiUiYicigikioisiwi0i4i8jAjEjIjMjQjUjYjcjgjkjojsjwj0j4j8"
    "kAkEkIkMkQkUkYkckgkkkokskwk0k4k8lAlElIlMlQlUlYlclglklolslwl0l4l8"
    "mAmEmImMmQmUmYmcmgmkmomsmwm0m4m8nAnEnInMnQnUnYncngnknonsnwn0n4n8"
    "oAoEoIoMoQoUoYocogokooosowo0o4o8pApEpIpMpQpUpYpcpgpkpopspwp0p4p8"
    "qAqEqIqMqQqUqYqcqgqkqoqsqwq0q4q8rArErIrMrQrUrYrcrgrkrorsrwr0r4r8"
    "sAsEsIsMsQsUsYscsgsksosssws0s4s8tAtEtItMtQtUtYtctgtktotstwt0t4t8"
    "uAuEuIuMuQuUuYucugukuousuwu0u4u8vAvEvIvMvQvUvYvcvgvkvovsvwv0v4v8"
    "wAwEwIwMwQwUwYwcwgwkwowswww0w4w8xAxExIxMxQxUxYxcxgxkxoxsxwx0x4x8"
    "yAyEyIyMyQyUyYycygykyoysywy0y4y8zAzEzIzMzQzUzYzczgzkzozszwz0z4z8"
    "0A0E0I0M0Q0U0Y0c0g0k0o0s0w0004081A1E1I1M1Q1U1Y1c1g1k1o1s1w101418"
    "2A2E2I2M2Q2U2Y2c2g2k2o2s2w2024283A3E3I3M3Q3U3Y3c3g3k3o3s3w303438"
    "4A4E4I4M4Q4U4Y4c4g4k4o4s4w4044485A5E5I5M5Q5U5Y5c5g5k5o5s5w505458"
    "6A6E6I6M6Q6U6Y6c6g6k6o6s6w6064687A7E7I7M7Q7U7Y7c7g7k7o7s7w707478"
    "8A8E8I8M8Q8U8Y8c8g8k8o8s8w8084889A9E9I9M9Q9U9Y9c9g9k9o9s9w909498"
    "-A-E-I-M-Q-U-Y-c-g-k-o-s-w-0-4-8_A_E_I_M_Q_U_Y_c_g_k_o_s_w_0_4_8";

extern EBase64_Result base64url_encode(const void* src_buf, size_t src_size,
    void* dst_buf, size_t dst_size, size_t* output_len)
{
    const unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;
    const unsigned char* bytes2and3;

    if ((*output_len = ((src_size << 2) + 2) / 3) > dst_size)
        return eBase64_BufferTooSmall;

    while (src_size > 2) {
        *dst++ = xlat_bytes1and4[*src >> 2];
        bytes2and3 = xlat_bytes2and3 + ((*src & 3) << 9);
        bytes2and3 += *++src << 1;
        *dst++ = *bytes2and3;
        *dst++ = bytes2and3[1] + (*++src >> 6);
        *dst++ = xlat_bytes1and4[*src++ & 0x3F];
        src_size -= 3;
    }

    if (src_size > 0) {
        *dst = xlat_bytes1and4[*src >> 2];
        bytes2and3 = xlat_bytes2and3 + ((*src & 3) << 9);
        if (src_size == 1)
            *++dst = *bytes2and3;
        else { /* src_size == 2 */
            *++dst = *(bytes2and3 += src[1] << 1);
            *++dst = bytes2and3[1];
        }
    }

    return eBase64_OK;
}

static const unsigned char xlat_base64_char[] =
{
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200,   62, 0200, 0200,
      52,   53,   54,   55,   56,   57,   58,   59,
      60,   61, 0200, 0200, 0200, 0200, 0200, 0200,
    0200,    0,    1,    2,    3,    4,    5,    6,
       7,    8,    9,   10,   11,   12,   13,   14,
      15,   16,   17,   18,   19,   20,   21,   22,
      23,   24,   25, 0200, 0200, 0200, 0200,   63,
    0200,   26,   27,   28,   29,   30,   31,   32,
      33,   34,   35,   36,   37,   38,   39,   40,
      41,   42,   43,   44,   45,   46,   47,   48,
      49,   50,   51, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
    0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200
};

#define XLAT_BASE64_CHAR(var) \
    if ((signed char) (var = xlat_base64_char[*src++]) < 0) \
        return eBase64_InvalidInput;

extern EBase64_Result base64url_decode(const void* src_buf, size_t src_size,
    void* dst_buf, size_t dst_size, size_t* output_len)
{
    const unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;
    unsigned char src_ch0, src_ch1;

    if ((*output_len = (src_size * 3) >> 2) > dst_size)
        return eBase64_BufferTooSmall;

    while (src_size > 3) {
        XLAT_BASE64_CHAR(src_ch0);
        XLAT_BASE64_CHAR(src_ch1);
        *dst++ = src_ch0 << 2 | src_ch1 >> 4;
        XLAT_BASE64_CHAR(src_ch0);
        *dst++ = src_ch1 << 4 | src_ch0 >> 2;
        XLAT_BASE64_CHAR(src_ch1);
        *dst++ = src_ch0 << 6 | src_ch1;
        src_size -= 4;
    }

    if (src_size > 1) {
        XLAT_BASE64_CHAR(src_ch0);
        XLAT_BASE64_CHAR(src_ch1);
        *dst++ = src_ch0 << 2 | src_ch1 >> 4;
        if (src_size > 2) {
            XLAT_BASE64_CHAR(src_ch0);
            *dst = src_ch1 << 4 | src_ch0 >> 2;
        }
    } else if (src_size == 1)
        return eBase64_InvalidInput;

    return eBase64_OK;
}

#endif /*NCBI_CXX_TOOLKIT*/
