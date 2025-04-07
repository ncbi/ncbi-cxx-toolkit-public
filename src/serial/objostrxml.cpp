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
#include <serial/impl/memberid.hpp>
#include <serial/impl/memberlist.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/impl/aliasinfo.hpp>
#include <serial/delaybuf.hpp>
#include <serial/impl/ptrinfo.hpp>
#include <serial/error_codes.hpp>

#include <stdio.h>
#include <math.h>


#define NCBI_USE_ERRCODE_X   Serial_OStream

BEGIN_NCBI_SCOPE

CObjectOStream* CObjectOStream::OpenObjectOStreamXml(CNcbiOstream& out,
                                                     EOwnership deleteOut)
{
    return new CObjectOStreamXml(out, deleteOut);
}


string CObjectOStreamXml::sm_DefaultDTDFilePrefix = "";
const char* sm_DefaultNamespacePrefix = "ns";
const char* sm_DefaultSchemaNamespace = "http://www.ncbi.nlm.nih.gov";
static
const char* s_SchemaInstanceNamespace = "http://www.w3.org/2001/XMLSchema-instance";


CObjectOStreamXml::CObjectOStreamXml(CNcbiOstream& out, bool deleteOut)
    : CObjectOStream(eSerial_Xml, out, deleteOut ? eTakeOwnership : eNoOwnership),
      m_LastTagAction(eTagClose), m_SpecRef(eSpecRefNotSet), m_EndTag(true),
      m_UseDefaultDTDFilePrefix( true),
      m_UsePublicId( true),
      m_Attlist( false), m_StdXml( false), m_EnforcedStdXml( false),
      m_RealFmt( eRealScientificFormat ),
      m_Encoding( eEncoding_Unknown ), m_StringEncoding( eEncoding_UTF8 ),
      m_UseXmlDecl( true ), m_UseSchemaLoc( true ),
      m_DefaultSchemaNamespace( sm_DefaultSchemaNamespace ),
      m_SkipIndent( false ), m_SkipNextTag( false )
{
    m_Output.SetBackLimit(1);
}

CObjectOStreamXml::CObjectOStreamXml(CNcbiOstream& out, EOwnership deleteOut)
    : CObjectOStream(eSerial_Xml, out, deleteOut),
      m_LastTagAction(eTagClose), m_SpecRef(eSpecRefNotSet), m_EndTag(true),
      m_UseDefaultDTDFilePrefix( true),
      m_UsePublicId( true),
      m_Attlist( false), m_StdXml( false), m_EnforcedStdXml( false),
      m_RealFmt( eRealScientificFormat ),
      m_Encoding( eEncoding_Unknown ), m_StringEncoding( eEncoding_UTF8 ),
      m_UseXmlDecl( true ), m_UseSchemaLoc( true ),
      m_DefaultSchemaNamespace( sm_DefaultSchemaNamespace ),
      m_SkipIndent( false ), m_SkipNextTag( false )
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

EEncoding CObjectOStreamXml::GetEncoding(void) const
{
    return m_Encoding;
}

void CObjectOStreamXml::SetDefaultStringEncoding(EEncoding enc)
{
    m_StringEncoding = enc;
}

EEncoding CObjectOStreamXml::GetDefaultStringEncoding(void) const
{
    return m_StringEncoding;
}

