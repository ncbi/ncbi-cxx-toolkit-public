#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/classinfo.hpp>

BEGIN_CLASS_INFO(CSerialObject)
    ADD_CLASS_MEMBER(m_Name);
    ADD_PTR_CLASS_MEMBER(m_NamePtr);
    ADD_CLASS_MEMBER(m_Size);
    ADD_STL_CLASS_MEMBER(m_Attributes);
    ADD_STL_CLASS_MEMBER(m_Data);
    ADD_STL_CLASS_MEMBER(m_Offsets);
    ADD_STL_CLASS_MEMBER(m_Names);
    ADD_PTR_CLASS_MEMBER(m_Next);
END_CLASS_INFO

/*

Serial-Object ::= SEQUENCE {
    name VisibleString,
    attributes SEQUENCE OF VisibleString { "none" },
    next Serial-Object OPTIONAL
}

const CTypeInfo* GetTypeInfo_Serial_Object(void)
{
    static CClassInfoTmpl* info = 0;
    typedef CSerialObject CClass;
    if ( info == 0 ) {
        CTypeRef typeRef = GetTypeRef(static_cast<const CSerialObject*>(0));
        info = new CClassInfoTmpl("Serial-Object", typeRef);
        info->ADD_ALIAS_MEMBER("name", "m_Name");
        info->ADD_ALIAS_MEMBER("attributes", "m_Attributes", new list<string>("none"));
        info->ADD_ALIAS_MEMBER("next", "m_Next", GetTypeInfo_Serial_Object(), null);
    }
    return info;
}

    CSerialObject s;
    CObjectOStream out;
    out.Write(&s, GetTypeInfo_Serial_Object());

*/

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
