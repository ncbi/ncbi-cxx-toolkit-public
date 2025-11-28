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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_signal.hpp>

#include <util/static_map.hpp>
#include <util/stream_source.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/taxon1/local_taxon.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/general/Object_id.hpp>

#ifdef HAVE_NCBI_VDB
#  include <sra/readers/sra/csraread.hpp>
#endif

#include <objtools/readers/fasta.hpp>
#include <objtools/readers/message_listener.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/data_loaders/asn_cache/Cache_blob.hpp>
#include <objtools/data_loaders/asn_cache/asn_index.hpp>
#include <objtools/data_loaders/asn_cache/chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/seq_id_chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_util.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

typedef SStaticPair<const char*, const CMolInfo::EBiomol> TBiomolTypeKey;

static const TBiomolTypeKey db_biomol_type_name_to_enum[] = {
    { "crna",            CMolInfo::eBiomol_cRNA                },
    { "genomic",         CMolInfo::eBiomol_genomic             },
    { "genomic-mrna",    CMolInfo::eBiomol_genomic_mRNA        },
    { "mrna",            CMolInfo::eBiomol_mRNA                },
    { "ncrna",           CMolInfo::eBiomol_ncRNA               },
    { "other",           CMolInfo::eBiomol_other               },
    { "other-genetic",   CMolInfo::eBiomol_other_genetic       },
    { "peptide",         CMolInfo::eBiomol_peptide             },
    { "pre-rna",         CMolInfo::eBiomol_pre_RNA             },
    { "rrna",            CMolInfo::eBiomol_rRNA                },
    { "scrna",           CMolInfo::eBiomol_scRNA               },
    { "snorna",          CMolInfo::eBiomol_snoRNA              },
    { "snrna",           CMolInfo::eBiomol_snRNA               },
    { "tmrna",           CMolInfo::eBiomol_tmRNA               },
    { "transcribed-rna", CMolInfo::eBiomol_transcribed_RNA     },
    { "trna",            CMolInfo::eBiomol_tRNA                }
};

typedef CStaticPairArrayMap<const char*, const CMolInfo::EBiomol, PCase> TBiomolTypeMap;
DEFINE_STATIC_ARRAY_MAP(TBiomolTypeMap, sm_BiomolTypes, db_biomol_type_name_to_enum);


typedef SStaticPair<const char*, const CBioSource::EGenome> TGenomeTypeKey;

static const TGenomeTypeKey db_genome_type_name_to_enum[] = {
    { "chromosome",    CBioSource::eGenome_chromosome    },
    { "genomic",       CBioSource::eGenome_genomic       },
    { "mitochondrion", CBioSource::eGenome_mitochondrion },
    { "plasmid",       CBioSource::eGenome_plasmid       },
    { "plastid",       CBioSource::eGenome_plastid       }
};

typedef CStaticPairArrayMap<const char*, const CBioSource::EGenome, PCase> TGenomeTypeMap;
DEFINE_STATIC_ARRAY_MAP(TGenomeTypeMap, sm_GenomeTypes, db_genome_type_name_to_enum);


typedef SStaticPair<const char*, const CSeq_inst::EMol> TInstMolTypeKey;

static const TInstMolTypeKey db_inst_mol_type_name_to_enum[] = {
    { "aa",    CSeq_inst::eMol_aa    },
    { "dna",   CSeq_inst::eMol_dna   },
    { "na",    CSeq_inst::eMol_na    },
    { "other", CSeq_inst::eMol_other },
    { "rna",   CSeq_inst::eMol_rna   }
};

typedef CStaticPairArrayMap<const char*, const CSeq_inst::EMol, PCase> TInstMolTypeMap;
DEFINE_STATIC_ARRAY_MAP(TInstMolTypeMap, sm_InstMolTypes, db_inst_mol_type_name_to_enum);



/////////////////////////////////////////////////////////////////////////////
//  CPrimeCacheApplication::


class CPrimeCacheApplication : public CNcbiApplication
{
public:
    CPrimeCacheApplication()
        : m_MainIndex(CAsnIndex::e_main)
        , m_SeqIdIndex(CAsnIndex::e_seq_id)
        , m_Genome (CBioSource::eGenome_unknown)
        , m_ExtractDelta(false)
        , m_MaxDeltaLevel(UINT_MAX)
    {
    }
    
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    void x_Read_Ids(CNcbiIstream& istr,
                    set<CSeq_id_Handle> &ids);

    void x_Process_Ids(const set<CSeq_id_Handle> &ids,
                       CNcbiOstream& ostr_seqids,
                       unsigned delta_level,
                       size_t count);

#ifdef HAVE_NCBI_VDB
    void x_Process_SRA(CNcbiIstream& istr,
                       CNcbiOstream& ostr_seqids);

    void x_Process_SRA_ids(CNcbiIstream& istr,
                           CNcbiOstream& ostr_seqids);

    void x_Process_SRA(CCSraShortReadIterator iter,
                       CNcbiOstream& ostr_seqids);
#endif

    void x_Process_Fasta(CNcbiIstream& istr,
                         CNcbiOstream& ostr_seqids);

    void x_Process_SeqEntry(CNcbiIstream& istr,
                            CNcbiOstream& ostr_seqids,
                            ESerialDataFormat serial_fmt,
                            set<CSeq_id_Handle> &delta_ids,
                            size_t &count);

    void x_ExtractAndIndex(const CSeq_entry& entry,
                           CAsnIndex::TTimestamp timestamp,
                           CAsnIndex::TChunkId chunk_id,
                           CAsnIndex::TOffset offset,
                           CAsnIndex::TSize size);
   
    bool x_StripSeqEntry(CScope& scope, CSeq_entry& entry, set<CSeq_id_Handle>& trimmed_bioseqs);

    // x_CacheSeqEntry: cache group of sequences packaged together
    // as a single blob. 
    void x_CacheSeqEntry(CNcbiIstream& istr,
                         CNcbiOstream& ostr_seqids,
                         ESerialDataFormat serial_fmt,
                         set<CSeq_id_Handle> &delta_ids,
                         size_t &count);

    // Split group of sequences packaged together
    // and cache each sequence separately from the others.
    void x_SplitAndCacheSeqEntry(CNcbiIstream& istr,
                                 CNcbiOstream& ostr_seqids,
                                 ESerialDataFormat serial_fmt);

    void x_ExtractDelta(CBioseq_Handle         bsh,
                     set<CSeq_id_Handle>& delta_ids);
    void x_UpsertDescriptor(list<CRef<CSeqdesc> >& descs, CRef<CSeqdesc> new_desc);

