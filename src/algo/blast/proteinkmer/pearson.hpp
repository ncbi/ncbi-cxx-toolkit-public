/*  $Id: pearson.hpp
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
 * Author: Tom Madden
 *
 * File Description:
 *   Calculate Pearson hash per:
 *   http://portal.acm.org/citation.cfm?id=78978
 *   http://cs.mwsu.edu/~griffin/courses/2133/downloads/Spring11/p677-pearson.pdf
 *
 * 	
 */
#include <corelib/ncbistd.hpp>

/* copied from wikipedia.  produce my own? */

const unsigned char pearson_tab[256] = {
// 256 values 0-255 in any (random) order suffices
 98,  6, 85,150, 36, 23,112,164,135,207,169,  5, 26, 64,165,219, //  1
 61, 20, 68, 89,130, 63, 52,102, 24,229,132,245, 80,216,195,115, //  2
 90,168,156,203,177,120,  2,190,188,  7,100,185,174,243,162, 10, //  3
237, 18,253,225,  8,208,172,244,255,126,101, 79,145,235,228,121, //  4
123,251, 67,250,161,  0,107, 97,241,111,181, 82,249, 33, 69, 55, //  5
 59,153, 29,  9,213,167, 84, 93, 30, 46, 94, 75,151,114, 73,222, //  6
197, 96,210, 45, 16,227,248,202, 51,152,252,125, 81,206,215,186, //  7
 39,158,178,187,131,136,  1, 49, 50, 17,141, 91, 47,129, 60, 99, //  8
154, 35, 86,171,105, 34, 38,200,147, 58, 77,118,173,246, 76,254, //  9
133,232,196,144,198,124, 53,  4,108, 74,223,234,134,230,157,139, // 10
189,205,199,128,176, 19,211,236,127,192,231, 70,233, 88,146, 44, // 11
183,201, 22, 83, 13,214,116,109,159, 32, 95,226,140,220, 57, 12, // 12
221, 31,209,182,143, 92,149,184,148, 62,113, 65, 37, 27,106,166, // 13
3, 14,204, 72, 21, 41, 56, 66, 28,193, 40,217, 25, 54,179,117, // 14
238, 87,240,155,180,170,242,212,191,163, 78,218,137,194,175,110, // 15
43,119,224, 71,122,142, 42,160,104, 48,247,103, 15, 11,138,239  // 16
};

// Pearson hash takes a string of bytes and returns one byte key.
unsigned char pearson_hash(unsigned char* key, int length, int init);
              
// Do three pearson hashes in a row on the key.
uint32_t do_pearson_hash(unsigned char *key, int length);

/// Do two pearson hashes in a row on the key.
uint32_t do_pearson_hash2bytes(unsigned char *key, int length);

/// Pearson hash an integer into one byte.
unsigned char pearson_hash_int2byte(uint32_t input, int seed1=11); 

/// Pearson hash an integer into two bytes.
uint16_t pearson_hash_int2short(uint32_t input, int seed1=11, int seed2=101);
