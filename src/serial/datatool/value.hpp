#ifndef VALUE_HPP
#define VALUE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <list>
#include "autoptr.hpp"

USING_NCBI_SCOPE;

class CDataTypeModule;

class CDataValue {
public:
    CDataValue(void);
    virtual ~CDataValue(void);

    virtual void PrintASN(CNcbiOstream& out, int indent) const = 0;

    void Warning(const string& mess) const;

    string LocationString(void) const;
    const string& GetSourceFileName(void) const;
    void SetModule(const CDataTypeModule* module);
    int GetSourceLine(void) const
        {
            return m_SourceLine;
        }
    void SetSourceLine(int line);
    
    virtual bool IsComplex(void) const;

private:
    const CDataTypeModule* m_Module;
    int m_SourceLine;
};

class CNullDataValue : public CDataValue {
public:
    void PrintASN(CNcbiOstream& out, int indent) const;
};

template<typename Type>
class CDataValueTmpl : public CDataValue {
public:
    typedef Type TValueType;

    CDataValueTmpl(const TValueType& v)
        : m_Value(v)
        {
        }

    void PrintASN(CNcbiOstream& out, int indent) const;

    const TValueType& GetValue(void) const
        {
            return m_Value;
        }

private:
    TValueType m_Value;
};

typedef CDataValueTmpl<bool> CBoolDataValue;
typedef CDataValueTmpl<long> CIntDataValue;
typedef CDataValueTmpl<string> CStringDataValue;

class CBitStringDataValue : public CStringDataValue {
public:
    CBitStringDataValue(const string& v)
        : CStringDataValue(v)
        {
        }

    void PrintASN(CNcbiOstream& out, int indent) const;
};

class CIdDataValue : public CStringDataValue {
public:
    CIdDataValue(const string& v)
        : CStringDataValue(v)
        {
        }

    void PrintASN(CNcbiOstream& out, int indent) const;
};

class CNamedDataValue : public CDataValue {
public:
    CNamedDataValue(const string& id, const AutoPtr<CDataValue>& v)
        : m_Name(id), m_Value(v)
        {
        }

    void PrintASN(CNcbiOstream& out, int indent) const;

    const string& GetName(void) const
        {
            return m_Name;
        }

    const CDataValue& GetValue(void) const
        {
            return *m_Value;
        }
    CDataValue& GetValue(void)
        {
            return *m_Value;
        }

    virtual bool IsComplex(void) const;

private:
    string m_Name;
    AutoPtr<CDataValue> m_Value;
};

class CBlockDataValue : public CDataValue {
public:
    typedef list<AutoPtr<CDataValue> > TValues;

    void PrintASN(CNcbiOstream& out, int indent) const;

    TValues& GetValues(void)
        {
            return m_Values;
        }
    const TValues& GetValues(void) const
        {
            return m_Values;
        }

    virtual bool IsComplex(void) const;

private:
    TValues m_Values;
};

#endif
