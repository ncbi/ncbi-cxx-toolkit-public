#ifndef CHOICESTR_HPP
#define CHOICESTR_HPP

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

#include "typestr.hpp"
#include "classstr.hpp"
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

class CChoiceTypeStrings : public CClassTypeStrings
{
    typedef CClassTypeStrings CParent;
public:
    enum EMemberType {
        eSimpleMember,
        eStringMember,
        eUtf8StringMember,
        ePointerMember,
        eObjectPointerMember,
        eBufferMember
    };
    struct SVariantInfo {
        string externalName;
        string cName;
        EMemberType memberType;
        AutoPtr<CTypeStrings> type;
        bool delayed;
        bool in_union;
        int memberTag;
        bool noPrefix;
        bool attlist;
        bool noTag;
        bool simple;
        const CDataType* dataType;
        CComments comments;

        SVariantInfo(const string& external_name, const string& name, const AutoPtr<CTypeStrings>& type,
                     bool delayed, bool in_union,
                     int tag, bool noPrefx, bool attlst, bool noTg,
                     bool simpl, const CDataType* dataTp, const CComments& commnts);
    };
    typedef list<SVariantInfo> TVariants;

    CChoiceTypeStrings(const string& externalName, const string& className,
                       const string& namespaceName, const CDataType* dataType,
                       const CComments& comments);
    ~CChoiceTypeStrings(void);

    bool HaveAssignment(void) const
        {
            return m_HaveAssignment;
        }

    void AddVariant(const string& external_name,
                    const string& name,
                    const AutoPtr<CTypeStrings>& type,
                    bool delayed, bool in_union, int tag,
                    bool noPrefix, bool attlist,
                    bool noTag, bool simple, const CDataType* dataType,
                    const CComments& commnts);

protected:
    void GenerateClassCode(CClassCode& code,
                           CNcbiOstream& getters,
                           const string& methodPrefix,
                           bool haveUserClass,
                           const string& classPrefix) const;
    bool x_IsNullType(TVariants::const_iterator i) const;
    bool x_IsNullWithAttlist(TVariants::const_iterator i) const;

private:
    TVariants m_Variants;
    bool m_HaveAssignment;
};

class CChoiceRefTypeStrings : public CClassRefTypeStrings
{
    typedef CClassRefTypeStrings CParent;
public:
    CChoiceRefTypeStrings(const string& className, const CNamespace& ns,
                          const string& fileName, const CComments& comments);

};

END_NCBI_SCOPE

#endif
