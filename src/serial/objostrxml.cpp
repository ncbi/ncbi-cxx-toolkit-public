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
*   XML object output stream
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <serial/objostrxml.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/memberid.hpp>
#include <serial/memberlist.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objhook.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/continfo.hpp>
#include <serial/delaybuf.hpp>
#include <serial/ptrinfo.hpp>

#include <stdio.h>
#include <math.h>

BEGIN_NCBI_SCOPE

CObjectOStream* CObjectOStream::OpenObjectOStreamXml(CNcbiOstream& out,
                                                     bool deleteOut)
{
    return new CObjectOStreamXml(out, deleteOut);
}


string CObjectOStreamXml::sm_DefaultDTDFilePrefix = "";
string CObjectOStreamXml::sm_DefaultSchemaNamespace = "http://www.ncbi.nlm.nih.gov";

CObjectOStreamXml::CObjectOStreamXml(CNcbiOstream& out, bool deleteOut)
    : CObjectOStream(eSerial_Xml, out, deleteOut),
      m_LastTagAction(eTagClose), m_EndTag(true),
      m_UseDefaultDTDFilePrefix( true),
      m_UsePublicId( true),
      m_Attlist( false), m_StdXml( false), m_EnforcedStdXml( false),
      m_RealFmt( eRealScientificFormat ), m_Encoding( eEncoding_Unknown ),
      m_UseSchemaRef( false ), m_UseSchemaLoc( true ), m_UseDTDRef( true ),
      m_SkipIndent( false )
{
    m_Output.SetBackLimit(1);
}

CObjectOStreamXml::~CObjectOStreamXml(void)
{
}

void CObjectOStreamXml::SetEncoding(EEncoding enc)
{
    m_Encoding = enc;
}

CObjectOStreamXml::EEncoding CObjectOStreamXml::GetEncoding(void) const
{
    return m_Encoding;
}

void CObjectOStreamXml::SetReferenceSchema(bool use_schema)
{
    m_UseSchemaRef = use_schema;
}

bool CObjectOStreamXml::GetReferenceSchema(void) const
{
    return m_UseSchemaRef;
}
void CObjectOStreamXml::SetReferenceDTD(bool use_dtd)
{
    m_UseDTDRef = use_dtd;
}
bool CObjectOStreamXml::GetReferenceDTD(void) const
{
    return m_UseDTDRef;
}

void CObjectOStreamXml::SetUseSchemaLocation(bool use_loc)
{
    m_UseSchemaLoc = use_loc;
}
bool CObjectOStreamXml::GetUseSchemaLocation(void) const
{
    return m_UseSchemaLoc;
}


CObjectOStreamXml::ERealValueFormat
    CObjectOStreamXml::GetRealValueFormat(void) const
{
    return m_RealFmt;
}
void CObjectOStreamXml::SetRealValueFormat(
    CObjectOStreamXml::ERealValueFormat fmt)
{
    m_RealFmt = fmt;
}
void CObjectOStreamXml::SetEnforcedStdXml(bool set)
{
    m_EnforcedStdXml = set;
    if (m_EnforcedStdXml) {
        m_StdXml = false;
    }
}


string CObjectOStreamXml::GetPosition(void) const
{
    return "line "+NStr::UIntToString(m_Output.GetLine());
}

static string GetPublicModuleName(TTypeInfo type)
{
    const string& s = type->GetModuleName();
    string name;
    for ( string::const_iterator i = s.begin(); i != s.end(); ++i ) {
        char c = *i;
        if ( !isalnum(c) )
            name += ' ';
        else
            name += c;
    }
    return name;
}

string CObjectOStreamXml::GetModuleName(TTypeInfo type)
{
    string name;
    if( !m_DTDFileName.empty() ) {
        name = m_DTDFileName;
    }
    else {
        const string& s = type->GetModuleName();
        for ( string::const_iterator i = s.begin(); i != s.end(); ++i ) {
            char c = *i;
            if ( c == '-' )
                name += '_';
            else
                name += c;
        }
    }
    return name;
}

void CObjectOStreamXml::WriteFileHeader(TTypeInfo type)
{
    m_Output.PutString("<?xml version=\"1.0");
    switch (m_Encoding) {
    default:
        break;
    case eEncoding_UTF8:
        m_Output.PutString("\" encoding=\"UTF-8");
        break;
    case eEncoding_ISO8859_1:
        m_Output.PutString("\" encoding=\"ISO-8859-1");   
        break;
    case eEncoding_Windows_1252:
        m_Output.PutString("\" encoding=\"Windows-1252");   
        break;
    }
    m_Output.PutString("\"?>");

    if (!m_UseSchemaRef && m_UseDTDRef) {
        m_Output.PutEol();
        m_Output.PutString("<!DOCTYPE ");
        m_Output.PutString(type->GetName());
    
        if (m_UsePublicId) {
            m_Output.PutString(" PUBLIC \"");
            if (m_PublicId.empty()) {
                m_Output.PutString("-//NCBI//");
                m_Output.PutString(GetPublicModuleName(type));
                m_Output.PutString("/EN");
            } else {
                m_Output.PutString(m_PublicId);
            }
            m_Output.PutString("\"");
        } else {
            m_Output.PutString(" SYSTEM");
        }
        m_Output.PutString(" \"");
        m_Output.PutString(GetDTDFilePrefix() + GetModuleName(type));
        m_Output.PutString(".dtd\">");
    }
    m_LastTagAction = eTagClose;
}

void CObjectOStreamXml::EndOfWrite(void)
{
    m_Output.PutEol(false);
    CObjectOStream::EndOfWrite();
}

