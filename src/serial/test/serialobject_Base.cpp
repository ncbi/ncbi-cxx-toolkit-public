#include "serialobject.hpp"
#include <serial/serialimpl.hpp>
#if HAVE_NCBI_C
# include <asn.h>
# include "twebenv.h"
#endif

CSerialObject::CSerialObject(void)
    : m_NamePtr(0), m_Size(0), m_Next(0)
{
}

CSerialObject::~CSerialObject(void)
{
}

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
    ADD_STD_M(m_Name2);
}
END_DERIVED_CLASS_INFO
