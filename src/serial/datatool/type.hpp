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
#include <set>

BEGIN_NCBI_SCOPE

class CTypeInfo;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class CDataType;
class CDataTypeModule;
class CDataValue;
class CChoiceDataType;
class CUniSequenceDataType;
class CReferenceDataType;
class CClassCode;
class CTypeStrings;

typedef int TInteger;

struct AnyType {
    union {
        bool booleanValue;
        TInteger integerValue;
        double realValue;
        void* pointerValue;
    };
    AnyType(void)
        {
            pointerValue = 0;
        }
};

class CAnyTypeSource : public CTypeInfoSource
{
public:
    CAnyTypeSource(CDataType* type)
        : m_Type(type)
        {
        }

    TTypeInfo GetTypeInfo(void);

private:
    CDataType* m_Type;
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

    const string& GetSourceFileName(void) const;
    int GetSourceLine(void) const
        {
            return m_SourceLine;
        }
    void SetSourceLine(int line);
    string LocationString(void) const;

    string GetKeyPrefix(void) const;
    string IdName(void) const;
    bool Skipped(void) const;
    string ClassName(void) const;
    string FileName(void) const;
    string Namespace(void) const;
    string InheritFromClass(void) const;
    const CDataType* InheritFromType(void) const;
    const string& GetVar(const string& value) const;

    bool InChoice(void) const;

    virtual void PrintASN(CNcbiOstream& out, int indent) const = 0;

    virtual const CTypeInfo* GetTypeInfo(void);

    static CNcbiOstream& NewLine(CNcbiOstream& out, int indent);

    void Warning(const string& mess) const;

    virtual void GenerateCode(CClassCode& code) const;

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    virtual const CDataType* Resolve(void) const;
    virtual CDataType* Resolve(void);

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

    static string Identifier(const string& typeName, bool capitalize = true);
    static string GetTemplateHeader(const string& tmpl);
    static bool IsSimplePointerTemplate(const string& tmpl);
    static string GetTemplateNamespace(const string& tmpl);
    static string GetTemplateMacro(const string& tmpl);

    void SetParent(const CDataType* parent, const string& memberName);
    void SetParent(const CDataTypeModule* module, const string& typeName);
    virtual void FixTypeTree(void) const;

    bool Check(void);
    virtual bool CheckType(void) const;
    virtual bool CheckValue(const CDataValue& value) const = 0;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const = 0;
    virtual CTypeInfo* CreateTypeInfo(void);

private:
    const CDataType* m_ParentType;       // parent type
    const CDataTypeModule* m_Module;
    string m_MemberName;
    int m_SourceLine;

    // tree info
    const CUniSequenceDataType* m_Set;
    const CChoiceDataType* m_Choice;
    AutoPtr<TReferences> m_References;

    bool m_Checked;

    AutoPtr<CTypeInfo> m_CreatedTypeInfo;
};

#define CheckValueType(value, type, name) do{ \
if ( dynamic_cast<const type*>(&(value)) == 0 ) { \
    (value).Warning(name " value expected"); return false; \
} } while(0)

#endif
