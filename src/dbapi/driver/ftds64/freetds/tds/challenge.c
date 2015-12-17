/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2005  Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <ctype.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#include "tdsstring.h"
#include "md4.h"
#include "md5.h"
#include "hmac_md5.h"
#include "des.h"
#include "tdsiconv.h"
#include "tds_checks.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id$");

/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 *  \@{
 */

/*
 * The following code is based on some psuedo-C code from ronald@innovation.ch
 */

static void tds_encrypt_answer(const unsigned char *hash, const unsigned char *challenge, unsigned char *answer);
static void tds_convert_key(const unsigned char *key_56, DES_KEY * ks);


static void convert_to_upper(char* buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        buf[i] = toupper(buf[i]);
    }
}


static const char*
convert_to_usc2le_string(TDSSOCKET * tds,
                         const TDSICONV * char_conv,
                         const char *s,
                         ssize_t len,
                         size_t *out_len)
{
	char *buf;

	const char *ib;
	char *ob;
	size_t il, ol;

	/* char_conv is only mostly const */
	TDS_ERRNO_MESSAGE_FLAGS *suppress = (TDS_ERRNO_MESSAGE_FLAGS*) &char_conv->suppress;

	CHECK_TDS_EXTRA(tds);

	if (len < 0)
		len = strlen(s);
	if (char_conv->flags == TDS_ENCODING_MEMCPY) {
		*out_len = len;
		return s;
	}

	/* allocate needed buffer (+1 is to exclude 0 case) */
	ol = len * char_conv->server_charset.max_bytes_per_char / char_conv->client_charset.min_bytes_per_char + 1;
	buf = (char *) malloc(ol);
	if (!buf)
		return NULL;

	ib = s;
	il = len;
	ob = buf;
	memset(suppress, 0, sizeof(char_conv->suppress));
	if (tds_iconv(tds, char_conv, to_server, &ib, &il, &ob, &ol) == (size_t)-1) {
		free(buf);
		return NULL;
	}
	*out_len = ob - buf;
	return buf;
}


static void
convert_to_usc2le_string_free(const char *original, const char *converted)
{
	if (original != converted)
		free((char *) converted);
}


void generate_random_buffer(unsigned char *out, int len)
{
    int i;

    /* TODO find a better random... */
    for (i = 0; i < len; ++i) {
        out[i] = rand() / (RAND_MAX/256);
    }
}


static void make_ntlm_hash(TDSSOCKET * tds, const char* passwd, unsigned char ntlm_hash[16])
{
    MD4_CTX context;
    size_t passwd_len = 0;
    const char* passwd_usc2le = NULL;
    size_t passwd_usc2le_len = 0;

    passwd_len = strlen(passwd);

    if (passwd_len > 128)
        passwd_len = 128;

    passwd_usc2le = convert_to_usc2le_string(tds,
                                             tds->char_convs[client2ucs2],
                                             passwd,
                                             passwd_len,
                                             &passwd_usc2le_len);

    /* compute NTLM hash */
    MD4Init(&context);
    MD4Update(&context, (const unsigned char*)passwd_usc2le, passwd_usc2le_len);
    MD4Final(&context, ntlm_hash);

    /* with security is best be pedantic */
    memset((char*)passwd_usc2le, 0, passwd_usc2le_len);
    memset(&context, 0, sizeof(context));
    convert_to_usc2le_string_free(passwd, passwd_usc2le);
}


static void make_ntlm_v2_hash(TDSSOCKET * tds,
                              const char* target,
                              const char* user,
                              const char* passwd,
                              unsigned char ntlm_v2_hash[16])
{
    unsigned char ntlm_hash[16];
    char buf[256];
    size_t buf_len = 0;
    size_t user_len = 0;
    size_t target_len = 0;
    const char* buf_usc2le;
    size_t buf_usc2le_len = 0;

    user_len = strlen(user);
    if (user_len > 128)
        user_len = 128;
    memcpy(buf, user, user_len);

    convert_to_upper(buf, user_len);

    target_len = strlen(target);
    if (target_len > 128)
        target_len = 128;
    memcpy(buf + user_len, target, target_len);
    /* Target is supposed to be case-sensitive */

    buf_len = user_len + target_len;

    buf_usc2le = convert_to_usc2le_string(tds,
                                          tds->char_convs[client2ucs2],
                                          buf,
                                          buf_len,
                                          &buf_usc2le_len);

    make_ntlm_hash(tds, passwd, ntlm_hash);
    hmac_md5(ntlm_hash, (const unsigned char*)buf_usc2le, buf_usc2le_len, ntlm_v2_hash);

    /* with security is best be pedantic */
    memset(&ntlm_hash, 0, sizeof(ntlm_hash));
    memset(buf, 0, sizeof(buf));
    memset((char*)buf_usc2le, 0, buf_usc2le_len);
    convert_to_usc2le_string_free(buf, buf_usc2le);
}


