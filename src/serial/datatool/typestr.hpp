#ifndef TYPESTR_HPP
#define TYPESTR_HPP

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
*   C++ class info: includes, used classes, C++ code etc.
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include "comments.hpp"

BEGIN_NCBI_SCOPE

class CClassContext;
class CNamespace;
class CDataType;

class CTypeStrings {
public:
    CTypeStrings(void);
    CTypeStrings(const CComments& comments);
    CTypeStrings(const string& namespaceName, const CDataType* dataType);
    CTypeStrings(const string& namespaceName, const CDataType* dataType, const CComments& comments);
    virtual ~CTypeStrings(void);

    const string& GetModuleName(void) const
        {
            return m_ModuleName;
        }
    struct SInternalNames {
        string m_OwnerName; // null if global type
        string m_MemberName;
    };
    string GetModuleName(SInternalNames* names) const;
    string GetDoxygenModuleName(void) const
        {
            return NStr::Replace(m_ModuleName,"-","_");
        }
    void SetModuleName(const string& name);
    void SetNamespaceName(const string& name)
    {
        m_NamespaceName = name;
    }
    const string& GetNamespaceName(void) const
    {
        return m_NamespaceName;
    }

    // kind of C++ representation
    enum EKind {
        eKindStd, // standard type
        eKindEnum, // enum
        eKindString, // std::string
        eKindPointer, // plain pointer
        eKindRef, // CRef<>
        eKindObject, // class (CHOICE, SET, SEQUENCE) inherited from CObject
        eKindClass, // any other class (CHOICE, SET, SEQUENCE)
        eKindContainer, // stl container
        eKindOther
    };
    virtual EKind GetKind(void) const = 0;

    virtual string GetCType(const CNamespace& ns) const = 0;
    virtual string GetPrefixedCType(const CNamespace& ns,
                                    const string& methodPrefix) const = 0;
    virtual bool HaveSpecialRef(void) const;
    virtual string GetRef(const CNamespace& ns) const = 0;

    // for external types
    virtual const CNamespace& GetNamespace(void) const;

    // for enum types
    virtual const string& GetEnumName(void) const;

    virtual bool CanBeKey(void) const;
    virtual bool CanBeCopied(void) const;
    virtual bool NeedSetFlag(void) const;

    static void AdaptForSTL(AutoPtr<CTypeStrings>& type);

    virtual string NewInstance(const string& init,
                               const string& place = kEmptyStr) const;

    virtual string GetInitializer(void) const;
    virtual string GetDestructionCode(const string& expr) const;
    virtual string GetIsSetCode(const string& var) const;
    virtual string GetResetCode(const string& var) const;
    virtual string GetDefaultCode(const string& var) const;

    virtual void GenerateCode(CClassContext& ctx) const;
    virtual void GenerateUserHPPCode(CNcbiOstream& out) const;
    virtual void GenerateUserCPPCode(CNcbiOstream& out) const;

    virtual void GenerateTypeCode(CClassContext& ctx) const;
    virtual void GeneratePointerTypeCode(CClassContext& ctx) const;
    
    void BeginClassDeclaration(CClassContext& ctx) const;
    void PrintHPPComments(CNcbiOstream& out) const
    {
        m_Comments.PrintHPPClass(out);
    }
    const CComments& Comments(void) const
    {
        return m_Comments;
    }
    const CDataType* DataType(void) const
    {
        return m_DataType;
    }
    void SetDataType(const CDataType* type)
    {
        m_DataType = type;
    }

private:
    string m_ModuleName;
    string m_NamespaceName;
    const CDataType* m_DataType;
    CComments m_Comments;
};

END_NCBI_SCOPE

#endif
