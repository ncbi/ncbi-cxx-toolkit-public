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

    void Warning(const string& mess) const;

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
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2006/10/18 13:06:26  gouriano
* Moved Log to bottom
*
* Revision 1.6  2004/02/25 19:45:48  gouriano
* Made it possible to define DEFAULT for data members of type REAL
*
* Revision 1.5  2003/10/02 19:39:48  gouriano
* properly handle invalid enumeration values in ASN spec
*
* Revision 1.4  2003/06/16 14:40:15  gouriano
* added possibility to convert DTD to XML schema
*
* Revision 1.3  2000/12/15 15:38:36  vasilche
* Added support of Int8 and long double.
* Added support of BigInt ASN.1 extension - mapped to Int8.
* Enum values now have type Int4 instead of long.
*
* Revision 1.2  2000/04/07 19:26:15  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:25  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.9  1999/12/29 16:01:53  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.8  1999/11/19 15:48:11  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.7  1999/11/15 19:36:22  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/
