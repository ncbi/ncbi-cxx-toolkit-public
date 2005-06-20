#ifndef SERIALOBJECT_HPP
#define SERIALOBJECT_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/serialbase.hpp>
#include <string>
#include <list>
#include <vector>
#include <map>


#ifdef HAVE_NCBI_C

struct struct_Web_Env;
typedef struct_Web_Env TWebEnv;
USING_NCBI_SCOPE;

#else

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
class CWeb_Env;
typedef CWeb_Env TWebEnv;
END_SCOPE(objects)
END_NCBI_SCOPE

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#endif


class CTestSerialObject // : public CSerialObject
{
public:
    CTestSerialObject(void);
    virtual ~CTestSerialObject(void);

    virtual void Dump(CNcbiOstream& out) const;

    DECLARE_INTERNAL_TYPE_INFO();

    string m_Name;
    bool m_HaveName;
    string* m_NamePtr;
    int m_Size;
    list<string> m_Attributes;
    vector<char> m_Data;
    vector<short> m_Offsets;
    map<long, string> m_Names;
    
    CTestSerialObject* m_Next;
    TWebEnv* m_WebEnv;
};

class CTestSerialObject2 : public CTestSerialObject, public CSerialUserOp
{
public:
    CTestSerialObject2(void);

    DECLARE_INTERNAL_TYPE_INFO();

    string m_Name2;
protected:
    virtual void UserOp_Assign(const CSerialUserOp& source);
    virtual bool UserOp_Equals(const CSerialUserOp& object) const;
};

#endif
