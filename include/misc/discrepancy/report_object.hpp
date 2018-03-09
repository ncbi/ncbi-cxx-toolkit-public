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


class NCBI_DISCREPANCY_EXPORT CReportObject : public CReportObj
{
public:
    CReportObject(CConstRef<CBioseq> obj, CScope& scope) : m_Type(eType_sequence), m_Obj(obj), m_Scope(scope) {}
    CReportObject(CConstRef<CSeq_feat> obj, CScope& scope) : m_Type(eType_feature), m_Obj(obj), m_Scope(scope) {}
    CReportObject(CConstRef<CSeqdesc> obj, CScope& scope) : m_Type(eType_descriptor), m_Obj(obj), m_Scope(scope) {}
    CReportObject(CConstRef<CSubmit_block> obj, CScope& scope, const string& text) : m_Type(eType_submit_block), m_Text(text), m_Obj(obj), m_Scope(scope) {}
    CReportObject(CConstRef<CBioseq_set> obj, CScope& scope) : m_Type(eType_seq_set), m_Obj(obj), m_Scope(scope) {}
    CReportObject(const string& str, CScope& scope) : m_Type(eType_string), m_Text(str), m_Scope(scope) {}
    CReportObject(const CReportObject& other) :
        m_Type(other.m_Type),
        m_Text(other.m_Text),
        m_FeatureType(other.m_FeatureType),
        m_Product(other.m_Product),
        m_Location(other.m_Location),
        m_LocusTag(other.m_LocusTag),
        m_ShortName(other.m_ShortName),
        m_Obj(other.m_Obj),
        m_FileID(other.m_FileID),
        m_Scope(other.m_Scope) {}
    ~CReportObject() {}

    const string& GetText() const { return m_Text; }
    const string& GetFeatureType() const { return m_FeatureType; }
    const string& GetProductName() const { return m_Product; }
    const string& GetLocation() const { return m_Location; }
    const string& GetLocusTag() const { return m_LocusTag; }
    const string& GetShort() const { return m_ShortName; }
    EType GetType(void) const { return m_Type; }

    void SetText(CScope& scope, const string& str = kEmptyStr);
    void SetText(const string& str) { m_Text = str; }
    void SetFeatureType(const string& str) { m_FeatureType = str; }
    void SetProductName(const string& str) { m_Product = str; }
    void SetLocation(const string& str) { m_Location = str; }
    void SetLocusTag(const string& str) { m_LocusTag = str; }
    void SetShort(const string& str) { m_ShortName = str; }

    CConstRef<CBioseq> GetBioseq() const { return CConstRef<CBioseq>(dynamic_cast<const CBioseq*>(&*m_Obj)); }
    void SetBioseq(CConstRef<CBioseq> obj) { m_Obj = obj; }

    // if we have read in Seq-entries from multiple files, the 
    // report should include the filename that the object was
    // originally found in
    const size_t GetFileID() const { return m_FileID; }
    void SetFileID(const size_t fileID) { m_FileID = fileID; }

    // the DropReferences methods save text representations of the objects
    // and reset references to underlying objects.
    // This makes it possible to allow the Seq-entry (and all the objects 
    // used to create the text representations) to go out of scope and not be
    // stored in memory
    void DropReference();

    static string GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope);
    static string GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, const string& product);  
    static string GetTextObjectDescription(const CSeqdesc& sd);
    static string GetTextObjectDescription(const CBioseq& bs, CScope& scope);
    static string GetTextObjectDescription(CBioseq_set_Handle bssh);
    static void GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, string &type, string &context, string &location, string &locus_tag);
    static void GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, string &type, string &location, string &locus_tag);

    CScope& GetScope(void) const { return m_Scope; }
    CConstRef<CSerialObject> GetObject(void) const { return m_Obj; }

protected:
    EType                  m_Type;
    string                 m_Text;
    string                 m_FeatureType;
    string                 m_Product;
    string                 m_Location;
    string                 m_LocusTag;
    string                 m_ShortName;
    CConstRef<CSerialObject> m_Obj;
    //string                 m_Filename;
    size_t                 m_FileID;
    CScope&                m_Scope;
friend struct CReportObjPtr;
};

string GetProductForCDS(const CSeq_feat& cds, CScope& scope);

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE

#endif 
