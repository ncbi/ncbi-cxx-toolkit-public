#ifndef __TABLE2ASN_CONTEXT_HPP_INCLUDED__
#define __TABLE2ASN_CONTEXT_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CBioseq;
class CSeq_descr;
class ILineErrorListener;
class CUser_object;
class CBioSource;
class CScope;
class CObjectManager;
class CSeq_entry_EditHandle;
class CSeq_feat;

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
    string m_accession;
    string m_OrganismName;
    string m_source_qualifiers;
    string m_single_source_qual_file;
    string m_Comment;
    string m_single_table5_file;
    string m_find_open_read_frame;
    string m_strain;
    string m_ft_url;
    string m_ft_url_mod;
    CTime m_HoldUntilPublish;
    string F;
    string A;
    string m_genome_center_id;
    string m_validate;
    bool   m_delay_genprodset;
    bool   m_copy_genid_to_note;
    string G;
    bool   R;
    bool   S;
    string Q;
    bool   m_remove_unnec_xref;
    bool   L;
    bool   W;
    bool   m_save_bioseq_set;
    string c;
    CNcbiOstream* m_discrepancy_file;
    string zOufFile;
    string X;
    string m_master_genome_flag;
    string m;
    string m_cleanup;
    string m_single_structure_cmt;   
    string m_ProjectVersionNumber;
    bool   m_flipped_struc_cmt;
    bool   m_RemoteTaxonomyLookup;
    bool   m_RemotePubLookup;
    bool   m_HandleAsSet;
    bool   m_GenomicProductSet;
    bool   m_SetIDFromFile;
    bool   m_NucProtSet;
    int    m_taxid;
    bool   m_avoid_orf_lookup;
    TSeqPos m_gapNmin;
    TSeqPos m_gap_Unknown_length;
    TSeqPos m_minimal_sequence_length;
    set<int> m_gap_evidences;
    int    m_gap_type;
    bool   m_fcs_trim;
    bool   m_avoid_submit_block;
    bool   m_split_log_files;
    bool   m_optmap_use_locations;
    bool   m_postprocess_pubs;
    bool   m_handle_as_aa;
    bool   m_handle_as_nuc;
    string m_asn1_suffix;

    CRef<objects::CSeq_descr>  m_descriptors;
    auto_ptr<objects::edit::CRemoteUpdater>   m_remote_updater;

    //string conffile;

/////////////
    CTable2AsnContext();
    ~CTable2AsnContext();

    static
    void AddUserTrack(objects::CSeq_descr& SD, const string& type, const string& label, const string& data);
    void SetOrganismData(objects::CSeq_descr& SD, int genome_code, const string& taxname, int taxid, const string& strain) const;
    void ApplySourceQualifiers(CSerialObject& obj, const string& src_qualifiers) const;
    void ApplySourceQualifiers(objects::CSeq_entry_EditHandle& obj, const string& src_qualifiers) const;

    static
    objects::CUser_object& SetUserObject(objects::CSeq_descr& descr, const string& type);
    bool ApplyCreateUpdateDates(objects::CSeq_entry& entry) const;
    bool ApplyCreateDate(objects::CSeq_entry& entry) const;
    void ApplyUpdateDate(objects::CSeq_entry& entry) const;
    void ApplyAccession(objects::CSeq_entry& entry) const;
    void ApplyFileTracks(objects::CSeq_entry& entry) const;
    CRef<CSerialObject> CreateSubmitFromTemplate(
        CRef<objects::CSeq_entry>& object, 
        CRef<objects::CSeq_submit>& submit,
        const string& toolname) const;
    CRef<CSerialObject>
        CreateSeqEntryFromTemplate(CRef<objects::CSeq_entry> object) const;

    typedef void (*BioseqVisitorMethod)(CTable2AsnContext& context, objects::CBioseq& bioseq);
    typedef void (*FeatureVisitorMethod)(CTable2AsnContext& context, objects::CSeq_feat& feature);
    typedef void (*SeqdescVisitorMethod)(CTable2AsnContext& context, objects::CSeqdesc& seqdesc);

    void VisitAllBioseqs(objects::CSeq_entry& entry, BioseqVisitorMethod m);
    void VisitAllBioseqs(objects::CSeq_entry_EditHandle& entry_h, BioseqVisitorMethod m);
    void VisitAllFeatures(objects::CSeq_entry_EditHandle& entry_h, FeatureVisitorMethod m);
    void VisitAllSeqDesc(objects::CSeq_entry_EditHandle& entry_h, SeqdescVisitorMethod m);

    static void UpdateOrgFromTaxon(CTable2AsnContext& context, objects::CSeqdesc& seqdesc);
    void MergeWithTemplate(objects::CSeq_entry& entry) const;
    void SetSeqId(objects::CSeq_entry& entry) const;
    void CopyFeatureIdsToComments(objects::CSeq_entry& entry) const;
    void RemoveUnnecessaryXRef(objects::CSeq_entry& entry) const;
    void SmartFeatureAnnotation(objects::CSeq_entry& entry) const;

    void MakeGenomeCenterId(objects::CSeq_entry_EditHandle& entry);
    static void MakeGenomeCenterId(CTable2AsnContext& context, objects::CBioseq& bioseq);
    static void MakeDelayGenProdSet(CTable2AsnContext& context, objects::CSeq_feat& feature);

    CRef<objects::CSeq_submit> m_submit_template;
    CRef<objects::CSeq_entry>  m_entry_template;
    objects::ILineErrorListener* m_logger;

    CRef<objects::CScope>      m_scope;
    CRef<objects::CObjectManager> m_ObjMgr;

private:
    void MergeSeqDescr(objects::CSeq_descr& dest, const objects::CSeq_descr& src, bool only_pub) const;
};


END_NCBI_SCOPE


#endif
