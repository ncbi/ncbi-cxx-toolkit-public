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
* Author:  Michael Kholodov
*
* File Description:
*   General serializable interface for different output formats
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2001/05/21 14:38:32  kholodov
* Added: method WriteAsString() for string representation of an object.
*
* Revision 1.2  2001/04/17 04:08:27  vakatov
* Redesigned from a pure interface (ISerializable) into a regular
* base class (CSerializable) to make its usage safer, more formal and
* less bulky.
*
* Revision 1.1  2001/04/12 17:01:11  kholodov
* General serializable interface for different output formats
*
* ===========================================================================
*/

#include <serial/serializable.hpp>
#include <serial/exception.hpp>

BEGIN_NCBI_SCOPE


void CSerializable::WriteAsFasta(ostream& /*out*/)
    const
{
    THROW1_TRACE(CSerialNotImplemented,
                 "CSerializable::WriteAsFasta() not implemented");
}


void CSerializable::WriteAsAsnText(ostream& /*out*/)
    const
{
    THROW1_TRACE(CSerialNotImplemented,
                 "CSerializable::WriteAsAsnText() not implemented");
}


void CSerializable::WriteAsAsnBinary(ostream& /*out*/)
    const
{
    THROW1_TRACE(CSerialNotImplemented,
                 "CSerializable::WriteAsAsnBinary() not implemented");
}


void CSerializable::WriteAsXML(ostream& /*out*/)
    const
{
    THROW1_TRACE(CSerialNotImplemented,
                 "CSerializable::WriteAsXML() not implemented");
}

void CSerializable::WriteAsString(ostream& /*out*/)
    const
{
    THROW1_TRACE(CSerialNotImplemented,
                 "CSerializable::WriteAsString() not implemented");
}


ostream& operator << (ostream& out, const CSerializable& src) 
{
    switch ( src.m_OutputType ) {
    case CSerializable::eAsFasta:
        src.WriteAsFasta(out);
        break;
    case CSerializable::eAsAsnText:
        src.WriteAsAsnText(out);
        break;
    case CSerializable::eAsAsnBinary:
        src.WriteAsAsnBinary(out);
        break;
    case CSerializable::eAsXML:
        src.WriteAsXML(out);
        break;
    case CSerializable::eAsString:
        src.WriteAsString(out);
        break;
    default:
        THROW1_TRACE(runtime_error,
                     "operator<<(ostream&,CSerializable&): wrong output type");
    }

    return out;
};


END_NCBI_SCOPE
