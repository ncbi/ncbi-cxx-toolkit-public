#ifndef SERIALOBJECT_HPP
#define SERIALOBJECT_HPP

#include <corelib/ncbistd.hpp>
#include <string>
#include <list>
#include <vector>
#include <map>

struct struct_Web_Env;

BEGIN_NCBI_SCOPE

class CTypeInfo;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class CSerialObject
{
public:
    CSerialObject(void);

    void Dump(ostream& out) const;

    static const CTypeInfo* GetTypeInfo(void);

    string m_Name;
    const string* m_NamePtr;
    int m_Size;
    list<string> m_Attributes;
    vector<char> m_Data;
    vector<int> m_Offsets;
    map<int, string> m_Names;
    
    CSerialObject* m_Next;
    struct_Web_Env* m_WebEnv;
};

#endif
