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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbithr.hpp>
#include <serial/serialbase.hpp>
#include <serial/typeinfo.hpp>

#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/objostrxml.hpp>
#include <serial/objistrxml.hpp>

#include <serial/impl/classinfob.hpp>
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_Core


BEGIN_NCBI_SCOPE

static bool IsSameTypeInfo( const CSerialObject& obj1,
                            const CSerialObject& obj2 )
{
    TTypeInfo type1 = obj1.GetThisTypeInfo();
    while (type1->GetTypeFamily() == eTypeFamilyPointer) {
        const CPointerTypeInfo* t = dynamic_cast<const CPointerTypeInfo*>(type1);
        type1 = t->GetPointedType();
    }
    TTypeInfo type2 = obj2.GetThisTypeInfo();
    while (type2->GetTypeFamily() == eTypeFamilyPointer) {
        const CPointerTypeInfo* t = dynamic_cast<const CPointerTypeInfo*>(type2);
        type2 = t->GetPointedType();
    }
    return (type1 == type2);
}


CSerialObject::CSerialObject(void)
{
}

CSerialObject::~CSerialObject()
{
}

void CSerialObject::Assign(const CSerialObject& source, ESerialRecursionMode how)
{
    if (this == &source) {
        ERR_POST_X(3, Warning <<
            "CSerialObject::Assign(): an attempt to assign a serial object to itself");
        return;
    }
    if ( typeid(source) != typeid(*this) && !IsSameTypeInfo(source, *this) ) {
        string msg("Assignment of incompatible types: ");
        msg += typeid(*this).name();
        msg += " = ";
        msg += typeid(source).name();
        NCBI_THROW(CSerialException,eIllegalCall, msg);
    }
    GetThisTypeInfo()->Assign(this, &source, how);
}


bool CSerialObject::Equals(const CSerialObject& object, ESerialRecursionMode how) const
{
    if ( typeid(object) != typeid(*this)  && !IsSameTypeInfo(object, *this) ) {
        string msg("Cannot compare types: ");
        msg += typeid(*this).name();
        msg += " == ";
        msg += typeid(object).name();
        NCBI_THROW(CSerialException,eIllegalCall, msg);
    }
    return GetThisTypeInfo()->Equals(this, &object, how);
}

void CSerialObject::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CSerialObject");
    CObject::DebugDump( ddc, depth);
// this is not good, but better than nothing
    CNcbiOstrstream ostr;
    ostr << "\n****** begin ASN dump ******\n";
    {{
        auto_ptr<CObjectOStream> oos(CObjectOStream::Open(eSerial_AsnText,
                                                          ostr));
        oos->SetAutoSeparator(false);
        oos->Write(this, GetThisTypeInfo());
    }}
    ostr << "\n****** end   ASN dump ******\n" << '\0';
    const char* str = ostr.str();
    ostr.freeze();
    ddc.Log( "Serial_AsnText", str);
}

/////////////////////////////////////////////////////////////////////////////
// data verification setup

const char* CSerialObject::ms_UnassignedStr = "<*unassigned*>";
const char  CSerialObject::ms_UnassignedByte = char(0xcd);

ESerialVerifyData CSerialObject::ms_VerifyDataDefault = eSerialVerifyData_Default;
static CStaticTls<int> s_VerifyTLS;


void CSerialObject::SetVerifyDataThread(ESerialVerifyData verify)
{
    x_GetVerifyData();
    ESerialVerifyData tls_verify = ESerialVerifyData(intptr_t(s_VerifyTLS.GetValue()));
    if (tls_verify != eSerialVerifyData_Never &&
        tls_verify != eSerialVerifyData_Always &&
        tls_verify != eSerialVerifyData_DefValueAlways) {
        s_VerifyTLS.SetValue(reinterpret_cast<int*>(verify));
    }
}

void CSerialObject::SetVerifyDataGlobal(ESerialVerifyData verify)
{
    x_GetVerifyData();
    if (ms_VerifyDataDefault != eSerialVerifyData_Never &&
        ms_VerifyDataDefault != eSerialVerifyData_Always &&
        ms_VerifyDataDefault != eSerialVerifyData_DefValueAlways) {
        ms_VerifyDataDefault = verify;
    }
}