void CObjectOStreamXml::x_WriteClassNamespace(TTypeInfo type)
{
    OpenTagEndBack();

    if (m_UseSchemaLoc) {
        m_Output.PutEol();
        m_Output.PutString("   ");
    }
    m_Output.PutString(" xmlns");
    if (!m_CurrNsPrefix.empty()) {
       m_Output.PutChar(':');
       m_Output.PutString(m_CurrNsPrefix);
    }
    m_Output.PutString("=\"");

    string ns_name( m_NsPrefixToName[m_CurrNsPrefix]);
    if (ns_name.empty()) {
        ns_name = GetDefaultSchemaNamespace();
    }
    m_Output.PutString(ns_name + "\"");

    if (m_UseSchemaLoc) {
        m_Output.PutEol();
        string xs_name("http://www.w3.org/2001/XMLSchema-instance");
        string xs_prefix("xs");
        if (m_NsNameToPrefix.find(xs_name) == m_NsNameToPrefix.end()) {
            for (char a='a';
                m_NsPrefixToName.find(xs_prefix) != m_NsPrefixToName.end(); ++a) {
                xs_prefix += a;
            }
            m_NsPrefixToName[xs_prefix] = xs_name;
            m_NsNameToPrefix[xs_name] = xs_prefix;
            m_Output.PutString("    xmlns:");
            m_Output.PutString(xs_prefix + "=\"");
            m_Output.PutString(xs_name + "\"");
            m_Output.PutEol();
            m_Output.PutString("    ");
            m_Output.PutString(xs_prefix);
            m_Output.PutString(":schemaLocation=\"");
            m_Output.PutString(ns_name + " ");
            m_Output.PutString(GetDTDFilePrefix() + GetModuleName(type));
            m_Output.PutString(".xsd\"");
            m_Output.PutEol();
        }
    }
    OpenTagEnd();
}

bool CObjectOStreamXml::x_ProcessTypeNamespace(TTypeInfo type)
{
    if (m_UseSchemaRef) {
        string nsName;
        if (type->HasNamespaceName()) {
            nsName = type->GetNamespaceName();
        } else if (m_NsPrefixes.empty()) {
            nsName = GetDefaultSchemaNamespace();
        }
        return x_BeginNamespace(nsName,type->GetNamespacePrefix());
    }
    return false;
}

void CObjectOStreamXml::x_EndTypeNamespace(void)
{
    if (m_UseSchemaRef) {
        if (TopFrame().HasTypeInfo()) {
            TTypeInfo type = TopFrame().GetTypeInfo();
            if (type->HasNamespaceName()) {
                x_EndNamespace(type->GetNamespaceName());
            }
        }
    }
}

bool CObjectOStreamXml::x_BeginNamespace(const string& ns_name,
                                         const string& ns_prefix)
{
    if (!m_UseSchemaRef || ns_name.empty()) {
        return false;
    }
    string nsPrefix(ns_prefix);
    if (m_NsNameToPrefix.find(ns_name) == m_NsNameToPrefix.end()) {
        for (char a='a';
            m_NsPrefixToName.find(nsPrefix) != m_NsPrefixToName.end(); ++a) {
            nsPrefix += a;
        }
        m_CurrNsPrefix = nsPrefix;
        m_NsNameToPrefix[ns_name] = nsPrefix;
        m_NsPrefixToName[nsPrefix] = ns_name;
        m_NsPrefixes.push(nsPrefix);
        return true;
    } else {
        m_CurrNsPrefix = m_NsNameToPrefix[ns_name];
        m_NsPrefixes.push(m_CurrNsPrefix);
    }
    return false;
}

void CObjectOStreamXml::x_EndNamespace(const string& ns_name)
{
    if (!m_UseSchemaRef || ns_name.empty()) {
        return;
    }
    string nsPrefix = m_NsNameToPrefix[ns_name];
// not sure about it - should we erase them or not?
//    m_NsNameToPrefix.erase(ns_name);
//    m_NsPrefixToName.erase(nsPrefix);
    m_NsPrefixes.pop();
    m_CurrNsPrefix = m_NsPrefixes.empty() ? kEmptyStr : m_NsPrefixes.top();
    if (!m_Attlist && GetStackDepth() <= 2) {
        m_NsNameToPrefix.clear();
        m_NsPrefixToName.clear();
    }
}

void CObjectOStreamXml::WriteEnum(const CEnumeratedTypeValues& values,
                                  TEnumValueType value,
                                  const string& valueName)
{
    if ( !values.GetName().empty() ) {
        // global enum
        OpenTagStart();
        m_Output.PutString(values.GetName());
        if ( !valueName.empty() ) {
            m_Output.PutString(" value=\"");
            m_Output.PutString(valueName);
            m_Output.PutChar('\"');
        }
        if ( values.IsInteger() ) {
            OpenTagEnd();
            m_Output.PutInt4(value);
            CloseTagStart();
            m_Output.PutString(values.GetName());
            CloseTagEnd();
        }
        else {
            _ASSERT(!valueName.empty());
            SelfCloseTagEnd();
            m_LastTagAction = eTagClose;
        }
    }
    else {
        // local enum (member, variant or element)
        if ( valueName.empty() ) {
            _ASSERT(values.IsInteger());
            m_Output.PutInt4(value);
        }
        else {
            if (m_LastTagAction == eAttlistTag) {
                m_Output.PutString(valueName);
            } else {
                OpenTagEndBack();
                m_Output.PutString(" value=\"");
                m_Output.PutString(valueName);
                m_Output.PutChar('"');
                if ( values.IsInteger() ) {
                    OpenTagEnd();
                    m_Output.PutInt4(value);
                }
                else {
                    SelfCloseTagEnd();
                }
            }
        }
    }
}

void CObjectOStreamXml::WriteEnum(const CEnumeratedTypeValues& values,
                                  TEnumValueType value)
{
    WriteEnum(values, value, values.FindName(value, values.IsInteger()));
}

void CObjectOStreamXml::CopyEnum(const CEnumeratedTypeValues& values,
                                 CObjectIStream& in)
{
    TEnumValueType value = in.ReadEnum(values);
    WriteEnum(values, value, values.FindName(value, values.IsInteger()));
}