void CObjectOStreamXml::SetReferenceSchema(bool use_schema)
{
    m_SpecRef = use_schema ? eSpecRefSchema : eSpecRefNone;
}
bool CObjectOStreamXml::GetReferenceSchema(void) const
{
    return m_SpecRef == eSpecRefSchema;
}
void CObjectOStreamXml::SetReferenceDTD(bool use_dtd)
{
    m_SpecRef = use_dtd ? eSpecRefDTD : eSpecRefNone;
}
bool CObjectOStreamXml::GetReferenceDTD(void) const
{
    return m_SpecRef == eSpecRefDTD;
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

void CObjectOStreamXml::SetFormattingFlags(TSerial_Format_Flags flags)
{
    TSerial_Format_Flags accepted =
        fSerial_Xml_NoIndentation | fSerial_Xml_NoEol    |
        fSerial_Xml_NoXmlDecl     | fSerial_Xml_NoRefDTD |
        fSerial_Xml_RefSchema     | fSerial_Xml_NoSchemaLoc;
    if (flags & ~accepted) {
        ERR_POST_X_ONCE(12, Warning <<
            "CObjectOStreamXml::SetFormattingFlags: ignoring unknown formatting flags");
    }
    m_UseXmlDecl   = (flags & fSerial_Xml_NoXmlDecl)   == 0;
    if ((flags & fSerial_Xml_NoRefDTD)    != 0) {m_SpecRef = eSpecRefNone;}
    if ((flags & fSerial_Xml_RefSchema)   != 0) {m_SpecRef = eSpecRefSchema;}
    m_UseSchemaLoc = (flags & fSerial_Xml_NoSchemaLoc) == 0;

    CObjectOStream::SetFormattingFlags(
        flags & (fSerial_Xml_NoIndentation | fSerial_Xml_NoEol));
}


string CObjectOStreamXml::GetPosition(void) const
{
    return "line "+NStr::SizetToString(m_Output.GetLine());
}

static string GetPublicModuleName(TTypeInfo type)
{
    const string& s = type->GetModuleName();
    string name;
    for ( string::const_iterator i = s.begin(); i != s.end(); ++i ) {
        char c = *i;
        if ( !isalnum((unsigned char) c) )
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
    if (m_UseXmlDecl) {
        m_Output.PutString("<?xml version=\"1.0");
        if (m_Encoding != eEncoding_Unknown) {
            m_Output.PutString("\" encoding=\"");
            m_Output.PutString(CUtf8::EncodingToString(m_Encoding));
        }
        m_Output.PutString("\"?>");
    }

    if (m_SpecRef == eSpecRefNotSet) {
        CheckStdXml(type);
        m_SpecRef = (type->GetDataSpec() == EDataSpec::eDTD || !x_IsStdXml()) ? eSpecRefDTD : eSpecRefSchema;
    }
    if (GetReferenceDTD()) {
        if (m_UseXmlDecl) {
            m_Output.PutEol();
        }
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
    } else if (!m_UseXmlDecl) {
        m_SkipIndent = true;
    }
    m_LastTagAction = eTagClose;
    m_NsNameToPrefix.clear();
    m_NsPrefixToName.clear();
}

void CObjectOStreamXml::EndOfWrite(void)
{
    m_Output.PutEol(false);
    CObjectOStream::EndOfWrite();
}

void CObjectOStreamXml::x_WriteClassNamespace(TTypeInfo type)
{
    if (type->GetName().find(':') != string::npos) {
        return;
    }
    if (!m_Attlist) {
        OpenTagEndBack();
    }

    string ns_name( m_NsPrefixToName[m_CurrNsPrefix]);
    if (ns_name.empty()) {
        ns_name = GetDefaultSchemaNamespace();
    }
    if (type->HasNamespaceName() || ((type->GetDataSpec() != EDataSpec::eXSD) &&
         m_NsNameToPrefix.find(ns_name) == m_NsNameToPrefix.end())) {
        if (m_Attlist) {
            m_Output.PutString(" xmlns");
        } else {
            m_Output.PutEol();
            m_Output.PutString("    xmlns");
        }
        if (!m_CurrNsPrefix.empty()) {
           m_Output.PutChar(':');
           m_Output.PutString(m_CurrNsPrefix);
        }
        m_Output.PutString("=\"");
        m_Output.PutString(ns_name + "\"");
        m_NsPrefixToName[m_CurrNsPrefix] = ns_name;
        m_NsNameToPrefix[ns_name] = m_CurrNsPrefix;
    }

    if (m_UseSchemaLoc) {
        string xs_name(s_SchemaInstanceNamespace);
        string xs_prefix("xs");
        if (m_NsNameToPrefix.find(xs_name) == m_NsNameToPrefix.end()) {
            for (char a='a';
                m_NsPrefixToName.find(xs_prefix) != m_NsPrefixToName.end(); ++a) {
                xs_prefix += a;
            }
            m_NsPrefixToName[xs_prefix] = xs_name;
            m_NsNameToPrefix[xs_name] = xs_prefix;
            m_Output.PutEol();
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
    if (!m_Attlist) {
        OpenTagEnd();
    }
}

bool CObjectOStreamXml::x_ProcessTypeNamespace(TTypeInfo type)
{
    if (GetReferenceSchema()) {
        if (type->HasNamespaceName()) {
            string prefix(type->GetNamespacePrefix());
            if (prefix.empty() && (type->IsNsQualified() == eNSUnqualified || (m_Attlist && type->IsNsQualified() == eNSQualified))) {
                prefix = sm_DefaultNamespacePrefix;
            }
            return x_BeginNamespace(type->GetNamespaceName(),prefix);
        }
        return true;
    }
    return false;
}

void CObjectOStreamXml::x_EndTypeNamespace(void)
{
    if (GetReferenceSchema()) {
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
    if (!GetReferenceSchema() || ns_name.empty()) {
        return false;
    }
    string nsPrefix(ns_prefix);
    if (m_Attlist || m_NsNameToPrefix.find(ns_name) == m_NsNameToPrefix.end()) {
        for (char a='a';
            m_NsPrefixToName.find(nsPrefix) != m_NsPrefixToName.end(); ++a) {
            nsPrefix += a;
        }
        if (m_Attlist && m_NsNameToPrefix.find(ns_name) != m_NsNameToPrefix.end()) {
            if (!m_NsNameToPrefix.at(ns_name).empty()) {
                m_CurrNsPrefix = m_NsNameToPrefix.at(ns_name);
                m_NsPrefixes.push_back(m_CurrNsPrefix);
                return false;
            }
        }
        m_CurrNsPrefix = nsPrefix;
        if (!m_Attlist) {
            m_NsNameToPrefix[ns_name] = nsPrefix;
        }
        m_NsPrefixToName[nsPrefix] = ns_name;
        m_NsPrefixes.push_back(nsPrefix);
        return true;
    } else {
        m_CurrNsPrefix = m_NsNameToPrefix[ns_name];
        m_NsPrefixes.push_back(m_CurrNsPrefix);
    }
    return false;
}

void CObjectOStreamXml::x_EndNamespace(const string& ns_name)
{
    if (!GetReferenceSchema() || ns_name.empty()) {
        return;
    }
    string nsPrefix = m_CurrNsPrefix;
// we should erase them according to Namespace Scoping rules
// http://www.w3.org/TR/REC-xml-names/#scoping 
    m_NsPrefixes.pop_back();
    if (find(m_NsPrefixes.begin(), m_NsPrefixes.end(), nsPrefix)
        == m_NsPrefixes.end()) {
        if (!m_Attlist) {
            m_NsNameToPrefix.erase(ns_name);
        }
        m_NsPrefixToName.erase(nsPrefix);
    }
    m_CurrNsPrefix = m_NsPrefixes.empty() ? kEmptyStr : m_NsPrefixes.back();
    if (!m_Attlist && GetStackDepth() <= 2) {
        m_NsNameToPrefix.clear();
        m_NsPrefixToName.clear();
    }
}

void CObjectOStreamXml::WriteEnum(const CEnumeratedTypeValues& values,
                                  TEnumValueType value,
                                  const string& valueName)
{
    bool skipname = valueName.empty() ||
                  (m_WriteNamedIntegersByValue && values.IsInteger());
    bool valueonly = m_StdXml;
    if (valueonly) {
        if ( values.IsInteger() ) {
            m_Output.PutInt4(value);
        } else {
            m_Output.PutString(valueName);
        }
        return;
    }
    if ( !m_SkipNextTag && !values.GetName().empty() ) {
        OpenTagStart();
        m_Output.PutString(values.GetName());
        if ( !skipname ) {
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
        if ( skipname ) {
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
    WriteEnum(values, value, values.FindNameEx(value, values.IsInteger()));
}

void CObjectOStreamXml::CopyEnum(const CEnumeratedTypeValues& values,
                                 CObjectIStream& in)
{
    TEnumValueType value = in.ReadEnum(values);
    WriteEnum(values, value, values.FindNameEx(value, values.IsInteger()));
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
        } else {
            m_Output.PutChar(c);
        }
        break;
    }
}

/*
In XML 1.1, almost all chars are allowed:
http://www.w3.org/TR/xml11/#NT-Char
BUT, we declare this as xml 1.0:
    CObjectOStreamXml::WriteFileHeader
Once so, some chars are not allowed
http://www.w3.org/TR/xml/#charsets
    
*/
inline bool BAD_CHAR(char x) {
    return (x < 0x20 && x > 0x0 && x != 0x9 && x != 0xA && x != 0xD);
}
inline char CObjectOStreamXml::x_VerifyChar(char x) {
    return BAD_CHAR(x) ?
        ReplaceVisibleChar(x, x_FixCharsMethod(), this, kEmptyStr, x_FixCharsSubst()) : x;
}

void CObjectOStreamXml::WriteEncodedChar(const char*& src, EStringType type)
{
    EEncoding enc_in( type == eStringTypeUTF8 ? eEncoding_UTF8 : m_StringEncoding);
    EEncoding enc_out(m_Encoding == eEncoding_Unknown ? eEncoding_UTF8 : m_Encoding);

    if (enc_in == enc_out || enc_in == eEncoding_Unknown || (*src & 0x80) == 0) {
        char s = x_VerifyChar(*src);
        if (s != '\0') {
            WriteEscapedChar(s);
        }
    } else if (enc_out != eEncoding_UTF8) {
        TUnicodeSymbol chU = (enc_in == eEncoding_UTF8) ?
            CUtf8::Decode(src) : CUtf8::CharToSymbol( *src, enc_in);
        char s = x_VerifyChar( CUtf8::SymbolToChar( chU, enc_out));
        if (s != '\0') {
            WriteEscapedChar(s);
        }
    } else {
        CStringUTF8 tmp( CUtf8::AsUTF8( CTempString(src,1),enc_in));
        for ( string::const_iterator t = tmp.begin(); t != tmp.end(); ++t ) {
            char s = x_VerifyChar(*t);
            if (s != '\0') {
                WriteEscapedChar(s);
            }
        }
    }
}

bool CObjectOStreamXml::x_SpecialCaseWrite(void)
{
    if (m_SpecialCaseWrite == eWriteAsDefault) {
        OpenTagEndBack();
        SelfCloseTagEnd();
        return true;
    }
    else if (m_SpecialCaseWrite == eWriteAsNil) {
        OpenTagEndBack();
         m_Output.PutChar(' ');
        if (GetReferenceSchema()) {
            m_Output.PutString("xs:");
        }
        m_Output.PutString("nil=\"true\"");
        SelfCloseTagEnd();
        return true;
    }
    return false;
}

void CObjectOStreamXml::WriteBool(bool data)
{
    if (m_SpecialCaseWrite && x_SpecialCaseWrite()) {
        return;
    }
    if ( !x_IsStdXml() ) {
        OpenTagEndBack();
        if ( data )
            m_Output.PutString(" value=\"true\"");
        else
            m_Output.PutString(" value=\"false\"");
        SelfCloseTagEnd();
    } else {
        if ( data )
            m_Output.PutString("true");
        else
            m_Output.PutString("false");
    }
}

void CObjectOStreamXml::WriteChar(char data)
{
    WriteEscapedChar(data);
}

void CObjectOStreamXml::WriteInt4(Int4 data)
{
    if (m_SpecialCaseWrite && x_SpecialCaseWrite()) {
        return;
    }
    m_Output.PutInt4(data);
}

void CObjectOStreamXml::WriteUint4(Uint4 data)
{
    if (m_SpecialCaseWrite && x_SpecialCaseWrite()) {
        return;
    }
    m_Output.PutUint4(data);
}

void CObjectOStreamXml::WriteInt8(Int8 data)
{
    if (m_SpecialCaseWrite && x_SpecialCaseWrite()) {
        return;
    }
    m_Output.PutInt8(data);
}

void CObjectOStreamXml::WriteUint8(Uint8 data)
{
    if (m_SpecialCaseWrite && x_SpecialCaseWrite()) {
        return;
    }
    m_Output.PutUint8(data);
}

void CObjectOStreamXml::WriteDouble2(double data, unsigned digits)
{
    if (m_SpecialCaseWrite && x_SpecialCaseWrite()) {
        return;
    }
    if (isnan(data)) {
        m_Output.PutString("NaN", 3);
        return;
    }
    if (!finite(data)) {
        if (data < 0) {
            m_Output.PutChar('-');
        }
        m_Output.PutString("INF", 3);
        return;
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
                                    buffer, sizeof(buffer), NStr::fDoublePosix);
        if (precision != 0) {
            while (buffer[width-1] == '0') {
                --width;
            }
            if (buffer[width-1] == '.') {
                --width;
            }
        }
    } else {
        if (m_FastWriteDouble) {
            width = NStr::DoubleToStringPosix(data, digits, buffer, sizeof(buffer));
        } else {
            width = snprintf(buffer, sizeof(buffer)/sizeof(char), "%.*g", (int)digits, data);
            char* dot = strchr(buffer,',');
            if (dot) {
                *dot = '.'; // enforce C locale
            }
        }
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
    string ns_name(obj.GetNamespaceName());
    bool needNs = x_BeginNamespace(ns_name,obj.GetNamespacePrefix());
    string obj_name = obj.GetName();
    if (obj_name.empty()) {
        if (!StackIsEmpty() && TopFrame().HasMemberId()) {
            obj_name = TopFrame().GetMemberId().GetName();
        }
    }
    if (obj_name.empty()) {
        ThrowError(fInvalidData, "AnyContent object must have name");
    }
    OpenTag(obj_name);

    if (GetReferenceSchema()) {
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
                WriteString(it->GetValue(),eStringTypeUTF8);
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
    bool is_tag = false;
    char attr_close ='\0';
    for (const char* is = value.c_str(); *is; ++is) {
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
                is_tag = was_open = false;
            } else {
                if (was_close) {
                    m_Output.PutEol();
                }
                m_Output.IncIndentLevel();
                is_tag = was_open = true;
            }
        }
        if (*is != '>' && *is != '<' && *is != attr_close) {
            WriteEncodedChar(is,eStringTypeUTF8);
        } else {
            m_Output.PutChar(*is);
        }
        if (*is == '<') {
            if (*(is+1) == '/') {
                m_Output.PutChar(*(++is));
            }
            if (GetReferenceSchema() && !m_CurrNsPrefix.empty()) {
                m_Output.PutString(m_CurrNsPrefix);
                m_Output.PutChar(':');
            }
        }
        if (*is == '>') {
            was_close = true;
            is_tag = false;
            attr_close = '\0';
        } else {
            was_close = false;
        }
        if (is_tag && *is == '=' && (*(is+1) == '\"' || *(is+1) == '\'')) {
            attr_close = *(is+1);
        }
    }

// close tag
    if (!was_open) {
        m_EndTag = true;
    }
    m_SkipIndent = !was_close;
    CloseTag(obj_name);
    x_EndNamespace(ns_name);
}

void CObjectOStreamXml::CopyAnyContentObject(CObjectIStream& in)
{
    CAnyContentObject obj;
    in.ReadAnyContentObject(obj);
    WriteAnyContentObject(obj);
}

void CObjectOStreamXml::WriteBitString(const CBitString& obj)
{
#if BITSTRING_AS_VECTOR
    static const char ToHex[] = "0123456789ABCDEF";
    Uint1 data, mask;
    bool done = false;
    for ( CBitString::const_iterator i = obj.begin(); !done; ) {
        for (data=0, mask=0x8; !done && mask!=0; mask >>= 1) {
            if (*i) {
                data |= mask;
            }
            done = (++i == obj.end());
        }
        m_Output.PutChar(ToHex[data]);
    }
#else
    if (IsCompressed()) {
        bm::word_t* tmp_block = (bm::word_t*)bm::aligned_new_malloc(bm::set_block_alloc_size);
        CBitString::statistics st;
        obj.calc_stat(&st);
        char* buf = (char*)malloc(st.max_serialize_mem);
        size_t len = bm::serialize(obj, (unsigned char*)buf, tmp_block);
        WriteBytes(buf,len);
        free(buf);
        bm::aligned_free(tmp_block);
        return;
    }
    CBitString::size_type i=0;
    CBitString::size_type ilast = obj.size();
    CBitString::enumerator e = obj.first();
    for (; i < ilast; ++i) {
        m_Output.PutChar( (i == *e) ? '1' : '0');
        if (i == *e) {
            ++e;
        }
    }
#endif
}

void CObjectOStreamXml::CopyBitString(CObjectIStream& in)
{
    CBitString obj;
    in.ReadBitString(obj);
    WriteBitString(obj);
}

void CObjectOStreamXml::WriteCString(const char* str)
{
    if ( str == 0 ) {
        OpenTagEndBack();
        SelfCloseTagEnd();
    }
    else {
        for ( ; *str; ++str) {
            WriteEncodedChar(str);
        }
    }
}

void CObjectOStreamXml::WriteString(const string& str, EStringType type)
{
    if (m_SpecialCaseWrite && x_SpecialCaseWrite()) {
        return;
    }
    for ( const char* src = str.c_str(); *src; ++src ) {
        WriteEncodedChar(src,type);
    }
}

void CObjectOStreamXml::WriteStringStore(const string& str)
{
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
    }
}

void CObjectOStreamXml::CopyString(CObjectIStream& in,
                                   EStringType type)
{
    string str;
    in.ReadString(str, type);
    SetSpecialCaseWrite( (CObjectOStream::ESpecialCaseWrite)in.GetSpecialCaseUsed());
    in.SetSpecialCaseUsed(CObjectIStream::eReadAsNormal);
    WriteString(str, type);
    SetSpecialCaseWrite(CObjectOStream::eWriteAsNormal);
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
    bool notag = TopFrame().HasMemberId() && TopFrame().GetMemberId().HasNotag();
    bool nillable =  TopFrame().HasMemberId() && TopFrame().GetMemberId().IsNillable();
    if (TopFrame().GetNotag() && !notag) {
        if (m_LastTagAction == eTagClose) {
            OpenStackTag(0);
            m_SpecialCaseWrite = eWriteAsNil;
            x_SpecialCaseWrite();
            m_SpecialCaseWrite = eWriteAsNormal;
            CloseStackTag(0);
        }
        return;
    }
    if (m_EndTag) {
        return;
    }
    m_SpecialCaseWrite = nillable ? eWriteAsNil : eWriteAsDefault;
    x_SpecialCaseWrite();
    m_SpecialCaseWrite = eWriteAsNormal;
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
    if (!m_CurrNsPrefix.empty() && IsNsQualified()) {
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
    if (!m_StdXml) {
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
    if (!m_StdXml && !TopFrame().GetNotag()) {
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
    if (x_IsStdXml()) {
        CObjectTypeInfo type(GetRealTypeInfo(elementType));
        if (type.GetTypeFamily() != eTypeFamilyPrimitive ||
            type.GetPrimitiveValueType() == ePrimitiveValueAny) {
            TopFrame().SetNotag();
            return;
        }
        if (m_SkipNextTag && type.GetTypeFamily() == eTypeFamilyPrimitive) {
            m_Output.PutChar(' ');
            TopFrame().SetNotag();
            return;
        }
    }
    OpenStackTag(0);
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

            const CPointerTypeInfo* pointerType =
                dynamic_cast<const CPointerTypeInfo*>(elementType);
            do {
                TConstObjectPtr elementPtr = cType->GetElementPtr(i);
                if ( pointerType &&
                     !pointerType->GetObjectPointer(elementPtr) ) {
                    if ( GetVerifyData() == eSerialVerifyData_Yes ) {
                        ThrowError(fUnassigned,
                                   "NULL element while writing container "+
                                   cType->GetName());
                    }
                    continue;
                }

                WriteObject(elementPtr, elementType);

            } while ( cType->NextElement(i) );
        }
    }
    else {
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        if ( cType->InitIterator(i, containerPtr) ) {

            const CPointerTypeInfo* pointerType =
                dynamic_cast<const CPointerTypeInfo*>(elementType);
            do {
                TConstObjectPtr elementPtr = cType->GetElementPtr(i);
                if ( pointerType &&
                     !pointerType->GetObjectPointer(elementPtr) ) {
                    if ( GetVerifyData() == eSerialVerifyData_Yes ) {
                        ThrowError(fUnassigned,
                                   "NULL element while writing container "+
                                   cType->GetName());
                    }
                    continue;
                }

                BeginArrayElement(elementType);
                WriteObject(elementPtr, elementType);
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
    CheckStdXml(namedTypeInfo);
    if (m_SkipNextTag || namedTypeInfo->GetName().empty()) {
        TopFrame().SetNotag();
        m_SkipNextTag = false;
    } else {
        TTypeInfo realtype = GetRealTypeInfo(namedTypeInfo);
        if (realtype->GetTypeFamily() == eTypeFamilyPrimitive &&
            GetStackDepth() > 2 && m_StdXml) {
            TopFrame().SetNotag();
            m_SkipNextTag = false;
            return;
        }
        bool needNs = x_ProcessTypeNamespace(namedTypeInfo);
        OpenTag(namedTypeInfo);
        if (needNs) {
            x_WriteClassNamespace(namedTypeInfo);
        }
    }
    const CAliasTypeInfo* aliasType = 
        dynamic_cast<const CAliasTypeInfo*>(namedTypeInfo);
    if (aliasType) {
        m_SkipNextTag = aliasType->IsFullAlias();
    }
    else if (m_StdXml) {
        const CClassTypeInfo* classType = dynamic_cast<const CClassTypeInfo*>(namedTypeInfo);
        m_SkipNextTag = (classType && classType->Implicit());
    }
}

void CObjectOStreamXml::EndNamedType(void)
{
    m_SkipNextTag = false;
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
        return;
    }
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

void CObjectOStreamXml::CheckStdXml(TTypeInfo typeinfo)
{
    if (typeinfo->GetCodeVersion() > 21600) {
        m_StdXml = typeinfo->GetDataSpec() != EDataSpec::eASN;
    } else {
        const CClassTypeInfo* classType =
            dynamic_cast<const CClassTypeInfo*>(typeinfo);
        if (classType) {
            TMemberIndex first = classType->GetItems().FirstIndex();
            m_StdXml = classType->GetItems().GetItemInfo(first)->GetId().HaveNoPrefix();
        }
    }
}

void CObjectOStreamXml::BeginClass(const CClassTypeInfo* classInfo)
{
    CheckStdXml(classInfo);
    if (m_SkipNextTag) {
        TopFrame().SetNotag();
        m_SkipNextTag = false;
        return;
    }
    bool needNs = x_ProcessTypeNamespace(classInfo);
    OpenTagIfNamed(classInfo);
    if (needNs) {
        x_WriteClassNamespace(classInfo);
    }
}

void CObjectOStreamXml::EndClass(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
        return;
    }
    if (!m_Attlist && m_LastTagAction != eTagSelfClosed) {
        EolIfEmptyTag();
    }
    if (m_LastTagAction == eTagSelfClosed) {
        m_LastTagAction = eTagClose;
    } else {
        CloseTagIfNamed(TopFrame().GetTypeInfo());
    }
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
                    TTypeInfo mem_type  = GetRealTypeInfo(memberType);
                    TTypeInfo elem_type = GetContainerElementTypeInfo(mem_type);
                    needTag = (elem_type->GetTypeFamily() != eTypeFamilyPrimitive ||
                        elem_type->GetName() != mem_type->GetName());
                }
            } else {
                needTag = !id.HasNotag() && !id.HasAnyContent() && type != eTypeFamilyContainer;
                m_SkipNextTag = type != eTypeFamilyPrimitive && type != eTypeFamilyContainer;
                if (needTag && m_SkipNextTag) {
                    if (memberType->HasNamespaceName() && m_NsNameToPrefix.find(memberType->GetNamespaceName()) == m_NsNameToPrefix.end()) {
                        needTag = m_SkipNextTag = false;
                    }
                }
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
    m_SkipNextTag = false;
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
    if ( (m_Attlist && classType->IsNsQualified() == eNSQualified) || !classType->GetName().empty() ) {
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

void CObjectOStreamXml::WriteClassMemberSpecialCase(const CMemberId& memberId,
    TTypeInfo /*memberType*/, TConstObjectPtr /*memberPtr*/, ESpecialCaseWrite how)
{
    if (m_Attlist) {
        return;
    }
    m_SpecialCaseWrite = how;
    if (memberId.HasNotag() || m_SkipNextTag) {
        x_SpecialCaseWrite();
    } else {
        BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
        OpenStackTag(0);
        x_SpecialCaseWrite();
        m_SpecialCaseWrite = eWriteAsNormal;
        CloseStackTag(0);
        END_OBJECT_FRAME();
    }
    m_SpecialCaseWrite = eWriteAsNormal;
}
#endif

void CObjectOStreamXml::BeginChoice(const CChoiceTypeInfo* choiceType)
{
    CheckStdXml(choiceType);
    if (m_SkipNextTag) {
        TopFrame().SetNotag();
        m_SkipNextTag = false;
        return;
    }
    bool needNs = x_ProcessTypeNamespace(choiceType);
    OpenTagIfNamed(choiceType);
    if (needNs) {
        x_WriteClassNamespace(choiceType);
    }
}

void CObjectOStreamXml::EndChoice(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
        return;
    }
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
                TTypeInfo var_type  = GetRealTypeInfo(var_info->GetTypeInfo());
                TTypeInfo elem_type = GetContainerElementTypeInfo(var_type);
                needTag = (elem_type->GetTypeFamily() != eTypeFamilyPrimitive ||
                    elem_type->GetName() != var_type->GetName());
            }
        } else {
            needTag = !id.HasNotag() && !id.HasAnyContent() && type != eTypeFamilyContainer;
            m_SkipNextTag = type != eTypeFamilyPrimitive && type != eTypeFamilyContainer;
            if (needTag && m_SkipNextTag) {
                TTypeInfo var_type = choiceType->GetItemInfo(id.GetName())->GetTypeInfo();
                if (var_type->HasNamespaceName() && m_NsNameToPrefix.find(var_type->GetNamespaceName()) == m_NsNameToPrefix.end()) {
                    needTag = m_SkipNextTag = false;
                }
            }
        }
        if (needTag) {
            OpenStackTag(0);
        } else {
            TopFrame().SetNotag();
        }
        if (type == eTypeFamilyPrimitive) {
            m_SkipIndent = id.HasNotag();
        }
    } else {
        OpenStackTag(0);
    }
}

void CObjectOStreamXml::EndChoiceVariant(void)
{
    m_SkipNextTag = false;
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    } else {
        CloseStackTag(0);
    }
}

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
    if (IsCompressed()) {
        WriteBase64Bytes(bytes,length);
        return;
    }
    WriteBytes(bytes,length);
}

void CObjectOStreamXml::WriteBase64Bytes(const char* bytes, size_t length)
{
    const size_t chunk_in  = 57;
    const size_t chunk_out = 80;
    if (length > chunk_in) {
        m_Output.PutEol(false);
    }
    char dst_buf[chunk_out];
    size_t bytes_left = length;
    size_t  src_read=0, dst_written=0, line_len=0;
    while (bytes_left > 0 && bytes_left <= length) {
        BASE64_Encode(bytes,  min(bytes_left,chunk_in),  &src_read,
                        dst_buf, chunk_out, &dst_written, &line_len);
        m_Output.PutString(dst_buf,dst_written);
        bytes_left -= src_read;
        bytes += src_read;
        if (bytes_left > 0) {
            m_Output.PutEol(false);
        }
    }
    if (length > chunk_in) {
        m_Output.PutEol(false);
    }
}

void CObjectOStreamXml::WriteBytes(const char* bytes, size_t length)
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