ESerialVerifyData CSerialObject::x_GetVerifyData(void)
{
    ESerialVerifyData verify;
    if (ms_VerifyDataDefault == eSerialVerifyData_Never ||
        ms_VerifyDataDefault == eSerialVerifyData_Always ||
        ms_VerifyDataDefault == eSerialVerifyData_DefValueAlways) {
        verify = ms_VerifyDataDefault;
    } else {
        verify = ESerialVerifyData(intptr_t(s_VerifyTLS.GetValue()));
        if (verify == eSerialVerifyData_Default) {
            if (ms_VerifyDataDefault == eSerialVerifyData_Default) {

                // change the default here, if you wish
                ms_VerifyDataDefault = eSerialVerifyData_Yes;
                //ms_VerifyDataDefault = eSerialVerifyData_No;

                const char* str = getenv(SERIAL_VERIFY_DATA_GET);
                if (str) {
                    if (NStr::CompareNocase(str,"YES") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_Yes;
                    } else if (NStr::CompareNocase(str,"NO") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_No;
                    } else if (NStr::CompareNocase(str,"NEVER") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_Never;
                    } else  if (NStr::CompareNocase(str,"ALWAYS") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_Always;
                    } else  if (NStr::CompareNocase(str,"DEFVALUE") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_DefValue;
                    } else  if (NStr::CompareNocase(str,"DEFVALUE_ALWAYS") == 0) {
                        ms_VerifyDataDefault = eSerialVerifyData_DefValueAlways;
                    }
                }
            }
            verify = ms_VerifyDataDefault;
        }
    }
    switch (verify) {
    default:
    case eSerialVerifyData_Default:
        return ms_VerifyDataDefault;
    case eSerialVerifyData_No:
    case eSerialVerifyData_Never:
        return eSerialVerifyData_No;
    case eSerialVerifyData_Yes:
    case eSerialVerifyData_Always:
        return eSerialVerifyData_Yes;
    case eSerialVerifyData_DefValue:
    case eSerialVerifyData_DefValueAlways:
        return eSerialVerifyData_No;
    }
}

BEGIN_LOCAL_NAMESPACE;

struct SPrintIdentifier
{
    SPrintIdentifier(const CTempString& s) : m_String(s) { }
    CTempString m_String;
};
CNcbiOstream& operator<<(CNcbiOstream& out, SPrintIdentifier s)
{
    SIZE_TYPE size = s.m_String.size();
    SIZE_TYPE e_pos = NPOS;
    if ( size > 2 && NStr::EndsWith(s.m_String, ".E") ) {
        e_pos = s.m_String.rfind('.', size-3);
        if ( e_pos != NPOS ) {
            size -= 2;
        }
    }
    bool capitalize = true;
    for ( SIZE_TYPE i = 0; i < size; ++i ) {
        char c = s.m_String[i];
        if ( c == '.' ) {
            out << "::C_";
            if ( i == e_pos ) {
                out << "E_";
            }
            capitalize = true;
        }
        else {
            if ( c == '-' ) {
                c = '_';
            }
            if ( capitalize ) {
                c = toupper((unsigned char)c);
                capitalize = false;
            }
            out << c;
        }
    }
    return out;
}

END_LOCAL_NAMESPACE;

void CSerialObject::ThrowUnassigned(TMemberIndex index,
                                    const char* file_name,
                                    int file_line) const
{
    if (x_GetVerifyData() == eSerialVerifyData_Yes) {
        const CTypeInfo* type = GetThisTypeInfo();
        const CClassTypeInfoBase* classtype =
            dynamic_cast<const CClassTypeInfoBase*>(type);
        // offset index as the argument is zero based but items are 1 based
        string member_name;
        if ( classtype ) {
            index += classtype->GetItems().FirstIndex();
            if ( index >= classtype->GetItems().FirstIndex() &&
                 index <= classtype->GetItems().LastIndex() ) {
                member_name = classtype->GetItems().GetItemInfo(index)->GetId().GetName();
            }
        }
        CNcbiOstrstream s;
        if ( true ) {
            // make class name
            s << "C" << SPrintIdentifier(type->GetAccessName());
        }
        if ( !member_name.empty() ) {
            // make method name
            s << "::Get" << SPrintIdentifier(member_name) << "()";
        }
        s << ": Attempt to get unassigned member "
          << type->GetAccessModuleName() <<"::"<< type->GetAccessName() << '.';
        if ( !member_name.empty() ) {
            s << member_name;
        } else {
            s << '[' << index << ']';
        }
// set temporary diag compile info to use argument file name and line
#undef DIAG_COMPILE_INFO
#define DIAG_COMPILE_INFO                                               \
        NCBI_NS_NCBI::CDiagCompileInfo(file_name? file_name: __FILE__,  \
                                       file_line? file_line: __LINE__,  \
                                       NCBI_CURRENT_FUNCTION,           \
                                       NCBI_MAKE_MODULE(NCBI_MODULE))
        NCBI_THROW(CUnassignedMember,eGet,CNcbiOstrstreamToString(s));
// restore original diag compile info definition
#undef DIAG_COMPILE_INFO
#define DIAG_COMPILE_INFO                                               \
        NCBI_NS_NCBI::CDiagCompileInfo(__FILE__,                        \
                                       __LINE__,                        \
                                       NCBI_CURRENT_FUNCTION,           \
                                       NCBI_MAKE_MODULE(NCBI_MODULE))
    }
}