void CObjectOStreamXml::WriteEscapedChar(char c)
{
//  http://www.w3.org/TR/2000/REC-xml-20001006#NT-Char
    switch ( c ) {
    case '&':
        m_Output.PutString("&amp;");
        break;
    case '<':
        m_Output.PutString("&lt;");
        break;
    case '>':
        m_Output.PutString("&gt;");
        break;
    case '\'':
        m_Output.PutString("&apos;");
        break;
    case '"':
        m_Output.PutString("&quot;");
        break;
    default:
        if ((unsigned int)c < 0x20) {
            m_Output.PutString("&#x");
            Uint1 ch = c;
            unsigned hi = ch >> 4;
            unsigned lo = ch & 0xF;
            if ( hi ) {
                m_Output.PutChar("0123456789abcdef"[hi]);
            }
            m_Output.PutChar("0123456789abcdef"[lo]);
            m_Output.PutChar(';');
        } else if ((unsigned int)c >= 0x80) {
            if ( m_Encoding == eEncoding_UTF8 ||
                 m_Encoding == eEncoding_Unknown ) {
                Uint1 ch = c;
                m_Output.PutChar((ch >> 6) | 0xC0);
                m_Output.PutChar((ch & 0x3F) | 0x80);
            } else {
                m_Output.PutChar(c);
            }
        } else {
            m_Output.PutChar(c);
        }
        break;
    }
}

void CObjectOStreamXml::WriteBool(bool data)
{
    OpenTagEndBack();
    if ( data )
        m_Output.PutString(" value=\"true\"");
    else
        m_Output.PutString(" value=\"false\"");
    SelfCloseTagEnd();
}

void CObjectOStreamXml::WriteChar(char data)
{
    WriteEscapedChar(data);
}

void CObjectOStreamXml::WriteInt4(Int4 data)
{
    m_Output.PutInt4(data);
}

void CObjectOStreamXml::WriteUint4(Uint4 data)
{
    m_Output.PutUint4(data);
}

void CObjectOStreamXml::WriteInt8(Int8 data)
{
    m_Output.PutInt8(data);
}

void CObjectOStreamXml::WriteUint8(Uint8 data)
{
    m_Output.PutUint8(data);
}

void CObjectOStreamXml::WriteDouble2(double data, size_t digits)
{
    if (isnan(data)) {
        ThrowError(fInvalidData, "invalid double: not a number");
    }
    if (!finite(data)) {
        ThrowError(fInvalidData, "invalid double: infinite");
    }
    char buffer[512];
    SIZE_TYPE width;
    if (m_RealFmt == eRealFixedFormat) {
        int shift = int(ceil(log10(fabs(data))));
        int precision = int(digits - shift);
        if ( precision < 0 )
            precision = 0;
        if ( precision > 64 ) // limit precision of data
            precision = 64;
        width = NStr::DoubleToString(data, (unsigned int)precision,
                                    buffer, sizeof(buffer));
        if (precision != 0) {
            while (buffer[width-1] == '0') {
                --width;
            }
            if (buffer[width-1] == '.') {
                --width;
            }
        }
    } else {
        width = sprintf(buffer, "%.*g", digits, data);
    }
    m_Output.PutString(buffer, width);
}

void CObjectOStreamXml::WriteDouble(double data)
{
    WriteDouble2(data, DBL_DIG);
}

void CObjectOStreamXml::WriteFloat(float data)
{
    WriteDouble2(data, FLT_DIG);
}

void CObjectOStreamXml::WriteNull(void)
{
    OpenTagEndBack();
    SelfCloseTagEnd();
}

void CObjectOStreamXml::WriteAnyContentObject(const CAnyContentObject& obj)
{
    if (obj.GetName().empty()) {
        return;
    }
    string ns_name(obj.GetNamespaceName());
    bool needNs = x_BeginNamespace(ns_name,obj.GetNamespacePrefix());
    OpenTag(obj.GetName());

    if (m_UseSchemaRef) {
        OpenTagEndBack();
        if (needNs) {
            m_Output.PutEol();
            m_Output.PutString("    xmlns");
            if (!m_CurrNsPrefix.empty()) {
                m_Output.PutChar(':');
                m_Output.PutString(m_CurrNsPrefix);
            }
            m_Output.PutString("=\"");
            m_Output.PutString(ns_name);
            m_Output.PutChar('\"');
        }

        const vector<CSerialAttribInfoItem>& attlist = obj.GetAttributes();
        if (!attlist.empty()) {
            m_Attlist = true;
            vector<CSerialAttribInfoItem>::const_iterator it;
            for ( it = attlist.begin(); it != attlist.end(); ++it) {
                string ns(it->GetNamespaceName());
                string ns_prefix;
                if (x_BeginNamespace(ns,kEmptyStr)) {
                    m_Output.PutEol();
                    m_Output.PutString("    xmlns");
                    ns_prefix = m_NsNameToPrefix[ns];
                    if (!ns_prefix.empty()) {
                        m_Output.PutChar(':');
                        m_Output.PutString(ns_prefix);
                    }
                    m_Output.PutString("=\"");
                    m_Output.PutString(ns);
                    m_Output.PutChar('\"');
                }
                ns_prefix = m_NsNameToPrefix[ns];

                m_Output.PutEol();
                m_Output.PutString("    ");
                if (!ns_prefix.empty()) {
                    m_Output.PutString(ns_prefix);
                    m_Output.PutChar(':');
                }
                m_Output.PutString(it->GetName());
                m_Output.PutString("=\"");
                m_Output.PutString(it->GetValue());
                m_Output.PutChar('\"');
                x_EndNamespace(ns);
            }
            m_Attlist = false;
        }
        OpenTagEnd();
    }

// value
// no verification on write!
    const string& value = obj.GetValue();
    if (value.empty()) {
        OpenTagEndBack();
        SelfCloseTagEnd();
        m_LastTagAction = eTagClose;
        x_EndNamespace(ns_name);
        return;
    }
    bool was_open = true, was_close=true;
    for (string::const_iterator is=value.begin(); is != value.end(); ++is) {
        if (*is == '/' && *(is+1) == '>') {
            m_Output.DecIndentLevel();
            was_open = false;
        }
        if (*is == '<') {
            if (*(is+1) == '/') {
                m_Output.DecIndentLevel();
                if (!was_open && was_close) {
                    m_Output.PutEol();
                }
                was_open = false;
            } else {
                if (was_close) {
                    m_Output.PutEol();
                }
                m_Output.IncIndentLevel();
                was_open = true;
            }
        }
        m_Output.PutChar(*is);
        if (*is == '<') {
            if (*(is+1) == '/') {
                m_Output.PutChar(*(++is));
            }
            if (m_UseSchemaRef && !m_CurrNsPrefix.empty()) {
                m_Output.PutString(m_CurrNsPrefix);
                m_Output.PutChar(':');
            }
        }
        was_close = (*is == '>');
    }

// close tag
    if (!was_open) {
        m_EndTag = true;
    }
    m_SkipIndent = !was_close;
    CloseTag(obj.GetName());
    x_EndNamespace(ns_name);
}

