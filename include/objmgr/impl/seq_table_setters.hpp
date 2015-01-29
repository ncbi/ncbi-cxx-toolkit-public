#ifndef SEQ_TABLE_SETTERS__HPP
#define SEQ_TABLE_SETTERS__HPP

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
*   Object field setters
*
*/

#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/seq_table_setter.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CUser_field;


/////////////////////////////////////////////////////////////////////////////
// CSeq_feat and CSeq_loc field setters
/////////////////////////////////////////////////////////////////////////////

class CSeqTableSetComment : public CSeqTableSetFeatField
{
public:
    virtual void SetString(CSeq_feat& feat, const string& value) const;
};


class CSeqTableSetDataImpKey : public CSeqTableSetFeatField
{
public:
    virtual void SetString(CSeq_feat& feat, const string& value) const;
};


class CSeqTableSetDataRegion : public CSeqTableSetFeatField
{
public:
    virtual void SetString(CSeq_feat& feat, const string& value) const;
};


class CSeqTableSetLocFuzzFromLim : public CSeqTableSetLocField
{
public:
    virtual void SetInt(CSeq_loc& loc, int value) const;
};


class CSeqTableSetLocFuzzToLim : public CSeqTableSetLocField
{
public:
    virtual void SetInt(CSeq_loc& loc, int value) const;
};


class CSeqTableSetQual : public CSeqTableSetFeatField
{
public:
    CSeqTableSetQual(const CTempString& name)
        : name(name.substr(2))
        {
        }

    virtual void SetString(CSeq_feat& feat, const string& value) const;

private:
    string name;
};


class CSeqTableSetExt : public CSeqTableSetFeatField
{
public:
    CSeqTableSetExt(const CTempString& name);

    virtual void SetInt(CSeq_feat& feat, int value) const;
    virtual void SetInt8(CSeq_feat& feat, Int8 value) const;
    virtual void SetReal(CSeq_feat& feat, double value) const;
    virtual void SetString(CSeq_feat& feat, const string& value) const;
    virtual void SetBytes(CSeq_feat& feat, const vector<char>& value) const;

protected:
    CUser_field& x_SetField(CSeq_feat& feat) const;

private:
    typedef vector<string> TSubfields;
    TSubfields subfields;
    string name;
};


class CSeqTableSetDbxref : public CSeqTableSetFeatField
{
public:
    CSeqTableSetDbxref(const CTempString& name)
        : name(name.substr(2))
        {
        }

    virtual void SetInt(CSeq_feat& feat, int value) const;
    virtual void SetInt8(CSeq_feat& feat, Int8 value) const;
    virtual void SetString(CSeq_feat& feat, const string& value) const;

private:
    string name;
};


class CSeqTableSetExtType : public CSeqTableSetFeatField
{
public:
    virtual void SetInt(CSeq_feat& feat, int value) const;
    virtual void SetInt8(CSeq_feat& feat, Int8 value) const;
    virtual void SetString(CSeq_feat& feat, const string& value) const;
};


class CSeqTableNextObject : public CObject
{
public:
    virtual CObjectInfo GetNextObject(const CObjectInfo& obj) const = 0;
};


class CSeqTableNextObjectPointer : public CSeqTableNextObject
{
public:
    virtual CObjectInfo GetNextObject(const CObjectInfo& obj) const;
};


class CSeqTableNextObjectElementNew : public CSeqTableNextObject
{
public:
    virtual CObjectInfo GetNextObject(const CObjectInfo& obj) const;
};


class CSeqTableNextObjectPtrElementNew : public CSeqTableNextObject
{
public:
    virtual CObjectInfo GetNextObject(const CObjectInfo& obj) const;
};


class CSeqTableNextObjectClassMember : public CSeqTableNextObject
{
public:
    CSeqTableNextObjectClassMember(TMemberIndex member_index)
        : m_MemberIndex(member_index)
        {
        }

    virtual CObjectInfo GetNextObject(const CObjectInfo& obj) const;

private:
    TMemberIndex m_MemberIndex;
};


class CSeqTableNextObjectChoiceVariant : public CSeqTableNextObject
{
public:
    CSeqTableNextObjectChoiceVariant(TMemberIndex variant_index)
        : m_VariantIndex(variant_index)
        {
        }

    virtual CObjectInfo GetNextObject(const CObjectInfo& obj) const;

private:
    TMemberIndex m_VariantIndex;
};


class CSeqTableNextObjectUserField : public CSeqTableNextObject
{
public:
    CSeqTableNextObjectUserField(const string& field_name)
        : m_FieldName(field_name)
        {
        }

    virtual CObjectInfo GetNextObject(const CObjectInfo& obj) const;

private:
    string m_FieldName;
};


class CSeqTableSetAnyObjField
{
public:
    CSeqTableSetAnyObjField(CObjectTypeInfo type, CTempString field);

    CObjectInfo GetFinalObject(CObjectInfo obj) const;

    void SetObjectField(CObjectInfo obj, int value) const;
    void SetObjectField(CObjectInfo obj, Int8 value) const;
    void SetObjectField(CObjectInfo obj, double value) const;
    void SetObjectField(CObjectInfo obj, const string& value) const;
    void SetObjectField(CObjectInfo obj, const vector<char>& value) const;

private:
    typedef vector< CConstRef<CSeqTableNextObject> > TNexters;
    TNexters m_Nexters;
    bool m_SetFinalObject;
    string m_SetUserField;
};


class CSeqTableSetAnyLocField :  public CSeqTableSetLocField,
                                 public CSeqTableSetAnyObjField
{
public:
    CSeqTableSetAnyLocField(const CTempString& field);

    virtual void SetInt(CSeq_loc& loc, int value) const;
    virtual void SetInt8(CSeq_loc& loc, Int8 value) const;
    virtual void SetReal(CSeq_loc& loc, double value) const;
    virtual void SetString(CSeq_loc& loc, const string& value) const;
    virtual void SetBytes(CSeq_loc& loc, const vector<char>& value) const;
};


class CSeqTableSetAnyFeatField : public CSeqTableSetFeatField,
                                 public CSeqTableSetAnyObjField
{
public:
    CSeqTableSetAnyFeatField(const CTempString& field);

    virtual void SetInt(CSeq_feat& feat, int value) const;
    virtual void SetInt8(CSeq_feat& feat, Int8 value) const;
    virtual void SetReal(CSeq_feat& feat, double value) const;
    virtual void SetString(CSeq_feat& feat, const string& value) const;
    virtual void SetBytes(CSeq_feat& feat, const vector<char>& value) const;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_TABLE_INFO__HPP
