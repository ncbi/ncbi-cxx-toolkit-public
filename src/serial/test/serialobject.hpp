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
#else
class CWeb_Env;
typedef CWeb_Env TWebEnv;
#endif

USING_NCBI_SCOPE;

class CSerialObject : public CObject
{
public:
    CSerialObject(void);
    virtual ~CSerialObject(void);

    virtual void Dump(CNcbiOstream& out) const;

    static const CTypeInfo* GetTypeInfo(void);

    string m_Name;
    bool m_HaveName;
    string* m_NamePtr;
    int m_Size;
    list<string> m_Attributes;
    vector<char> m_Data;
    vector<short> m_Offsets;
    map<long, string> m_Names;
    
    CSerialObject* m_Next;
    TWebEnv* m_WebEnv;
};

class CSerialObject2 : public CSerialObject, public CSerialUserOp
{
public:
    CSerialObject2(void);

    static const CTypeInfo* GetTypeInfo(void);

    string m_Name2;
protected:
    virtual void Assign(const CSerialUserOp& source);
    virtual bool Equals(const CSerialUserOp& object) const;
};

#endif
