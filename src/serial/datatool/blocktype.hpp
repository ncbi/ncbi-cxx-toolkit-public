#ifndef BLOCKTYPE_HPP
#define BLOCKTYPE_HPP

#include "type.hpp"
#include <list>

BEGIN_NCBI_SCOPE

class CClassInfoTmpl;

END_NCBI_SCOPE

class CDataMember {
public:
    CDataMember(const string& name, const AutoPtr<CDataType>& type);
    ~CDataMember(void);

    void PrintASN(CNcbiOstream& out, int indent) const;

    bool Check(void) const;

    const string& GetName(void) const
        {
            return m_Name;
        }
    CDataType* GetType(void)
        {
            return m_Type.get();
        }
    const CDataType* GetType(void) const
        {
            return m_Type.get();
        }
    bool Optional() const
        {
            return m_Optional || m_Default;
        }
    const CDataValue* GetDefault(void) const
        {
            return m_Default.get();
        }

    void SetOptional(void);
    void SetDefault(const AutoPtr<CDataValue>& value);

private:
    string m_Name;
    AutoPtr<CDataType> m_Type;
    bool m_Optional;
    AutoPtr<CDataValue> m_Default;
};

class CDataMemberContainerType : public CDataType {
    typedef CDataType CParent;
public:
    typedef list< AutoPtr<CDataMember> > TMembers;

    void PrintASN(CNcbiOstream& out, int indent) const;

    void FixTypeTree(void) const;
    bool CheckType(void) const;

    void AddMember(AutoPtr<CDataMember>& member);

    TObjectPtr CreateDefault(const CDataValue& value) const;

    virtual const char* GetASNKeyword(void) const = 0;

    const TMembers& GetMembers(void) const
        {
            return m_Members;
        }

private:
    TMembers m_Members;
};

class CDataContainerType : public CDataMemberContainerType {
    typedef CDataMemberContainerType CParent;
public:
    CTypeInfo* CreateTypeInfo(void);
    
    virtual void GenerateCode(CClassCode& code) const;
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

protected:
    virtual CClassInfoTmpl* CreateClassInfo(void);
};

class CDataSetType : public CDataContainerType {
    typedef CDataContainerType CParent;
public:
    bool CheckValue(const CDataValue& value) const;

    virtual const char* GetASNKeyword(void) const;

protected:
    CClassInfoTmpl* CreateClassInfo(void);
};

class CDataSequenceType : public CDataContainerType {
    typedef CDataContainerType CParent;
public:
    bool CheckValue(const CDataValue& value) const;

    virtual const char* GetASNKeyword(void) const;
};

class CChoiceDataType : public CDataMemberContainerType {
    typedef CDataMemberContainerType CParent;
public:
    bool CheckType(void) const;
    bool CheckValue(const CDataValue& value) const;

    CTypeInfo* CreateTypeInfo(void);
    virtual void GenerateCode(CClassCode& code) const;
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual const char* GetASNKeyword(void) const;
};

#endif
