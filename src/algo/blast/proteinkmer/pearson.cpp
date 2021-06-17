/*  $Id: pearson.cpp
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


#include <ncbi_pch.hpp>
#include "pearson.hpp"

// Pearson hash takes a string of bytes and returns one byte key.
unsigned char pearson_hash(unsigned char* key, int length, int init)
{
        unsigned char hash = init;
        int i;
        for (i=0; i<length; i++)
                hash=pearson_tab[hash^key[i]];
        return hash;
}
              
// Do three pearson hashes in a row on the key.
uint32_t do_pearson_hash(unsigned char *key, int length)
{
	unsigned char hash = pearson_hash(key, length, 11);
        int foo = (hash << 8);
        hash = pearson_hash(key, length, 101);
        foo = foo | hash;
        foo = (foo << 8);
        hash = pearson_hash(key, length, 231);
        foo = foo | hash;
	return foo;
}

// Pearson hash for an integer to one byte
unsigned char pearson_hash_int2byte(uint32_t input, int seed1)
{
	unsigned char key[4];
	const int kLength=4;

	key[0] = input & 0xff;
	key[1] = (input >> 8) & 0xff;
	key[2] = (input >> 16) & 0xff;
	key[3] = (input >> 24) & 0xff;
	
	/* return value */
	unsigned char hash = pearson_hash(key, kLength, seed1);
	return hash;
}


// Pearson hash for an integer to a short (two bytes)
uint16_t pearson_hash_int2short(uint32_t input, int seed1, int seed2)
{
	unsigned char key[4];
	const int kLength=4;
	uint16_t foo; /* return value; */

	key[0] = input & 0xff;
	key[1] = (input >> 8) & 0xff;
	key[2] = (input >> 16) & 0xff;
	key[3] = (input >> 24) & 0xff;

	unsigned char hash = pearson_hash(key, kLength, seed1);
	foo = (hash << 8);
        hash = pearson_hash(key, kLength, seed2);
        foo = foo | hash;
	return foo;
}


