#ifndef __TABLE2ASN_CONTEXT_HPP_INCLUDED__
#define __TABLE2ASN_CONTEXT_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>

#include "suspect_feat.hpp"

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CBioseq;
class CBioseq_set;
class CSeq_descr;
class ILineErrorListener;
class CUser_object;
class CBioSource;
class CScope;
class CObjectManager;
class CSeq_entry_EditHandle;
class CSeq_feat;
class CSourceModParser;
class CSeq_id;
class CFeat_CI;

namespace edit
{
    class CRemoteUpdater;
};

};

#include <objects/seq/Seqdesc.hpp>

// command line parameters are mapped into the context
// those with only only symbol still needs to be implemented
class CTable2AsnContext
{
public:
    string m_current_file;
    string m_ResultsDirectory;
    CNcbiOstream* m_output;
    string m_output_filename;
    string m_base_name;
    CRef<objects::CSeq_id> m_accession;
    string m_OrganismName;
    string m_single_source_qual_file;
    string m_Comment;
    string m_single_annot_file;
    string m_ft_url;
    string m_ft_url_mod;
    CTime m_HoldUntilPublish;
    string F;
    string A;
    string m_genome_center_id;
    string m_validate;
    bool   m_delay_genprodset;
    string G;
    bool   R;
    bool   S;
    string Q;
    bool   L;
    bool   W;
    bool   m_save_bioseq_set;
    string c;
    bool   m_discrepancy;
    string zOufFile;
    string X;
    string m_master_genome_flag;
    string m;
    string m_cleanup;
    string m_single_structure_cmt;   
    string m_ProjectVersionNumber;
    string m_disc_lineage;
    bool   m_disc_eucariote;
    bool   m_RemoteTaxonomyLookup;
    bool   m_RemotePubLookup;
    bool   m_HandleAsSet;
    objects::CBioseq_set::TClass m_ClassValue;
    bool   m_GenomicProductSet;
    bool   m_SetIDFromFile;
    int    m_taxid;
    TSeqPos m_gapNmin;
    TSeqPos m_gap_Unknown_length;
    TSeqPos m_minimal_sequence_length;
    set<int> m_gap_evidences;
    int    m_gap_type;
    bool   m_fcs_trim;
    bool   m_split_log_files;
    bool   m_postprocess_pubs;
    string m_asn1_suffix;
    string m_locus_tag_prefix;
    bool   m_use_hypothetic_protein;
    bool   m_eukariote;
    bool   m_di_fasta;
    bool   m_allow_accession;
    bool   m_verbose;
    bool   m_augustus_fix;
    bool   m_make_flatfile;

    CRef<objects::CSeq_descr>  m_descriptors;
    auto_ptr<objects::edit::CRemoteUpdater>   m_remote_updater;

    objects::CFixSuspectProductName m_suspect_rules;

    //string conffile;

/////////////
    CTable2AsnContext();
    ~CTable2AsnContext();

    static
    void AddUserTrack(objects::CSeq_descr& SD, const string& type, const string& label, const string& data);
    void SetOrganismData(objects::CSeq_descr& SD, int genome_code, const string& taxname, int taxid, const string& strain) const;

    CNcbiOstream& GetOstream(CTempString suffix, CTempString basename=kEmptyStr);
    string GenerateOutputFilename(const CTempString& ext, CTempString basename=kEmptyStr) const;
    void ReleaseOutputs();

    static
    objects::CUser_object& SetUserObject(objects::CSeq_descr& descr, const CTempString& type);
    static
    objects::CSeq_descr& SetBioseqOrParentDescr(objects::CBioseq& bioseq);
    bool ApplyCreateUpdateDates(objects::CSeq_entry& entry) const;
    void ApplyUpdateDate(objects::CSeq_entry& entry) const;
    void ApplyAccession(objects::CSeq_entry& entry);
    void ApplyFileTracks(objects::CSeq_entry& entry) const;
    void ApplyComments(objects::CSeq_entry& entry);
    CRef<CSerialObject> CreateSubmitFromTemplate(
        CRef<objects::CSeq_entry>& object, 
        CRef<objects::CSeq_submit>& submit) const;
    CRef<CSerialObject>
        CreateSeqEntryFromTemplate(CRef<objects::CSeq_entry> object) const;
    void UpdateSubmitObject(CRef<objects::CSeq_submit>& submit) const;

    void MergeWithTemplate(objects::CSeq_entry& entry) const;
    void SetSeqId(objects::CSeq_entry& entry) const;
    void CopyFeatureIdsToComments(objects::CSeq_entry& entry) const;
    void RemoveUnnecessaryXRef(objects::CSeq_entry& entry) const;
    void SmartFeatureAnnotation(objects::CSeq_entry& entry) const;

    void CorrectCollectionDates(objects::CSeq_entry& entry);

    void MakeGenomeCenterId(objects::CSeq_entry& entry);
    void RenameProteinIdsQuals(objects::CSeq_feat& feature);
    void RemoveProteinIdsQuals(objects::CSeq_feat& feature);
    static bool IsDBLink(const objects::CSeqdesc& desc);


    CRef<objects::CSeq_submit> m_submit_template;
    CRef<objects::CSeq_entry>  m_entry_template;
    objects::ILineErrorListener* m_logger;

    CRef<objects::CScope>      m_scope;
    CRef<objects::CObjectManager> m_ObjMgr;
    string m_source_mods;

    static bool GetOrgName(string& name, const objects::CSeq_entry& entry);
    static CRef<objects::COrg_ref> GetOrgRef(objects::CSeq_descr& descr);
    static void UpdateTaxonFromTable(objects::CBioseq& bioseq);

private:
    void x_MergeSeqDescr(objects::CSeq_descr& dest, const objects::CSeq_descr& src, bool only_set) const;
    static void x_ApplyAccession(CTable2AsnContext& context, objects::CBioseq& bioseq);
    map<string, pair<string, unique_ptr<CNcbiOstream>>> m_outputs;
};


END_NCBI_SCOPE


#endif
