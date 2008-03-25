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
*/

#include "type.hpp"

BEGIN_NCBI_SCOPE

class CReferenceDataType : public CDataType {
    typedef CDataType CParent;
public:
    CReferenceDataType(const string& n);

    void PrintASN(CNcbiOstream& out, int indent) const;
    virtual string      GetSpecKeyword(void) const;
    void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const;
    void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const;
    void PrintDTDExtra(CNcbiOstream& out) const;

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
