#ifndef CHOICEPTRSTR__HPP
#define CHOICEPTRSTR__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
*/

#include <corelib/ncbistd.hpp>
#include "classstr.hpp"
#include "ptrstr.hpp"

BEGIN_NCBI_SCOPE

class CChoicePtrTypeStrings : public CClassTypeStrings
{
    typedef CClassTypeStrings CParent;
public:
    struct SVariantInfo {
        string externalName;
        string cName;
        AutoPtr<CTypeStrings> type;

        SVariantInfo(const string& name, AutoPtr<CTypeStrings> type);
    };
    typedef list<SVariantInfo> TVariants;

    CChoicePtrTypeStrings(const string& globalName,
                          const string& className,
                          const string& namespaceName,
                          const CComments& comments);

    void AddVariant(const string& name, AutoPtr<CTypeStrings> type);

protected:
    void GenerateClassCode(CClassCode& code,
                           CNcbiOstream& getters,
                           const string& methodPrefix,
                           bool haveUserClass,
                           const string& classPrefix) const;

private:                             
    TVariants m_Variants;
};

class CChoicePtrRefTypeStrings : public CRefTypeStrings
{
    typedef CRefTypeStrings CParent;
public:
    CChoicePtrRefTypeStrings(CTypeStrings* type);
    CChoicePtrRefTypeStrings(AutoPtr<CTypeStrings> type);

    string GetRef(const CNamespace& ns) const;
};


END_NCBI_SCOPE

#endif  /* CHOICEPTRSTR__HPP */
