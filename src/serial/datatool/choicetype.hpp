#ifndef CHOICETYPE_HPP
#define CHOICETYPE_HPP

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
*   Type description of CHOICE type
*
*/

#include "blocktype.hpp"

BEGIN_NCBI_SCOPE

class CChoiceDataType : public CDataMemberContainerType
{
    typedef CDataMemberContainerType CParent;
public:
    virtual void PrintASN(CNcbiOstream& out, int indent) const;

    void FixTypeTree(void) const;
    bool CheckValue(const CDataValue& value) const;

    virtual const char* XmlMemberSeparator(void) const;

    CTypeInfo* CreateTypeInfo(void);
    AutoPtr<CTypeStrings> GenerateCode(void) const;
    AutoPtr<CTypeStrings> GetRefCType(void) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual string      GetSpecKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
};

END_NCBI_SCOPE

#endif
