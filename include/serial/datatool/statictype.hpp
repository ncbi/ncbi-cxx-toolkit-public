#ifndef STATICTYPE_HPP
#define STATICTYPE_HPP

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
*   Predefined types: INTEGER, BOOLEAN, VisibleString etc.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2000/11/15 20:34:43  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.6  2000/11/14 21:41:14  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.5  2000/08/25 15:58:48  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.4  2000/05/24 20:08:31  vasilche
* Implemented DTD generation.
*
* Revision 1.3  2000/04/07 19:26:12  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.2  2000/03/10 15:01:45  vasilche
* Fixed OPTIONAL members reading.
*
* Revision 1.1  2000/02/01 21:46:22  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.6  2000/01/10 19:46:46  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.5  1999/12/03 21:42:13  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.4  1999/12/01 17:36:27  vasilche
* Fixed CHOICE processing.
*
* Revision 1.3  1999/11/18 17:13:07  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.2  1999/11/15 19:36:19  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/datatool/type.hpp>

BEGIN_NCBI_SCOPE

class CStaticDataType : public CDataType {
    typedef CDataType CParent;
public:
    void PrintASN(CNcbiOstream& out, int indent) const;
    void PrintDTDElement(CNcbiOstream& out) const;

    TObjectPtr CreateDefault(const CDataValue& value) const;

    AutoPtr<CTypeStrings> GetFullCType(void) const;
    //virtual string GetDefaultCType(void) const;
    virtual const char* GetDefaultCType(void) const = 0;
    virtual const char* GetASNKeyword(void) const = 0;
    virtual const char* GetXMLContents(void) const = 0;
};

class CNullDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    CTypeRef GetTypeInfo(void);
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
};

class CBoolDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    CTypeRef GetTypeInfo(void);
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetXMLContents(void) const;

    void PrintDTDExtra(CNcbiOstream& out) const;
};

class CRealDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;

    const CTypeInfo* GetRealTypeInfo(void);
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
};

class CStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    CStringDataType(void);

    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    const CTypeInfo* GetRealTypeInfo(void);
    bool NeedAutoPointer(const CTypeInfo* typeInfo) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
};

class CStringStoreDataType : public CStringDataType {
    typedef CStringDataType CParent;
public:
    CStringStoreDataType(void);

    const CTypeInfo* GetRealTypeInfo(void);
    bool NeedAutoPointer(const CTypeInfo* typeInfo) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetASNKeyword(void) const;
};

class COctetStringDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    const CTypeInfo* GetRealTypeInfo(void);
    bool NeedAutoPointer(const CTypeInfo* typeInfo) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
};

class CBitStringDataType : public COctetStringDataType {
    typedef COctetStringDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
};

class CIntDataType : public CStaticDataType {
    typedef CStaticDataType CParent;
public:
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    CTypeRef GetTypeInfo(void);
    virtual const char* GetDefaultCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetXMLContents(void) const;
};

END_NCBI_SCOPE

#endif