/*
    hash - The NTLMv2 Hash.
    client_data - The client data (blob or client nonce).
    challenge - The server challenge from the Type 2 message.
*/
static unsigned char*
make_lm_v2_response(const unsigned char ntlm_v2_hash[16],
                    const unsigned char* client_data,
                    TDS_INT client_data_len,
                    const unsigned char challenge[8])
{
    int mac_len = 16 + client_data_len;
    unsigned char* mac;
    int data_len = 8 + client_data_len;
    unsigned char* data;

    data = malloc(data_len);
    mac = malloc(mac_len);

    memcpy(data, challenge, 8);
    memcpy(data + 8, client_data, client_data_len);
    hmac_md5(ntlm_v2_hash, data, data_len, mac);
    free(data);
    memcpy(mac + 16, client_data, client_data_len);

    return mac;
}

/*
static unsigned char*
get_lm_v2_response(unsigned char hash[16], const unsigned char* challenge)
{
    unsigned char client_chalenge[8];

    generate_random_buffer(client_chalenge, sizeof(client_chalenge));
    return make_lm_v2_response(hash, client_chalenge, sizeof(client_chalenge), challenge);
}
*/

/**
 * Crypt a given password using schema required for NTLMv1 authentication
 * @param passwd clear text domain password
 * @param challenge challenge data given by server
 * @param flags NTLM flags from server side
 * @param answer buffer where to store crypted password
 */
