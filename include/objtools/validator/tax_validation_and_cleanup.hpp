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
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/taxon3/T3Reply.hpp>
#include <objects/taxon3/itaxon3.hpp>
#include <objects/valerr/ValidErrItem.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)

class CValidError_imp;


// For Taxonomy Lookups and Fixups
//
// For validation, we need to be able to look up an Org-ref and determine
// whether the tax ID in the record is the same as what is returned by
// the taxonomy service.
// For cleanup, we want to look up an Org-ref and replace the existing Org-ref
// in the record with what is returned by the taxonomy service.
//
// Several qualifiers other than Org-ref.taxname may also contain scientific names.
// It is possible that the scientific name is merely a portion of the string.
//
// In the case of specific host, we want to be able to identify names that are
// mis-spelled or unrecognized. Unfortunately, common names are also
// acceptable for specific host, and it can be difficult to detect whether a
// value is a scientific name or a common name. The current method looks for
// the string to contain at least two words, the first of which must be capitalized.
// Unfortunately, this fails for "Rhesus monkey", "Atlantic white-sided dolphin",
// and others, and fails to catch the obvious miscapitalization "homo sapiens".
// See SQD-4325 for ongoing discussion.
// For validation, these values are reported. For cleanup, we replace the
// original value with a corrected value where possible.
//
// In the case of strain, scientific names should *not* be present in certain
// situations. For validation, these values will be reported, once TM-725 is
// resolved.
//
// Often the same value will occur many, many times in the same record, and we
// would like to avoid redundant lookups.
// Taxonomy requests should be separated into manageable chunks.
// In order for the undo commands to work correctly in Genome Workbench, we need
// a method that allows Genome Workbench to control when the updates are made.
//
// Note that Org-refs can be found in both features and source descriptors.
// It is necessary to record the parents of the Org-refs for which lookups are
// made and for which lookups of qualifiers are made, in order to report
// and/or clean them.
//

typedef struct {
    EDiagSev severity;
    EErrType err_type;
    string err_msg;
} TTaxError;


// This base class represents a request for a qualifier value.
// The same qualifier value will be found in multiple Org-refs, which will
// be represented in the parents (m_Descs and m_Feats).
// A single qualifier could have multiple strings to be sent to taxonomy
// (try the whole value, try just the first two tokens, etc.). These will be
// represented in m_ValuesToTry.
class NCBI_VALIDATOR_EXPORT CQualifierRequest : public CObject
{
public:
    CQualifierRequest();
//  ~CQualifierRequest() override {}

    void AddParent(CConstRef<CSeqdesc> desc, CConstRef<CSeq_entry> ctx);
    void AddParent(CConstRef<CSeq_feat> feat);

    void AddRequests(vector<CRef<COrg_ref> >& request_list) const;
    bool MatchTryValue(const string& val) const;
    size_t NumRemainingReplies() const { return m_ValuesToTry.size() - m_RepliesProcessed; }

    virtual void AddReply(const CT3Reply& reply) = 0;
    void PostErrors(CValidError_imp& imp);
    virtual void ListErrors(vector<TTaxError>& errs) const = 0;

protected:
    void x_Init();

    vector<string> m_ValuesToTry;
    size_t m_RepliesProcessed;

    typedef pair<CConstRef<CSeqdesc>, CConstRef<CSeq_entry> > TDescPair;
    vector<TDescPair> m_Descs;
    vector<CConstRef<CSeq_feat> > m_Feats;
};

// Specific host values can be classified as normal, ambiguous, or unrecognized.
// We can also suggest a better value to use instead.
class NCBI_VALIDATOR_EXPORT CSpecificHostRequest : public CQualifierRequest
{
public:
    CSpecificHostRequest(const string& orig_val, const COrg_ref& org, bool for_fix = false);
//  ~CSpecificHostRequest() override {}

    enum EHostResponseFlags{
        eNormal = 0,
        eAmbiguous,
        eUnrecognized,
        eAlternateName
    };
    typedef int TResponseFlags;

    void AddReply(const CT3Reply& reply) override;
    void ListErrors(vector<TTaxError>& errs) const override;

    const string& SuggestFix() const;

private:
    string m_Host;
    TResponseFlags m_Response;
    string m_SuggestedFix;
    string m_Error;
    string m_HostLineage;
    string m_OrgLineage;
};


class NCBI_VALIDATOR_EXPORT CStrainRequest : public CQualifierRequest
{
public:
    CStrainRequest(const string& strain, const COrg_ref& org);
//  ~CStrainRequest() override {}

    void AddReply(const CT3Reply& reply) override;
    void ListErrors(vector<TTaxError>& errs) const override;

