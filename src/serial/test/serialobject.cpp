#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/classinfo.hpp>
#include <asn.h>
#include "webenv.h"

BEGIN_CLASS_INFO(CSerialObject)
{
    ADD_STD_M(m_Name);
    ADD_STD_M(m_HaveName);
    ADD_M(m_NamePtr, POINTER, (STD, (string)));
    ADD_STD_M(m_Size);
    ADD_M(m_Attributes, STL_list, (STD, (string)));
    ADD_M(m_Data, STL_CHAR_vector, (char));
    ADD_M(m_Offsets, STL_vector, (STD, (short)));
    ADD_M(m_Names, STL_map, (STD, (long), STD, (string)));
    ADD_M(m_Next, POINTER, (CLASS, (CSerialObject)))->SetOptional();
#ifdef HAVE_NCBI_C
    ADD_OLD_ASN_MEMBER2("webEnv", m_WebEnv, "Web-Env", WebEnv)->SetOptional();
#endif

    ADD_SUB_CLASS(CSerialObject2);
}
END_CLASS_INFO

BEGIN_DERIVED_CLASS_INFO(CSerialObject2, CSerialObject)
{
    ADD_STD_M(m_Name2);
}
END_DERIVED_CLASS_INFO

CSerialObject::CSerialObject(void)
    : m_NamePtr(0), m_Size(0), m_Next(0)
{
}

CSerialObject::~CSerialObject(void)
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
    for ( vector<short>::const_iterator i1 = m_Offsets.begin();
          i1 != m_Offsets.end();
          ++i1 ) {
        out << "    " << *i1 << endl;
    }
    out << "}" << endl;
    
    out << "m_Names: {" << endl;
    for ( map<long, string>::const_iterator i2 = m_Names.begin();
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
