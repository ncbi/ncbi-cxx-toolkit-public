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
    CReferenceDataType(const string& n, bool ref_to_parent=false);

    virtual void PrintASN(CNcbiOstream& out, int indent) const override;
    virtual string GetSpecKeyword(void) const override;
    virtual void PrintJSONSchema(CNcbiOstream& out, int indent, list<string>& required, bool contents_only=false) const override;
    virtual void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const override;
    virtual void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const override;
    virtual void PrintDTDExtra(CNcbiOstream& out) const override;

    virtual void FixTypeTree(void) const override;
    virtual bool CheckType(void) const override;
    virtual bool CheckValue(const CDataValue& value) const override;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;
    virtual string GetDefaultString(const CDataValue& value) const override;

    virtual const CTypeInfo* GetRealTypeInfo(void) override;
    virtual CTypeInfo* CreateTypeInfo(void) override;

    virtual AutoPtr<CTypeStrings> GenerateCode(void) const override;
    virtual AutoPtr<CTypeStrings> GetRefCType(void) const override;
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;

    virtual const CDataType* Resolve(void) const override; // resolve or this
    virtual CDataType* Resolve(void) override; // resolve or this

    const string& GetUserTypeName(void) const
        {
            return m_UserTypeName;
        }

    const string& UserTypeXmlTagName(void) const
        {
            return GetUserTypeName();
        }
    bool IsRefToParent(void) const
        {
            return m_RefToParent;
        }

protected:
    CDataType* ResolveLocalOrParent(const string& name) const;
    CDataType* ResolveOrNull(void) const;
    CDataType* ResolveOrThrow(void) const;

private:
    string m_UserTypeName;
    bool m_RefToParent;
};

END_NCBI_SCOPE

#endif
