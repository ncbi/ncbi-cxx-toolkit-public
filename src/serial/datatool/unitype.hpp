#ifndef UNITYPE_HPP
#define UNITYPE_HPP

#include "type.hpp"

class CUniSequenceDataType : public CDataType {
    typedef CDataType CParent;
public:
    CUniSequenceDataType(const AutoPtr<CDataType>& elementType);

    void PrintASN(CNcbiOstream& out, int indent) const;

    void FixTypeTree(void) const;
    bool CheckType(void) const;
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    CDataType* GetElementType(void)
        {
            return m_ElementType.get();
        }
    const CDataType* GetElementType(void) const
        {
            return m_ElementType.get();
        }
    void SetElementType(const AutoPtr<CDataType>& type);

    CTypeInfo* CreateTypeInfo(void);
    
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

    virtual const char* GetASNKeyword(void) const;

private:
    AutoPtr<CDataType> m_ElementType;
};

class CUniSetDataType : public CUniSequenceDataType {
    typedef CUniSequenceDataType CParent;
public:
    CUniSetDataType(const AutoPtr<CDataType>& elementType);

    CTypeInfo* CreateTypeInfo(void);
    
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

    virtual const char* GetASNKeyword(void) const;
};

#endif
