#ifndef DBAPI___DBAPI_VARIANT_CONVERT__HPP
#define DBAPI___DBAPI_VARIANT_CONVERT__HPP

/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NTOICE
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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *
 */

#include <dbapi/variant.hpp>
#include <dbapi/driver/dbapi_object_convert.hpp>

BEGIN_NCBI_SCOPE

namespace value_slice
{

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CVariant>
{
public: 
    typedef CVariant obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(&value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return Convert(*m_Value->GetData());
    }

private:
    const obj_type* m_Value; 
};

} // namespace value_slice

END_NCBI_SCOPE

#endif // DBAPI___DBAPI_VARIANT_CONVERT__HPP
