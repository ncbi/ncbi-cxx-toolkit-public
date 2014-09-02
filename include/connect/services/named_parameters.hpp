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
 * Author:  Dmitry Kazimirov
 *
 * File Description: Support for named parameters in methods.
 *
 */


#ifndef CONNECT_SERVICES__NAMED_PARAMETERS_HPP
#define CONNECT_SERVICES__NAMED_PARAMETERS_HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

class CNamedParameterList
{
public:
    CNamedParameterList(int tag, const CNamedParameterList* more_params) :
        m_Tag(tag),
        m_MoreParams(more_params)
    {
    }

    operator const CNamedParameterList*() const {return this;}

    const CNamedParameterList& operator ,(
            const CNamedParameterList& more_params) const
    {
        more_params.m_MoreParams = this;
        return more_params;
    }

    bool Is(int tag) const {return m_Tag == tag;}

    int m_Tag;
    mutable const CNamedParameterList* m_MoreParams;
};

template <typename TYPE>
class CNamedParameterValue : public CNamedParameterList
{
public:
    CNamedParameterValue(int tag, const CNamedParameterList* more_params,
            const TYPE& value) :
        CNamedParameterList(tag, more_params),
        m_Value(value)
    {
    }

    TYPE m_Value;
};

template <typename TYPE, int TAG>
class CNamedParameter
{
public:
    class CValue : public CNamedParameterValue<TYPE>
    {
    public:
        CValue(const TYPE& value,
                const CNamedParameterList* more_params = NULL) :
            CNamedParameterValue<TYPE>(TAG, more_params, value)
        {
        }
    };

    CValue operator =(const TYPE& value)
    {
        return CValue(value);
    }
};

template <typename TYPE>
const TYPE& Get(const CNamedParameterList* param)
{
    return static_cast<const CNamedParameterValue<TYPE>*>(param)->m_Value;
}

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__NAMED_PARAMETERS_HPP */
