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
* Revision 1.4  2002/07/30 20:24:58  grichenk
* Fixed error messages in Assign() and Equals()
*
* Revision 1.3  2002/05/30 14:16:44  gouriano
* fix in debugdump: memory leak
*
* Revision 1.2  2002/05/29 21:19:17  gouriano
* added debug dump
*
* Revision 1.1  2002/05/15 20:20:38  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#include <serial/serialbase.hpp>
#include <serial/typeinfo.hpp>

#include <serial/objostr.hpp>


BEGIN_NCBI_SCOPE


void CSerialObject::Assign(const CSerialObject& source)
{
    if ( typeid(source) != typeid(*this) ) {
        ERR_POST(Fatal <<
            "CSerialObject::Assign() -- Assignment of incompatible types: " <<
            typeid(*this).name() << " = " << typeid(source).name());
    }
    GetThisTypeInfo()->Assign(this, &source);
}


bool CSerialObject::Equals(const CSerialObject& object) const
{
    if ( typeid(object) != typeid(*this) ) {
        ERR_POST(Fatal <<
            "CSerialObject::Equals() -- Can not compare types: " <<
            typeid(*this).name() << " == " << typeid(object).name());
    }
    return GetThisTypeInfo()->Equals(this, &object);
}

void CSerialObject::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CSerialObject");
    CObject::DebugDump( ddc, depth);
// this is not good, but better than nothing
    ostrstream ostr;
    ostr << endl << "****** begin ASN dump ******" << endl;
    auto_ptr<CObjectOStream> oos(CObjectOStream::Open(eSerial_AsnText, ostr));
    oos->Write(this, GetThisTypeInfo());
    ostr << endl << "****** end   ASN dump ******" << endl;
    ostr << '\0';
    ddc.Log( "Serial_AsnText", string(ostr.str()));
}


END_NCBI_SCOPE
