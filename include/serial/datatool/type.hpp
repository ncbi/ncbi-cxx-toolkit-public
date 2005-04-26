#ifndef TYPE_HPP
#define TYPE_HPP

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
*   Type definition
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.25  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.24  2005/02/02 19:08:59  gouriano
* Corrected DTD generation
*
* Revision 1.23  2004/05/12 18:33:23  gouriano
* Added type conversion check (when using _type DEF file directive)
*
* Revision 1.22  2003/10/21 13:48:48  grichenk
* Redesigned type aliases in serialization library.
* Fixed the code (removed CRef-s, added explicit
* initializers etc.)
*
* Revision 1.21  2003/06/16 14:40:15  gouriano
* added possibility to convert DTD to XML schema
*
* Revision 1.20  2003/05/14 14:42:55  gouriano
* added generation of XML schema
*
* Revision 1.19  2003/04/29 18:29:34  gouriano
* object data member initialization verification
*
* Revision 1.18  2003/02/12 21:41:26  gouriano
* added check for "primitive" data type (int, bool, etc)
*
* Revision 1.17  2003/02/10 17:56:40  gouriano
* make it possible to disable scope prefixes when reading and writing objects generated from ASN specification in XML format, or when converting an ASN spec into DTD.
*
* Revision 1.16  2002/12/12 21:03:25  gouriano
* changed code generation so XML attribute list became random access class
*
* Revision 1.15  2002/11/14 21:07:10  gouriano
* added support of XML attribute lists
*
* Revision 1.14  2001/06/11 14:34:58  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.13  2001/05/17 15:00:42  lavr
* Typos corrected
*
* Revision 1.12  2001/02/15 21:39:28  kholodov
* Modified: pointer to parent CDataMember added to CDataType class.
* Modified: default value for BOOLEAN type in DTD is copied from ASN.1 spec.
*
* Revision 1.11  2000/12/15 15:38:35  vasilche
* Added support of Int8 and long double.
* Added support of BigInt ASN.1 extension - mapped to Int8.
* Enum values now have type Int4 instead of long.
*
* Revision 1.10  2000/11/29 17:42:31  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.9  2000/11/15 20:34:44  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.8  2000/11/14 21:41:15  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.7  2000/11/07 17:25:30  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.6  2000/09/26 17:38:17  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.5  2000/05/24 20:08:32  vasilche
* Implemented DTD generation.
*
* Revision 1.4  2000/04/07 19:26:14  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.3  2000/03/29 15:51:42  vasilche
* Generated files names limited to 31 symbols due to limitations of Mac.
*
* Revision 1.2  2000/03/10 15:01:45  vasilche
* Fixed OPTIONAL members reading.
*
* Revision 1.1  2000/02/01 21:46:24  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.25  1999/12/29 16:01:53  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.24  1999/12/21 17:18:38  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.23  1999/12/03 21:42:13  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.22  1999/12/01 17:36:27  vasilche
* Fixed CHOICE processing.
*
* Revision 1.21  1999/11/19 15:48:11  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.20  1999/11/16 15:41:17  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.19  1999/11/15 19:36:20  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/typeref.hpp>
#include <serial/datatool/comments.hpp>
#include <list>
#include <set>

BEGIN_NCBI_SCOPE

class CTypeInfo;
class CDataType;
class CDataTypeModule;
class CDataValue;
class CChoiceDataType;
class CUniSequenceDataType;
class CReferenceDataType;
class CTypeStrings;
class CFileCode;
class CClassTypeStrings;
class CNamespace;
class CDataMember;

struct AnyType {
    union {
        bool booleanValue;
        Int4 integerValue;
        void* pointerValue;
    };
    AnyType(void)
        {
            pointerValue = 0;
        }
};

class CDataType {
public:
    typedef void* TObjectPtr;
    typedef list<const CReferenceDataType*> TReferences;

    CDataType(void);
    virtual ~CDataType(void);

    const CDataType* GetParentType(void) const
        {
            return m_ParentType;
        }
    const CDataTypeModule* GetModule(void) const
        {
            _ASSERT(m_Module != 0);
            return m_Module;
        }
    bool HaveModuleName(void) const
        {
            return m_ParentType == 0;
        }

    const string& GetSourceFileName(void) const;
    int GetSourceLine(void) const
        {
            return m_SourceLine;
        }
    void SetSourceLine(int line);
    string LocationString(void) const;

    string GetKeyPrefix(void) const;
    string IdName(void) const;
    string XmlTagName(void) const;
    const string& GlobalName(void) const; // name of type or empty
    bool Skipped(void) const;
    string ClassName(void) const;
    string FileName(void) const;
    const CNamespace& Namespace(void) const;
    string InheritFromClass(void) const;
    const CDataType* InheritFromType(void) const;
    const string GetVar(const string& value) const;
    bool GetBoolVar(const string& value, bool default_value = false) const;
    void  ForbidVar(const string& var, const string& value);
    void  AllowVar(const string& var, const string& value);
    const string GetAndVerifyVar(const string& value) const;

    bool InChoice(void) const;

    void PrintASNTypeComments(CNcbiOstream& out, int indent) const;
    virtual void PrintASN(CNcbiOstream& out, int indent) const = 0;
    void PrintDTD(CNcbiOstream& out) const;
    void PrintDTD(CNcbiOstream& out, const CComments& extra) const;
    virtual void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const = 0;
    virtual void PrintDTDExtra(CNcbiOstream& out) const;

