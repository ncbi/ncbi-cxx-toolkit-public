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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  1999/11/19 15:48:11  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.7  1999/11/15 19:36:22  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiutil.hpp>
#include <list>

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
