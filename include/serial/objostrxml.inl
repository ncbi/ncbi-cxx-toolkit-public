#if defined(OBJOSTRXML__HPP)  &&  !defined(OBJOSTRXML__INL)
#define OBJOSTRXML__INL

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
* Revision 1.1  2000/09/18 20:00:07  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/

inline
void CObjectOStreamXml::OpenTagEnd(void)
{
    m_Output.PutChar('>');
}

inline
void CObjectOStreamXml::CloseTagEnd(void)
{
    m_Output.PutChar('>');
}

inline
void CObjectOStreamXml::OpenStackTag(size_t level)
{
    OpenTagStart();
    PrintTagName(level);
    OpenTagEnd();
}

inline
void CObjectOStreamXml::CloseStackTag(size_t level, bool forceEolBefore)
{
    if ( CloseTagStart(forceEolBefore) ) {
        PrintTagName(level);
        CloseTagEnd();
    }
}

inline
void CObjectOStreamXml::OpenTag(const string& name)
{
    OpenTagStart();
    m_Output.PutString(name);
    OpenTagEnd();
}

inline
void CObjectOStreamXml::CloseTag(const string& name, bool forceEolBefore)
{
    if ( CloseTagStart(forceEolBefore) ) {
        m_Output.PutString(name);
        CloseTagEnd();
    }
}

inline
void CObjectOStreamXml::OpenTag(TTypeInfo type)
{
    _ASSERT(!type->GetName().empty());
    OpenTag(type->GetName());
}

inline
void CObjectOStreamXml::CloseTag(TTypeInfo type, bool forceEolBefore)
{
    _ASSERT(!type->GetName().empty());
    CloseTag(type->GetName(), forceEolBefore);
}

inline
void CObjectOStreamXml::OpenTagIfNamed(TTypeInfo type)
{
    if ( !type->GetName().empty() )
        OpenTag(type->GetName());
}

inline
void CObjectOStreamXml::CloseTagIfNamed(TTypeInfo type, bool forceEolBefore)
{
    if ( !type->GetName().empty() )
        CloseTag(type->GetName(), forceEolBefore);
}

#endif /* def OBJOSTRXML__HPP  &&  ndef OBJOSTRXML__INL */