    class CCacheBioseq
    {
        CPrimeCacheApplication* parent_; 
        CNcbiOstream* ostr_seqids_;
        CRef<CObjectManager> om_;
        time_t timestamp_;
        unsigned int count_;
    public:
        CCacheBioseq(CPrimeCacheApplication* p,
                     CNcbiOstream* ostr);
        void operator () (CBioseq& bseq);
    };
    friend class CCacheBioseq;
    
private: // data 
    struct SOrgData {
        CRef<CSeqdesc> biosource;
        CRef<COrg_ref> orgref;
    };

    string m_CachePath;
    CChunkFile m_MainChunk;
    CSeqIdChunkFile m_SeqIdChunk;
    CAsnIndex  m_MainIndex;
    CAsnIndex  m_SeqIdIndex;
    CSeq_inst::EMol m_InstMol;
    CRef<CSeqdesc> m_MolInfo;
    map<TTaxId, SOrgData> m_Orgs;
    map<string, TTaxId> m_SequenceTaxids;
    map<string, string> m_SequenceUniprotSources;
    CBioSource::EGenome m_Genome ;
    list< CRef<CSeqdesc> > m_other_descs;
    sequence::EGetIdType m_id_type;
    set<CSeq_inst::EMol> m_StripInstMol;
    bool m_ExtractDelta;
    unsigned m_MaxDeltaLevel;
    set<CSeq_id_Handle> m_CachedIds;
    set<CSeq_id_Handle> m_PreviousExecutionIds;
    set<string> m_PreviousExecutionRuns;
};

template <typename T, typename Consumer>
class CObjectEnum : public CSkipObjectHook
{
public:
    explicit CObjectEnum(Consumer consumer)
    : m_Consumer(consumer)
    {
    }

    void SkipObject(CObjectIStream& istr,
                    const CObjectTypeInfo& info)
    {
        T record;
        istr.ReadObject(&record, info.GetTypeInfo());
        m_Consumer(record);
    }

private:
    CObjectEnum();
    CObjectEnum(const CObjectEnum&);
    CObjectEnum& operator=(const CObjectEnum&);

    Consumer m_Consumer;
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CPrimeCacheApplication::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Seq-id-to-ASN-cache converter");

