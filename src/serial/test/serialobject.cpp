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
        info->ADD_CLASS_MEMBER(m_Size);
        info->ADD_STL_CLASS_MEMBER(m_Attributes);
    }
    return info;
}

CSerialObject::CSerialObject(void)
    : m_NamePtr(0), m_Size(0)
{
}

void CSerialObject::Dump(ostream& out) const
{
    out << "m_Name: \"" << m_Name << '\"' << endl;
    out << "m_Size: " << m_Size << endl;
    out << "m_Attributes: {" << endl;
    for ( list<string>::const_iterator i = m_Attributes.begin();
          i != m_Attributes.end();
          ++i ) {
        out << "    \"" << *i << '"' << endl;
    }
    out << '}' << endl;
}
