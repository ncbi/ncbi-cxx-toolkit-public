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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1999/11/15 19:36:18  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include "type.hpp"

class CReferenceDataType : public CDataType {
    typedef CDataType CParent;
public:
    CReferenceDataType(const string& n);

    void PrintASN(CNcbiOstream& out, int indent) const;

    bool CheckType(void) const;
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    const CTypeInfo* GetTypeInfo(void);

    virtual void GenerateCode(CClassCode& code) const;
    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;

    virtual const CDataType* Resolve(void) const; // resolve or this
    virtual CDataType* Resolve(void); // resolve or this

    void SetInSet(void);
    void SetInChoice(const CChoiceDataType* choice);

    const string& GetUserTypeName(void) const
        {
            return m_UserTypeName;
        }

protected:
    CDataType* ResolveOrNull(void) const;
    CDataType* ResolveOrThrow(void) const;

private:
    string m_UserTypeName;
};

#endif
