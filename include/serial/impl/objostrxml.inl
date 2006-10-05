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



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.11  2003/08/25 15:58:32  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.10  2003/07/02 13:01:00  gouriano
* added ability to read/write XML files with reference to schema
*
* Revision 1.9  2003/01/10 16:53:36  gouriano
* fixed a bug with optional class members
*
* Revision 1.8  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.7  2002/12/12 21:10:26  gouriano
* implemented handling of complex XML containers
*
* Revision 1.6  2002/11/14 20:52:55  gouriano
* added support of attribute lists
*
* Revision 1.5  2002/10/18 14:25:51  gouriano
* added possibility to enable/disable/set public identifier
*
* Revision 1.4  2001/11/09 19:07:22  grichenk
* Fixed DTDFilePrefix functions
*
* Revision 1.3  2001/10/17 18:18:28  grichenk
* Added CObjectOStreamXml::xxxFilePrefix() and
* CObjectOStreamXml::xxxFileName()
*
* Revision 1.2  2000/11/07 17:25:12  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.1  2000/09/18 20:00:07  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