void CObjectOStreamXml::CopyAnyContentObject(CObjectIStream& /* in */)
{
    NCBI_THROW(CSerialException,eNotImplemented,"not yet");
}

void CObjectOStreamXml::WriteCString(const char* str)
{
    if ( str == 0 ) {
        OpenTagEndBack();
        SelfCloseTagEnd();
    }
    else {
        while ( *str ) {
            WriteEscapedChar(*str++);
        }
    }
}

void CObjectOStreamXml::WriteString(const string& str, EStringType type)
{
    EEncoding enc = m_Encoding;
    if (type == eStringTypeUTF8) {
        if (m_Encoding == eEncoding_UTF8 || m_Encoding == eEncoding_Unknown) {
            m_Encoding = eEncoding_ISO8859_1;
        } else {
            string tmp = (static_cast<const CStringUTF8&>(str)).AsAscii();
            WriteString( tmp, eStringTypeVisible);
            return;
        }
    }
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
    }
    m_Encoding = enc;
}

void CObjectOStreamXml::WriteStringStore(const string& str)
{
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
    }
}

void CObjectOStreamXml::CopyString(CObjectIStream& in)
{
    string str;
    in.ReadStd(str);
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
    }
}

void CObjectOStreamXml::CopyStringStore(CObjectIStream& in)
{
    string str;
    in.ReadStringStore(str);
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
    }
}

void CObjectOStreamXml::WriteNullPointer(void)
{
    OpenTagEndBack();
    SelfCloseTagEnd();
}

void CObjectOStreamXml::WriteObjectReference(TObjectIndex index)
{
    m_Output.PutString("<object index=");
    if ( sizeof(TObjectIndex) == sizeof(Int4) )
        m_Output.PutInt4(Int4(index));
    else if ( sizeof(TObjectIndex) == sizeof(Int8) )
        m_Output.PutInt8(index);
    else
        ThrowError(fIllegalCall, "invalid size of TObjectIndex"
            "must be either sizeof(Int4) or sizeof(Int8)");
    m_Output.PutString("/>");
    m_EndTag = true;
}

void CObjectOStreamXml::WriteTag(const string& name)
{
    if (!m_Attlist && !m_CurrNsPrefix.empty()) {
        m_Output.PutString(m_CurrNsPrefix);
        m_Output.PutChar(':');
    }
    m_Output.PutString(name);
}

void CObjectOStreamXml::OpenTagStart(void)
{
    if (m_Attlist) {
        if ( m_LastTagAction == eTagOpen ) {
            m_Output.PutChar(' ');
            m_LastTagAction = eAttlistTag;
        }
    } else {
        if (m_SkipIndent) {
            m_SkipIndent = false;
        } else {
            m_Output.PutEol(false);
            m_Output.PutIndent();
        }
        m_Output.PutChar('<');
        m_LastTagAction = eTagOpen;
    }
    m_EndTag = false;
}

void CObjectOStreamXml::OpenTagEnd(void)
{
    if (m_Attlist) {
        if ( m_LastTagAction == eAttlistTag ) {
            m_Output.PutString("=\"");
        }
    } else {
        if ( m_LastTagAction == eTagOpen ) {
            m_Output.PutChar('>');
            m_Output.IncIndentLevel();
            m_LastTagAction = eTagClose;
        }
    }
}

void CObjectOStreamXml::OpenTagEndBack(void)
{
    _ASSERT(m_LastTagAction == eTagClose);
    m_LastTagAction = eTagOpen;
    m_Output.BackChar('>');
    m_Output.DecIndentLevel();
}

void CObjectOStreamXml::SelfCloseTagEnd(void)
{
    _ASSERT(m_LastTagAction == eTagOpen);
    m_Output.PutString("/>");
    m_LastTagAction = eTagSelfClosed;
    m_EndTag = true;
    m_SkipIndent = false;
}

void CObjectOStreamXml::EolIfEmptyTag(void)
{
    _ASSERT(m_LastTagAction != eTagSelfClosed);
    if ( m_LastTagAction == eTagOpen ) {
        m_LastTagAction = eTagClose;
    }
}

void CObjectOStreamXml::CloseTagStart(void)
{
    _ASSERT(m_LastTagAction != eTagSelfClosed);
    m_Output.DecIndentLevel();
    if (m_EndTag && !m_SkipIndent) {
        m_Output.PutEol(false);
        m_Output.PutIndent();
    }
    m_Output.PutString("</");
    m_LastTagAction = eTagOpen;
}

void CObjectOStreamXml::CloseTagEnd(void)
{
    m_Output.PutChar('>');
    m_LastTagAction = eTagClose;
    m_EndTag = true;
    m_SkipIndent = false;
}

