#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/classinfo.hpp>

const CTypeInfo* CSerialObject::GetTypeInfo(void)
{
    static CClassInfoTmpl* info = 0;
    typedef CSerialObject CClass;
    if ( info == 0 ) {
        info = new CClassInfo<CClass>();
        info->ADD_CLASS_MEMBER(m_Name);
        info->ADD_PTR_CLASS_MEMBER(m_NamePtr);
        info->ADD_CLASS_MEMBER(m_Size);
        info->ADD_STL_CLASS_MEMBER(m_Attributes);
        info->ADD_PTR_CLASS_MEMBER(m_Next);
    }
    return info;
}

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
    out << "m_Next: ";
    if ( m_Next )
        out << Ptr(m_Next);
    else
        out << "null";
    out << endl;
    out << '}' << endl;
}
