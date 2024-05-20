#ifndef __TABLE2ASN_CONTEXT_HPP_INCLUDED__
#define __TABLE2ASN_CONTEXT_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>
#include <objtools/edit/gaps_edit.hpp>
#include <misc/discrepancy/discrepancy.hpp>
#include <mutex>
#include <optional>

#include "fileset.hpp"
#include "table2asn_validator.hpp"  // RW-2262 - Must fix this

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
class CFixSuspectProductName;

namespace edit
{
    class CRemoteUpdater;
}

}

#include <objects/seq/Seqdesc.hpp>

enum class eFiles
{
    asn,
    log,
    ecn,
    gbf,
    val,
    dr,
    stats,
    fixedproducts
};

template<typename _enum, _enum ... _options>
class TMultiFileSet;

template<typename _enum, _enum ... _options>
class TMultiFileSet: public TMultiSourceFileSet<_enum, _options...>
{
public:
    using _MyBase   = TMultiSourceFileSet<_enum, _options...>;
    using enum_type = typename _MyBase::enum_type;
    //static constexpr size_t enum_size = _MyBase::enum_size;
    //std::ostream& operator[](enum_type);
    //void Close(enum_type);
    //void Close();
private:
};

using CDiagnosticFileSet = TMultiFileSet<eFiles,
    //eFiles::asn,
    eFiles::log,
    eFiles::ecn,
    eFiles::gbf,
    eFiles::val,
    eFiles::dr,
    eFiles::stats,
    eFiles::fixedproducts
>;

using CDataFileSet = TMultiFileSet<eFiles,
    eFiles::asn
>;

class CValidMessageHandler;

// command line parameters are mapped into the context
// those with only only symbol still needs to be implemented
class CTable2AsnContext
{
public:
    string m_current_file;
    string m_ResultsDirectory;
    CNcbiOstream* m_output{ nullptr };
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
    bool   m_delay_genprodset { false };
    string G;
    bool   R{ false };
    bool   S{ false };
    string Q;
    bool   L{ false };
    bool   W{ false };
    bool   m_t{ false };
    bool   m_save_bioseq_set{ false };
    string c;
    string zOufFile;
    string X;
    string m_master_genome_flag;
    string m;
    string m_cleanup;
    string m_single_structure_cmt;
    string m_ProjectVersionNumber;
    string m_disc_lineage;
    bool   m_RemoteTaxonomyLookup{ false };
    bool   m_RemotePubLookup{ false };
    bool   m_HandleAsSet{ false };
    objects::CBioseq_set::TClass m_ClassValue{ objects::CBioseq_set::eClass_genbank };
    bool   m_SetIDFromFile{ false };

    TSeqPos m_gapNmin{ 0 };
    TSeqPos m_gap_Unknown_length{ 0 };
    TSeqPos m_minimal_sequence_length{ 0 };
    objects::CGapsEditor::TEvidenceSet m_DefaultEvidence;
    objects::CGapsEditor::TCountToEvidenceMap m_GapsizeToEvidence;

    int    m_gap_type{ -1 };
    bool   m_fcs_trim{ false };
    bool   m_split_log_files{ false };
    bool   m_postprocess_pubs{ false };
    string m_asn1_suffix;
    string m_locus_tag_prefix;
    bool   m_locus_tags_needed;
    bool   m_use_hypothetic_protein{ false };
    bool   m_eukaryote{ false };
    bool   m_di_fasta{ false };
    bool   m_d_fasta{ false };
    bool   m_allow_accession{ false };
    bool   m_verbose{ false };

    struct SPrtAlnOptions {
        bool refineAlignment { false };
        bool intronless { false };
        string filterQueryString;
    };
    SPrtAlnOptions prtAlnOptions;

    bool   m_make_flatfile{ false };
    bool   m_run_discrepancy{ false };
    bool   m_split_discrepancy{ false };
    bool   m_accumulate_mods{ false };
    bool   m_binary_asn1_output { false };
    bool   m_can_use_huge_files { false };
    bool   m_huge_files_mode { false };
    bool   m_disable_huge_files{ false };
    optional<size_t> m_use_threads {};


    NDiscrepancy::EGroup m_discrepancy_group{ NDiscrepancy::eOncaller };

    unique_ptr<objects::edit::CRemoteUpdater> m_remote_updater;

    unique_ptr<objects::CFixSuspectProductName> m_suspect_rules;


/////////////
    CTable2AsnContext();
    ~CTable2AsnContext();

    static
    void AddUserTrack(objects::CSeq_descr& SD, const string& type, const string& label, const string& data);
    void SetOrganismData(objects::CSeq_descr& SD, int genome_code, const string& taxname, int taxid, const string& strain) const;

    std::ostream& GetOstream(eFiles suffix);

    void SetOutputFilename(eFiles kind, const string& filename);
    void SetOutputFile(eFiles kind, ostream& ostr);
    //void OpenOutputs();
    void OpenDiagnosticOutputs();
    void OpenDataOutputs();
    void DeleteOutputs();
    void CloseDiagnosticOutputs();
    void CloseDataOutputs();

    string GenerateOutputFilename(eFiles kind, string_view basename = kEmptyStr) const;

    static
    objects::CUser_object& SetUserObject(objects::CSeq_descr& descr, const CTempString& type);
    bool ApplyCreateUpdateDates(objects::CSeq_entry& entry) const;
    void ApplyUpdateDate(objects::CSeq_entry& entry) const;
    void ApplyAccession(objects::CSeq_entry& entry) const;
    void ApplyFileTracks(objects::CSeq_entry& entry) const;
    void ApplyComments(objects::CSeq_entry& entry) const;
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

    void CorrectCollectionDates(objects::CSeq_entry& entry) const;

    void MakeGenomeCenterId(objects::CSeq_entry& entry) const;
    void RenameProteinIdsQuals(objects::CSeq_feat& feature) const;
    void RemoveProteinIdsQuals(objects::CSeq_feat& feature) const;
    static bool IsDBLink(const objects::CSeqdesc& desc);


    CRef<objects::CSeq_submit> m_submit_template;
    CRef<objects::CSeq_entry>  m_entry_template;
    objects::ILineErrorListener* m_logger{ nullptr };
    unique_ptr<CValidMessageHandler> pValMsgHandler;

    CRef<objects::CObjectManager> m_ObjMgr;
    string mCommandLineMods;

    static bool GetOrgName(string& name, const objects::CSeq_entry& entry);
    static CRef<objects::COrg_ref> GetOrgRef(objects::CSeq_descr& descr);
    static void UpdateTaxonFromTable(objects::CBioseq& bioseq);

    static void MergeSeqDescr(objects::CSeq_entry& dest, const objects::CSeq_descr& src, bool only_set);
    mutable std::mutex m_mutex;

private:
    static void x_ApplyAccession(const CTable2AsnContext& context, objects::CBioseq& bioseq);
    CDiagnosticFileSet mDiagnosticWriters;
    CDataFileSet mDataWriters;
    // these are used in single threaded mode
    CDiagnosticFileSet::fileset_type mCurrentDiagnosticOutputs;
    CDataFileSet::fileset_type mCurrentDataOutputs;
};

void g_LoadLinkageEvidence(const string& linkageEvidenceFilename,
        objects::CGapsEditor::TCountToEvidenceMap& gapsizeToEvidence,
        objects::ILineErrorListener* pMessageListener);
void g_LogDiagMessage(objects::ILineErrorListener*, EDiagSev, const string& msg);

END_NCBI_SCOPE


#endif
