#ifndef VALUE_HPP
#define VALUE_HPP

/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author: Eugene Vasilchenko
*
* File Description:
*   Value definition (used in DEFAULT clause)
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiutil.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CDataTypeModule;

class CDataValue {
public:
    CDataValue(void);
    virtual ~CDataValue(void);

    virtual void PrintASN(CNcbiOstream& out, int indent) const = 0;
    virtual string GetXmlString(void) const = 0;

    void Warning(const string& mess, int err_subcode = 0) const;

    string LocationString(void) const;
    const string& GetSourceFileName(void) const;
    void SetModule(const CDataTypeModule* module) const;
    int GetSourceLine(void) const
        {
            return m_SourceLine;
        }
    void SetSourceLine(int line);
    
    virtual bool IsComplex(void) const;

private:
    mutable const CDataTypeModule* m_Module;
    int m_SourceLine;

private:
    CDataValue(const CDataValue&);
    CDataValue& operator=(const CDataValue&);
};

class CNullDataValue : public CDataValue {
public:
    ~CNullDataValue(void);
    void PrintASN(CNcbiOstream& out, int indent) const;
    virtual string GetXmlString(void) const;
};

template<typename Type>
class CDataValueTmpl : public CDataValue {
public:
    typedef Type TValueType;

    CDataValueTmpl(const TValueType& v)
        : m_Value(v)
        {
        }
    ~CDataValueTmpl(void)
        {
        }

    void PrintASN(CNcbiOstream& out, int indent) const;
    virtual string GetXmlString(void) const;

    const TValueType& GetValue(void) const
        {
            return m_Value;
        }

private:
    TValueType m_Value;
};

typedef CDataValueTmpl<bool> CBoolDataValue;
typedef CDataValueTmpl<Int4> CIntDataValue;
typedef CDataValueTmpl<double> CDoubleDataValue;
typedef CDataValueTmpl<string> CStringDataValue;

class CBitStringDataValue : public CStringDataValue {
public:
    CBitStringDataValue(const string& v)
        : CStringDataValue(v)
        {
        }
    ~CBitStringDataValue(void);

    void PrintASN(CNcbiOstream& out, int indent) const;
    virtual string GetXmlString(void) const;
};

class CIdDataValue : public CStringDataValue {
public:
    CIdDataValue(const string& v)
        : CStringDataValue(v)
        {
        }
    ~CIdDataValue(void);

    void PrintASN(CNcbiOstream& out, int indent) const;
    virtual string GetXmlString(void) const;
};

class CNamedDataValue : public CDataValue {
public:
    CNamedDataValue(const string& id, const AutoPtr<CDataValue>& v)
        : m_Name(id), m_Value(v)
        {
        }
    ~CNamedDataValue(void);

    void PrintASN(CNcbiOstream& out, int indent) const;
    virtual string GetXmlString(void) const;

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

    ~CBlockDataValue(void);

    void PrintASN(CNcbiOstream& out, int indent) const;
    virtual string GetXmlString(void) const;

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

END_NCBI_SCOPE

#endif
