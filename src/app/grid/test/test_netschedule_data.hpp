/*  $Id$
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Data for NetSchedule tests
 *
 */

#ifndef TEST_NETSCHEDULE_DATA_HPP
#define TEST_NETSCHEDULE_DATA_HPP

#include <corelib/ncbi_system.hpp>

static const char output_buffer_seed[] =
    "AZBYCXDWEVFUGTHSIRJQKPLOMNNMOLPKQJRISHTGUFVEWDXCYB\n"
    "ZAYBXCWDVEUFTGSHRIQJPKOLNMMNLOKPJQIRHSGTFUEVDWCXBY\n";

#define OUTPUT_SEED_SIZE (sizeof(output_buffer_seed) - 1)

#define MAX_OUTPUT_SIZE (1024 * 1024 * OUTPUT_SEED_SIZE / \
    (OUTPUT_SEED_SIZE + 3 + 3)) /* Because each \n will be encoded as 4 bytes */

static char output_buffer[MAX_OUTPUT_SIZE];


static void InitOutputBuffer()
{
    memcpy(output_buffer, output_buffer_seed, OUTPUT_SEED_SIZE);

    size_t initialized = OUTPUT_SEED_SIZE;

    while (initialized * 2 < MAX_OUTPUT_SIZE) {
        memcpy(output_buffer + initialized, output_buffer, initialized);
        initialized *= 2;
    }

    memcpy(output_buffer + initialized, output_buffer,
        MAX_OUTPUT_SIZE - initialized);
}

#endif // TEST_NETSCHEDULE_DATA_HPP
