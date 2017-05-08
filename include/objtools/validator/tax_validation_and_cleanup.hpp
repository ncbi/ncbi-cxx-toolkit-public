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
 *`
 * Author:  Colleen Bollin
 *
 * File Description:
 *   Tools for batch processing taxonomy-related validation and cleanup
 *   .......
 *
 */

#ifndef VALIDATOR___TAX_VALIDATION_AND_CLEANUP__HPP
#define VALIDATOR___TAX_VALIDATION_AND_CLEANUP__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/utilities.hpp>

//#include <objtools/alnmgr/sparse_aln.hpp>

//#include <objmgr/util/create_defline.hpp>

//#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)

class CValidError_imp;


// For Taxonomy Lookups and Fixups

class CQualifierRequest : public CObject
{
public:
    CQualifierRequest();
    virtual ~CQualifierRequest() {};

    void Init();
    virtual void Init(const string& val, const COrg_ref& org) {};

    void AddParent(CConstRef<CSeqdesc> desc, CConstRef<CSeq_entry> ctx);
    void AddParent(CConstRef<CSeq_feat> feat);

    void AddRequests(vector<CRef<COrg_ref> >& request_list) const;
    bool MatchTryValue(const string& val) const;
    size_t NumRemainingReplies() const { return m_ValuesToTry.size() - m_RepliesProcessed; }

    virtual void AddReply(const CT3Reply& reply){};
    virtual void PostErrors(CValidError_imp& imp){};

protected:
    vector<string> m_ValuesToTry;
    size_t m_RepliesProcessed;

    typedef pair<CConstRef<CSeqdesc>, CConstRef<CSeq_entry> > TDescPair;
    vector<TDescPair> m_Descs;
    vector<CConstRef<CSeq_feat> > m_Feats;
};

class CSpecificHostRequest : public CQualifierRequest
{
public:
    CSpecificHostRequest();
    ~CSpecificHostRequest() {};

    virtual void Init(const string& host, const COrg_ref& org);

    enum EHostResponseFlags{
        eNormal = 0,
        eAmbiguous = 1 << 0,
        eUnrecognized = 1 << 1
    };
    typedef int TResponseFlags;

    virtual void AddReply(const CT3Reply& reply);
    virtual void PostErrors(CValidError_imp& imp);

    const string& SuggestFix() const;

private:
    string m_Host;
    TResponseFlags m_Response;
    string m_SuggestedFix;
    string m_Error;
};


class CStrainRequest : public CQualifierRequest
{
public:
    CStrainRequest();
    ~CStrainRequest() {};

    virtual void Init(const string& strain, const COrg_ref& org);

    virtual void AddReply(const CT3Reply& reply);
    virtual void PostErrors(CValidError_imp& imp);

    static string MakeKey(const string& strain, const string& taxname);
    static bool RequireTaxname(const string& taxname);
    static bool Check(const COrg_ref& org);

private:
    string m_Strain;
    string m_Taxname;
    bool m_IsInvalid;
    static bool x_IsUnwanted(const string& str);
};


class CQualLookupMap
{
public:
    CQualLookupMap(COrgMod::ESubtype subtype) : m_Subtype(subtype), m_Populated(false) {};
    virtual ~CQualLookupMap() {};

    bool IsPopulated() const { return m_Populated; };
    virtual string GetKey(const string& orig_val, const COrg_ref& org) { return kEmptyStr; };
    virtual bool Check(const COrg_ref& org) { return true; };
    virtual CRef<CQualifierRequest> MakeNewRequest(const string& orig_val, const COrg_ref& org) { return CRef<CQualifierRequest>(NULL); };

    void AddDesc(CConstRef<CSeqdesc> desc, CConstRef<CSeq_entry> ctx);
    void AddFeat(CConstRef<CSeq_feat> feat);

    vector<CRef<COrg_ref> > GetRequestList();
    string IncrementalUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply);
    bool IsUpdateComplete() const;
    void PostErrors(CValidError_imp& imp);

    virtual bool ApplyToOrg(COrg_ref& org) const { return false; }

protected:
    typedef map<string, CRef<CQualifierRequest> > TQualifierRequests;

    TQualifierRequests m_Map;
    COrgMod::ESubtype m_Subtype;
    bool m_Populated;

    TQualifierRequests::iterator x_FindRequest(const string& val);
};


class CSpecificHostMap : public CQualLookupMap
{
public:
    CSpecificHostMap() : CQualLookupMap(COrgMod::eSubtype_nat_host) {};
    ~CSpecificHostMap() {};

    virtual string GetKey(const string& orig_val, const COrg_ref& org) { return orig_val; };
    virtual bool Check(const COrg_ref& org) { return true; };

    virtual CRef<CQualifierRequest> MakeNewRequest(const string& orig_val, const COrg_ref& org);
};

class CSpecificHostMapForFix : public CQualLookupMap
{
public:
    CSpecificHostMapForFix() : CQualLookupMap(COrgMod::eSubtype_nat_host) {};
    ~CSpecificHostMapForFix() {};

    virtual string GetKey(const string& orig_val, const COrg_ref& org) { string host = orig_val; x_DefaultSpecificHostAdjustments(host); return host; };
    virtual bool Check(const COrg_ref& org) { return true; };
    virtual CRef<CQualifierRequest> MakeNewRequest(const string& orig_val, const COrg_ref& org);
    virtual bool ApplyToOrg(COrg_ref& org) const;

protected:
    static void x_DefaultSpecificHostAdjustments(string& host_val);

};


class CStrainMap : public CQualLookupMap
{
public:
    CStrainMap() : CQualLookupMap(COrgMod::eSubtype_strain) {};
    ~CStrainMap() {};

    virtual string GetKey(const string& orig_val, const COrg_ref& org) { return CStrainRequest::MakeKey(orig_val, org.IsSetTaxname() ? org.GetTaxname() : kEmptyStr); };
    virtual bool Check(const COrg_ref& org) { return CStrainRequest::Check(org); };
    virtual CRef<CQualifierRequest> MakeNewRequest(const string& orig_val, const COrg_ref& org);

};

typedef map<string, CSpecificHostRequest> TSpecificHostRequests;

class NCBI_VALIDATOR_EXPORT CTaxValidationAndCleanup
{
public:
    CTaxValidationAndCleanup();
    ~CTaxValidationAndCleanup() {};

    void Init(const CSeq_entry& se);
    vector< CRef<COrg_ref> > GetTaxonomyLookupRequest() const;
    void ReportTaxLookupErrors(const CTaxon3_reply& reply, CValidError_imp& imp, bool is_insd_patent) const;
    bool AdjustOrgRefsWithTaxLookupReply(const CTaxon3_reply& reply, 
                                         vector<CRef<COrg_ref> > org_refs, 
                                         string& error_message) const;
    vector<CRef<COrg_ref> > GetSpecificHostLookupRequest(bool for_fix);

    string IncrementalSpecificHostMapUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply);
    bool IsSpecificHostMapUpdateComplete() const;
    void ReportSpecificHostErrors(const CTaxon3_reply& reply, CValidError_imp& imp);
    void ReportSpecificHostErrors(CValidError_imp& imp);
    bool AdjustOrgRefsWithSpecificHostReply(vector<CRef<COrg_ref> > requests,
                                            const CTaxon3_reply& reply,
                                            vector<CRef<COrg_ref> > org_refs, 
                                            string& error_message);
    bool AdjustOrgRefsForSpecificHosts(vector<CRef<COrg_ref> > org_refs);

    vector<CRef<COrg_ref> > GetStrainLookupRequest();
    string IncrementalStrainMapUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply);
    bool IsStrainMapUpdateComplete() const;
    void ReportStrainErrors(CValidError_imp& imp);

    CConstRef<CSeq_entry> GetTopReportObject() const;

    size_t NumDescs() const { return m_SrcDescs.size(); }
    size_t NumFeats() const { return m_SrcFeats.size(); }

    CConstRef<CSeqdesc> GetDesc(size_t num) const { return m_SrcDescs[num]; };
    CConstRef<CSeq_feat> GetFeat(size_t num) const { return m_SrcFeats[num]; };

protected:
    void x_GatherSources(const CSeq_entry& se);
    void x_CreateSpecificHostMap(bool for_fix);
    void x_UpdateSpecificHostMapWithReply(const CTaxon3_reply& reply, string& error_message);
    bool x_ApplySpecificHostMap(COrg_ref& org_ref) const;
    static void x_DefaultSpecificHostAdjustments(string& host_val);
    TSpecificHostRequests::iterator x_FindHostFixRequest(const string& val);

    void x_CreateStrainMap();
    void x_CreateQualifierMap(CQualLookupMap& lookup);

    vector<CConstRef<CSeqdesc> > m_SrcDescs;
    vector<CConstRef<CSeq_entry> > m_DescCtxs;
    vector<CConstRef<CSeq_feat> > m_SrcFeats;
    TSpecificHostRequests m_SpecificHostRequests;
    bool m_SpecificHostRequestsBuilt;
    bool m_SpecificHostRequestsUpdated;

    bool m_StrainRequestsBuilt;

    CSpecificHostMap m_HostMap;
    CSpecificHostMapForFix m_HostMapForFix;
    CStrainMap m_StrainMap;
};



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* TAX_VALIDATION_AND_CLEANUP__HPP */
