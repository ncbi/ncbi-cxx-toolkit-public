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
*/

inline
void CObjectOStreamXml::OpenStackTag(size_t level)
{
    OpenTagStart();
    PrintTagName(level);
    OpenTagEnd();
}

inline
void CObjectOStreamXml::CloseStackTag(size_t level)
{
    if ( m_LastTagAction == eTagSelfClosed ) {
        m_LastTagAction = eTagClose;
    } else if ( m_LastTagAction == eAttlistTag ) { 
        m_Output.PutChar('\"');
        m_LastTagAction = eTagOpen;
    } else {
        CloseTagStart();
        PrintTagName(level);
        CloseTagEnd();
    }
}

inline
void CObjectOStreamXml::OpenTag(const string& name)
{
    OpenTagStart();
    WriteTag(name);
#if defined(NCBI_SERIAL_IO_TRACE)
    TraceTag(name);
#endif
    OpenTagEnd();
}

inline
void CObjectOStreamXml::CloseTag(const string& name)
{
    if ( m_LastTagAction == eTagSelfClosed ) {
        m_LastTagAction = eTagClose;
    } else {
        CloseTagStart();
        WriteTag(name);
#if defined(NCBI_SERIAL_IO_TRACE)
    TraceTag(name);
#endif
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
void CObjectOStreamXml::CloseTag(TTypeInfo type)
{
    _ASSERT(!type->GetName().empty());
    CloseTag(type->GetName());
}

inline
void CObjectOStreamXml::OpenTagIfNamed(TTypeInfo type)
{
    if ( !type->GetName().empty() )
        OpenTag(type->GetName());
}

inline
void CObjectOStreamXml::CloseTagIfNamed(TTypeInfo type)
{
    if ( !type->GetName().empty() )
        CloseTag(type->GetName());
}

inline
void CObjectOStreamXml::SetDTDFilePrefix(const string& prefix)
{
    m_DTDFilePrefix = prefix;
    m_UseDefaultDTDFilePrefix = false;
}

inline
void CObjectOStreamXml::SetDTDFileName(const string& filename)
{
    m_DTDFileName = filename;
}

inline
string CObjectOStreamXml::GetDTDFilePrefix(void) const
{
    if ( !m_UseDefaultDTDFilePrefix ) {
        return m_DTDFilePrefix;
    }
    else {
        return sm_DefaultDTDFilePrefix;
    }
}

inline
string CObjectOStreamXml::GetDTDFileName(void) const
{
    return m_DTDFileName;
}

inline
void CObjectOStreamXml::SetDefaultDTDFilePrefix(const string& def_prefix)
{
    sm_DefaultDTDFilePrefix = def_prefix;
}

inline
string CObjectOStreamXml::GetDefaultDTDFilePrefix(void)
{
    return sm_DefaultDTDFilePrefix;
}

inline
void CObjectOStreamXml::EnableDTDPublicId(void)
{
    m_UsePublicId = true;
}

inline
void CObjectOStreamXml::DisableDTDPublicId(void)
{
    m_UsePublicId = false;
}
inline

void CObjectOStreamXml::SetDTDPublicId(const string& publicId)
{
    m_PublicId = publicId;
}

inline
string CObjectOStreamXml::GetDTDPublicId(void) const
{
    return m_PublicId;
}

inline
void  CObjectOStreamXml::SetDefaultSchemaNamespace(const string& schema_ns)
{
    sm_DefaultSchemaNamespace = schema_ns;
}

inline
string CObjectOStreamXml::GetDefaultSchemaNamespace(void)
{
    return sm_DefaultSchemaNamespace;
}

#endif /* def OBJOSTRXML__HPP  &&  ndef OBJOSTRXML__INL */
