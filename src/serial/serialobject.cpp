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
* Revision 1.13  2003/08/13 15:47:45  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.12  2003/07/17 18:48:01  gouriano
* re-enabled initialization verification
*
* Revision 1.11  2003/06/25 17:49:05  gouriano
* fixed verification flag initialization, disabled verification
*
* Revision 1.10  2003/06/04 21:23:04  gouriano
* changed the value of ms_UnassignedStr
*
* Revision 1.9  2003/05/21 16:12:04  vasilche
* Correct index argument to ThrowUnassigned() to match index of GetItemInfo().
*
* Revision 1.8  2003/05/21 16:01:52  vasilche
* Avoid failed assert while generating exception message.
*
* Revision 1.7  2003/04/29 18:30:37  gouriano
* object data member initialization verification
*
* Revision 1.6  2003/01/22 18:54:09  gouriano
* added unfreezing of the string stream
*
* Revision 1.5  2003/01/06 17:14:15  gouriano
* corrected CSerialObject::DebugDump: disabled autoseparator in output stream
*
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

#include <corelib/ncbi_safe_static.hpp>
#include <serial/serialbase.hpp>
#include <serial/typeinfo.hpp>

#include <serial/objostr.hpp>

#include <serial/classinfob.hpp>


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
    oos->SetAutoSeparator(false);
    oos->Write(this, GetThisTypeInfo());
    ostr << endl << "****** end   ASN dump ******" << endl;
    ostr << '\0';
    ddc.Log( "Serial_AsnText", string(ostr.str()));
    ostr.freeze(false);
}

/////////////////////////////////////////////////////////////////////////////
// data verification setup

const char* CSerialObject::ms_UnassignedStr = "<*unassigned*>";
const char  CSerialObject::ms_UnassignedByte = char(0xcd);

ESerialVerifyData CSerialObject::ms_VerifyDataDefault = eSerialVerifyData_Default;
static CSafeStaticRef< CTls<int> > s_VerifyTLS;


void CSerialObject::SetVerifyData(ESerialVerifyData verify)
{
    ESerialVerifyData tls_verify = ESerialVerifyData(long(s_VerifyTLS->GetValue()));
    if (tls_verify != eSerialVerifyData_Never &&
        tls_verify != eSerialVerifyData_Always) {
        s_VerifyTLS->SetValue(reinterpret_cast<int*>(verify));
    }
}

void CSerialObject::SetVerifyDataGlobal(ESerialVerifyData verify)
{
    if (ms_VerifyDataDefault != eSerialVerifyData_Never &&
        ms_VerifyDataDefault != eSerialVerifyData_Always) {
        ms_VerifyDataDefault = verify;
    }
}

bool CSerialObject::x_GetVerifyData(void)
{
    ESerialVerifyData verify;
    if (ms_VerifyDataDefault == eSerialVerifyData_Never ||
        ms_VerifyDataDefault == eSerialVerifyData_Always) {
        verify = ms_VerifyDataDefault;
    } else {
        verify = ESerialVerifyData(long(s_VerifyTLS->GetValue()));
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
                    }
                }
            }
            verify = ms_VerifyDataDefault;
        }
    }
    return verify == eSerialVerifyData_Yes ||
           verify == eSerialVerifyData_Always;
}

void CSerialObject::ThrowUnassigned(TMemberIndex index) const
{
    if (x_GetVerifyData()) {
        const CTypeInfo* type = GetThisTypeInfo();
        CNcbiOstrstream s;
        s << type->GetModuleName() << "::" << type->GetName() << ".";
        const CClassTypeInfoBase* classtype =
            dynamic_cast<const CClassTypeInfoBase*>(type);
        // offset index as the argument is zero based but items are 1 based
        index += classtype->GetItems().FirstIndex();
        if ( classtype &&
             index >= classtype->GetItems().FirstIndex() &&
             index <= classtype->GetItems().LastIndex() ) {
            s << classtype->GetItems().GetItemInfo(index)->GetId().GetName();
        } else {
            s << '[' << index << ']';
        }
        NCBI_THROW(CUnassignedMember,eGet,CNcbiOstrstreamToString(s));
    }
}


CAnyContentObject::CAnyContentObject(void)
{
}

CAnyContentObject::CAnyContentObject(const CAnyContentObject& other)
{
    m_Name = other.m_Name;
    m_Value= other.m_Value;
}

CAnyContentObject::~CAnyContentObject(void)
{
}

void CAnyContentObject::Reset(void)
{
    m_Name.erase();
    m_Value.erase();
}

CAnyContentObject& CAnyContentObject::operator= (const CAnyContentObject& other)
{
    m_Name = other.GetName();
    m_Value= other.GetValue();
    return *this;
}

bool CAnyContentObject::operator== (const CAnyContentObject& other) const
{
    return m_Name == other.GetName() && m_Value == other.GetValue();
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
    m_Value = value;
}
const string& CAnyContentObject::GetValue(void) const
{
    return m_Value;
}

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE
