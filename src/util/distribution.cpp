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
 *   Implementations of the CDiscreteDistribution class.
 */

#include <ncbi_pch.hpp>

#include <util/distribution.hpp>

BEGIN_NCBI_SCOPE

const char* CInvalidParamException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eUndefined:
        return "eUndefined";

    case eNotANumber:
        return "eNotANumber";

    case eInvalidCharacter:
        return "eInvalidCharacter";

    default:
        return CException::GetErrCodeString();
    }
}

void CDiscreteDistribution::InitFromParameter(const char* parameter_name,
    const char* parameter_value, CRandom* random_gen)
{
    m_RandomGen = random_gen;

    if (*parameter_value == '\0') {
        NCBI_THROW(CInvalidParamException, eUndefined,
            string("Configuration parameter '") + parameter_name +
                "' was not defined.");
    }

    m_RangeVector.clear();

    const char* pos = parameter_value;

    TRange new_range;

    unsigned* current_bound_ptr = &new_range.first;
    new_range.second = 0;

    for (;;) {
        pos = SkipSpaces(pos);

        unsigned bound = (unsigned) (*pos - '0');

        if (bound > 9) {
            NCBI_THROW(CInvalidParamException, eNotANumber,
                string("In configuration parameter '") + parameter_name +
                "': not a number at position " + NStr::ULongToString(
                    (unsigned long) (pos - parameter_value) + 1) + ".");
        }

        unsigned digit;

        while ((digit = (unsigned) (*++pos - '0')) <= 9)
            bound = bound * 10 + digit;

        *current_bound_ptr = bound;

        pos = SkipSpaces(pos);

        switch (*pos) {
        case '\0':
            m_RangeVector.push_back(new_range);
            return;

        case ',':
            m_RangeVector.push_back(new_range);
            ++pos;
            current_bound_ptr = &new_range.first;
            new_range.second = 0;
            break;

        case '-':
            ++pos;
            current_bound_ptr = &new_range.second;
            break;

        default:
            NCBI_THROW(CInvalidParamException, eInvalidCharacter,
                string("In configuration parameter '") + parameter_name +
                "': invalid character at position " + NStr::ULongToString(
                    (unsigned long) (pos - parameter_value) + 1) + ".");
        }
    }
}

unsigned CDiscreteDistribution::GetNextValue() const
{
    CRandom::TValue random_number = m_RandomGen->GetRand();

    TRangeVector::const_iterator random_range =
        m_RangeVector.begin() + (random_number % m_RangeVector.size());

    return random_range->second == 0 ? random_range->first :
        random_range->first +
            (random_number % (random_range->second - random_range->first + 1));
}

const char* CDiscreteDistribution::SkipSpaces(const char* input_string)
{
    while (*input_string == ' ' || *input_string == '\t')
        ++input_string;

    return input_string;
}

END_NCBI_SCOPE
