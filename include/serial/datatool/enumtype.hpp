#ifndef ENUMTYPE_HPP
#define ENUMTYPE_HPP

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
*   Enumerated type definition
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2005/02/02 19:08:59  gouriano
* Corrected DTD generation
*
* Revision 1.12  2004/05/12 18:33:23  gouriano
* Added type conversion check (when using _type DEF file directive)
*
* Revision 1.11  2003/05/14 14:42:55  gouriano
* added generation of XML schema
*
* Revision 1.10  2001/05/17 15:00:42  lavr
* Typos corrected
*
* Revision 1.9  2000/12/15 15:38:35  vasilche
* Added support of Int8 and long double.
* Added support of BigInt ASN.1 extension - mapped to Int8.
* Enum values now have type Int4 instead of long.
*
* Revision 1.8  2000/11/29 17:42:30  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.7  2000/11/20 17:26:11  vasilche
* Fixed warnings on 64 bit platforms.
*
* Revision 1.6  2000/11/15 20:34:42  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.5  2000/11/14 21:41:12  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.4  2000/08/25 15:58:46  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.3  2000/05/24 20:08:31  vasilche
* Implemented DTD generation.
*
* Revision 1.2  2000/04/07 19:26:08  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:17  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.3  1999/12/03 21:42:12  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.2  1999/11/15 19:36:14  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/datatool/type.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CEnumDataTypeValue
{
public:
    CEnumDataTypeValue(const string& name, TEnumValueType value)
        : m_Name(name), m_Value(value)
        {
        }
    
    const string& GetName(void) const
        {
            return m_Name;
        }
    TEnumValueType GetValue(void) const
        {
            return m_Value;
        }

    CComments& GetComments(void)
        {
            return m_Comments;
        }
    const CComments& GetComments(void) const
        {
            return m_Comments;
        }

private:
    string m_Name;
    TEnumValueType m_Value;
    CComments m_Comments;
};

class CEnumDataType : public CDataType
{
    typedef CDataType CParent;
public:
    typedef CEnumDataTypeValue TValue;
    typedef list<TValue> TValues;

    CEnumDataType(void);
    virtual bool IsInteger(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual string GetXMLContents(void) const;

    TValue& AddValue(const string& name, TEnumValueType value);

    void PrintASN(CNcbiOstream& out, int indent) const;
    void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const;
    void PrintDTDExtra(CNcbiOstream& out) const;
    void PrintXMLSchemaElement(CNcbiOstream& out) const;

    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    struct SEnumCInfo {
        string enumName;
        string cType;
        string valuePrefix;
        
        SEnumCInfo(const string& name, const string& type,
                   const string& prefix)
            : enumName(name), cType(type), valuePrefix(prefix)
            {
            }
    };
    
    string DefaultEnumName(void) const;
    SEnumCInfo GetEnumCInfo(void) const;

    CComments& LastComments(void)
        {
            return m_LastComments;
        }

public:

    CTypeInfo* CreateTypeInfo(void);
    AutoPtr<CTypeStrings> GetRefCType(void) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    AutoPtr<CTypeStrings> GenerateCode(void) const;

private:
    TValues m_Values;
    CComments m_LastComments;
};

class CIntEnumDataType : public CEnumDataType {
    typedef CEnumDataType CParent;
public:
    virtual bool IsInteger(void) const;
    virtual const char* GetASNKeyword(void) const;
};

class CBigIntEnumDataType : public CIntEnumDataType {
    typedef CIntEnumDataType CParent;
public:
    virtual const char* GetASNKeyword(void) const;
};

END_NCBI_SCOPE

#endif
