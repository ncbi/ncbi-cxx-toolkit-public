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
class CDate;
class IMessageListener;
class CUser_object;
class CBioSource;
};

#include <objects/seq/Seqdesc.hpp>

// command line parameters are mapped into the context
// those with only only symbol still needs to be implemented

class CAutoAddDesc
{
public:
    CAutoAddDesc(objects::CSeq_descr& descr, objects::CSeqdesc::E_Choice which)
    {
        m_descr.Reset(&descr);
        m_which = which;
    }
    static
    CRef<objects::CSeqdesc> LocateDesc(objects::CSeq_descr& descr, objects::CSeqdesc::E_Choice which);
    const objects::CSeqdesc& Get();
    objects::CSeqdesc& Set();

private:
    CAutoAddDesc();
    CAutoAddDesc(const CAutoAddDesc&);

    objects::CSeqdesc::E_Choice m_which;
    CRef<objects::CSeq_descr> m_descr;
    CRef<objects::CSeqdesc>   m_desc;
};


class CTable2AsnContext
{
public:
    string m_current_file;
    string m_ResultsDirectory;
    CNcbiOstream* m_output;
    string m_accession;
    string m_OrganismName;
    string m_source_qualifiers;
    string m_Comment;
    string m_single_table5_file;
    string m_find_open_read_frame;
    string m_strain;
    string m_url;
    CRef<objects::CDate> m_HoldUntilPublish;
    string F;
    string A;
    string C;
    string V;
    bool   J;
    bool   I;
    string G;
    bool   R;
    bool   S;
    string Q;
    bool   U;
    bool   L;
    bool   W;
    bool   K;
    string c;
    string ZOutFile;
    string zOufFile;
    string X;
    string M;
    string l;
    string m;
    string m_single_structure_cmt;
    int    m_ProjectVersionNumber;
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
    bool   m_fcs_trim;
    bool   m_avoid_submit_block;


    CRef<objects::CSeq_descr>  m_descriptors;

    //string conffile;

/////////////
    CTable2AsnContext();
    ~CTable2AsnContext();

    void AddUserTrack(objects::CSeq_descr& SD, const string& type, const string& label, const string& data) const;
    void SetOrganismData(objects::CSeq_descr& SD, int genome_code, const string& taxname, int taxid, const string& strain) const;
    void ApplySourceQualifiers(CSerialObject& obj, const string& src_qualifiers) const;

    static
    objects::CUser_object& SetUserObject(objects::CSeq_descr& descr, const string& type);
    static
    objects::CBioSource& SetBioSource_old(objects::CSeq_descr& SD);
    bool ApplyCreateDate(objects::CSeq_entry& entry) const;
    void ApplyUpdateDate(objects::CSeq_entry& entry) const;
    void ApplyAccession(objects::CSeq_entry& entry) const;
    void HandleGaps(objects::CSeq_entry& entry) const;
    CRef<CSerialObject> 
        CreateSubmitFromTemplate(CRef<objects::CSeq_entry> object) const;
    CRef<CSerialObject>
        CreateSeqEntryFromTemplate(CRef<objects::CSeq_entry> object) const;

    void MergeSeqDescr(objects::CSeq_descr& dest, const objects::CSeq_descr& src) const;
    void MergeWithTemplate(objects::CSeq_entry& entry) const;

    CRef<objects::CSeq_submit> m_submit_template;
    CRef<objects::CSeq_entry>  m_entry_template;
    objects::IMessageListener* m_logger;
private:
};


END_NCBI_SCOPE


#endif