    static string MakeKey(const string& strain, const string& taxname);
    static bool RequireTaxname(const string& taxname);
    static bool Check(const COrg_ref& org);

private:
    string m_Strain;
    string m_Taxname;
    bool m_IsInvalid;
    static bool x_IsUnwanted(const string& str);
    static bool x_IgnoreStrain(const string& str);
};


// The map is used to eliminate duplicate taxonomy requests.
// The keys used may depend on just the qualifier value or may
// be a combination of the qualifier value and other values from
// the Org-ref (in the case of strain, this is sometimes taxname).
class NCBI_VALIDATOR_EXPORT CQualLookupMap
{
public:
    CQualLookupMap(COrgMod::ESubtype subtype) : m_Subtype(subtype), m_Populated(false) {}
    virtual ~CQualLookupMap() {}

    bool IsPopulated() const { return m_Populated; }

    void Clear();

    // GetKey gets a string key that is used to determine whether the lookup for two Org-refs
    // will be the same.
    // * For validating specific hosts, this would be the original value.
    // * For fixing specific hosts, this would be the original value after default
    //   fixes have been applied
    // * For validating strain, this might be the original value or it might be the original
    //   value plus the organism name.
    virtual string GetKey(const string& orig_val, const COrg_ref& org) const = 0;

    // Check indicates whether this Org-ref should be examined or ignored.
    // strain values are ignored for some values of lineage or taxname
    virtual bool Check(const COrg_ref& /*org*/) const { return true; }

    // used to add items to be looked up, when appropriate for this
    // descriptor or feature
    void AddDesc(CConstRef<CSeqdesc> desc, CConstRef<CSeq_entry> ctx);
    void AddFeat(CConstRef<CSeq_feat> feat);
    void AddOrg(const COrg_ref& org);

    // add an item to be looked up independently of a feature or descriptor
    void AddString(const string& val);

    // GetRequestList returns a list of Org-refs to be sent to taxonomy.
    // Note that the number of requests may be greater than the number of
    // values being checked.
    vector<CRef<COrg_ref> > GetRequestList();

    // It is the responsibility of the calling program to chunk the request
    // list and pass the input and reply to the map until all requests
    // have responses
    string IncrementalUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply);

    // Indicates whether the map is waiting for more responses
    bool IsUpdateComplete() const;

    // Posts errors to the validator based on responses
    void PostErrors(CValidError_imp& imp);

    virtual void ListErrors(vector<TTaxError>& errs) const;

    // Applies the change to an Org-ref. Note that there might be multiple
    // qualifiers of the same subtype on the Org-ref, and we need to be sure
    // to apply the change to the correct qualifier
    virtual bool ApplyToOrg(COrg_ref& org) const = 0;

protected:
    typedef map<string, CRef<CQualifierRequest> > TQualifierRequests;

    TQualifierRequests m_Map;
    COrgMod::ESubtype m_Subtype;
    bool m_Populated;

    TQualifierRequests::iterator x_FindRequest(const string& val);

    // x_MakeNewRequest creates a new CQualifierRequest object for the given pair of orig_val and org
    virtual CRef<CQualifierRequest> x_MakeNewRequest(const string& orig_val, const COrg_ref& org) = 0;
};


class NCBI_VALIDATOR_EXPORT CSpecificHostMap : public CQualLookupMap
{
public:
    CSpecificHostMap() : CQualLookupMap(COrgMod::eSubtype_nat_host) {}
//  ~CSpecificHostMap() override {}

    string GetKey(const string& orig_val, const COrg_ref& /*org*/) const override { return orig_val; }
    bool ApplyToOrg(COrg_ref& /*org*/) const override { return false; }

protected:
    CRef<CQualifierRequest> x_MakeNewRequest(const string& orig_val, const COrg_ref& org) override;
};

class NCBI_VALIDATOR_EXPORT CSpecificHostMapForFix : public CQualLookupMap
{
public:
    CSpecificHostMapForFix() : CQualLookupMap(COrgMod::eSubtype_nat_host) {}
//  ~CSpecificHostMapForFix() override {}

    string GetKey(const string& orig_val, const COrg_ref& /*org*/) const override { return x_DefaultSpecificHostAdjustments(orig_val); }
    bool ApplyToOrg(COrg_ref& org) const override;

protected:
    static string x_DefaultSpecificHostAdjustments(const string& host_val);
    CRef<CQualifierRequest> x_MakeNewRequest(const string& orig_val, const COrg_ref& org) override;
};


class NCBI_VALIDATOR_EXPORT CStrainMap : public CQualLookupMap
{
public:
    CStrainMap() : CQualLookupMap(COrgMod::eSubtype_strain) {}
//  ~CStrainMap() override {}