void CObjectOStreamXml::PrintTagName(size_t level)
{
    const TFrame& frame = FetchFrameFromTop(level);
    switch ( frame.GetFrameType() ) {
    case TFrame::eFrameNamed:
    case TFrame::eFrameArray:
    case TFrame::eFrameClass:
    case TFrame::eFrameChoice:
        {
            _ASSERT(frame.GetTypeInfo());
            const string& name = frame.GetTypeInfo()->GetName();
            if ( !name.empty() ) {
                WriteTag(name);
#if defined(NCBI_SERIAL_IO_TRACE)
    TraceTag(name);
#endif
            } else {
                PrintTagName(level + 1);
            }
            return;
        }
    case TFrame::eFrameClassMember:
    case TFrame::eFrameChoiceVariant:
        {
            bool attlist = m_Attlist;
            if (!x_IsStdXml()) {
                PrintTagName(level + 1);
                m_Output.PutChar('_');
                m_Attlist = true;
            }
            WriteTag(frame.GetMemberId().GetName());
            m_Attlist = attlist;
#if defined(NCBI_SERIAL_IO_TRACE)
    TraceTag(frame.GetMemberId().GetName());
#endif
            return;
        }
    case TFrame::eFrameArrayElement:
        {
            PrintTagName(level + 1);
            if (!x_IsStdXml()) {
                m_Output.PutString("_E");
            }
            return;
        }
    default:
        break;
    }
    ThrowError(fIllegalCall, "illegal frame type");
}

void CObjectOStreamXml::WriteOtherBegin(TTypeInfo typeInfo)
{
    OpenTag(typeInfo);
}

void CObjectOStreamXml::WriteOtherEnd(TTypeInfo typeInfo)
{
    CloseTag(typeInfo);
}

void CObjectOStreamXml::WriteOther(TConstObjectPtr object,
                                   TTypeInfo typeInfo)
{
    OpenTag(typeInfo);
    WriteObject(object, typeInfo);
    CloseTag(typeInfo);
}

void CObjectOStreamXml::BeginContainer(const CContainerTypeInfo* containerType)
{
    bool needNs = x_ProcessTypeNamespace(containerType);
    if (!x_IsStdXml()) {
        if (TopFrame().GetFrameType() == CObjectStackFrame::eFrameArray &&
            FetchFrameFromTop(1).GetFrameType() == CObjectStackFrame::eFrameNamed) {
            const CClassTypeInfo* clType =
                dynamic_cast<const CClassTypeInfo*>(FetchFrameFromTop(1).GetTypeInfo());
            if (clType && clType->Implicit()) {
                TopFrame().SetNotag();
                return;
            }
        }
        OpenTagIfNamed(containerType);
    }
    if (needNs) {
        x_WriteClassNamespace(containerType);
    }
}

void CObjectOStreamXml::EndContainer(void)
{
    if (!x_IsStdXml() && !TopFrame().GetNotag()) {
        CloseTagIfNamed(TopFrame().GetTypeInfo());
    }
    x_EndTypeNamespace();
}

bool CObjectOStreamXml::WillHaveName(TTypeInfo elementType)
{
    while ( elementType->GetName().empty() ) {
        if ( elementType->GetTypeFamily() != eTypeFamilyPointer )
            return false;
        elementType = CTypeConverter<CPointerTypeInfo>::SafeCast(elementType)->GetPointedType();
    }
    // found named type
    return true;
}

void CObjectOStreamXml::BeginContainerElement(TTypeInfo elementType)
{
    if ( !WillHaveName(elementType) ) {
        BeginArrayElement(elementType);
    }
}

void CObjectOStreamXml::EndContainerElement(void)
{
    if ( !WillHaveName(TopFrame().GetTypeInfo()) ) {
        EndArrayElement();
    }
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamXml::WriteContainer(const CContainerTypeInfo* cType,
                                       TConstObjectPtr containerPtr)
{
    if ( !cType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameArray, cType);
        OpenTag(cType);

        WriteContainerContents(cType, containerPtr);

        EolIfEmptyTag();
        CloseTag(cType);
        END_OBJECT_FRAME();
    }
    else {
        WriteContainerContents(cType, containerPtr);
    }
}
#endif
void CObjectOStreamXml::BeginArrayElement(TTypeInfo elementType)
{
    if (x_IsStdXml() && GetRealTypeFamily(elementType) != eTypeFamilyPrimitive) {
        TopFrame().SetNotag();
    } else {
        OpenStackTag(0);
    }
}

void CObjectOStreamXml::EndArrayElement(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    } else {
        CloseStackTag(0);
    }
}

void CObjectOStreamXml::WriteContainerContents(const CContainerTypeInfo* cType,
                                               TConstObjectPtr containerPtr)
{
    TTypeInfo elementType = cType->GetElementType();
    CContainerTypeInfo::CConstIterator i;
    if ( WillHaveName(elementType) ) {
        if ( cType->InitIterator(i, containerPtr) ) {
            do {
                if (elementType->GetTypeFamily() == eTypeFamilyPointer) {
                    const CPointerTypeInfo* pointerType =
                        CTypeConverter<CPointerTypeInfo>::SafeCast(elementType);
                    _ASSERT(pointerType->GetObjectPointer(cType->GetElementPtr(i)));
                    if ( !pointerType->GetObjectPointer(cType->GetElementPtr(i)) ) {
                        ERR_POST(Warning << " NULL pointer found in container: skipping");
                        continue;
                    }
                }

                WriteObject(cType->GetElementPtr(i), elementType);

            } while ( cType->NextElement(i) );
        }
    }
    else {
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        if ( cType->InitIterator(i, containerPtr) ) {
            do {
                BeginArrayElement(elementType);
                WriteObject(cType->GetElementPtr(i), elementType);
                EndArrayElement();
            } while ( cType->NextElement(i) );
        } else {
            const TFrame& frame = FetchFrameFromTop(1);
            if (frame.GetFrameType() == CObjectStackFrame::eFrameNamed) {
                const CClassTypeInfo* clType =
                    dynamic_cast<const CClassTypeInfo*>(frame.GetTypeInfo());
                if (clType && clType->Implicit() && clType->IsImplicitNonEmpty()) {
                    ThrowError(fInvalidData, "container is empty");
                }
            }
        }

        END_OBJECT_FRAME();
    }
}