void CSerialObject::ThrowUnassigned(TMemberIndex index) const
{
    ThrowUnassigned(index, 0, 0);
}

bool CSerialObject::HasNamespaceName(void) const
{
    return GetThisTypeInfo()->HasNamespaceName();
}

const string& CSerialObject::GetNamespaceName(void) const
{
    return GetThisTypeInfo()->GetNamespaceName();
}

void CSerialObject::SetNamespaceName(const string& ns_name)
{
    GetThisTypeInfo()->SetNamespaceName(ns_name);
}

bool CSerialObject::HasNamespacePrefix(void) const
{
    return GetThisTypeInfo()->HasNamespacePrefix();
}

const string& CSerialObject::GetNamespacePrefix(void) const
{
    return GetThisTypeInfo()->GetNamespacePrefix();
}

void CSerialObject::SetNamespacePrefix(const string& ns_prefix)
{
    GetThisTypeInfo()->SetNamespacePrefix(ns_prefix);
}


CSerialAttribInfoItem::CSerialAttribInfoItem(
    const string& name, const string& ns_name, const string& value)
    : m_Name(name), m_NsName(ns_name), m_Value(value)
{
}
CSerialAttribInfoItem::CSerialAttribInfoItem(const CSerialAttribInfoItem& other)
    : m_Name(other.m_Name), m_NsName(other.m_NsName), m_Value(other.m_Value)
{
}

CSerialAttribInfoItem::~CSerialAttribInfoItem(void)
{
}
const string& CSerialAttribInfoItem::GetName(void) const
{
    return m_Name;
}
const string& CSerialAttribInfoItem::GetNamespaceName(void) const
{
    return m_NsName;
}
const string& CSerialAttribInfoItem::GetValue(void) const
{
    return m_Value;
}


CAnyContentObject::CAnyContentObject(void)
{
}

CAnyContentObject::CAnyContentObject(const CAnyContentObject& other)
{
    x_Copy(other);
}

CAnyContentObject::~CAnyContentObject(void)
{
}

const CTypeInfo* CAnyContentObject::GetTypeInfo(void)
{
    return CStdTypeInfo<ncbi::CAnyContentObject>::GetTypeInfo();
}

void CAnyContentObject::Reset(void)
{
    m_Name.erase();
    m_Value.erase();
    m_NsName.erase();
    m_NsPrefix.erase();
    m_Attlist.clear();
}

void CAnyContentObject::x_Copy(const CAnyContentObject& other)
{
    m_Name = other.m_Name;
    m_Value= other.m_Value;
    m_NsName= other.m_NsName;
    m_NsPrefix= other.m_NsPrefix;
    m_Attlist.clear();
    vector<CSerialAttribInfoItem>::const_iterator it;
    for (it = other.m_Attlist.begin(); it != other.m_Attlist.end(); ++it) {
        m_Attlist.push_back( *it);
    }
}
CAnyContentObject& CAnyContentObject::operator= (const CAnyContentObject& other)
{
    x_Copy(other);
    return *this;
}

bool CAnyContentObject::operator== (const CAnyContentObject& other) const
{
    return m_Name == other.GetName() &&
           m_Value == other.GetValue() &&
           m_NsName == other.m_NsName;
}

void CAnyContentObject::SetName(const string& name)
{
    m_Name = name;
}
const string& CAnyContentObject::GetName(void) const
{
    return m_Name;
}
void CAnyContentObject::SetValue(const string& value)
{
    x_Decode(value);
}
const string& CAnyContentObject::GetValue(void) const
{
    return m_Value;
}
void CAnyContentObject::SetNamespaceName(const string& ns_name)
{
    m_NsName = ns_name;
}
const string& CAnyContentObject::GetNamespaceName(void) const
{
    return m_NsName;
}
void CAnyContentObject::SetNamespacePrefix(const string& ns_prefix)
{
    m_NsPrefix = ns_prefix;
}
const string& CAnyContentObject::GetNamespacePrefix(void) const
{
    return m_NsPrefix;
}
void CAnyContentObject::x_Decode(const string& value)
{
    m_Value = value;
}
void CAnyContentObject::AddAttribute(
    const string& name, const string& ns_name, const string& value)
{
// TODO: check if an attrib with this name+ns_name already exists
    m_Attlist.push_back( CSerialAttribInfoItem( name,ns_name,value));
}

