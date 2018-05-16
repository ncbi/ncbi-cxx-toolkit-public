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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin
 * Created:  17/09/2013 15:38:53
 */


#ifndef _REPORT_OBJECT_H_
#define _REPORT_OBJECT_H_

#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
BEGIN_SCOPE(NDiscrepancy)


struct CReportObjPtr;
class CReportObject;
class CDiscrepancyContext;

class CReportObjectData : public CObject
{
protected:
    CReportObj::EType      m_Type;
    string                 m_Text;
    string                 m_FeatureType;
    string                 m_Product;
    string                 m_Location;
    string                 m_LocusTag;
    string                 m_ShortName;
    CConstRef<CSerialObject> m_Obj;
    size_t                 m_FileID;
    CScope&                m_Scope;

    CReportObjectData(const CSerialObject* obj, CScope& scope, bool keep);
    CReportObjectData(const string& str, CScope& scope) : m_Type(CReportObj::eType_string), m_Text(str), m_Scope(scope) {}

friend class CReportObject;
friend struct CReportObjPtr;
friend class CDiscrepancyContext;
};


class NCBI_DISCREPANCY_EXPORT CReportObject : public CReportObj
{
public:
    CReportObject(const CReportObjectData& data) : m_Data(&data) {}
    CReportObject(const CReportObject& other) : m_Data(other.m_Data) {}
    ~CReportObject() {}

    const string& GetText() const { return m_Data->m_Text; }
    const string& GetFeatureType() const { return m_Data->m_FeatureType; }
    const string& GetProductName() const { return m_Data->m_Product; }
    const string& GetLocation() const { return m_Data->m_Location; }
    const string& GetLocusTag() const { return m_Data->m_LocusTag; }
    const string& GetShort() const { return m_Data->m_ShortName; }
    EType GetType(void) const { return m_Data->m_Type; }
    CScope& GetScope(void) const { return m_Data->m_Scope; }
    CConstRef<CSerialObject> GetObject(void) const { return m_Data->m_Obj; }
    CConstRef<CBioseq> GetBioseq() const { return CConstRef<CBioseq>(dynamic_cast<const CBioseq*>(&*m_Data->m_Obj)); }
    size_t GetFileID() const { return m_Data->m_FileID; }

    static string GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope);
    static string GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, const string& product);  
    static string GetTextObjectDescription(const CSeqdesc& sd);
    static string GetTextObjectDescription(const CBioseq& bs, CScope& scope);
    static string GetTextObjectDescription(CBioseq_set_Handle bssh);
    static void GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, string &type, string &context, string &location, string &locus_tag);
    static void GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, string &type, string &location, string &locus_tag);

protected:
    CConstRef<CReportObjectData>   m_Data;

friend struct CReportObjPtr;
};

string GetProductForCDS(const CSeq_feat& cds, CScope& scope);

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE

#endif 