void CObjectOStreamXml::BeginNamedType(TTypeInfo namedTypeInfo)
{
    const CClassTypeInfo* classType =
        dynamic_cast<const CClassTypeInfo*>(namedTypeInfo);
    if (classType) {
        CheckStdXml(classType);
    }
    bool needNs = x_ProcessTypeNamespace(namedTypeInfo);
    OpenTag(namedTypeInfo);
    if (needNs) {
        x_WriteClassNamespace(namedTypeInfo);
    }
}

void CObjectOStreamXml::EndNamedType(void)
{
    CloseTag(TopFrame().GetTypeInfo());
    x_EndTypeNamespace();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamXml::WriteNamedType(TTypeInfo namedTypeInfo,
                                       TTypeInfo typeInfo,
                                       TConstObjectPtr object)
{
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    
    BeginNamedType(namedTypeInfo);
    WriteObject(object, typeInfo);
    EndNamedType();

    END_OBJECT_FRAME();
}

void CObjectOStreamXml::CopyNamedType(TTypeInfo namedTypeInfo,
                                      TTypeInfo objectType,
                                      CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameNamed, namedTypeInfo);
    copier.In().BeginNamedType(namedTypeInfo);
    BeginNamedType(namedTypeInfo);
    CopyObject(objectType, copier);
    EndNamedType();
    copier.In().EndNamedType();
    END_OBJECT_2FRAMES_OF(copier);
}
#endif

void CObjectOStreamXml::CheckStdXml(const CClassTypeInfoBase* classType)
{
    if (m_EnforcedStdXml) {
        m_StdXml = false;
        return;
    }
    TMemberIndex first = classType->GetItems().FirstIndex();
    m_StdXml = classType->GetItems().GetItemInfo(first)->GetId().HaveNoPrefix();
}

ETypeFamily CObjectOStreamXml::GetRealTypeFamily(TTypeInfo typeInfo)
{
    ETypeFamily type = typeInfo->GetTypeFamily();
    if (type == eTypeFamilyPointer) {
        const CPointerTypeInfo* ptr =
            dynamic_cast<const CPointerTypeInfo*>(typeInfo);
        if (ptr) {
            type = ptr->GetPointedType()->GetTypeFamily();
        }
    }
    return type;
}

ETypeFamily CObjectOStreamXml::GetContainerElementTypeFamily(TTypeInfo typeInfo)
{
    if (typeInfo->GetTypeFamily() == eTypeFamilyPointer) {
        const CPointerTypeInfo* ptr =
            dynamic_cast<const CPointerTypeInfo*>(typeInfo);
        if (ptr) {
            typeInfo = ptr->GetPointedType();
        }
    }
    _ASSERT(typeInfo->GetTypeFamily() == eTypeFamilyContainer);
    const CContainerTypeInfo* ptr =
        dynamic_cast<const CContainerTypeInfo*>(typeInfo);
    return GetRealTypeFamily(ptr->GetElementType());
}

void CObjectOStreamXml::BeginClass(const CClassTypeInfo* classInfo)
{
    CheckStdXml(classInfo);
    bool needNs = x_ProcessTypeNamespace(classInfo);
    OpenTagIfNamed(classInfo);
    if (needNs) {
        x_WriteClassNamespace(classInfo);
    }
}

void CObjectOStreamXml::EndClass(void)
{
    if (!m_Attlist && m_LastTagAction != eTagSelfClosed) {
        EolIfEmptyTag();
    }
    CloseTagIfNamed(TopFrame().GetTypeInfo());
    x_EndTypeNamespace();
}

void CObjectOStreamXml::BeginClassMember(const CMemberId& id)
{
    const CClassTypeInfoBase* classType = dynamic_cast<const CClassTypeInfoBase*>
        (FetchFrameFromTop(1).GetTypeInfo());
    _ASSERT(classType);
    BeginClassMember(classType->GetItemInfo(id.GetName())->GetTypeInfo(),id);
}

void CObjectOStreamXml::BeginClassMember(TTypeInfo memberType,
                                         const CMemberId& id)
{
    if (x_IsStdXml()) {
        if(id.IsAttlist()) {
            if(m_LastTagAction == eTagClose) {
                OpenTagEndBack();
            }
            m_Attlist = true;
            TopFrame().SetNotag();
        } else {
            ETypeFamily type = GetRealTypeFamily(memberType);
            bool needTag = true;
            if (GetEnforcedStdXml()) {
                if (type == eTypeFamilyContainer) {
                    needTag = (GetContainerElementTypeFamily(memberType) !=
                               eTypeFamilyPrimitive);
                }
            } else {
                needTag = (type == eTypeFamilyPrimitive && !id.HasNotag());
            }
            if (needTag) {
                OpenStackTag(0);
            } else {
                TopFrame().SetNotag();
            }
            if (type == eTypeFamilyPrimitive) {
                m_SkipIndent = id.HasNotag();
            }
        }
    } else {
        OpenStackTag(0);
    }
}

void CObjectOStreamXml::EndClassMember(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
        m_Attlist = false;
        if(m_LastTagAction == eTagOpen) {
            OpenTagEnd();
        }
    } else {
        CloseStackTag(0);
    }
}


#ifdef VIRTUAL_MID_LEVEL_IO

