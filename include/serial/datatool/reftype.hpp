#ifndef REFTYPE_HPP
#define REFTYPE_HPP

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
*   Type reference definition
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2005/02/02 19:08:59  gouriano
* Corrected DTD generation
*
* Revision 1.8  2003/06/16 14:40:15  gouriano
* added possibility to convert DTD to XML schema
*
* Revision 1.7  2003/05/14 14:42:55  gouriano
* added generation of XML schema
*
* Revision 1.6  2000/11/14 21:41:14  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.5  2000/11/01 20:36:12  vasilche
* OPTIONAL and DEFAULT are not permitted in CHOICE.
* Fixed code generation for DEFAULT.
*
* Revision 1.4  2000/08/25 15:58:48  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.3  2000/05/24 20:08:31  vasilche
* Implemented DTD generation.
*
* Revision 1.2  2000/04/07 19:26:11  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:21  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.4  1999/12/03 21:42:13  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.3  1999/12/01 17:36:26  vasilche
* Fixed CHOICE processing.
*
* Revision 1.2  1999/11/15 19:36:18  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/datatool/type.hpp>

BEGIN_NCBI_SCOPE

class CReferenceDataType : public CDataType {
    typedef CDataType CParent;
public:
    CReferenceDataType(const string& n);

    void PrintASN(CNcbiOstream& out, int indent) const;
    void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const;
    void PrintDTDExtra(CNcbiOstream& out) const;
    void PrintXMLSchemaElement(CNcbiOstream& out) const;
    void PrintXMLSchemaExtra(CNcbiOstream& out) const;

    void FixTypeTree(void) const;
    bool CheckType(void) const;
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    string GetDefaultString(const CDataValue& value) const;

    const CTypeInfo* GetRealTypeInfo(void);
    CTypeInfo* CreateTypeInfo(void);

    AutoPtr<CTypeStrings> GenerateCode(void) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;

    virtual const CDataType* Resolve(void) const; // resolve or this
    virtual CDataType* Resolve(void); // resolve or this

    const string& GetUserTypeName(void) const
        {
            return m_UserTypeName;
        }

    const string& UserTypeXmlTagName(void) const
        {
            return GetUserTypeName();
        }

protected:
    CDataType* ResolveOrNull(void) const;
    CDataType* ResolveOrThrow(void) const;

private:
    string m_UserTypeName;
};

END_NCBI_SCOPE

#endif