void
tds_answer_challenge(TDSSOCKET * tds,
                     TDSCONNECTION* connection,
                     const unsigned char* challenge,
                     TDS_UINT* flags,
                     const unsigned char* names_blob,
                     TDS_INT names_blob_len,
                     TDSANSWER* answer,
                     unsigned char** ntlm_v2_response)
{
#define MAX_PW_SZ 14
    const char* passwd;
    const char* user_name;
    const char *p;
    size_t domain_len;
    /* int user_name_len; */
    int ntlm_v;
    unsigned char* lm_v2_response;

    size_t len;
    size_t i;
    static const des_cblock magic = { 0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25 };
    DES_KEY ks;
    unsigned char hash[24];
    unsigned char ntlm2_challenge[16];
    unsigned char passwd_buf[256];
    char domain[128];
    /* unsigned char client_chalenge[8]; */

    unsigned char ntlm_v2_hash[16];
    const names_blob_prefix_t* names_blob_prefix;

    memset(answer, 0, sizeof(TDSANSWER));

    passwd = tds_dstr_cstr(&connection->password);
    user_name = tds_dstr_cstr(&connection->user_name);
    /* user_name_len = user_name ? strlen(user_name) : 0; */

    /* parse domain\username */
    p = user_name/*_len*/ ? strchr(user_name, '\\') : NULL;

    domain_len = p ? p - user_name : 0;
    memcpy(domain, user_name, domain_len);
    domain[domain_len] = '\0';

    if (p != NULL) {
        user_name = p + 1;
        /* user_name_len = strlen(user_name); */
    }

    ntlm_v = 2;

    if (ntlm_v == 1) {
        if (!(*flags & 0x80000)) {
            /* convert password to upper and pad to 14 chars */
            memset(passwd_buf, 0, MAX_PW_SZ);
            len = strlen(passwd);
            if (len > MAX_PW_SZ)
                len = MAX_PW_SZ;
            for (i = 0; i < len; i++)
                passwd_buf[i] = toupper((unsigned char) passwd[i]);

            /* hash the first 7 characters */
            tds_convert_key(passwd_buf, &ks);
            tds_des_ecb_encrypt(&magic, sizeof(magic), &ks, (hash + 0));

            /* hash the second 7 characters */
            tds_convert_key(passwd_buf + 7, &ks);
            tds_des_ecb_encrypt(&magic, sizeof(magic), &ks, (hash + 8));

            memset(hash + 16, 0, 5);

            tds_encrypt_answer(hash, challenge, answer->lm_resp);

            /* with security is best be pedantic */
            memset(&ks, 0, sizeof(ks));
        } else {
            MD5_CTX md5_ctx;

            /* NTLM2 */
            generate_random_buffer(hash, 8);
            memset(hash + 8, 0, 16);
            memcpy(answer->lm_resp, hash, 24);

            MD5Init(&md5_ctx);
            MD5Update(&md5_ctx, challenge, 8);
            MD5Update(&md5_ctx, hash, 8);
            MD5Final(&md5_ctx, ntlm2_challenge);
            challenge = ntlm2_challenge;
            memset(&md5_ctx, 0, sizeof(md5_ctx));
        }
        *flags = 0x8201;

        /* NTLM/NTLM2 response */
        make_ntlm_hash(tds, passwd, hash);
        memset(hash + 16, 0, 5);

        tds_encrypt_answer(hash, challenge, answer->nt_resp);
        *ntlm_v2_response = NULL;

        /* with security is best be pedantic */
        memset(hash, 0, sizeof(hash));
        memset(passwd_buf, 0, sizeof(passwd_buf));
        memset(ntlm2_challenge, 0, sizeof(ntlm2_challenge));
    } else {
        /* NTLMv2 */

        make_ntlm_v2_hash(tds, domain, user_name, passwd, ntlm_v2_hash);

        /* NTLMv2 response */
        /* Size of lm_v2_response is 16 + names_blob_len */
        *ntlm_v2_response = make_lm_v2_response(ntlm_v2_hash, names_blob, names_blob_len, challenge);

        /* LMv2 response */
        /* Take client's chalenge from names_blob */
        names_blob_prefix = (const names_blob_prefix_t*)names_blob;
        lm_v2_response = make_lm_v2_response(ntlm_v2_hash, names_blob_prefix->challenge, 8, challenge);
        memcpy(answer->lm_resp, lm_v2_response, 24);
        free(lm_v2_response);
    }
}


/*
* takes a 21 byte array and treats it as 3 56-bit DES keys. The
* 8 byte plaintext is encrypted with each key and the resulting 24
* bytes are stored in the results array.
*/
static void
tds_encrypt_answer(const unsigned char *hash, const unsigned char *challenge, unsigned char *answer)
{
    DES_KEY ks;

    tds_convert_key(hash, &ks);
    tds_des_ecb_encrypt(challenge, 8, &ks, answer);

    tds_convert_key(&hash[7], &ks);
    tds_des_ecb_encrypt(challenge, 8, &ks, &answer[8]);

    tds_convert_key(&hash[14], &ks);
    tds_des_ecb_encrypt(challenge, 8, &ks, &answer[16]);

    memset(&ks, 0, sizeof(ks));
}


/*
* turns a 56 bit key into the 64 bit, odd parity key and sets the key.
* The key schedule ks is also set.
*/
static void
tds_convert_key(const unsigned char *key_56, DES_KEY * ks)
{
    des_cblock key;

    key[0] = key_56[0];
    key[1] = ((key_56[0] << 7) & 0xFF) | (key_56[1] >> 1);
    key[2] = ((key_56[1] << 6) & 0xFF) | (key_56[2] >> 2);
    key[3] = ((key_56[2] << 5) & 0xFF) | (key_56[3] >> 3);
    key[4] = ((key_56[3] << 4) & 0xFF) | (key_56[4] >> 4);
    key[5] = ((key_56[4] << 3) & 0xFF) | (key_56[5] >> 5);
    key[6] = ((key_56[5] << 2) & 0xFF) | (key_56[6] >> 6);
    key[7] = (key_56[6] << 1) & 0xFF;

    tds_des_set_odd_parity(key);
    tds_des_set_key(ks, key, sizeof(key));

    memset(&key, 0, sizeof(key));
}


/** \@} */