const vector<CSerialAttribInfoItem>&
CAnyContentObject::GetAttributes(void) const
{
    return m_Attlist;
}

/////////////////////////////////////////////////////////////////////////////
//  I/O stream manipulators and helpers for serializable objects

#define  eFmt_AsnText     (1l <<  0)
#define  eFmt_AsnBinary   (1l <<  1)
#define  eFmt_Xml         (1l <<  2)
#define  eFmt_Json        (1l <<  3)
#define  eFmt_All         (eFmt_AsnText | eFmt_AsnBinary | eFmt_Xml | eFmt_Json)
#define  eVerify_No       (1l <<  8)
#define  eVerify_Yes      (1l <<  9)
#define  eVerify_DefValue (1l << 10)
#define  eVerify_All      (eVerify_No | eVerify_Yes | eVerify_DefValue)
#define  eEncoding_All    (255l << 16)

static
long& s_SerFlags(CNcbiIos& io)
{
    static int s_SerIndex;
    static bool s_HaveIndex = false;

    if ( !s_HaveIndex ) {
        // Make sure to get a unique IOS index
        DEFINE_STATIC_FAST_MUTEX(s_IndexMutex);
        CFastMutexGuard guard(s_IndexMutex);
        if ( !s_HaveIndex ) {
            s_SerIndex = CNcbiIos::xalloc();
            s_HaveIndex = true;
        }
    }

    return io.iword(s_SerIndex);
}
static
ESerialDataFormat s_FlagsToFormat(CNcbiIos& io)
{
    switch (s_SerFlags(io) & eFmt_All) {
    case eFmt_AsnText:     return eSerial_AsnText;
    case eFmt_AsnBinary:   return eSerial_AsnBinary;
    case eFmt_Xml:         return eSerial_Xml;
    case eFmt_Json:        return eSerial_Json;
    default:               return eSerial_None;
    }
}
static
long s_FormatToFlags(ESerialDataFormat fmt)
{
    switch (fmt) {
    case eSerial_AsnText:    return eFmt_AsnText;
    case eSerial_AsnBinary:  return eFmt_AsnBinary;
    case eSerial_Xml:        return eFmt_Xml;
    case eSerial_Json:       return eFmt_Json;
    default:                 return 0;
    }
}

static
ESerialVerifyData s_FlagsToVerify(CNcbiIos& io)
{
    switch (s_SerFlags(io) & eVerify_All) {
    case eVerify_No:       return eSerialVerifyData_No;
    case eVerify_Yes:      return eSerialVerifyData_Yes;
    case eVerify_DefValue: return eSerialVerifyData_DefValue;
    default:               return eSerialVerifyData_Default;
    }
}

static
long s_VerifyToFlags(ESerialVerifyData fmt)
{
    switch (fmt) {
    case eSerialVerifyData_Never:
    case eSerialVerifyData_No:       return eVerify_No;
    case eSerialVerifyData_Always:
    case eSerialVerifyData_Yes:      return eVerify_Yes;
    case eSerialVerifyData_DefValueAlways:
    case eSerialVerifyData_DefValue: return eVerify_DefValue;
    default:                         return 0;
    }
}

static
EEncoding s_FlagsToEncoding(CNcbiIos& io)
{
    long enc = (s_SerFlags(io) & eEncoding_All) >> 16;
    switch (enc) {
    default: return eEncoding_Unknown;
    case 1:  return eEncoding_UTF8;
    case 2:  return eEncoding_Ascii;
    case 3:  return eEncoding_ISO8859_1;
    case 4:  return eEncoding_Windows_1252;
    }
}

static
long s_EncodingToFlags(EEncoding fmt)
{
    long enc = 0;
    switch (fmt) {
    default:                     enc = 0; break;
    case eEncoding_UTF8:         enc = 1; break;
    case eEncoding_Ascii:        enc = 2; break;
    case eEncoding_ISO8859_1:    enc = 3; break;
    case eEncoding_Windows_1252: enc = 4; break;
    }
    return (enc << 16);
}

MSerial_Flags::MSerial_Flags(unsigned long all, unsigned long flags)
    : m_All(all), m_Flags(flags)
{
}
MSerial_Format::MSerial_Format(ESerialDataFormat fmt)
    : MSerial_Flags(eFmt_All, s_FormatToFlags(fmt))
{
}

