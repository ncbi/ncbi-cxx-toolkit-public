#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/classinfo.hpp>

#if 0
#define NEW_CLASS_INFO(Class, PClass) \
new CClassInfo(typeid(Class).name(), typeid(PClass).name(), Class::s_Create)

#define ADD_CLASS_MEMBER(Class, Member) \
AddMember(CMemberInfo(#Member, \
                      size_t(&static_cast<const Class*>(0)->Member), \
                      GetTypeRef(static_cast<const Class*>(0)->Member)))
#endif

#define NEW_CLASS_INFO(PClass) \
new CClassInfo(typeid(CClass).name(), typeid(PClass).name(), Class::s_Create)

#define ADD_CLASS_MEMBER(Member) \
AddMember(CMemberInfo(#Member, \
                      size_t(&KObject->Member), \
                      GetTypeRef(&KObject->Member)))

const CTypeInfo* CSerialObject::GetTypeInfo(void)
{
    static CClassInfoTmpl* info = 0;
    typedef CSerialObject CClass;
    if ( info == 0 ) {
        info = new CClassInfo<CClass>();
        const CClass* const KObject = 0;
        info->ADD_CLASS_MEMBER(m_Name);
        info->ADD_CLASS_MEMBER(m_Size);
        info->ADD_CLASS_MEMBER(m_Attributes);
    }
    return info;
}

CSerialObject::CSerialObject(void)
    : m_NamePtr(0), m_Size(-1)
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