    arg_desc->AddDefaultKey("i", "InputFile",
                            "FASTA file to process",
                            CArgDescriptions::eInputFile,
                            "-");
    arg_desc->AddOptionalKey("input-manifest", "Manifest",
                             "Manifest file listing FASTA files to process, "
                             "one per line",
                             CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("submit-block-template", "Manifest", 
                             "Manifest file with template",
                             CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey("ifmt", "InputFormat",
                            "Format of input data",
                            CArgDescriptions::eString,
                            "ids");

    arg_desc->SetConstraint("ifmt",
                            &(*new CArgAllow_Strings,
                              "ids", "fasta",
#ifdef HAVE_NCBI_VDB
                              "csra",
                              "csra-ids",
#endif
                              "asnb-seq-entry",
                              "asn-seq-entry"));

    arg_desc->AddOptionalKey("taxid", "Taxid",
                             "Taxid of input FASTA sequences",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("taxid-table", "TaxidTable",
                             "Table of taxids for individual sequences",
                             CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("taxid-table-manifest", "TaxidTableManifest",
                             "Manifest of taxid tables",
                             CArgDescriptions::eInputFile);
    arg_desc->SetDependency("taxid-table-manifest",
                            CArgDescriptions::eExcludes, "taxid-table");
    arg_desc->AddDefaultKey("taxid-column", "TaxidColumn",
                            "column in taxid table with taxid",
                            CArgDescriptions::eInteger, "2");

    CLocalTaxon::AddArguments(*arg_desc);

    arg_desc->AddOptionalKey("molinfo", "Molinfo",
                             "Type of molecule that sequences represent",
                             CArgDescriptions::eString);

    CArgAllow_Strings *molinfo_options = new CArgAllow_Strings;
    ITERATE (TBiomolTypeMap, it, sm_BiomolTypes) {
        molinfo_options->Allow(it->first);
    }
    arg_desc->SetConstraint("molinfo", molinfo_options);

    arg_desc->AddOptionalKey("biosource", "Biosource",
                             "genome source of sequences",
                             CArgDescriptions::eString);

    arg_desc->SetDependency("biosource", CArgDescriptions::eExcludes,
                            "taxid-table");
    arg_desc->SetDependency("biosource", CArgDescriptions::eExcludes,
                            "taxid-table-manifest");
    arg_desc->SetDependency("submit-block-template", CArgDescriptions::eExcludes,
                            "taxid-table");
    arg_desc->SetDependency("submit-block-template", CArgDescriptions::eExcludes,
                            "taxid-table-manifest");


    CArgAllow_Strings *biosource_options = new CArgAllow_Strings;
    ITERATE (TGenomeTypeMap, it, sm_GenomeTypes) {
        biosource_options->Allow(it->first);
    }
    arg_desc->SetConstraint("biosource", biosource_options);

    arg_desc->AddDefaultKey("inst-mol", "InstMol",
                             "Value for Seq.inst.mol",
                             CArgDescriptions::eString, "na");

    CArgAllow_Strings *inst_mol_options = new CArgAllow_Strings;
    ITERATE (TInstMolTypeMap, it, sm_InstMolTypes) {
        inst_mol_options->Allow(it->first);
    }
    arg_desc->SetConstraint("inst-mol", inst_mol_options);

    arg_desc->AddOptionalKey("uniprot-source-table", "UniprotSourceTable",
                             "Table of uniprot source for individual sequences",
                             CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("uniprot-source-table-manifest",
                             "UniprotSourceTableManifest",
                             "Manifest of uniprot source tables",
                             CArgDescriptions::eInputFile);
    arg_desc->SetDependency("uniprot-source-table-manifest",
                          CArgDescriptions::eExcludes, "uniprot-source-table");
    arg_desc->AddDefaultKey("uniprot-source-column", "UniprotSourceColumn",
                          "column in uniprot source table with uniprot source",
                          CArgDescriptions::eInteger, "2");

    arg_desc->AddKey("cache", "OutputFile",
                     "Path to the cache directory",
                     CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("oseq-ids", "OutputFile",
                            "Seq-ids that were added to the cache",
                            CArgDescriptions::eOutputFile,
                            "-");

    arg_desc->AddDefaultKey("seq-id-type", "SeqIdType",
                            "If sequence has several seq-ids, which one to choose",
                            CArgDescriptions::eString, "canonical");
    arg_desc->SetConstraint("seq-id-type",
                            &(*new CArgAllow_Strings,
                              "canonical",
                              "best"));

    arg_desc->AddFlag("no-title",
                      "For FASTA input, don't put a title on the Bioseq");

    arg_desc->AddOptionalKey("max-fasta-id", "MaxFastaIDLength",
                             "For FASTA input, maximum ID size, overriding "
                             "CSeq_id-defined limits",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("id-prefix", "FASTAIdPrefix",
                      "For FASTA input with local ids, add this prefix to each id",
                      CArgDescriptions::eString);

    arg_desc->AddOptionalKey("strip-annots-and-inst-mol", "StripAnnotsAndInstMol",
                             "Comma-separated list of molecule classes of instances - Seq.inst.mol - to strip.",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("split-sequences",
                      "Split group of sequences packaged together. Applicable to asn(b)-seq-entry format.");

    arg_desc->AddFlag("extract-delta",
                      "Extract and index delta-seq far-pointers");
    arg_desc->SetDependency("split-sequences",
                            CArgDescriptions::eExcludes, "extract-delta");

    arg_desc->AddOptionalKey("delta-level", "RecursionLevel",
                             "Number of levels to descend when retrieving "
                             "items in delta sequences",
                             CArgDescriptions::eInteger);
    arg_desc->SetDependency("delta-level",
                            CArgDescriptions::eRequires, "extract-delta");

    arg_desc->AddFlag("resume", "Resume interrupted previous execution");

    arg_desc->AddFlag("non-exclusive",
                      "Can run this cache process in parallel with other "
                      "tasks; use this if writing to a dedicated cache rather "
                      "than the build's standard cache. Ignored by "
                      "application, but provides information to action node");

    // Setup arg.descriptions for this application
    arg_desc->SetCurrentGroup("Default application arguments");
    SetupArgDescriptions(arg_desc.release());
}



void CPrimeCacheApplication::x_ExtractAndIndex(const CSeq_entry&      entry,
                                             CAsnIndex::TTimestamp  timestamp,
                                             CAsnIndex::TChunkId    chunk_id,
                                             CAsnIndex::TOffset     offset,
                                             CAsnIndex::TSize       size)
{
    if (entry.IsSet()) {
        ITERATE (CSeq_entry::TSet::TSeq_set, iter,
                 entry.GetSet().GetSeq_set()) {
            x_ExtractAndIndex(**iter, timestamp, chunk_id, offset, size);
        }
    } else if (entry.IsSeq()) {
        const objects::CBioseq& bioseq = entry.GetSeq();
        IndexABioseq( bioseq, m_MainIndex, timestamp, chunk_id, offset, size );
        Int8 seq_id_offset = m_SeqIdChunk.GetOffset();
        m_SeqIdChunk.Write( bioseq.GetId() );
        IndexABioseq( bioseq, m_SeqIdIndex, timestamp, 0,
                      seq_id_offset, m_SeqIdChunk.GetOffset() - seq_id_offset );
        ITERATE (CBioseq::TId, id_it, bioseq.GetId()) {
            m_CachedIds.insert(CSeq_id_Handle::GetHandle(**id_it));
        }
    }
}

void CPrimeCacheApplication::x_UpsertDescriptor(list<CRef<CSeqdesc> >& descs, CRef<CSeqdesc> new_desc)
{
    bool updated = false;
    // some descriptors must be singletons some not
    for(auto& orig_desc: descs) {
        if(new_desc->Which() != orig_desc->Which()  ) continue;
        switch ( orig_desc->Which() ) {
            case CSeqdesc::e_Source:
            case CSeqdesc::e_Molinfo:
            case CSeqdesc::e_Name:
            case CSeqdesc::e_Title:
            case CSeqdesc::e_Pir:
            case CSeqdesc::e_Genbank:
            case CSeqdesc::e_Sp:
            case CSeqdesc::e_Embl:
            case CSeqdesc::e_Create_date:
            case CSeqdesc::e_Update_date:
            case CSeqdesc::e_Prf:
            case CSeqdesc::e_Pdb:
                updated = true;
                orig_desc->Assign(*new_desc);
                break;
            default:
                break;
        }
    }
    if(!updated) {
        descs.push_back(new_desc);
    }
}

void CPrimeCacheApplication::x_Process_Fasta(CNcbiIstream& istr,
                                             CNcbiOstream& ostr_seqids)
{
    CRef<CObjectManager> om(CObjectManager::GetInstance());

    time_t timestamp = CTime(CTime::eCurrent).GetTimeT();
    CStopWatch sw;
    sw.Start();
    size_t count = 0;
    CFastaReader::TFlags flags =
        CFastaReader::fRequireID |
        CFastaReader::fForceType |
        CFastaReader::fNoUserObjs |
        CFastaReader::fDisableParseRange |
        CFastaReader::fAddMods;
    switch(m_InstMol) {
    case CSeq_inst::eMol_aa:
        flags |= CFastaReader::fAssumeProt;
        break;
    default:
        flags |= CFastaReader::fAssumeNuc;
        break;
    }
    CFastaReader reader(istr, flags);
    if (GetArgs()["max-fasta-id"]) {
        reader.SetMaxIDLength(GetArgs()["max-fasta-id"].AsInteger());
    }
    objects::CGPipeMessageListener messageListener;
    while ( !reader.AtEOF() ) {

        if (CSignal::IsSignaled()) {
            ostr_seqids << "#Clean wrapup\n";
            NCBI_THROW(CException, eUnknown,
                       "trapped signal, exiting");
        }

        CRef<CSeq_entry> entry = reader.ReadOneSeq(&messageListener);

        if (GetArgs()["id-prefix"]) {
            NON_CONST_ITERATE (CBioseq::TId, id_it, entry->SetSeq().SetId()) {
                if ((*id_it)->IsLocal()) {
                    if ((*id_it)->GetLocal().IsStr()) {
                        (*id_it)->SetLocal().SetStr()
                            .insert(0, GetArgs()["id-prefix"].AsString());
                    } else {
                        string str_id = NStr::NumericToString(
                                            (*id_it)->GetLocal().GetId());
                        (*id_it)->SetLocal().SetStr(
                             GetArgs()["id-prefix"].AsString() + str_id);
                    }
                }
            }
        }

        // extract canonical ID
        CSeq_id_Handle idh = sequence::GetId(entry->GetSeq(), m_id_type);
        if (m_PreviousExecutionIds.count(idh)) {
            // This is a resumption of a previous task, and we already
            // cached this sequence
            continue;
        }

        TTaxId taxid = m_SequenceTaxids.count(idh.AsString())
                    ? m_SequenceTaxids[idh.AsString()]
                    : (m_SequenceTaxids.count("")
                    ? m_SequenceTaxids[""] : ZERO_TAX_ID);
        SOrgData &org_data = m_Orgs[taxid];

        entry->SetSeq().SetInst().SetMol(m_InstMol);
        CSeq_descr::Tdata& descs  = entry->SetSeq().SetDescr().Set();
        bool molinfo_found=false;
        bool source_found=false;
        for(CRef<CSeqdesc>& desc: descs) {
            switch ( desc->Which() ) {
                case CSeqdesc::e_Molinfo:
                    molinfo_found=true;
                    if (m_MolInfo) {
                        if( !      desc->GetMolinfo().IsSetCompleteness() 
                           && m_MolInfo->GetMolinfo().IsSetCompleteness() ) {
                            desc->SetMolinfo().SetCompleteness(  m_MolInfo->GetMolinfo().GetCompleteness() );
                        } 
                        if( !      desc->GetMolinfo().IsSetBiomol() 
                           && m_MolInfo->GetMolinfo().IsSetBiomol() ) {
                            desc->SetMolinfo().SetBiomol(  m_MolInfo->GetMolinfo().GetBiomol() );
                        } 
                    }
                    break;
                case CSeqdesc::e_Source:
                    source_found=true;
                    if(m_Genome != CBioSource::eGenome_unknown && 
                        ( 
                            ! desc->GetSource().IsSetGenome() || desc->GetSource().GetGenome() == CBioSource::eGenome_unknown 
                        ) 
                      ) {
                            desc->SetSource().SetGenome(m_Genome );
                    }
                    if(org_data.orgref &&
                        ( 
                            ! desc->GetSource().IsSetOrg() || ! desc->GetSource().GetOrg().IsSetOrgname()
                        )
                       ) {
                        desc->SetSource().SetOrg().Assign(*org_data.orgref);
                    }
                    break;
                default:
                    break;
            }
        }
        if(!molinfo_found && m_MolInfo) {
            descs.push_back(m_MolInfo);
        }
        if(!source_found && org_data.biosource) {
            descs.push_back(org_data.biosource);
        }
            
        if (m_other_descs.size()>0) {
            NON_CONST_ITERATE(list<CRef<CSeqdesc> >, desc, m_other_descs) {
                x_UpsertDescriptor(descs, *desc);
            }
        }

        if (GetArgs()["no-title"]) {
            NON_CONST_ITERATE (list<CRef<CSeqdesc> >, desc, descs) {
                if ((*desc)->IsTitle()) {
                    descs.erase(desc);
                    break;
                }
            }
        }

        if (m_SequenceUniprotSources.count(idh.AsString())) {
            CRef<CSeqdesc> uniprot_source_comment(new CSeqdesc);
            uniprot_source_comment->SetComment("Uniprot Source: "
                    + m_SequenceUniprotSources[idh.AsString()]);
            descs.push_back(uniprot_source_comment);
        }

        if (entry->IsSetDescr() && entry->GetDescr().Get().empty()) {
            entry->SetSeq().ResetDescr();
        }
        CCache_blob blob;
        blob.SetTimestamp(timestamp);
        blob.Pack(*entry);

        m_MainChunk.OpenForWrite(m_CachePath);
        size_t offset = m_MainChunk.GetOffset();
        m_MainChunk.Write(blob);
        size_t size = m_MainChunk.GetOffset() - offset;
        Uint4 chunk_id = m_MainChunk.GetChunkSerialNum();

        entry->Parentize();
        x_ExtractAndIndex(*entry, timestamp, chunk_id, offset, size);

        ostr_seqids << idh << endl;

        ++count;
        if (count % 100000 == 0) {
            LOG_POST(Error << "  processed " << count << " entries...");
        }
    }

    LOG_POST(Error << "done, dumped " << count << " items");

}

#ifdef HAVE_NCBI_VDB
void CPrimeCacheApplication::x_Process_SRA(CCSraShortReadIterator iter,
                                           CNcbiOstream& ostr_seqids)
{
    CRef<CBioseq> bs = iter.GetShortBioseq();
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*bs->GetFirstId());
    if (m_PreviousExecutionIds.count(idh)) {
        // This is a resumption of a previous task, and we already
        // cached this read
        return;
    }
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(*bs);
    entry->SetSeq().SetInst().SetMol(m_InstMol);

    if (m_MolInfo) {
        entry->SetSeq().SetDescr().Set().push_back(m_MolInfo);
    }
    if (m_Orgs.begin()->second.biosource) {
        entry->SetSeq().SetDescr().Set().push_back(
            m_Orgs.begin()->second.biosource);
    }
    if (m_other_descs.size()>0) {
        NON_CONST_ITERATE(list<CRef<CSeqdesc> >, desc, m_other_descs) {
            entry->SetSeq().SetDescr().Set().push_back(*desc); 
        }
    }

    if (CSignal::IsSignaled()) {
        ostr_seqids << "#Clean wrapup\n";
        NCBI_THROW(CException, eUnknown,
                   "trapped signal, exiting");
    }

    time_t timestamp = CTime(CTime::eCurrent).GetTimeT();
    CCache_blob blob;
    blob.SetTimestamp(timestamp);
    blob.Pack(*entry);

    m_MainChunk.OpenForWrite(m_CachePath);
    size_t offset = m_MainChunk.GetOffset();
    m_MainChunk.Write(blob);
    size_t size = m_MainChunk.GetOffset() - offset;
    Uint4 chunk_id = m_MainChunk.GetChunkSerialNum();

    entry->Parentize();
    x_ExtractAndIndex(*entry, timestamp, chunk_id, offset, size);
    ostr_seqids << idh << endl;
}

void CPrimeCacheApplication::x_Process_SRA_ids(CNcbiIstream& istr,
                                               CNcbiOstream& ostr_seqids)
{
    CStopWatch sw;
    sw.Start();
    size_t count = 0;

    CVDBMgr mgr;
    CCSraDb sra_db;
    CCSraShortReadIterator iter;
    string acc, seq_id;
    while (NcbiGetlineEOL(istr, seq_id)) {
        NStr::TruncateSpacesInPlace(seq_id);
        if (seq_id.empty()  ||  seq_id[0] == '#') {
            continue;
        }
        vector<string> id_tokens;
        NStr::Split(seq_id.substr(8), ".", id_tokens);
        if (!NStr::StartsWith(seq_id, "gnl|SRA|") || id_tokens.size() != 3) {
          NCBI_THROW(CException, eUnknown, "Can't parse SRA seq id " + seq_id);
        }
        if (id_tokens[0] != acc) {
            /// Starting new SRA run
            if (!acc.empty()) {
                ostr_seqids << "#Completed run " << acc << endl;
            }
            acc = id_tokens[0];
            sra_db = CCSraDb(mgr, acc);
            iter = CCSraShortReadIterator(sra_db);
        }
	iter.Select(NStr::StringToInt8(id_tokens[1]),
                    NStr::StringToUInt(id_tokens[2]));
        x_Process_SRA(iter, ostr_seqids);
        ++count;
        if (count % 100000 == 0) {
            LOG_POST(Error << "  processed " << count << " reads...");
        }
    }
    if (!acc.empty()) {
        ostr_seqids << "#Completed run " << acc << endl;
    }

    LOG_POST(Error << "done, dumped " << count << " items");
}

void CPrimeCacheApplication::x_Process_SRA(CNcbiIstream& istr,
                                           CNcbiOstream& ostr_seqids)
{
    CStopWatch sw;
    sw.Start();
    size_t count = 0;

    string acc;
    while (NcbiGetlineEOL(istr, acc)) {
        NStr::TruncateSpacesInPlace(acc);
        if (acc.empty()  ||  acc[0] == '#' || m_PreviousExecutionRuns.count(acc))
        {
            continue;
        }

        CVDBMgr mgr;
        CCSraDb sra_db(mgr, acc);
        CCSraShortReadIterator iter(sra_db);

        for ( ;  iter;  ++iter) {
            x_Process_SRA(iter, ostr_seqids);
            ++count;
            if (count % 100000 == 0) {
                LOG_POST(Error << "  processed " << count << " reads...");
            }
        }
        ostr_seqids << "#Completed run " << acc << endl;
    }

    LOG_POST(Error << "done, dumped " << count << " items");

}
#endif

void CPrimeCacheApplication::x_Process_SeqEntry(CNcbiIstream& istr,
                                                CNcbiOstream& ostr_seqids,
                                                ESerialDataFormat serial_fmt,
                                                set<CSeq_id_Handle> &delta_ids,
                                                size_t &count)
{
    const CArgs& args = GetArgs();
 
    if ( args["split-sequences"] ) {
        x_SplitAndCacheSeqEntry(istr, ostr_seqids, serial_fmt);
    }
    else {
        x_CacheSeqEntry(istr, ostr_seqids, serial_fmt, delta_ids, count);
    }
}
void CPrimeCacheApplication::x_SplitAndCacheSeqEntry(CNcbiIstream& istr,
                                                     CNcbiOstream& ostr_seqids,
                                                     ESerialDataFormat serial_fmt)
{
    CPrimeCacheApplication::CCacheBioseq cache_bioseq(this, &ostr_seqids);
    unique_ptr<CObjectIStream> is(CObjectIStream::Open(serial_fmt, istr));
 
    CObjectTypeInfo(CType<CBioseq>())
        .SetLocalSkipHook(*is, new CObjectEnum<CBioseq, CPrimeCacheApplication::CCacheBioseq>(cache_bioseq));

    while ( !is->EndOfData() ) {
        if (CSignal::IsSignaled()) {
            NCBI_THROW(CException, eUnknown,
                       "trapped signal, exiting");
        }
        is->Skip(CType<CSeq_entry>());
    }  
    is->ResetLocalHooks();
}

void CPrimeCacheApplication::x_CacheSeqEntry(CNcbiIstream& istr,
                                             CNcbiOstream& ostr_seqids,
                                             ESerialDataFormat serial_fmt,
                                             set<CSeq_id_Handle> &delta_ids,
                                             size_t &count)
{
    CRef<CObjectManager> om(CObjectManager::GetInstance());

    time_t timestamp = CTime(CTime::eCurrent).GetTimeT();
    CStopWatch sw;
    sw.Start();

    unique_ptr<CObjectIStream> is(CObjectIStream::Open(serial_fmt, istr));
    while ( !is->EndOfData() ) {

        if (CSignal::IsSignaled()) {
            NCBI_THROW(CException, eUnknown,
                       "trapped signal, exiting");
        }

        CRef<CSeq_entry> entry(new CSeq_entry);
        *is >> *entry;

        // Private scope that uses no data loaders.
        CScope scope(*om);
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
        
        // Trim seq-entry of annotations and instances of Bioseq of specified classes.
        // Store ids of trimmed instances of type Bioseq in the @trimmed_bioseqs.
        set<CSeq_id_Handle> trimmed_bioseqs;
        if (!m_StripInstMol.empty()) {
            if (false == x_StripSeqEntry(scope, *entry, trimmed_bioseqs)) {
                return;
            }
        }

        CCache_blob blob;
        blob.SetTimestamp(timestamp);
        blob.Pack(*entry);

        m_MainChunk.OpenForWrite(m_CachePath);
        size_t offset = m_MainChunk.GetOffset();
        m_MainChunk.Write(blob);
        size_t size = m_MainChunk.GetOffset() - offset;
        Uint4 chunk_id = m_MainChunk.GetChunkSerialNum();

        entry->Parentize();
        x_ExtractAndIndex(*entry, timestamp, chunk_id, offset, size);

        // extract canonical IDs
        // note that we do this in a private scope and use no data loaders
        for (CBioseq_CI bioseq_it(seh);  bioseq_it;  ++bioseq_it) {
            CSeq_id_Handle idh = sequence::GetId(*bioseq_it, m_id_type);
            if ( trimmed_bioseqs.empty() || !trimmed_bioseqs.count(idh) ) {
                ostr_seqids << idh << '\n';
                if (m_ExtractDelta) {
                     x_ExtractDelta(*bioseq_it, delta_ids);
                }
            }
        }

        ++count;
        if (count % 100000 == 0) {
            LOG_POST(Error << "Cache Seq-entry: processed " << count << " entries...");
        }
    }

    LOG_POST(Error << "Cache Seq-entry: done, cached " << count << " items");
}

void CPrimeCacheApplication::x_Read_Ids(CNcbiIstream& istr,
                                        set<CSeq_id_Handle> &ids)
{
    string line;
    while (NcbiGetlineEOL(istr, line)) {
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()  ||  line[0] == '#') {
            continue;
        }
        ids.insert(CSeq_id_Handle::GetHandle(line));
    }
}

void CPrimeCacheApplication::x_Process_Ids(const set<CSeq_id_Handle> &ids,
                                           CNcbiOstream& ostr_seqids,
                                           unsigned delta_level,
                                           size_t count)
{
    CGBDataLoader::RegisterInObjectManager(*CObjectManager::GetInstance());
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    time_t timestamp = CTime(CTime::eCurrent).GetTimeT();
    CStopWatch sw;
    sw.Start();

    set<CSeq_id_Handle> delta_ids;
    ITERATE (set<CSeq_id_Handle>, id_it, ids) {
        CSeq_id_Handle idh = *id_it;
        if (m_CachedIds.count(idh)) {
            /// ID already cached
            continue;
        }
        CBioseq_Handle bsh = scope.GetBioseqHandle(idh);
        if ( !bsh ) {
            NCBI_THROW(CException, eUnknown,
                       "failed to retrieve sequence for id: " + idh.AsString());
        }

        CSeq_entry_Handle seh = bsh.GetTopLevelEntry();
        CConstRef<CSeq_entry> entry = seh.GetCompleteSeq_entry();
        // Trim seq-entry of annotations and instances of Bioseq of specified classes.
        // Store ids of trimmed instances of type Bioseq in the @trimmed_bioseqs.
        set<CSeq_id_Handle> trimmed_bioseqs;
        if (!m_StripInstMol.empty()) {
            if (false == x_StripSeqEntry(scope, const_cast<CSeq_entry&>(*entry), trimmed_bioseqs)) {
                return;
            }
        }

        CCache_blob blob;
        blob.SetTimestamp(timestamp);
        blob.Pack(*entry);

        m_MainChunk.OpenForWrite(m_CachePath);
        size_t offset = m_MainChunk.GetOffset();
        m_MainChunk.Write(blob);
        size_t size = m_MainChunk.GetOffset() - offset;
        Uint4 chunk_id = m_MainChunk.GetChunkSerialNum();

        x_ExtractAndIndex(*entry, timestamp, chunk_id, offset, size);

        // extract canonical IDs
        for (CBioseq_CI bioseq_it(seh);  bioseq_it;  ++bioseq_it) {
            idh = sequence::GetId(*bioseq_it, m_id_type);
            if ( trimmed_bioseqs.empty() || !trimmed_bioseqs.count(idh) ) {
                if (delta_level == 0) {
                    ostr_seqids << idh << '\n';
                }
                if (m_ExtractDelta) {
                     x_ExtractDelta(*bioseq_it, delta_ids);
                }
            }
        }

        ++count;
        if (count % 100000 == 0) {
            LOG_POST(Error << "  processed " << count << " entries...");
        }
    }

    if (!delta_ids.empty() && delta_level++ < m_MaxDeltaLevel) {
        x_Process_Ids(delta_ids, ostr_seqids, delta_level, count);
    } else {
        LOG_POST(Error << "done, cached " << count << " items");
    }
}

bool CPrimeCacheApplication::x_StripSeqEntry(CScope& scope, CSeq_entry& seq_entry, set<CSeq_id_Handle>& trimmed_bioseqs)
{
    if ( seq_entry.IsSet() ) {
        CBioseq_set& bset = seq_entry.SetSet();
        list<CRef<CSeq_entry> >& coll = bset.SetSeq_set();
        for ( list<CRef<CSeq_entry> >::iterator i = coll.begin(); i != coll.end();  ) {
            if ( false == x_StripSeqEntry(scope, **i, trimmed_bioseqs) ) {
                i = coll.erase(i);
            }
            else {
                ++i;
            }
        }
        
        if ( 0 == bset.GetSeq_set().size() ) {
            return false;
        }
        
        bset.ResetAnnot();
        bset.ResetDescr();
        return true;
    }
    else if ( seq_entry.IsSeq() ) {
        CBioseq const& bioseq = seq_entry.GetSeq();
        if ( bioseq.CanGetInst() ) {
            if (m_StripInstMol.count(bioseq.GetInst().GetMol()) > 0) {
                CBioseq_Handle bsh = scope.GetBioseqHandle(*bioseq.GetFirstId());
                trimmed_bioseqs.insert(sequence::GetId(bsh, m_id_type));
                return false;
            }
        }

        seq_entry.SetSeq().ResetAnnot();
        seq_entry.SetSeq().ResetDescr();
        return true;
    }
    else {  
        return true;
    }
}

CPrimeCacheApplication::CCacheBioseq::CCacheBioseq(CPrimeCacheApplication* p,
                                                   CNcbiOstream* ostr)
: parent_(p),
  ostr_seqids_(ostr),
  om_(CObjectManager::GetInstance()),
  timestamp_(CTime(CTime::eCurrent).GetTimeT()),
  count_(0)
{
}        
 
void CPrimeCacheApplication::CCacheBioseq::operator () (CBioseq & bioseq)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(bioseq);
    CScope scope(*om_);
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    // Trim seq-entry of annotations and instances of Bioseq of specified classes.
    // Store ids of trimmed instances of type Bioseq in the @trimmed_bioseqs.
    set<CSeq_id_Handle> trimmed_bioseqs;
    if (!parent_->m_StripInstMol.empty()) {
        if (false == parent_->x_StripSeqEntry(scope, *entry, trimmed_bioseqs)) {
            return;
        }
    }

    CCache_blob blob;
    blob.SetTimestamp(timestamp_);
    blob.Pack(*entry);

    parent_->m_MainChunk.OpenForWrite(parent_->m_CachePath);
    size_t offset = parent_->m_MainChunk.GetOffset();
    parent_->m_MainChunk.Write(blob);
    size_t size = parent_->m_MainChunk.GetOffset() - offset;
    Uint4 chunk_id = parent_->m_MainChunk.GetChunkSerialNum();

    entry->Parentize();
    parent_->x_ExtractAndIndex(*entry, timestamp_, chunk_id, offset, size);

    // extract canonical IDs
    // note that we do this in a private scope and use no data loaders
    for (CBioseq_CI bioseq_it(seh);  bioseq_it;  ++bioseq_it) {
        CSeq_id_Handle idh = sequence::GetId(*bioseq_it, parent_->m_id_type);
        (*ostr_seqids_) << idh << '\n';
    }
    
    ++count_;
    if (count_ % 100000 == 0) {
        LOG_POST(Error << "  processed " << count_ << " entries...");
    }
}


int CPrimeCacheApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();
    {{
         GetDiagContext().GetRequestContext().GetRequestTimer().Start();
         CDiagContext_Extra extra = GetDiagContext().PrintRequestStart();
     }}

    CSignal::TrapSignals(CSignal::eSignal_HUP |
                         CSignal::eSignal_QUIT |
                         CSignal::eSignal_INT |
                         CSignal::eSignal_TERM);

    string ifmt = args["ifmt"].AsString();

    if ((args["taxid"] || args["taxid-table"] || args["taxid-table-manifest"] ||
         args["molinfo"] || args["biosource"]  || args["submit-block-template"])
        && ifmt != "fasta" && ifmt != "csra" && ifmt != "csra-ids")
    {
        NCBI_THROW(CException, eUnknown,
                   "metadata parameters only allowed with fasta or SRA input");
    }
    if ((args["uniprot-source-table"] || args["uniprot-source-table-manifest"])
        && ifmt != "fasta")
    {
        NCBI_THROW(CException, eUnknown,
                   "uniprot source parameters only allowed with fasta input");
    }
    if (args["resume"] && ifmt != "fasta" && ifmt != "csra"
                       && ifmt != "csra-ids")
    {
        NCBI_THROW(CException, eUnknown,
                   "Resume only supported with fasta or SRA input");
    }

    if (args["taxid"] || args["taxid-table"] || args["taxid-table-manifest"]) {
        if (args["taxid"]) {
            m_SequenceTaxids[""] = TAX_ID_FROM(TIntId, args["taxid"].AsIntId());
        }
        CInputStreamSource taxids_source;
        if (args["taxid-table"]) {
            taxids_source.InitStream(args["taxid-table"].AsInputFile());
        } else if (args["taxid-table-manifest"]) {
            taxids_source.InitManifest(args["taxid-table-manifest"].AsString());
        }
        unsigned col = args["taxid-column"].AsInteger() - 1;
        for (; taxids_source; ++taxids_source) {
            string line;
            while (NcbiGetlineEOL(*taxids_source, line)) {
                if (line.empty() || line[0] == '#') {
                    continue;
                }
                vector<string> tokens;
                NStr::Split(line, "\t", tokens);
                m_SequenceTaxids[CSeq_id_Handle::GetHandle(tokens[0]).AsString()]
                    = TAX_ID_FROM(int, NStr::StringToInt(tokens[col]));
            }
        }

        CLocalTaxon taxon(args);
        for (const pair<const string, TTaxId> &seq_taxid : m_SequenceTaxids)
        {
            TTaxId taxid = seq_taxid.second;
            if (m_Orgs.count(taxid)) {
                continue;
            }

            CConstRef<COrg_ref> ref = taxon.GetOrgRef(taxid);
            if ( !ref ) {
                NCBI_THROW(CException, eUnknown,
                            "failed to find Org-ref for taxid " +
                            NStr::NumericToString(taxid));
            }

            m_Orgs[taxid].biosource.Reset(new CSeqdesc);
            m_Orgs[taxid].biosource->SetSource().SetOrg().Assign(*ref);
            m_Orgs[taxid].orgref.Reset(new COrg_ref);
            m_Orgs[taxid].orgref->Assign(*ref);
        }
    }

    if (args["uniprot-source-table"] || args["uniprot-source-table-manifest"]) {
        unsigned col = args["uniprot-source-column"].AsInteger() - 1;
        CInputStreamSource uniprot_sources_source;
        if (args["uniprot-source-table"]) {
            uniprot_sources_source.InitStream(args["uniprot-source-table"].AsInputFile());
        } else if (args["uniprot-source-table-manifest"]) {
            uniprot_sources_source.InitManifest(args["uniprot-source-table-manifest"].AsString());
        }
        for (; uniprot_sources_source; ++uniprot_sources_source) {
            string line;
            while (NcbiGetlineEOL(*uniprot_sources_source, line)) {
                if (line.empty() || line[0] == '#') {
                    continue;
                }
                vector<string> tokens;
                NStr::Split(line, "\t", tokens);
                m_SequenceUniprotSources[CSeq_id_Handle::GetHandle(tokens[0])
                    . AsString()] = tokens[col];
            }
        }
    }

    if (m_Orgs.empty()) {
        /// No taxid specified in arguments; create catch-all organism entry
        m_Orgs[ZERO_TAX_ID];
    }

    m_InstMol = sm_InstMolTypes.find(args["inst-mol"].AsString().c_str())->second;

    if (args["biosource"]) {
        if (!m_Orgs.begin()->second.biosource) {
            m_Orgs.begin()->second.biosource.Reset(new CSeqdesc);
        }
        m_Orgs.begin()->second.biosource->SetSource().SetGenome(
            sm_GenomeTypes.find(args["biosource"].AsString().c_str())->second);
        m_Genome = sm_GenomeTypes.find(args["biosource"].AsString().c_str())->second;
        
    }

    if (args["molinfo"]) {
        m_MolInfo.Reset(new CSeqdesc);
        m_MolInfo->SetMolinfo().SetBiomol(
            sm_BiomolTypes.find(args["molinfo"].AsString().c_str())->second);
    }
    if (args["submit-block-template"]) {
        CRef<CSubmit_block> submit_block;
        CNcbiIstream& istr_manifest = args["submit-block-template"].AsInputFile();
        unique_ptr<CObjectIStream> is
            (CObjectIStream::Open(eSerial_AsnText, istr_manifest));
        while ( !is->EndOfData() ) {
            if ( !submit_block ) {
                submit_block.Reset(new CSubmit_block);
                *is >> *submit_block;
            }
            else {
                CRef<CSeqdesc> desc(new CSeqdesc);
                *is >> *desc;

                switch (desc->Which()) {
                case CSeqdesc::e_Source:
                    {{
                    SOrgData &org_data = m_Orgs.begin()->second;
                    org_data.biosource = desc;
                    if(!org_data.orgref) {
                        org_data.orgref.Reset(new COrg_ref);
                    }
                    org_data.orgref->Assign(desc->GetSource().GetOrg() );
                    break;
                    }}

                case CSeqdesc::e_Molinfo:
                    m_MolInfo = desc;
                    break;

                default:
                    m_other_descs.push_back(desc);
                    break;
                }
            }
        }
    }

    CRef<CObjectManager> om(CObjectManager::GetInstance());

    {{
         m_CachePath = args["cache"].AsString();
         CDir dir(m_CachePath);
         if ( !dir.Exists() ) {
             dir.CreatePath();
         }
         m_SeqIdChunk.OpenForWrite(m_CachePath);

         m_MainIndex.SetCacheSize(1 * 1024 * 1024 * 1024);
         m_MainIndex.Open(NASNCacheFileName::GetBDBIndex(m_CachePath, CAsnIndex::e_main), CBDB_RawFile::eReadWriteCreate);
         m_SeqIdIndex.SetCacheSize(1 * 1024 * 1024 * 1024);
         m_SeqIdIndex.Open(NASNCacheFileName::GetBDBIndex(m_CachePath, CAsnIndex::e_seq_id), CBDB_RawFile::eReadWriteCreate);
     }}

    bool resuming_from_clean_wrapup = false;
    if (args["resume"]) {
        CFile output_file(args["oseq-ids"].AsString());
        if (!output_file.Exists()) {
            NCBI_THROW(CException, eUnknown,
                       "Can't resums; " + output_file.GetPath() + " not found");
        }
        CNcbiIfstream istr(output_file.GetPath());
        string line;
        set<CSeq_id_Handle> previous_execution_ids;
        set<string> previous_execution_runs;
        while (NcbiGetlineEOL(istr, line)) {
            if (!line.empty() && line[0] != '#') {
                previous_execution_ids.insert(CSeq_id_Handle::GetHandle(line));
            } else if (NStr::StartsWith(line, "#Completed run ")) {
                previous_execution_runs.insert(line.substr(15));
            }
            if ((resuming_from_clean_wrapup = (line == "#Clean wrapup"))) {
                /// Execution ended in a clean wrapup, so the ids and runs that
                /// it completed have been successfully cached
                m_PreviousExecutionIds = previous_execution_ids;
                m_PreviousExecutionRuns = previous_execution_runs;
            }
        }
    }

    CNcbiOstream &ostr = args["oseq-ids"].AsOutputFile(
         resuming_from_clean_wrapup ? CArgValue::fAppend : 0);
    if (!resuming_from_clean_wrapup && !m_PreviousExecutionIds.empty()) {
        /// Had a clean wrapup at some point, but last execution ended
        /// uncleanly; need to re-write output file with those ids that were
        /// cached cleanly, then we'll redo the rest
        ostr << "#interrupted-execution\n";
        for (const string &run : m_PreviousExecutionRuns) {
            ostr << "#Completed run " << run << '\n';
        }
        for (const CSeq_id_Handle &id : m_PreviousExecutionIds) {
            if (id.Which() == CSeq_id::e_General &&
                id.GetSeqId()->GetGeneral().GetTag().IsStr())
            {
                string tag = id.GetSeqId()->GetGeneral().GetTag().GetStr();
                if (m_PreviousExecutionRuns.count(tag.substr(0, tag.find('.'))))
                {
                    continue;
                }
            }
            ostr << id << '\n';
        }
        ostr << "#Clean wrapup\n";
    }
    ostr << "#" << args["seq-id-type"].AsString() << "-id" << endl;
    m_id_type = args["seq-id-type"].AsString() == "canonical"
              ? sequence::eGetId_Canonical : sequence::eGetId_Best;

    if (args["strip-annots-and-inst-mol"]) {
        list<string> mol_types;
        NStr::Split(args["strip-annots-and-inst-mol"].AsString(), string(","), mol_types, 0);
        vector<string> unknown_mols;
        for (list<string>::const_iterator mol = mol_types.cbegin(); mol != mol_types.cend(); ++mol) {
            string key = NStr::TruncateSpaces(*mol);
            TInstMolTypeMap::const_iterator record = sm_InstMolTypes.find(key.c_str());
            if ( record != sm_InstMolTypes.end() ) {
                m_StripInstMol.insert(record->second);
            }
            else {
                unknown_mols.push_back(key);
            }
        }
        
        if (!unknown_mols.empty()) {
            ostringstream oss;
            oss << "Unknown molecule classes: [";
            for (vector<string>::const_iterator i = unknown_mols.cbegin(); i != unknown_mols.end(); ++i ) {
                oss << *i;
            }
            oss << "]. Valid classes: [aa, dna, na, other, rna]";

            NCBI_THROW(CException, eUnknown, oss.str());
        }
    }

    m_ExtractDelta = args["extract-delta"];
    if (args["delta-level"]) {
        m_MaxDeltaLevel = args["delta-level"].AsInteger();
    }

    size_t count = 0;
    set<CSeq_id_Handle> ids;
    if (args["input-manifest"]) {
        CNcbiIstream& istr = args["input-manifest"].AsInputFile();
        string line;
        while (NcbiGetlineEOL(istr, line)) {
            NStr::TruncateSpacesInPlace(line);
            if (line.empty()  ||  line[0] == '#') {
                continue;
            }

            CNcbiIfstream is(line.c_str());
            if (ifmt == "ids") {
                x_Read_Ids(is, ids);
            }
            else if (ifmt == "fasta") {
                x_Process_Fasta(is, ostr);
            }
#ifdef HAVE_NCBI_VDB
            else if (ifmt == "csra") {
                x_Process_SRA(is, ostr);
            }
            else if (ifmt == "csra-ids") {
                x_Process_SRA_ids(istr, ostr);
            }
#endif
            else if (ifmt == "asn-seq-entry") {
                x_Process_SeqEntry(is, ostr, eSerial_AsnText, ids, count);
            }
            else if (ifmt == "asnb-seq-entry") {
                x_Process_SeqEntry(is, ostr, eSerial_AsnBinary, ids, count);
            }
            else {
                NCBI_THROW(CException, eUnknown,
                           "unhandled input format");
            }
        }
    }
    else {
        CNcbiIstream& istr = args["i"].AsInputFile();
        if (ifmt == "ids") {
            x_Read_Ids(istr, ids);
        }
        else if (ifmt == "fasta") {
            x_Process_Fasta(istr, ostr);
        }
#ifdef HAVE_NCBI_VDB
            else if (ifmt == "csra") {
                x_Process_SRA(istr, ostr);
            }
            else if (ifmt == "csra-ids") {
                x_Process_SRA_ids(istr, ostr);
            }
#endif        
        else if (ifmt == "asn-seq-entry") {
            x_Process_SeqEntry(istr, ostr, eSerial_AsnText, ids, count);
        }
        else if (ifmt == "asnb-seq-entry") {
            x_Process_SeqEntry(istr, ostr, eSerial_AsnBinary, ids, count);
        }
        else {
            NCBI_THROW(CException, eUnknown,
                       "unhandled input format");
        }
    }

    if (!ids.empty()) {
        x_Process_Ids(ids, ostr, ifmt == "ids" ? 0 : 1, count);
    }

    GetDiagContext().GetRequestContext().SetRequestStatus(200);
    GetDiagContext().PrintRequestStop();

    return 0;
}

void CPrimeCacheApplication::x_ExtractDelta(CBioseq_Handle         bsh,
                     set<CSeq_id_Handle>& delta_ids)
{
    ///
    /// process any delta-seqs
    ///
    if (bsh.GetInst().IsSetExt()  &&
        bsh.GetInst().GetExt().IsDelta()) {
        ITERATE (CBioseq::TInst::TExt::TDelta::Tdata, iter,
                 bsh.GetInst().GetExt().GetDelta().Get()) {
            const CDelta_seq& seg = **iter;
            CTypeConstIterator<CSeq_id> id_iter(seg);
            for ( ;  id_iter;  ++id_iter) {
                delta_ids.insert
                    (CSeq_id_Handle::GetHandle(*id_iter));
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CPrimeCacheApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CPrimeCacheApplication().AppMain(argc, argv);
}
