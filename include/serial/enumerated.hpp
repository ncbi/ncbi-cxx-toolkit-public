#ifndef ENUMERATED__HPP
#define ENUMERATED__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/09/24 18:20:05  vasilche
* Removed dependency on NCBI toolkit.
*
*
* ===========================================================================
*/

#include <serial/stdtypes.hpp>
#include <map>

BEGIN_NCBI_SCOPE

class CEnumeratedTypeInfo : public CStdTypeInfo<int>
{
    typedef CStdTypeInfo<int> CParent;
public:
    typedef CParent::TObjectType TValue;
    typedef map<string, TValue> TNameToValue;
    typedef map<TValue, string> TValueToName;

    CEnumeratedTypeInfo(const string& name, bool isInteger = false);

    void AddValue(const string& name, TValue value);

    TValue FindValue(const string& name) const;
    const string& FindName(TValue value) const;

protected:
    void ReadData(CObjectIStream& in, TObjectPtr object) const;
    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

private:
    bool m_Integer;
    TNameToValue m_NameToValue;
    TValueToName m_ValueToName;
};

END_NCBI_SCOPE

#endif  /* ENUMERATED__HPP */
