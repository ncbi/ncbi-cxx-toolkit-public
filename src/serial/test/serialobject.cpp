#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/classinfo.hpp>

template<typename T>
inline
CMemberInfo MemberInfo(const string& name, const T* member, const CTypeRef typeRef)
{
	return CMemberInfo(name, size_t(member), typeRef);
}

template<typename T>
inline
CMemberInfo MemberInfo(const string& name, const T* member)
{
	return MemberInfo(name, member, GetTypeRef(member));
}

#define ADD_CLASS_MEMBER(Member) \
	AddMember(MemberInfo(#Member, &static_cast<const CClass*>(0)->Member))

const CTypeInfo* CSerialObject::GetTypeInfo(void)
{
    static CClassInfoTmpl* info = 0;
    typedef CSerialObject CClass;
    if ( info == 0 ) {
        info = new CClassInfo<CClass>();
        //const CClass* const KObject = 0;
        info->ADD_CLASS_MEMBER(m_Name);
        info->ADD_CLASS_MEMBER(m_Size);
        info->AddMember(MemberInfo("m_Attributes", &static_cast<const CClass*>(0)->m_Attributes,
				GetStlListRef(&static_cast<const CClass*>(0)->m_Attributes)));
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