MSerial_VerifyData::MSerial_VerifyData(ESerialVerifyData fmt)
    : MSerial_Flags(eVerify_All, s_VerifyToFlags(fmt))
{
}

MSerialXml_DefaultStringEncoding::MSerialXml_DefaultStringEncoding(EEncoding fmt)
    : MSerial_Flags(eEncoding_All, s_EncodingToFlags(fmt))
{
}
void MSerial_Flags::SetFlags(CNcbiIos& io) const
{
    s_SerFlags(io) = (s_SerFlags(io) & ~m_All) | m_Flags;
}

// Formatting
CNcbiIos& MSerial_AsnText(CNcbiIos& io)
{
    s_SerFlags(io) = (s_SerFlags(io) & ~eFmt_All) | eFmt_AsnText;
    return io;
}
CNcbiIos& MSerial_AsnBinary(CNcbiIos& io)
{
    s_SerFlags(io) = (s_SerFlags(io) & ~eFmt_All) | eFmt_AsnBinary;
    return io;
}
CNcbiIos& MSerial_Xml(CNcbiIos& io)
{
    s_SerFlags(io) = (s_SerFlags(io) & ~eFmt_All) | eFmt_Xml;
    return io;
}
CNcbiIos& MSerial_Json(CNcbiIos& io)
{
    s_SerFlags(io) = (s_SerFlags(io) & ~eFmt_All) | eFmt_Json;
    return io;
}


// Class member assignment verification
CNcbiIos& MSerial_VerifyDefault(CNcbiIos& io)
{
    s_SerFlags(io) = (s_SerFlags(io) & ~eVerify_All);
    return io;
}
CNcbiIos& MSerial_VerifyNo(CNcbiIos& io)
{
    s_SerFlags(io) = (s_SerFlags(io) & ~eVerify_All) | eVerify_No;
    return io;
}
CNcbiIos& MSerial_VerifyYes(CNcbiIos& io)
{
    s_SerFlags(io) = (s_SerFlags(io) & ~eVerify_All) | eVerify_Yes;
    return io;
}
CNcbiIos& MSerial_VerifyDefValue(CNcbiIos& io)
{
    s_SerFlags(io) = (s_SerFlags(io) & ~eVerify_All) | eVerify_DefValue;
    return io;
}


// Input/output
CNcbiOstream& operator<< (CNcbiOstream& os, const CSerialObject& obj)
{
    auto_ptr<CObjectOStream> ostr( CObjectOStream::Open( s_FlagsToFormat(os), os) );
    ostr->SetVerifyData( s_FlagsToVerify(os) );
    if (ostr->GetDataFormat() == eSerial_Xml) {
        dynamic_cast<CObjectOStreamXml*>(ostr.get())->
            SetDefaultStringEncoding( s_FlagsToEncoding(os) );
    }
    ostr->Write(&obj,obj.GetThisTypeInfo());
    return os;
}

CNcbiIstream& operator>> (CNcbiIstream& is, CSerialObject& obj)
{
    auto_ptr<CObjectIStream> istr( CObjectIStream::Open(s_FlagsToFormat(is), is) );
    istr->SetVerifyData(s_FlagsToVerify(is));
    if (istr->GetDataFormat() == eSerial_Xml) {
        dynamic_cast<CObjectIStreamXml*>(istr.get())->
            SetDefaultStringEncoding( s_FlagsToEncoding(is) );
    }
    istr->Read(&obj,obj.GetThisTypeInfo());
    return is;
}

CNcbiOstream& operator<< (CNcbiOstream& os, const CConstObjectInfo& obj)
{
    auto_ptr<CObjectOStream> ostr( CObjectOStream::Open(s_FlagsToFormat(os), os) );
    ostr->SetVerifyData(s_FlagsToVerify(os));
    if (ostr->GetDataFormat() == eSerial_Xml) {
        dynamic_cast<CObjectOStreamXml*>(ostr.get())->
            SetDefaultStringEncoding( s_FlagsToEncoding(os) );
    }
    ostr->Write(obj);
    return os;
}

CNcbiIstream& operator>> (CNcbiIstream& is, const CObjectInfo& obj)
{
    auto_ptr<CObjectIStream> istr( CObjectIStream::Open(s_FlagsToFormat(is), is) );
    istr->SetVerifyData(s_FlagsToVerify(is));
    if (istr->GetDataFormat() == eSerial_Xml) {
        dynamic_cast<CObjectIStreamXml*>(istr.get())->
            SetDefaultStringEncoding( s_FlagsToEncoding(is) );
    }
    istr->Read(obj);
    return is;
}


END_NCBI_SCOPE
