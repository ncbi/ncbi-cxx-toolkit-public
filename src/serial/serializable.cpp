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
*/

#include <ncbi_pch.hpp>
#include <serial/serializable.hpp>
#include <serial/exception.hpp>

BEGIN_NCBI_SCOPE


void CSerializable::WriteAsFasta(ostream& /*out*/)
    const
{
    NCBI_THROW(CSerialException,eNotImplemented,
        "CSerializable::WriteAsFasta: not implemented");
}


void CSerializable::WriteAsAsnText(ostream& /*out*/)
    const
{
    NCBI_THROW(CSerialException,eNotImplemented,
                 "CSerializable::WriteAsAsnText: not implemented");
}


void CSerializable::WriteAsAsnBinary(ostream& /*out*/)
    const
{
    NCBI_THROW(CSerialException,eNotImplemented,
                 "CSerializable::WriteAsAsnBinary: not implemented");
}


void CSerializable::WriteAsXML(ostream& /*out*/)
    const
{
    NCBI_THROW(CSerialException,eNotImplemented,
                 "CSerializable::WriteAsXML: not implemented");
}

void CSerializable::WriteAsString(ostream& /*out*/)
    const
{
    NCBI_THROW(CSerialException,eNotImplemented,
                 "CSerializable::WriteAsString: not implemented");
}


ostream& operator << (ostream& out, const CSerializable::CProxy& src) 
{
    switch ( src.m_OutputType ) {
    case CSerializable::eAsFasta:
        src.m_Obj.WriteAsFasta(out);
        break;
    case CSerializable::eAsAsnText:
        src.m_Obj.WriteAsAsnText(out);
        break;
    case CSerializable::eAsAsnBinary:
        src.m_Obj.WriteAsAsnBinary(out);
        break;
    case CSerializable::eAsXML:
        src.m_Obj.WriteAsXML(out);
        break;
    case CSerializable::eAsString:
        src.m_Obj.WriteAsString(out);
        break;
    default:
        NCBI_THROW(CSerialException,eFail,
                   "operator<<(ostream&,CSerializable::CProxy&):"
                   " wrong output type");
    }

    return out;
};


END_NCBI_SCOPE