void CObjectOStreamXml::WriteClass(const CClassTypeInfo* classType,
                                   TConstObjectPtr classPtr)
{
    if ( !classType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameClass, classType);

        BeginClass(classType);
        for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
            classType->GetMemberInfo(i)->WriteMember(*this, classPtr);
        }
        EndClass();

        END_OBJECT_FRAME();
    }
    else {
        for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
            classType->GetMemberInfo(i)->WriteMember(*this, classPtr);
        }
    }
}

void CObjectOStreamXml::WriteClassMember(const CMemberId& memberId,
                                         TTypeInfo memberType,
                                         TConstObjectPtr memberPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClassMember,memberId);

    BeginClassMember(memberType,memberId);
    WriteObject(memberPtr, memberType);
    EndClassMember();

    END_OBJECT_FRAME();
}

bool CObjectOStreamXml::WriteClassMember(const CMemberId& memberId,
                                         const CDelayBuffer& buffer)
{
    if ( !buffer.HaveFormat(eSerial_Xml) )
        return false;

    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    OpenStackTag(0);
    
    Write(buffer.GetSource());
    
    CloseStackTag(0);
    END_OBJECT_FRAME();

    return true;
}
#endif

void CObjectOStreamXml::BeginChoice(const CChoiceTypeInfo* choiceType)
{
    CheckStdXml(choiceType);
    bool needNs = x_ProcessTypeNamespace(choiceType);
    OpenTagIfNamed(choiceType);
    if (needNs) {
        x_WriteClassNamespace(choiceType);
    }
}

void CObjectOStreamXml::EndChoice(void)
{
    CloseTagIfNamed(TopFrame().GetTypeInfo());
    x_EndTypeNamespace();
}

void CObjectOStreamXml::BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                           const CMemberId& id)
{
    if (x_IsStdXml()) {
        const CVariantInfo* var_info = choiceType->GetVariantInfo(id.GetName());
        ETypeFamily type = GetRealTypeFamily(var_info->GetTypeInfo());
        bool needTag = true;
        if (GetEnforcedStdXml()) {
            if (type == eTypeFamilyContainer) {
                needTag = (GetContainerElementTypeFamily(
                    var_info->GetTypeInfo()) != eTypeFamilyPrimitive);
            }
        } else {
            needTag = (type == eTypeFamilyPrimitive && !id.HasNotag());
        }
        if (needTag) {
            OpenStackTag(0);
        } else {
            TopFrame().SetNotag();
        }
    } else {
        OpenStackTag(0);
    }
}

void CObjectOStreamXml::EndChoiceVariant(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    } else {
        CloseStackTag(0);
    }
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamXml::WriteChoice(const CChoiceTypeInfo* choiceType,
                                    TConstObjectPtr choicePtr)
{
    if ( !choiceType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);
        OpenTag(choiceType);

        WriteChoiceContents(choiceType, choicePtr);

        CloseTag(choiceType);
        END_OBJECT_FRAME();
    }
    else {
        WriteChoiceContents(choiceType, choicePtr);
    }
}
#endif

void CObjectOStreamXml::WriteChoiceContents(const CChoiceTypeInfo* choiceType,
                                            TConstObjectPtr choicePtr)
{
    TMemberIndex index = choiceType->GetIndex(choicePtr);
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());
    OpenStackTag(0);
    
    variantInfo->WriteVariant(*this, choicePtr);
    
    CloseStackTag(0);
    END_OBJECT_FRAME();
}

static const char* const HEX = "0123456789ABCDEF";

void CObjectOStreamXml::WriteBytes(const ByteBlock& ,
                                   const char* bytes, size_t length)
{
    while ( length-- > 0 ) {
        char c = *bytes++;
        m_Output.PutChar(HEX[(c >> 4) & 0xf]);
        m_Output.PutChar(HEX[c & 0xf]);
    }
}

void CObjectOStreamXml::WriteChars(const CharBlock& ,
                                   const char* chars, size_t length)
{
    while ( length-- > 0 ) {
        char c = *chars++;
        WriteEscapedChar(c);
    }
}


void CObjectOStreamXml::WriteSeparator(void)
{
    m_Output.PutString(GetSeparator());
    FlushBuffer();
}

#if defined(NCBI_SERIAL_IO_TRACE)

void CObjectOStreamXml::TraceTag(const string& name)
{
    cout << ", Tag=" << name;
}

#endif

