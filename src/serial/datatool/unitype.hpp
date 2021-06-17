#ifndef UNITYPE_HPP
#define UNITYPE_HPP

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
*   TYpe definition of 'SET OF' and 'SEQUENCE OF'
*
*/

#include "type.hpp"

BEGIN_NCBI_SCOPE

class CUniSequenceDataType : public CDataType {
    typedef CDataType CParent;
public:
    CUniSequenceDataType(const AutoPtr<CDataType>& elementType);

    virtual void PrintASN(CNcbiOstream& out, int indent) const override;
    virtual void PrintSpecDumpExtra(CNcbiOstream& out, int indent) const override;
    virtual void PrintJSONSchema(CNcbiOstream& out, int indent, list<string>& required, bool contents_only=false) const override;
    virtual void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const override;
    virtual void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const override;
    virtual void PrintDTDExtra(CNcbiOstream& out) const override;

    virtual void FixTypeTree(void) const override;
    virtual bool CheckType(void) const override;
    virtual bool CheckValue(const CDataValue& value) const override;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const override;
    virtual string GetDefaultString(const CDataValue& value) const override;

    CDataType* GetElementType(void)
        {
            return m_ElementType.get();
        }
    const CDataType* GetElementType(void) const
        {
            return m_ElementType.get();
        }
    void SetElementType(const AutoPtr<CDataType>& type);

    virtual CTypeInfo* CreateTypeInfo(void) override;
    virtual bool NeedAutoPointer(const CTypeInfo* typeInfo) const override;
    
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual string      GetSpecKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;

    bool IsNonEmpty(void) const
        {
            return m_NonEmpty;
        }
    void SetNonEmpty(bool nonEmpty)
        {
            m_NonEmpty = nonEmpty;
        }
    bool IsNoPrefix(void) const
        {
            return m_NoPrefix;
        }
    void SetNoPrefix(bool noprefix)
        {
            m_NoPrefix = noprefix;
        }

private:
    AutoPtr<CDataType> m_ElementType;
    bool m_NonEmpty;
    bool m_NoPrefix;
};

class CUniSetDataType : public CUniSequenceDataType {
    typedef CUniSequenceDataType CParent;
public:
    CUniSetDataType(const AutoPtr<CDataType>& elementType);

    virtual CTypeInfo* CreateTypeInfo(void) override;
    
    virtual AutoPtr<CTypeStrings> GetFullCType(void) const override;
    virtual const char* GetASNKeyword(void) const override;
    virtual const char* GetDEFKeyword(void) const override;
};

END_NCBI_SCOPE

#endif
