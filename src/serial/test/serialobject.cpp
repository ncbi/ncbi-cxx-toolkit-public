#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/classinfo.hpp>
#include <asn.h>
#include <webenv.h>

//    GET_PTR_INFO(CSerialObject)->ADD_SUB_CLASS(CSerialObject2);

BEGIN_CLASS_INFO(CSerialObject)
    ADD_CLASS_MEMBER(m_Name);
    ADD_CLASS_MEMBER(m_HaveName);
    ADD_PTR_CLASS_MEMBER(m_NamePtr);
    ADD_CLASS_MEMBER(m_Size);
    ADD_STL_CLASS_MEMBER(m_Attributes);
    ADD_STL_CLASS_MEMBER(m_Data);
    ADD_STL_CLASS_MEMBER(m_Offsets);
    ADD_STL_CLASS_MEMBER(m_Names);
    ADD_PTR_CLASS_MEMBER(m_Next)->SetOptional();
 //   ADD_OLD_ASN_MEMBER2("webEnv", m_WebEnv, WebEnv)->SetOptional();

    info->ADD_SUB_CLASS(CSerialObject2);
END_CLASS_INFO

BEGIN_DERIVED_CLASS_INFO(CSerialObject2, CSerialObject)
    ADD_CLASS_MEMBER(m_Name2);
END_DERIVED_CLASS_INFO

CSerialObject::CSerialObject(void)
    : m_NamePtr(0), m_Size(0), m_Next(0)
{
}

string Ptr(const void* p)
{
    char b[128];
    sprintf(b, "%p", p);
    return b;
}

void CSerialObject::Dump(ostream& out) const
{
    out << '{' << endl;
    out << "m_Name: \"" << m_Name << "\" (*" << Ptr(&m_Name) << ")" << endl;
    out << "m_NamePtr: ";
    if ( m_NamePtr )
        out << '"' << *m_NamePtr << "\" (*" << Ptr(m_NamePtr) << ")";
    else
        out << "null";
    out << endl;
    out << "m_Size: " << m_Size << endl;

    out << "m_Attributes: {" << endl;
    for ( list<string>::const_iterator i = m_Attributes.begin();
          i != m_Attributes.end();
          ++i ) {
        out << "    \"" << *i << '"' << endl;
    }
    out << "}" << endl;

    out << "m_Offsets: {" << endl;
    for ( vector<int>::const_iterator i1 = m_Offsets.begin();
          i1 != m_Offsets.end();
          ++i1 ) {
        out << "    " << *i1 << endl;
    }
    out << "}" << endl;
    
    out << "m_Names: {" << endl;
    for ( map<int, string>::const_iterator i2 = m_Names.begin();
          i2 != m_Names.end();
          ++i2 ) {
        out << "    " << i2->first << ": \"" << i2->second << '"' << endl;
    }
    out << "}" << endl;

    out << "m_Next: ";
    if ( m_Next )
        out << Ptr(m_Next);
    else
        out << "null";
    out << endl;
    out << '}' << endl;
}

CSerialObject2::CSerialObject2(void)
{
}
