#ifndef UNITYPE_HPP
#define UNITYPE_HPP

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
*   TYpe definition of 'SET OF' and 'SEQUENCE OF'
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2005/02/02 19:08:59  gouriano
* Corrected DTD generation
*
* Revision 1.9  2004/05/19 17:23:25  gouriano
* Corrected generation of C++ code by DTD for containers
*
* Revision 1.8  2003/06/24 20:53:39  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.7  2003/06/16 14:40:15  gouriano
* added possibility to convert DTD to XML schema
*
* Revision 1.6  2003/05/14 14:42:55  gouriano
* added generation of XML schema
*
* Revision 1.5  2000/11/14 21:41:15  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.4  2000/08/25 15:58:48  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.3  2000/05/24 20:08:32  vasilche
* Implemented DTD generation.
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
* Revision 1.3  1999/12/03 21:42:14  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.2  1999/11/15 19:36:21  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/datatool/type.hpp>

BEGIN_NCBI_SCOPE

class CUniSequenceDataType : public CDataType {
    typedef CDataType CParent;
public:
    CUniSequenceDataType(const AutoPtr<CDataType>& elementType);

    void PrintASN(CNcbiOstream& out, int indent) const;
    void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const;
    void PrintDTDExtra(CNcbiOstream& out) const;

    void PrintXMLSchemaElement(CNcbiOstream& out) const;
    void PrintXMLSchemaExtra(CNcbiOstream& out) const;

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
    bool NeedAutoPointer(const CTypeInfo* typeInfo) const;
    
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    const char* GetASNKeyword(void) const;

    bool IsNonEmpty(void) const
        {
            return m_NonEmpty;
        }
    void SetNonEmpty(bool nonEmpty)
        {
            m_NonEmpty = nonEmpty;
        }
    bool IsNoPrefix(void) const
        {
            return m_NoPrefix;
        }
    void SetNoPrefix(bool noprefix)
        {
            m_NoPrefix = noprefix;
        }

private:
    AutoPtr<CDataType> m_ElementType;
    bool m_NonEmpty;
    bool m_NoPrefix;
};

class CUniSetDataType : public CUniSequenceDataType {
    typedef CUniSequenceDataType CParent;
public:
    CUniSetDataType(const AutoPtr<CDataType>& elementType);

    CTypeInfo* CreateTypeInfo(void);
    
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    const char* GetASNKeyword(void) const;
};

END_NCBI_SCOPE

#endif
