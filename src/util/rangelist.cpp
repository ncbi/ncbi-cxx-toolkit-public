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
 * Authors:
 *   Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of the CRangeList class.
 */

#include <ncbi_pch.hpp>

#include <util/rangelist.hpp>

BEGIN_NCBI_SCOPE

static const char* s_SkipSpaces(const char* input_string)
{
    while (*input_string == ' ' || *input_string == '\t')
        ++input_string;

    return input_string;
}

void CRangeListImpl::Parse(const char* init_string,
        const char* config_param_name,
        TRangeVector* range_vector)
{
    if (*init_string == '\0') {
        NCBI_THROW_FMT(CInvalidParamException, eUndefined,
                "Configuration parameter '" << config_param_name <<
                        "' is not defined.");
    }

    range_vector->clear();

    const char* pos = init_string;

    TIntegerRange new_range;

    int* current_bound_ptr = &new_range.first;
    bool reading_range = false;

    for (;;) {
        pos = s_SkipSpaces(pos);

        bool negative = *pos == '-' ? (++pos, true) : false;

        unsigned number = (unsigned) (*pos - '0');

        if (number > 9) {
            NCBI_THROW_FMT(CInvalidParamException, eInvalidCharacter,
                    "'" << config_param_name <<
                    "': not a number at position " << (pos - init_string + 1));
        }

        unsigned digit;

        while ((digit = (unsigned) (*++pos - '0')) <= 9)
            number = number * 10 + digit;

        *current_bound_ptr = negative ? -int(number) : int(number);

        pos = s_SkipSpaces(pos);

        switch (*pos) {
        case '\0':
        case ',':
            if (!reading_range)
                new_range.second = new_range.first;
            range_vector->push_back(new_range);
            if (*pos == '\0')
                return;
            ++pos;
            current_bound_ptr = &new_range.first;
            new_range.second = 0;
            break;

        case '-':
            ++pos;
            current_bound_ptr = &new_range.second;
            break;

        default:
            NCBI_THROW_FMT(CInvalidParamException, eInvalidCharacter,
                    "'" << config_param_name <<
                    "': invalid character at position " <<
                            (pos - init_string + 1));
        }
    }
}

END_NCBI_SCOPE
