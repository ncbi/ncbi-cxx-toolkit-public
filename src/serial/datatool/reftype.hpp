#ifndef REFTYPE_HPP
#define REFTYPE_HPP

#include "type.hpp"

class CReferenceDataType : public CDataType {
    typedef CDataType CParent;
public:
    CReferenceDataType(const string& n);

    void PrintASN(CNcbiOstream& out, int indent) const;

    bool CheckType(void) const;
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);

    virtual void GenerateCode(CClassCode& code) const;
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

    virtual const CDataType* Resolve(void) const; // resolve or this
    virtual CDataType* Resolve(void); // resolve or this

    void SetInSet(void);
    void SetInChoice(const CChoiceDataType* choice);

    const string& GetUserTypeName(void) const
        {
            return m_UserTypeName;
        }

protected:
    CDataType* ResolveOrNull(void) const;
    CDataType* ResolveOrThrow(void) const;

private:
    string m_UserTypeName;
};

#endif