    void PrintXMLSchema(CNcbiOstream& out) const;
    virtual void PrintXMLSchemaElement(CNcbiOstream& out) const = 0;
    void PrintXMLSchema(CNcbiOstream& out, const CComments& extra) const;
    virtual void PrintXMLSchemaExtra(CNcbiOstream& out) const;

    virtual CTypeRef GetTypeInfo(void);
    virtual const CTypeInfo* GetAnyTypeInfo(void);
    virtual bool NeedAutoPointer(const CTypeInfo* typeInfo) const;
    virtual const CTypeInfo* GetRealTypeInfo(void);
    virtual CTypeInfo* CreateTypeInfo(void);
    CTypeInfo* UpdateModuleName(CTypeInfo* typeInfo) const;

    static CNcbiOstream& NewLine(CNcbiOstream& out, int indent);

    void Warning(const string& mess) const;

    virtual AutoPtr<CTypeStrings> GenerateCode(void) const;
    void SetParentClassTo(CClassTypeStrings& code) const;

    virtual AutoPtr<CTypeStrings> GetRefCType(void) const;
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    virtual const CDataType* Resolve(void) const;
    virtual CDataType* Resolve(void);

    // resolve type from global level
    CDataType* ResolveGlobal(const string& name) const;
    // resolve type from local level
    CDataType* ResolveLocal(const string& name) const;

    bool IsInSet(void) const
        {
            return m_Set != 0;
        }
    const CUniSequenceDataType* GetInSet(void) const
        {
            return m_Set;
        }
    void SetInSet(const CUniSequenceDataType* sequence);

    bool IsInChoice(void) const
        {
            return m_Choice != 0;
        }
    const CChoiceDataType* GetInChoice(void) const
        {
            return m_Choice;
        }
    void SetInChoice(const CChoiceDataType* choice);

    bool IsReferenced(void) const
        {
            return m_References;
        }
    void AddReference(const CReferenceDataType* reference);
    const TReferences& GetReferences(void) const
        {
            return *m_References;
        }

/*
    static string GetTemplateHeader(const string& tmpl);
    static bool IsSimplePointerTemplate(const string& tmpl);
    static string GetTemplateNamespace(const string& tmpl);
    static string GetTemplateMacro(const string& tmpl);
*/

    void SetParent(const CDataType* parent, const string& memberName);
    void SetParent(const CDataTypeModule* module, const string& typeName);
    virtual void FixTypeTree(void) const;

    bool Check(void);
    virtual bool CheckType(void) const;
    virtual bool CheckValue(const CDataValue& value) const = 0;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const = 0;
    
    CComments& Comments(void)
        {
            return m_Comments;
        }

    void SetDataMember(CDataMember* dm) {
        m_DataMember = dm;
    }

    const CDataMember* GetDataMember(void) const {
        return m_DataMember;
    }

    enum {
        eNoExplicitTag = -1
    };
    void SetTag(int tag) {
        m_Tag = tag;
    }

    int GetTag(void) const {
        return m_Tag;
    }

    bool HasTag(void) const {
        return m_Tag != eNoExplicitTag;
    }

    void SetTypeStr(CClassTypeStrings* TypeStr) const {
        m_TypeStr = TypeStr;
    }
    CClassTypeStrings* GetTypeStr(void) const {
        return m_TypeStr;
    }

    bool IsPrimitive(void) const;
    bool IsStdType(void) const;
    bool IsReference(void) const;

    void SetIsAlias(bool value) {
        m_IsAlias = value;
    }
    bool IsAlias(void) const {
        return m_IsAlias;
    }

    static void SetEnforcedStdXml(bool set = true) {
        sm_EnforcedStdXml = set;
    }
    static bool GetEnforcedStdXml(void) {
        return sm_EnforcedStdXml;
    }

protected:
    static bool x_IsSavedName(const string& name);
    static void x_AddSavedName(const string& name);

private:
    const CDataType* m_ParentType;       // parent type
    const CDataTypeModule* m_Module;
    string m_MemberName;
    int m_SourceLine;
    CComments m_Comments;
    CDataMember* m_DataMember;
    mutable CClassTypeStrings* m_TypeStr;

    // tree info
    const CUniSequenceDataType* m_Set;
    const CChoiceDataType* m_Choice;
    AutoPtr<TReferences> m_References;

    bool m_Checked;

    CTypeRef m_TypeRef;
    AutoPtr<CTypeInfo> m_AnyTypeInfo;
    AutoPtr<CTypeInfo> m_RealTypeInfo;
    mutable string m_CachedFileName;
    mutable auto_ptr<CNamespace> m_CachedNamespace;
    int m_Tag;
    bool m_IsAlias;
    multimap<string,string> m_ForbidVar;

    CDataType(const CDataType&);
    CDataType& operator=(const CDataType&);
    static bool sm_EnforcedStdXml;
    static set<string> sm_SavedNames;
};

#define CheckValueType(value, type, name) do{ \
if ( dynamic_cast<const type*>(&(value)) == 0 ) { \
    (value).Warning(name " value expected"); return false; \
} } while(0)

END_NCBI_SCOPE

#endif