END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.80  2005/02/23 21:07:44  vasilche
* Allow to skip underlying stream flush.
*
* Revision 1.79  2005/02/09 14:32:11  gouriano
* Implemented serialization of mixed content elements
*
* Revision 1.78  2005/02/01 21:47:14  grichenk
* Fixed warnings
*
* Revision 1.77  2004/10/27 18:59:48  gouriano
* Corrected writing namespace in case of named type
*
* Revision 1.76  2004/07/29 15:53:44  gouriano
* write XML namespace when the root object is container
*
* Revision 1.75  2004/06/22 14:58:25  gouriano
* Corrected writing AnyContentObject with no value
*
* Revision 1.74  2004/06/08 20:22:32  gouriano
* Moved several functions out of VIRTUAL_MID_LEVEL_IO condition:
* there is no need for them to be there
*
* Revision 1.73  2004/05/19 17:26:19  gouriano
* Corrected serialization of containers when the code was generated by DTD
*
* Revision 1.72  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.71  2004/05/04 17:04:43  gouriano
* Check double for being finite
*
* Revision 1.70  2004/01/29 20:43:04  gouriano
* Corrected writing of AnyContent objects in case it has attributes
* with previously undefined namespaces, or tags with empty content
*
* Revision 1.69  2004/01/22 20:48:38  gouriano
* corrected writing of namespace prefixes
*
* Revision 1.68  2004/01/08 17:42:00  gouriano
* Added encoding Windows-1252.
* Made it possible to omit document type declaration
*
* Revision 1.67  2003/11/26 19:59:41  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.66  2003/11/21 16:16:48  vasilche
* More efficient writing of hex numbers.
*
* Revision 1.65  2003/10/31 17:39:40  grichenk
* Allow empty containers
*
* Revision 1.64  2003/09/25 19:44:08  gouriano
* Corrected serialization of AnyContent object's attributes
*
* Revision 1.63  2003/09/22 20:55:23  gouriano
* Corrected writing of AnyContent objects in case of no schema reference
*
* Revision 1.62  2003/09/16 14:48:36  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.61  2003/08/25 15:59:09  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.60  2003/08/13 18:23:54  gouriano
* corrected XML schema namespace name
*
* Revision 1.59  2003/08/13 15:47:45  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.58  2003/07/02 13:01:29  gouriano
* added ability to read/write XML files with reference to schema
*
* Revision 1.57  2003/06/30 15:39:48  gouriano
* added encoding (utf-8 or iso-8859-1)
*
* Revision 1.56  2003/06/24 20:57:36  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.55  2003/05/22 20:10:02  gouriano
* added UTF8 strings
*
* Revision 1.54  2003/05/16 18:02:18  gouriano
* revised exception error messages
*
* Revision 1.53  2003/04/29 18:30:37  gouriano
* object data member initialization verification
*
* Revision 1.52  2003/03/26 16:14:23  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.51  2003/02/07 16:09:22  gouriano
* correction of GetContainerElementTypeFamily for the case of copying objects
*
* Revision 1.50  2003/02/05 17:08:00  gouriano
* added possibility to read/write objects generated from an ASN.1 spec as "standard" XML - without scope prefixes
*
* Revision 1.49  2003/02/04 17:06:25  gouriano
* added check for NaN in WriteDouble
*
* Revision 1.48  2003/01/22 21:03:16  gouriano
* remove trailing zeros when writing a float value
*
* Revision 1.47  2003/01/22 20:53:09  gouriano
* more control on how a float value is to be written
*
* Revision 1.46  2003/01/21 19:32:26  gouriano
* corrected reading containers of primitive types
*
* Revision 1.45  2003/01/10 17:48:41  gouriano
* added check for empty container
*
* Revision 1.44  2003/01/10 16:54:10  gouriano
* fixed a bug with optional class members
*
* Revision 1.43  2002/12/26 21:35:45  gouriano
* corrected handling choice's XML attributes
*
* Revision 1.42  2002/12/26 19:32:34  gouriano
* changed XML I/O streams to properly handle object copying
*
* Revision 1.41  2002/12/17 19:04:32  gouriano
* corrected reading/writing of character references
*
* Revision 1.40  2002/12/12 21:08:07  gouriano
* implemented handling of complex XML containers
*
* Revision 1.39  2002/11/26 22:10:31  gouriano
* added unnamed lists of sequences (choices) as container elements
*
* Revision 1.38  2002/11/20 21:22:51  gouriano
* corrected processing of unnamed sequences as choice variants
*
* Revision 1.37  2002/11/19 19:48:51  gouriano
* added support of XML attributes of choice variants
*
* Revision 1.36  2002/11/14 21:00:18  gouriano
* added support of XML attribute lists
*
* Revision 1.35  2002/10/25 14:49:27  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.34  2002/10/18 14:27:09  gouriano
* added possibility to enable/disable/set public identifier
*
* Revision 1.33  2002/10/15 13:47:59  gouriano
* modified to handle "standard" (generated from DTD) XML i/o
*
* Revision 1.32  2002/10/08 18:59:38  grichenk
* Check for null pointers in containers (assert in debug mode,
* warning in release).
*
* Revision 1.31  2002/09/25 19:37:36  gouriano
* added the possibility of having no tag prefix in XML I/O streams
*
* Revision 1.30  2002/03/07 22:02:02  grichenk
* Added "Separator" modifier for CObjectOStream
*
* Revision 1.29  2001/11/09 19:07:58  grichenk
* Fixed DTDFilePrefix functions
*
* Revision 1.28  2001/10/17 20:41:26  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.27  2001/10/17 18:18:30  grichenk
* Added CObjectOStreamXml::xxxFilePrefix() and
* CObjectOStreamXml::xxxFileName()
*
* Revision 1.26  2001/05/17 15:07:08  lavr
* Typos corrected
*
* Revision 1.25  2001/04/25 20:41:53  vakatov
* <limits.h>, <float.h>  --->  <corelib/ncbi_limits.h>
*
* Revision 1.24  2001/04/13 14:57:15  kholodov
* Added: SetDTDFileName function to set DTD module name in XML header
*
* Revision 1.23  2000/12/26 22:24:14  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.22  2000/12/15 15:38:45  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.21  2000/12/04 19:02:41  beloslyu
* changes for FreeBSD
*
* Revision 1.20  2000/11/07 17:25:41  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.19  2000/10/20 15:51:43  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.18  2000/10/17 18:45:35  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.17  2000/10/13 20:22:56  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.16  2000/10/13 16:28:40  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.15  2000/10/05 15:52:51  vasilche
* Avoid using snprintf because it's missing on osf1_gcc
*
* Revision 1.14  2000/10/05 13:17:17  vasilche
* Added missing #include <stdio.h>
*
* Revision 1.13  2000/10/04 19:19:00  vasilche
* Fixed processing floating point data.
*
* Revision 1.12  2000/10/03 17:22:45  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.11  2000/09/29 16:18:24  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.10  2000/09/26 17:38:22  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.9  2000/09/18 20:00:25  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.8  2000/09/01 13:16:20  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.7  2000/08/15 19:44:50  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.6  2000/07/03 18:42:46  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.5  2000/06/16 19:24:22  vasilche
* Updated MSVC project.
* Fixed error on MSVC with static const class member.
*
* Revision 1.4  2000/06/16 16:31:22  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.3  2000/06/07 19:46:00  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.2  2000/06/01 19:07:05  vasilche
* Added parsing of XML data.
*
* Revision 1.1  2000/05/24 20:08:49  vasilche
* Implemented XML dump.
*
* ===========================================================================
*/
