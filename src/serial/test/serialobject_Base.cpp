#include "serialobject.hpp"
#include <serial/serialbase.hpp>
#include <serial/serialimpl.hpp>
#if HAVE_NCBI_C
# include <asn.h>
# include "twebenv.h"
#endif

CSerialObject::CSerialObject(void)
    : m_HaveName(false), m_NamePtr(0), m_Size(0), m_Next(0)
{
}

CSerialObject::~CSerialObject(void)
{
}

BEGIN_CLASS_INFO(CSerialObject)
{
    ADD_STD_MEMBER(m_Name);
    ADD_STD_MEMBER(m_HaveName);
    ADD_MEMBER(m_NamePtr, POINTER, (STD, (string)))->SetOptional();
    ADD_STD_MEMBER(m_Size);
    ADD_MEMBER(m_Attributes, STL_list, (STD, (string)));
    ADD_MEMBER(m_Data, STL_CHAR_vector, (char));
    ADD_MEMBER(m_Offsets, STL_vector, (STD, (short)));
    ADD_MEMBER(m_Names, STL_map, (STD, (long), STD, (string)));
    ADD_MEMBER(m_Next, POINTER, (CLASS, (CSerialObject)))->SetOptional();
#if HAVE_NCBI_C
    ADD_NAMED_OLD_ASN_MEMBER("webEnv", m_WebEnv, "Web-Env", WebEnv)->SetOptional();
#endif

    ADD_SUB_CLASS(CSerialObject2);
}
END_CLASS_INFO

CSerialObject2::CSerialObject2(void)
{
}

BEGIN_DERIVED_CLASS_INFO(CSerialObject2, CSerialObject)
{
    ADD_STD_MEMBER(m_Name2);
}
END_DERIVED_CLASS_INFO
