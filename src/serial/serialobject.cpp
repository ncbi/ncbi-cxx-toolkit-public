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
* Author:  Aleksey Grichenko
*
* File Description:
*   Base class for serializable objects
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/05/15 20:20:38  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#include <serial/serialbase.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE


void CSerialObject::Assign(const CSerialObject& source)
{
    if ( typeid(source) != typeid(*this) ) {
        ERR_POST(Fatal <<
            "SerialAssign() -- Assignment of incompatible types: " <<
            typeid(*this).name() << " = " << typeid(source).name());
    }
    GetThisTypeInfo()->Assign(this, &source);
}


bool CSerialObject::Equals(const CSerialObject& object) const
{
    if ( typeid(object) != typeid(*this) ) {
        ERR_POST(Fatal <<
            "SerialAssign() -- Can not compare types: " <<
            typeid(*this).name() << " == " << typeid(object).name());
    }
    return GetThisTypeInfo()->Equals(this, &object);
}


END_NCBI_SCOPE