    string GetKey(const string& orig_val, const COrg_ref& org) const override { return CStrainRequest::MakeKey(orig_val, org.IsSetTaxname() ? org.GetTaxname() : kEmptyStr); }
    bool Check(const COrg_ref& org) const override { return CStrainRequest::Check(org); }
    bool ApplyToOrg(COrg_ref& /*org*/) const override { return false; }

protected:
    CRef<CQualifierRequest> x_MakeNewRequest(const string& orig_val, const COrg_ref& org) override;
};

typedef map<string, CSpecificHostRequest> TSpecificHostRequests;

// This class handles complete org-ref lookups, specific-host lookups,
// and strain lookups.
// These activities are bundled together in order to avoid doing a scan
// of the record looking for source features and source descriptors
// multiple times.
class NCBI_VALIDATOR_EXPORT CTaxValidationAndCleanup
{
public:
    CTaxValidationAndCleanup();
    ~CTaxValidationAndCleanup() {}

    void Init(const CSeq_entry& se);

    // for complete Org-ref validation/replacement
    vector< CRef<COrg_ref> > GetTaxonomyLookupRequest() const;
    void ListTaxLookupErrors(const CT3Reply& reply, const COrg_ref& org, CBioSource::TGenome genome, bool is_insd_patent, bool is_wp, vector<TTaxError>& errs) const;
    void ReportTaxLookupErrors(const CTaxon3_reply& reply, CValidError_imp& imp, bool is_insd_patent) const;
    void ReportIncrementalTaxLookupErrors(const CTaxon3_reply& reply, CValidError_imp& imp, bool is_insd_patent, size_t offset) const;
    bool AdjustOrgRefsWithTaxLookupReply(const CTaxon3_reply& reply,
                                         vector<CRef<COrg_ref> > org_refs,
                                         string& error_message,
                                         bool use_error_orgrefs = false) const;

    // for specific host validation/replacement
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

    // for strain validation
    vector<CRef<COrg_ref> > GetStrainLookupRequest();
    string IncrementalStrainMapUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply);
    bool IsStrainMapUpdateComplete() const;
    void ReportStrainErrors(CValidError_imp& imp);

    // Used when reporting a problem contacting the taxonomy service
    CConstRef<CSeq_entry> GetTopReportObject() const;

    // Genome Workbench uses these methods to update individual descriptors and features
    size_t NumDescs() const { return m_SrcDescs.size(); }
    size_t NumFeats() const { return m_SrcFeats.size(); }

    CConstRef<CSeqdesc> GetDesc(size_t num) const { return m_SrcDescs[num]; }
    CConstRef<CSeq_feat> GetFeat(size_t num) const { return m_SrcFeats[num]; }
    CConstRef<CSeq_entry> GetSeqContext(size_t num) const;

    bool DoTaxonomyUpdate(CSeq_entry_Handle seh, bool with_host);

    void FixOneSpecificHost(string& val);
    bool IsOneSpecificHostValid(const string& val, string& err_msg);

    void CheckOneOrg(const COrg_ref& org, int genome, CValidError_imp& imp);

protected:
    void x_InterpretTaxonomyError(const CT3Error& error, const COrg_ref& org, const EErrType type, vector<TTaxError>& errs) const;
    void x_GatherSources(const CSeq_entry& se);
    void x_CreateSpecificHostMap(bool for_fix);
    void x_UpdateSpecificHostMapWithReply(const CTaxon3_reply& reply, string& error_message);
    bool x_ApplySpecificHostMap(COrg_ref& org_ref) const;
    static string x_DefaultSpecificHostAdjustments(const string& host_val);
    TSpecificHostRequests::iterator x_FindHostFixRequest(const string& val);

    void x_CreateStrainMap();
    void x_CreateQualifierMap(CQualLookupMap& lookup);

    void x_ClearMaps() { m_HostMap.Clear(); m_HostMapForFix.Clear(); m_StrainMap.Clear(); }

    vector<CConstRef<CSeqdesc>> m_SrcDescs;
    vector<CConstRef<CSeq_entry>> m_DescCtxs;
    vector<CConstRef<CSeq_feat>> m_SrcFeats;

    TSpecificHostRequests m_SpecificHostRequests;
    bool m_SpecificHostRequestsBuilt{ false };
    bool m_SpecificHostRequestsUpdated{ false };

    bool m_StrainRequestsBuilt{ false };

    CSpecificHostMap m_HostMap;
    CSpecificHostMapForFix m_HostMapForFix;
    CStrainMap m_StrainMap;

    unique_ptr<ITaxon3> m_taxon3;
};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* TAX_VALIDATION_AND_CLEANUP__HPP */
