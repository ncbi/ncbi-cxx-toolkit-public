#ifndef BLOCKTYPE_HPP
#define BLOCKTYPE_HPP

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
*   Type description of compound types: SET, SEQUENCE and CHOICE
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2005/02/02 19:08:59  gouriano
* Corrected DTD generation
*
* Revision 1.13  2003/05/14 14:42:55  gouriano
* added generation of XML schema
*
* Revision 1.12  2002/11/19 19:47:50  gouriano
* added support of XML attributes of choice variants
*
* Revision 1.11  2002/11/14 21:07:11  gouriano
* added support of XML attribute lists
*
* Revision 1.10  2002/10/15 13:53:08  gouriano
* use "noprefix" flag
*
* Revision 1.9  2001/05/17 15:00:42  lavr
* Typos corrected
*
* Revision 1.8  2000/11/29 17:42:29  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.7  2000/11/15 20:34:41  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.6  2000/11/14 21:41:12  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.5  2000/08/25 15:58:45  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.4  2000/06/16 16:31:12  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.3  2000/05/24 20:08:30  vasilche
* Implemented DTD generation.
*
* Revision 1.2  2000/04/07 19:26:07  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:14  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.5  1999/12/03 21:42:11  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.4  1999/12/01 17:36:25  vasilche
* Fixed CHOICE processing.
*
* Revision 1.3  1999/11/15 20:31:38  vasilche
* Fixed error on GCC
*
* Revision 1.2  1999/11/15 19:36:13  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/datatool/type.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CClassTypeInfo;

class CDataMember {
public:
    CDataMember(const string& name, const AutoPtr<CDataType>& type);
    ~CDataMember(void);

    void PrintASN(CNcbiOstream& out, int indent, bool last) const;
    void PrintDTD(CNcbiOstream& out) const;
    void PrintXMLSchema(CNcbiOstream& out) const;
    void PrintXMLSchemaElement(CNcbiOstream& out) const;

    bool Check(void) const;

    const string& GetName(void) const
        {
            return m_Name;
        }
    CDataType* GetType(void)
        {
            return m_Type.get();
        }
    const CDataType* GetType(void) const
        {
            return m_Type.get();
        }
    bool Optional(void) const
        {
            return m_Optional || m_Default;
        }
    bool NoPrefix(void) const
        {
            return m_NoPrefix;
        }
    bool Attlist(void) const
        {
            return m_Attlist;
        }
    bool Notag(void) const
        {
            return m_Notag;
        }
    bool SimpleType(void) const
        {
            return m_SimpleType;
        }
    const CDataValue* GetDefault(void) const
        {
            return m_Default.get();
        }

    void SetOptional(void);
    void SetNoPrefix(void);
    void SetAttlist(void);
    void SetNotag(void);
    void SetSimpleType(void);
    void SetDefault(const AutoPtr<CDataValue>& value);

    CComments& Comments(void)
        {
            return m_Comments;
        }

private:
    string m_Name;
    AutoPtr<CDataType> m_Type;
    bool m_Optional;
    bool m_NoPrefix;
    bool m_Attlist;
    bool m_Notag;
    bool m_SimpleType;
    AutoPtr<CDataValue> m_Default;
    CComments m_Comments;
};

class CDataMemberContainerType : public CDataType {
    typedef CDataType CParent;
public:
    typedef list< AutoPtr<CDataMember> > TMembers;

    void PrintASN(CNcbiOstream& out, int indent) const;
    void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const;
    void PrintDTDExtra(CNcbiOstream& out) const;
    void PrintXMLSchemaElement(CNcbiOstream& out) const;
    void PrintXMLSchemaExtra(CNcbiOstream& out) const;

    void FixTypeTree(void) const;
    bool CheckType(void) const;

    void AddMember(const AutoPtr<CDataMember>& member);

    TObjectPtr CreateDefault(const CDataValue& value) const;

    virtual const char* GetASNKeyword(void) const = 0;
    virtual const char* XmlMemberSeparator(void) const = 0;

    const TMembers& GetMembers(void) const
        {
            return m_Members;
        }

    CComments& LastComments(void)
        {
            return m_LastComments;
        }

private:
    TMembers m_Members;
    CComments m_LastComments;
};

class CDataContainerType : public CDataMemberContainerType {
    typedef CDataMemberContainerType CParent;
public:
    CTypeInfo* CreateTypeInfo(void);
    
    virtual const char* XmlMemberSeparator(void) const;

    AutoPtr<CTypeStrings> GenerateCode(void) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    AutoPtr<CTypeStrings> GetRefCType(void) const;

protected:
    virtual CClassTypeInfo* CreateClassInfo(void);
};

class CDataSetType : public CDataContainerType {
    typedef CDataContainerType CParent;
public:
    bool CheckValue(const CDataValue& value) const;

    virtual const char* GetASNKeyword(void) const;

protected:
    CClassTypeInfo* CreateClassInfo(void);
};

class CDataSequenceType : public CDataContainerType {
    typedef CDataContainerType CParent;
public:
    bool CheckValue(const CDataValue& value) const;

    virtual const char* GetASNKeyword(void) const;
};

END_NCBI_SCOPE

#endif
