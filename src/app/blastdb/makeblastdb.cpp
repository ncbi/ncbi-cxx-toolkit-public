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
 * Author: Christiam Camacho
 *
 */

/** @file makeblastdb.cpp
 * Command line tool to create BLAST databases. This is the successor to
 * formatdb from the C toolkit
 */

#include <ncbi_pch.hpp>
#include <serial/objostrjson.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <corelib/ncbiapp.hpp>

#include <serial/iterator.hpp>
#include <objmgr/util/create_defline.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Seqdesc.hpp>
#include <objects/general/User_object.hpp>

#include <objects/seqfeat/Org_ref.hpp>
#include <objects/blastdb/Blast_db_mask_info.hpp>
#include <objects/blastdb/Blast_mask_list.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_writer/writedb.hpp>
#include <objtools/blast/seqdb_writer/writedb_error.hpp>
#include <util/format_guess.hpp>
#include <util/util_exception.hpp>
#include <util/compress/stream_util.hpp> 
#include <objtools/blast/seqdb_writer/build_db.hpp>

#include <serial/objostrjson.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include "../blast/blast_app_util.hpp"
#include "masked_range_set.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <random>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif /* SKIP_DOXYGEN_PROCESSING */

/// The main application class
class CMakeBlastDBApp : public CNcbiApplication {
public:
    /// Convenience typedef
    typedef CFormatGuess::EFormat TFormat;

    enum ESupportedInputFormats {
        eFasta,
        eBinaryASN,
        eTextASN,
        eBlastDb,
        eUnsupported = 256
    };

    /** @inheritDoc */
    CMakeBlastDBApp()
        : m_LogFile(NULL),
          m_IsModifyMode(false)
    {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
        m_StopWatch.Start();
        if (m_UsageReport.IsEnabled()) {
        	m_UsageReport.AddParam(CBlastUsageReport::eVersion, GetVersion().Print());
        	m_UsageReport.AddParam(CBlastUsageReport::eProgram, (string) "makeblastdb");
        }
    }
    ~CMakeBlastDBApp() {
    	m_UsageReport.AddParam(CBlastUsageReport::eRunTime, m_StopWatch.Elapsed());
    }

private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    void x_AddSequenceData(CNcbiIstream & input, TFormat fmt);

    vector<ESupportedInputFormats>
    x_GuessInputType(const vector<CTempString>& filenames,
                     vector<string>& blastdbs);
    ESupportedInputFormats x_GetUserInputTypeHint(void);
    TFormat x_ConvertToCFormatGuessType(ESupportedInputFormats fmt);
    ESupportedInputFormats x_ConvertToSupportedType(TFormat fmt);
    TFormat x_GuessFileType(CNcbiIstream & input);

    void x_BuildDatabase();

    void x_AddFasta(CNcbiIstream & data);

    void x_AddSeqEntries(CNcbiIstream & data, TFormat fmt);

    void x_ProcessMaskData();

    void x_ProcessInputData(const string & paths, bool is_protein);

    bool x_ShouldParseSeqIds(void);

    void x_VerifyInputFilesType(const vector<CTempString>& filenames,
                                CMakeBlastDBApp::ESupportedInputFormats input_type);

    void x_AddCmdOptions();

    // Data

    CNcbiOstream * m_LogFile;

    CRef<CBuildDatabase> m_DB;

    CRef<CMaskedRangeSet> m_Ranges;

    bool m_IsModifyMode;

    bool m_SkipUnver;
    CBlastUsageReport m_UsageReport;
    CStopWatch m_StopWatch;
};

/// Reads an object defined in a NCBI ASN.1 spec from a stream in multiple
/// formats: binary and text ASN.1 and XML
/// @param file stream to read the object from [in]
/// @param fmt specifies the format in which the object is encoded [in]
/// @param obj on input is an empty CRef<> object, on output it's populated
/// with the object read [in|out]
/// @param msg error message to display if reading fails [in]
template<class TObj>
void s_ReadObject(CNcbiIstream          & file,
                  CFormatGuess::EFormat   fmt,
                  CRef<TObj>            & obj,
                  const string          & msg)
{
    obj.Reset(new TObj);

    switch (fmt) {
    case CFormatGuess::eBinaryASN:
        file >> MSerial_AsnBinary >> *obj;
        break;

    case CFormatGuess::eTextASN:
        file >> MSerial_AsnText >> *obj;
        break;

    default:
        NCBI_THROW(CInvalidDataException, eInvalidInput, string("Unknown encoding for ") + msg);
    }
}

/// Overloaded version of s_ReadObject which uses CFormatGuess to determine
/// the encoding of the object in the file
/// @param file stream to read the object from [in]
/// @param obj on input is an empty CRef<> object, on output it's populated
/// with the object read [in|out]
/// @param msg error message to display if reading fails [in]
template<class TObj>
void s_ReadObject(CNcbiIstream & file,
                  CRef<TObj>    & obj,
                  const string  & msg)
{
    CFormatGuess fg(file);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
    fg.GetFormatHints().DisableAllNonpreferred();
    s_ReadObject(file, fg.GuessFormat(), obj, msg);
}

/// Command line flag to represent the input
static const string kInput("in");
/// Defines token separators when multiple inputs are present
static const string kInputSeparators(" ");
/// Command line flag to represent the output
static const string kOutput("out");

void CMakeBlastDBApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                  "Application to create BLAST databases, version "
                  + CBlastVersion().Print());

    string dflt("Default = input file name provided to -");
    dflt += kInput + " argument";

    arg_desc->SetCurrentGroup("Input options");
    arg_desc->AddDefaultKey(kInput, "input_file",
                            "Input file/database name",
                            CArgDescriptions::eInputFile, "-");
    arg_desc->AddDefaultKey("input_type", "type",
                             "Type of the data specified in input_file",
                             CArgDescriptions::eString, "fasta");
    arg_desc->SetConstraint("input_type", &(*new CArgAllow_Strings,
                                            "fasta", "blastdb",
                                            "asn1_bin",
                                            "asn1_txt"));

    arg_desc->AddKey(kArgDbType, "molecule_type",
                     "Molecule type of target db", CArgDescriptions::eString);
    arg_desc->SetConstraint(kArgDbType, &(*new CArgAllow_Strings,
                                        "nucl", "prot"));

    arg_desc->SetCurrentGroup("Configuration options");
    arg_desc->AddOptionalKey(kArgDbTitle, "database_title",
                             "Title for BLAST database\n" + dflt,
                             CArgDescriptions::eString);

    arg_desc->AddFlag("parse_seqids",
                      "Option to parse seqid for FASTA input if set, for all other input types seqids are parsed automatically", true);

    arg_desc->AddFlag("hash_index",
                      "Create index of sequence hash values.",
                      true);

    arg_desc->AddFlag("randomize",
                      "Randomize input db seqs", true,  CArgDescriptions::fHidden);

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    arg_desc->SetCurrentGroup("Sequence masking options");
    arg_desc->AddOptionalKey("mask_data", "mask_data_files",
                             "Comma-separated list of input files containing "
                             "masking data as produced by NCBI masking "
                             "applications (e.g. dustmasker, segmasker, "
                             "windowmasker)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("mask_id", "mask_algo_ids",
                             "Comma-separated list of strings to uniquely "
                             "identify the masking algorithm",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("mask_desc", "mask_algo_descriptions",
                             "Comma-separated list of free form strings to "
                             "describe the masking algorithm details",
                             CArgDescriptions::eString);

    arg_desc->SetDependency("mask_id", CArgDescriptions::eRequires, "mask_data");
    arg_desc->SetDependency("mask_desc", CArgDescriptions::eRequires, "mask_id");

    arg_desc->AddFlag("gi_mask",
                      "Create GI indexed masking data.", true);
    arg_desc->SetDependency("gi_mask", CArgDescriptions::eExcludes, "mask_id");
    arg_desc->SetDependency("gi_mask", CArgDescriptions::eRequires, "parse_seqids");

    arg_desc->AddOptionalKey("gi_mask_name", "gi_based_mask_names",
                             "Comma-separated list of masking data output files.",
                             CArgDescriptions::eString);
    arg_desc->SetDependency("gi_mask_name", CArgDescriptions::eRequires, "mask_data");
    arg_desc->SetDependency("gi_mask_name", CArgDescriptions::eRequires, "gi_mask");

#endif

    arg_desc->SetCurrentGroup("Output options");
    arg_desc->AddOptionalKey(kOutput, "database_name",
                             "Name of BLAST database to be created\n" + dflt +
                             "Required if multiple file(s)/database(s) are "
                             "provided as input",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("blastdb_version", "version",
                             "Version of BLAST database to be created",
                             CArgDescriptions::eInteger,
                             NStr::NumericToString(static_cast<int>(eBDB_Version5)));
    arg_desc->SetConstraint("blastdb_version",
                            new CArgAllow_Integers(eBDB_Version4, eBDB_Version5));
    arg_desc->AddDefaultKey("max_file_sz", "number_of_bytes",
                            "Maximum file size for BLAST database files",
                            CArgDescriptions::eString, "3GB");
    arg_desc->AddOptionalKey("metadata_output_prefix", "",
    						"Path prefix for location of database files in metadata", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("logfile", "File_Name",
                             "File to which the program log should be redirected",
                             CArgDescriptions::eOutputFile,
                             CArgDescriptions::fAppend);
#if _BLAST_DEBUG
    arg_desc->AddFlag("verbose", "Produce verbose output", true);
    arg_desc->AddFlag("limit_defline", "limit_defline", true);
#endif /* _BLAST_DEBUG */

    arg_desc->SetCurrentGroup("Taxonomy options");
    arg_desc->AddOptionalKey("taxid", "TaxID",
                             "Taxonomy ID to assign to all sequences",
                             CArgDescriptions::eInteger);
    arg_desc->SetConstraint("taxid", new CArgAllowValuesGreaterThanOrEqual(0));
    arg_desc->SetDependency("taxid", CArgDescriptions::eExcludes, "taxid_map");

    arg_desc->AddOptionalKey("taxid_map", "TaxIDMapFile",
             "Text file mapping sequence IDs to taxonomy IDs.\n"
             "Format:<SequenceId> <TaxonomyId><newline>",
             CArgDescriptions::eInputFile);
    arg_desc->SetDependency("taxid_map", CArgDescriptions::eRequires, "parse_seqids");

    arg_desc->AddOptionalKey("oid_masks", "oid_masks",
    		                 "0x01 Exclude Model", CArgDescriptions::eInteger);

    SetupArgDescriptions(arg_desc.release());
}

/// Converts a Uint8 into a string which contains a data size (converse to
/// NStr::StringToUInt8_DataSize)
/// @param v value to convert [in]
/// @param minprec minimum precision [in]
static string Uint8ToString_DataSize(Uint8 v, unsigned minprec = 10)
{
    static string kMods = "KMGTPEZY";

    size_t i(0);
    for(i = 0; i < kMods.size(); i++) {
        if (v < Uint8(minprec)*1024) {
            v /= 1024;
        }
    }

    string rv = NStr::UInt8ToString(v);

    if (i) {
        rv.append(kMods, i, 1);
        rv.append("B");
    }

    return rv;
}

void CMakeBlastDBApp::x_AddFasta(CNcbiIstream & data)
{
    m_DB->AddFasta(data);
}

static TTaxId s_GetTaxId(const CBioseq & bio)
{
    CSeq_entry * p_ptr = bio.GetParentEntry();
    while (p_ptr != NULL)
    {
        if(p_ptr->IsSetDescr())
        {
            ITERATE(CSeq_descr::Tdata, it, p_ptr->GetDescr().Get()) {
                 const CSeqdesc& desc = **it;
                 if(desc.IsSource()) {
                     return desc.GetSource().GetOrg().GetTaxId();
                 }

                 if(desc.IsOrg()) {
                      return desc.GetOrg().GetTaxId();
                 }
            }
        }

        p_ptr = p_ptr->GetParentEntry();
    }
    return ZERO_TAX_ID;
}

static bool s_HasTitle(const CBioseq & bio)
{
    if (! bio.CanGetDescr()) {
        return false;
    }

    ITERATE(list< CRef< CSeqdesc > >, iter, bio.GetDescr().Get()) {
        const CSeqdesc & desc = **iter;

        if (desc.IsTitle()) {
            return true;
        }
    }

    return false;
}

class CSeqEntrySource : public IBioseqSource {
public:
    /// Convenience typedef
    typedef CFormatGuess::EFormat TFormat;

    bool IsUnverified(const CSeq_descr& descr) {

        const string unv("Unverified");

        const CSeq_descr::Tdata& da = descr.Get();
        ITERATE(CSeq_descr::Tdata, da_iter, da) {
            CRef<CSeqdesc> dai = *da_iter;
            if (dai->IsUser()) {
                const CSeqdesc::TUser& du = dai->GetUser();
                if (du.IsSetType()) {
                    const CUser_object::TType& ty = du.GetType();
                    if (ty.IsStr()) {
                        const CObject_id::TStr& str = ty.GetStr();
                        if (NStr::CompareNocase(str, unv) == 0) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;

    }

    CSeqEntrySource(CNcbiIstream & is, TFormat fmt, bool skip_unver)
        :m_objmgr(CObjectManager::GetInstance()),
         m_scope(new CScope(*m_objmgr)),
         m_entry(new CSeq_entry),
         m_SkipUnver(skip_unver)
    {
        char ch = is.peek();

        // Get rid of white spaces
        while (!is.eof()
                &&
                (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')) {
            is.read(&ch, 1);
            ch = is.peek();
        }

        if (is.eof())
            return;

        // If input is a Bioseq_set
        if (ch == 'B' || ch == '0') {
            CRef<CBioseq_set> obj(new CBioseq_set);
            s_ReadObject(is, fmt, obj, "bioseq");
            m_entry->SetSet(*obj);
        } else {
            // If not, it should be a Seq-entry.
            s_ReadObject(is, fmt, m_entry, "bioseq");
        }

        CTypeIterator<CBioseq_set> it_bio_set;
        CTypeIterator<CBioseq> it_bio;

        // Step through Seq-entry, picking out Bioseq-set objects.
        for (it_bio_set = Begin(*m_entry); it_bio_set; ++it_bio_set) {
            // If Bioseq-set is of the 'nuc-prot' class,
            // and it's marked "Unverified", skip ALL of the
            // Bioseq objects it contains.
            if (it_bio_set->GetClass() == CBioseq_set::eClass_nuc_prot) {
                if (it_bio_set->CanGetDescr() && IsUnverified(it_bio_set->GetDescr())) {
                    for (it_bio = Begin(*it_bio_set); it_bio; ++it_bio) {
                        m_bio_skipped.insert(&(*it_bio));
                    }
                }
            }
        }

        // Step through Seq-entry, picking out Bioseq objects.
        for (it_bio = Begin(*m_entry); it_bio; ++it_bio) {
            // If Bioseq is marked as "Unverified", skip it.
            if (it_bio->CanGetDescr() && IsUnverified(it_bio->GetDescr())) {
                // Because m_bio_skipped is an STL set container,
                // inserting an item that's already in the set will leave
                // the set unaltered (i.e. no duplicate items).
                m_bio_skipped.insert(&(*it_bio));
            }
        }

        m_bio = Begin(*m_entry);
        m_entry->Parentize();
        m_scope->AddTopLevelSeqEntry(*m_entry);
    }

    virtual CConstRef<CBioseq> GetNext()
    {
        CConstRef<CBioseq> rv;

        if (m_bio) {

            // If skipping of "unverified" entries is enabled...
            if (m_SkipUnver) {
                // If the the address of the current Bioseq object (*m_bio)
                // is in the skip set, advance to the next Bioseq and
                // try again.
                while (m_bio_skipped.find(&(*m_bio)) != m_bio_skipped.end()) {
                    ++m_bio;    // will be null when incremented past end
                    if (!m_bio) return rv;
                }
            }

            const sequence::CDeflineGenerator::TUserFlags flags = sequence::CDeflineGenerator::fUseAutoDef;
            sequence::CDeflineGenerator gen;
            const string & title = gen.GenerateDefline(*m_bio , *m_scope, flags);
            string old_title;
            if (s_HasTitle(*m_bio)) {
                for (auto& i: m_bio->SetDescr().Set()) {
                    if (i->IsTitle()) {
                        old_title = i->GetTitle();
                        i->SetTitle(title);
                    }
                }
            } else {
                CRef<CSeqdesc> des(new CSeqdesc);
                des->SetTitle(title);
                CSeq_descr& desr(m_bio->SetDescr());
                desr.Set().push_back(des);
            }

            if (ZERO_TAX_ID == m_bio->GetTaxId()) {
                TTaxId taxid = s_GetTaxId(*m_bio);
                if (ZERO_TAX_ID != taxid) {
                    CRef<CSeqdesc> des(new CSeqdesc);
                    des->SetOrg().SetTaxId(taxid);
                    m_bio->SetDescr().Set().push_back(des);
                }
            }

            rv.Reset(&(*m_bio));
            ++m_bio;    // will be null when incremented past end
        }

        return rv;
    }

private:
    CRef<CObjectManager>    m_objmgr;
    CRef<CScope>            m_scope;
    CRef<CSeq_entry>        m_entry;
    CTypeIterator <CBioseq> m_bio;
    bool                    m_SkipUnver;
    set<CBioseq*>           m_bio_skipped;
};

void CMakeBlastDBApp::x_AddSeqEntries(CNcbiIstream & data, TFormat fmt)
{
    bool found = false;
	try {
        while(!data.eof())
        {
            CSeqEntrySource src(data, fmt, m_SkipUnver);
            found = found || m_DB->AddSequences(src);
        }
    } catch (const CEofException& e) {
        if (e.GetErrCode() == CEofException::eEof) {
            /* ignore */
        } else {
            throw e;
        }
    }
    if (!found) {
        ERR_POST(Warning << "No sequences written");
    }
}

class CRawSeqDBSource : public IRawSequenceSource {
public:
    CRawSeqDBSource(const string & name, bool protein, CBuildDatabase * outdb, bool randomize);

    virtual ~CRawSeqDBSource()
    {
        if (m_Sequence) {
            m_Source->RetSequence(& m_Sequence);
            m_Sequence = NULL;
        }
    }

    virtual bool GetNext(CTempString               & sequence,
                         CTempString               & ambiguities,
                         CRef<CBlast_def_line_set> & deflines,
                         vector<SBlastDbMaskData>  & mask_range,
                         vector<int>               & column_ids,
                         vector<CTempString>       & column_blobs);

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    virtual void GetColumnNames(vector<string> & names)
    {
        names = m_ColumnNames;
    }

    virtual int GetColumnId(const string & name)
    {
        return m_Source->GetColumnId(name);
    }

    virtual const map<string,string> & GetColumnMetaData(int id)
    {
        return m_Source->GetColumnMetaData(id);
    }
#endif

    void ClearSequence()
    {
        if (m_Sequence) {
            _ASSERT(m_Source.NotEmpty());
            m_Source->RetSequence(& m_Sequence);
        }
    }

private:
    CRef<CSeqDBExpert> m_Source;
    const char * m_Sequence;
    int m_Oid;
    vector<int> m_RandomOids;
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    vector<CBlastDbBlob> m_Blobs;
    vector<int> m_ColumnIds;
    vector<string> m_ColumnNames;
    vector<int> m_MaskIds;
    map<int, int> m_MaskIdMap;
#endif
};

CRawSeqDBSource::CRawSeqDBSource(const string & name, bool protein, CBuildDatabase * outdb, bool randomize)
    : m_Sequence(NULL), m_Oid(0)
{
    CSeqDB::ESeqType seqtype =
        protein ? CSeqDB::eProtein : CSeqDB::eNucleotide;

    m_Source.Reset(new CSeqDBExpert(name, seqtype));
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // Process mask meta data
    m_Source->GetAvailableMaskAlgorithms(m_MaskIds);
    ITERATE(vector<int>, algo_id, m_MaskIds) {
        objects::EBlast_filter_program algo;
        string algo_opts, algo_name;
        m_Source->GetMaskAlgorithmDetails(*algo_id, algo, algo_name, algo_opts);
        if (algo != EBlast_filter_program::eBlast_filter_program_other) {
            algo_name += NStr::IntToString(*algo_id);
        }
        m_MaskIdMap[*algo_id] = outdb->RegisterMaskingAlgorithm(algo, algo_opts, algo_name);
    }
    // Process columns
    m_Source->ListColumns(m_ColumnNames);
    for(int i = 0; i < (int)m_ColumnNames.size(); i++) {
        m_ColumnIds.push_back(m_Source->GetColumnId(m_ColumnNames[i]));
    }
    if(randomize) {
    	int num_oids = m_Source->GetNumOIDs();
    	m_RandomOids.clear();
        std::mt19937 rng(std::time(nullptr));
    	for (int i=0; m_Source->CheckOrFindOID(i); i++) {
    		m_RandomOids.push_back(i);
    	}
    	std::shuffle (m_RandomOids.begin(), m_RandomOids.end(), rng);
    	m_RandomOids.push_back(num_oids);
    }
#endif
}

bool
CRawSeqDBSource::GetNext(CTempString               & sequence,
                         CTempString               & ambiguities,
                         CRef<CBlast_def_line_set> & deflines,
                         vector<SBlastDbMaskData>  & mask_range,
                         vector<int>               & column_ids,
                         vector<CTempString>       & column_blobs)
{
	int oid = (m_RandomOids.size() > 0)? m_RandomOids[m_Oid]: m_Oid;
    if (! m_Source->CheckOrFindOID(oid))
        return false;

    if (m_Sequence) {
        m_Source->RetSequence(& m_Sequence);
        m_Sequence = NULL;
    }

    int slength(0), alength(0);

    m_Source->GetRawSeqAndAmbig(oid, & m_Sequence, & slength, & alength);

    sequence    = CTempString(m_Sequence, slength);
    ambiguities = CTempString(m_Sequence + slength, alength);

    deflines = m_Source->GetHdr(oid);

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // process masks
    ITERATE(vector<int>, algo_id, m_MaskIds) {

        CSeqDB::TSequenceRanges ranges;
        m_Source->GetMaskData(oid, *algo_id, ranges);

        SBlastDbMaskData mask_data;
        mask_data.algorithm_id = m_MaskIdMap[*algo_id];

        ITERATE(CSeqDB::TSequenceRanges, range, ranges) {
            mask_data.offsets.push_back(pair<TSeqPos, TSeqPos>(range->first, range->second));
        }

        mask_range.push_back(mask_data);
    }

    // The column IDs will be the same each time; another approach is
    // to only send back the IDs for those columns that are non-empty.
    column_ids = m_ColumnIds;
    column_blobs.resize(column_ids.size());
    m_Blobs.resize(column_ids.size());

    for(int i = 0; i < (int)column_ids.size(); i++) {
        m_Source->GetColumnBlob(column_ids[i], oid, m_Blobs[i]);
        column_blobs[i] = m_Blobs[i].Str();
    }
#endif

    if (m_RandomOids.size()== 0) {
    	m_Oid = oid;
    }
    m_Oid ++;

    return true;
}

CMakeBlastDBApp::TFormat
CMakeBlastDBApp::x_ConvertToCFormatGuessType(CMakeBlastDBApp::ESupportedInputFormats
                                             fmt)
{
    TFormat retval = CFormatGuess::eUnknown;
    switch (fmt) {
    case eFasta:        retval = CFormatGuess::eFasta; break;
    case eBinaryASN:    retval = CFormatGuess::eBinaryASN; break;
    case eTextASN:      retval = CFormatGuess::eTextASN; break;
    default:            break;
    }
    return retval;
}

CMakeBlastDBApp::ESupportedInputFormats
CMakeBlastDBApp::x_ConvertToSupportedType(CMakeBlastDBApp::TFormat fmt)
{
    ESupportedInputFormats retval = eUnsupported;
    switch (fmt) {
    case CFormatGuess::eFasta:        retval = eFasta; break;
    case CFormatGuess::eBinaryASN:    retval = eBinaryASN; break;
    case CFormatGuess::eTextASN:      retval = eTextASN; break;
    default:            break;
    }
    return retval;
}

CMakeBlastDBApp::ESupportedInputFormats
CMakeBlastDBApp::x_GetUserInputTypeHint(void)
{
    ESupportedInputFormats retval = eUnsupported;
    const CArgs& args = GetArgs();
    if (args["input_type"].HasValue()) {
        const string& input_type = args["input_type"].AsString();
        if (input_type == "fasta") {
            retval = eFasta;
        } else if (input_type == "asn1_bin") {
            retval = eBinaryASN;
        } else if (input_type == "asn1_txt") {
            retval = eTextASN;
        } else if (input_type == "blastdb") {
            retval = eBlastDb;
        } else {
            // need to add supported type to list of constraints!
            _ASSERT(false);
        }
    }
    return retval;
}

void
CMakeBlastDBApp::x_VerifyInputFilesType(const vector<CTempString>& filenames,
                                   CMakeBlastDBApp::ESupportedInputFormats input_type)
{
    //Let other part of the program deal with blastdb input
    if(eBlastDb == input_type)
        return;

    // Guess the input data type
    for (size_t i = 0; i < filenames.size(); i++) {
        string seq_file = filenames[i];

        CFile input_file(seq_file);
        if ( !input_file.Exists() ) {
            string error_msg = "File " + seq_file + " does not exist";
            NCBI_THROW(CInvalidDataException, eInvalidInput, error_msg);
        }
        if (input_file.GetLength() == 0) {
            string error_msg = "File " + seq_file + " is empty";
            NCBI_THROW(CInvalidDataException, eInvalidInput, error_msg);
        }

        CNcbiIfstream f(seq_file.c_str(), ios::binary);
	unique_ptr<CDecompressIStream>  comp_seq_istream;
	CNcbiIstream *data_in = & f;

	// SB-4223 : support for compressed input
	bool is_gz = false, is_bz2 = false, is_zst=false;
	CCompressStream::EMethod method = CDecompressIStream::eGZipFile; 
	if( (is_gz  = NStr::EndsWith( seq_file, ".gz", NStr::eNocase)) || 
	    (is_bz2 = NStr::EndsWith( seq_file, ".bz2",NStr::eNocase)) ||    
	    (is_zst = NStr::EndsWith( seq_file, ".zst",NStr::eNocase))    
	    )
	{
	    if( is_bz2  ) method = CDecompressIStream::eBZip2;
	    if( is_zst  ) method = CDecompressIStream::eZstd;
	    comp_seq_istream.reset(new CDecompressIStream(f , method ));
	    data_in =  comp_seq_istream.get();
	}

        if(input_type == eFasta && x_ConvertToSupportedType(x_GuessFileType( *data_in ) ) != eFasta) {
            string msg = "\nInput file " + seq_file + " does NOT appear to be FASTA (processing anyway).\n" \
                 + "Advise validating database with 'blastdbcheck -dbtype [prot|nucl] -db ${DBNAME}'\n";
            ERR_POST(Warning << msg);
        }
    }
    return;
}

CMakeBlastDBApp::TFormat
CMakeBlastDBApp::x_GuessFileType(CNcbiIstream & input)
{
    CFormatGuess fg(input);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eFasta);
    fg.GetFormatHints().DisableAllNonpreferred();
    return fg.GuessFormat();
}

void CMakeBlastDBApp::x_AddSequenceData(CNcbiIstream & input,
                                        CMakeBlastDBApp::TFormat fmt)
{
    switch(fmt) {
    case CFormatGuess::eFasta:
        x_AddFasta(input);
        break;

    case CFormatGuess::eTextASN:
    case CFormatGuess::eBinaryASN:
        x_AddSeqEntries(input, fmt);
        break;

    default:
        string msg("Input format not supported (");
        msg += string(CFormatGuess::GetFormatName(fmt)) + " format). ";
        msg += "Use -input_type to specify the input type being used.";
        NCBI_THROW(CInvalidDataException, eInvalidInput, msg);
    }
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )

void CMakeBlastDBApp::x_ProcessMaskData()
{
    const CArgs & args = GetArgs();

    const CArgValue & files    = args["mask_data"];
    const CArgValue & ids      = args["mask_id"];
    const CArgValue & descs    = args["mask_desc"];
    const CArgValue & gi_names = args["gi_mask_name"];

    vector<string> file_list;
    vector<string> id_list;
    vector<string> desc_list;
    vector<string> gi_mask_names;

    if (! files.HasValue()) return;
    NStr::Split(NStr::TruncateSpaces(files.AsString()), ",", file_list);
    if (! file_list.size()) {
        NCBI_THROW(CInvalidDataException, eInvalidInput,
                "mask_data option found, but no files were specified.");
    }

    if (ids.HasValue()) {
        NStr::Split(NStr::TruncateSpaces(ids.AsString()), ",", id_list);
        if (file_list.size() != id_list.size()) {
            NCBI_THROW(CInvalidDataException, eInvalidInput,
                "the size of mask_id does not match that of mask_data.");
        }
        // make sure this is not a numeric id
        for (unsigned int i = 0; i < id_list.size(); ++i) {
            Int4 nid(-1);
            if (NStr::StringToNumeric(id_list[i], &nid, NStr::fConvErr_NoThrow, 10)) {
                NCBI_THROW(CInvalidDataException, eInvalidInput,
                   "mask_id can not be numeric.");
            }
        }
    }

    if (descs.HasValue()) {
        NStr::Split(NStr::TruncateSpaces(descs.AsString()), ",", desc_list);
        if (file_list.size() != desc_list.size()) {
            NCBI_THROW(CInvalidDataException, eInvalidInput,
                "the size of mask_desc does not match that of mask_data.");
        }
    } else {
        // description is optional
        vector<string> default_desc(id_list.size(), "");
        desc_list.swap(default_desc);
    }

    if (gi_names.HasValue()) {
        NStr::Split(NStr::TruncateSpaces(gi_names.AsString()), ",", gi_mask_names);
        if (file_list.size() != gi_mask_names.size()) {
            NCBI_THROW(CInvalidDataException, eInvalidInput,
                "the size of gi_mask_name does not match that of mask_data.");
        }
    }

    for (unsigned int i = 0; i < file_list.size(); ++i) {
        if ( !CFile(file_list[i]).Exists() ) {
            ERR_POST(Error << "Ignoring mask file '" << file_list[i]
                           << "' as it does not exist.");
            continue;
        }

        CNcbiIfstream mask_file(file_list[i].c_str(), ios::binary);
        CFormatGuess::EFormat mask_file_format = CFormatGuess::eUnknown;
        {{
             CFormatGuess fg(mask_file);
             fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
             fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
             fg.GetFormatHints().DisableAllNonpreferred();
             mask_file_format = fg.GuessFormat();
         }}

        int algo_id = -1;
        while (true) {
            CRef<CBlast_db_mask_info> first_obj;

            try {
                s_ReadObject(mask_file, mask_file_format, first_obj, "mask data in '" + file_list[i] + "'");
            }
            catch (CEofException&) {
                // must be end of file
                break;
            }

            if (algo_id < 0) {
                *m_LogFile << "Mask file: " << file_list[i] << endl;
                string opts = first_obj->GetAlgo_options();
                if (id_list.size())  {
                    algo_id = m_DB->RegisterMaskingAlgorithm(id_list[i], desc_list[i], opts);
                } else {
                    EBlast_filter_program prog_id =
                        static_cast<EBlast_filter_program>(first_obj->GetAlgo_program());
                    string name = gi_mask_names.size() ? gi_mask_names[i] : file_list[i];
                    algo_id = m_DB->RegisterMaskingAlgorithm(prog_id, opts, name);
                }
            }

            CRef<CBlast_mask_list> masks(& first_obj->SetMasks());
            first_obj.Reset();

            while(1) {
                if (m_Ranges.Empty() && ! masks->GetMasks().empty()) {
                    m_Ranges.Reset(new CMaskedRangeSet);
                    m_DB->SetMaskDataSource(*m_Ranges);
                }

                ITERATE(CBlast_mask_list::TMasks, iter, masks->GetMasks()) {
                    CConstRef<CSeq_id> seqid((**iter).GetId());

                    if (seqid.Empty()) {
                        NCBI_THROW(CInvalidDataException, eInvalidInput,
                                     "Cannot get masked range Seq-id");
                    }

                    m_Ranges->Insert(algo_id, *seqid, **iter);
                }

                if (! masks->GetMore())
                    break;

                s_ReadObject(mask_file, mask_file_format, masks, "mask data (continuation)");
            }
        }
    }
}
#endif

bool CMakeBlastDBApp::x_ShouldParseSeqIds(void)
{
    const CArgs& args = GetArgs();
    if ("fasta" != args["input_type"].AsString())
        return true;
    else if (args["parse_seqids"])
        return true;

    return false;
}

void CMakeBlastDBApp::x_ProcessInputData(const string & paths,
                                         bool           is_protein)
{
    vector<CTempString> names;
    SeqDB_SplitQuoted(paths, names);
    vector<string> blastdb;
    TIdToLeafs leafTaxIds;

    ESupportedInputFormats input_fmt = x_GetUserInputTypeHint();
    CMakeBlastDBApp::TFormat build_fmt = x_ConvertToCFormatGuessType(input_fmt);
    if(eBlastDb != input_fmt) {
        if (names[0] == "-") {
            x_AddSequenceData(cin, build_fmt);
        }
        else {
             x_VerifyInputFilesType(names, input_fmt);
            for (size_t i = 0; i < names.size(); i++) {
                string seq_file = names[i];
                CNcbiIfstream f(seq_file.c_str(), ios::binary);
		CNcbiIstream *data_in = &f;  // uncompressed by default

		unique_ptr<CDecompressIStream>  comp_seq_istream;
		bool is_gz = false, is_bz2 = false, is_zst=false;
		CCompressStream::EMethod method = CDecompressIStream::eGZipFile; // default gzip method if compressed

		if( (is_gz  = NStr::EndsWith( seq_file, ".gz", NStr::eNocase)) || 
		    (is_bz2 = NStr::EndsWith( seq_file, ".bz2",NStr::eNocase)) ||
		    (is_zst = NStr::EndsWith( seq_file, ".zst",NStr::eNocase))    
		  )
		{
		    if( is_bz2  ) method = CDecompressIStream::eBZip2;
		    if( is_zst  ) method = CDecompressIStream::eZstd;
		    comp_seq_istream.reset(new CDecompressIStream(f , method ));
		    data_in =  comp_seq_istream.get();
		}
		// common part
		x_AddSequenceData( *data_in , build_fmt);
            }
        }
    }
    else {
        vector<string> blastdb;
        copy(names.begin(), names.end(), back_inserter(blastdb));
        CSeqDB::ESeqType seqtype = (is_protein ? CSeqDB::eProtein : CSeqDB::eNucleotide);
        const CArgs& args = GetArgs();
        bool randomize = args["randomize"].AsBoolean();

        vector<string> final_blastdb;

        if (m_IsModifyMode) {
            ASSERT(blastdb.size()==1);
            CSeqDB db(blastdb[0], seqtype);
            vector<string> paths;
            db.FindVolumePaths(paths);
            // if paths.size() == 1, we will happily take it to be the same
            // case as a single volume database and recreate a new db
            if (paths.size() > 1) {
                NCBI_THROW(CInvalidDataException, eInvalidInput,
                    "Modifying an alias BLAST db is currently not supported.");
            }
            final_blastdb.push_back(blastdb[0]);
        } else {
            ITERATE(vector<string>, iter, blastdb) {
                const string & s = *iter;

                try {
                     CSeqDB db(s, seqtype);
                }
                catch(const CSeqDBException &) {
                      ERR_POST(Error << "Unable to open input "
                                     << s << " as BLAST db");
                }
                final_blastdb.push_back(s);
            }
        }

        if (final_blastdb.size()) {
            string quoted;
            SeqDB_CombineAndQuote(final_blastdb, quoted);

            CRef<CSeqDB> indb(new CSeqDB(
                    quoted,
                    is_protein ? CSeqDB::eProtein : CSeqDB::eNucleotide
            ));

            for (int oid=0; indb->CheckOrFindOID(oid); oid++) {
                CRef<CBlast_def_line_set> hdr = indb->GetHdr(oid);
                ITERATE(CBlast_def_line_set::Tdata, itr, hdr->Get()) {
                    CRef<CBlast_def_line> bdl = *itr;
                    CBlast_def_line::TTaxIds leafs = bdl->GetLeafTaxIds();
                    if (!leafs.empty()) {
                        const string id =
                                bdl->GetSeqid().front()->AsFastaString();
                        set<TTaxId> ids = leafTaxIds[id];
                        ids.insert(leafs.begin(), leafs.end());
                        leafTaxIds[id] = ids;
                    }
                }
            }

            m_DB->SetLeafTaxIds(leafTaxIds, true);
            CRef<IRawSequenceSource> raw(
                    new CRawSeqDBSource(quoted, is_protein, m_DB, randomize)
            );
            m_DB->AddSequences(*raw);
        } else {
            NCBI_THROW(CInvalidDataException, eInvalidInput,
                "No valid input FASTA file or BLAST db is found.");
        }
    }
}

void CMakeBlastDBApp::x_BuildDatabase()
{
    const string env_skip =
            GetEnvironment().Get("NCBI_MAKEBLASTDB_SKIP_UNVERIFIED_BIOSEQS");
    m_SkipUnver = (env_skip.empty() == false);

    const string dont_scan_bioseq =
        GetEnvironment().Get("NCBI_MAKEBLASTDB_DONT_SCAN_BIOSEQ_FOR_CFASTAREADER_USER_OBJECTS");
    const bool scan_bioseq_4_cfastareader_usrobj = static_cast<bool>(dont_scan_bioseq.empty());

    const CArgs & args = GetArgs();

    // Get arguments to the CBuildDatabase constructor.

    bool is_protein = (args[kArgDbType].AsString() == "prot");

    // 1. database name option if present
    // 2. else, kInput
    string dbname = (args[kOutput].HasValue()
                     ? args[kOutput]
                     : args[kInput]).AsString();
    ESupportedInputFormats input_fmt = x_GetUserInputTypeHint();
    if (input_fmt == eBlastDb && dbname == args[kInput].AsString()) {
        NCBI_THROW(CInvalidDataException, eInvalidInput,
            "Cannot create a BLAST database from an existing one without "
            "changing the output name, please provide a (different) database name "
            "using -" + kOutput);
    }
    if (input_fmt != eBlastDb && args["randomize"].AsBoolean()) {
        NCBI_THROW(CInvalidDataException, eInvalidInput,
        		"Option randomize is only applicable to blastdb input type");
    }

    vector<string> input_files;
    NStr::Split(dbname, kInputSeparators, input_files);
    if (dbname == "-" || input_files.size() > 1) {
        NCBI_THROW(CInvalidDataException, eInvalidInput,
            "Please provide a database name using -" + kOutput);
    }

    if (args[kInput].AsString() == dbname) {
        m_IsModifyMode = true;
    }

    // 1. title option if present
    // 2. otherwise, kInput, UNLESS
    // 3. input is a BLAST database, in which we use that title
    string title = (args[kArgDbTitle].HasValue()
                    ? args[kArgDbTitle]
                    : args[kInput]).AsString();
    if (!args[kArgDbTitle].HasValue() && input_fmt == eBlastDb) {
        vector<CTempString> names;
        SeqDB_SplitQuoted(args[kInput].AsString(), names);
        if (names.size() > 1) {
            NCBI_THROW(CInvalidDataException, eInvalidInput,
                       "Please provide a title using -title");
        }
        CRef<CSeqDB> dbhandle(new CSeqDB(names.front(),
            (is_protein ? CSeqDB::eProtein : CSeqDB::eNucleotide)));
        title = dbhandle->GetTitle();
    }


    // N.B.: Source database(s) in the current working directory will
    // be overwritten (as in formatdb)

    if (title == "-") {
        NCBI_THROW(CInvalidDataException, eInvalidInput,
                         "Please provide a title using -title");
    }

    m_LogFile = & (args["logfile"].HasValue()
                   ? args["logfile"].AsOutputFile()
                   : cout);

    bool parse_seqids = x_ShouldParseSeqIds();
    bool hash_index = args["hash_index"];
    bool use_gi_mask = args["gi_mask"];

    CWriteDB::TIndexType indexing = CWriteDB::eNoIndex;
    indexing |= (hash_index ? CWriteDB::eAddHash : 0);
    indexing |= (parse_seqids ? CWriteDB::eFullIndex : 0);

    bool long_seqids = false;
    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app) {
        const CNcbiRegistry& registry = app->GetConfig();
        long_seqids = (registry.Get("BLAST", "LONG_SEQID") == "1");
    }

    const EBlastDbVersion dbver =
        static_cast<EBlastDbVersion>(args["blastdb_version"].AsInteger());

    bool limit_defline = false;
#if _BLAST_DEBUG
    if(args["limit_defline"]) {
    	limit_defline = true;
    }
#endif
    Uint8 oid_masks = 0;
    if(args["oid_masks"]) {
    	oid_masks = args["oid_masks"].AsInteger();
    }
    m_DB.Reset(new CBuildDatabase(dbname,
                                  title,
                                  is_protein,
                                  indexing,
                                  use_gi_mask,
                                  m_LogFile,
                                  long_seqids,
                                  dbver,
                                  limit_defline,
                                  oid_masks,
                                  scan_bioseq_4_cfastareader_usrobj));

#if _BLAST_DEBUG
    if (args["verbose"]) {
        m_DB->SetVerbosity(true);
    }
#endif /* _BLAST_DEBUG */

    // Should we keep the linkout and membership bits?  Sure.

    // Create empty linkout bit table in order to call these methods;
    // however, in the future it would probably be good to populate
    // this from a user provided option as multisource does.  Also, it
    // might be wasteful to copy membership bits, as the resulting
    // database will most likely not have corresponding mask files;
    // but until there is a way to configure membership bits with this
    // tool, I think it is better to keep, than to toss.

    TLinkoutMap no_bits;

//    m_DB->SetLinkouts(no_bits, true);     // DEPRECATED
    m_DB->SetMembBits(no_bits, true);


    // Max file size

    Uint8 bytes = NStr::StringToUInt8_DataSize(args["max_file_sz"].AsString());
    static const Uint8 MAX_VOL_FILE_SIZE = 0x100000000;
    if (bytes >= MAX_VOL_FILE_SIZE) {
        NCBI_THROW(CInvalidDataException, eInvalidInput,
                "max_file_sz must be < 4 GiB");
    }
    *m_LogFile << "Maximum file size: "
               << Uint8ToString_DataSize(bytes) << endl;

    m_DB->SetMaxFileSize(bytes);

    if (args["taxid"].HasValue()) {
        _ASSERT( !args["taxid_map"].HasValue() );
        CRef<CTaxIdSet> taxids(new CTaxIdSet(TAX_ID_FROM(int, args["taxid"].AsInteger())));
        m_DB->SetTaxids(*taxids);
    } else if (args["taxid_map"].HasValue()) {
        _ASSERT( !args["taxid"].HasValue() );
        CRef<CTaxIdSet> taxids(new CTaxIdSet());
        taxids->SetMappingFromFile(args["taxid_map"].AsInputFile());
        m_DB->SetTaxids(*taxids);
    }


#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    x_ProcessMaskData();
#endif
    x_ProcessInputData(args[kInput].AsString(), is_protein);

    bool success = m_DB->EndBuild();
    if(success) {
    	string new_db = m_DB->GetOutputDbName();
    	CSeqDB::ESeqType t = is_protein? CSeqDB::eProtein: CSeqDB::eNucleotide;
    	CSeqDB sdb(new_db, t);
        string output_prefix = args["metadata_output_prefix"]
                ? args["metadata_output_prefix"].AsString()
                : kEmptyStr;
        if (!output_prefix.empty() && (output_prefix.back() != CFile::GetPathSeparator()))
            output_prefix += CFile::GetPathSeparator();
    	CRef<CBlast_db_metadata> m = sdb.GetDBMetaData(output_prefix);
    	string extn (kEmptyStr);
    	SeqDB_GetMetadataFileExtension(is_protein, extn);
    	string metadata_filename = new_db + "." + extn;
        ofstream out(metadata_filename.c_str());
        unique_ptr<CObjectOStreamJson> json_out(new CObjectOStreamJson(out, eNoOwnership));
        json_out->SetDefaultStringEncoding(eEncoding_Ascii);
        json_out->PreserveKeyNames();
        CConstObjectInfo obj_info(m, m->GetTypeInfo());
        json_out->WriteObject(obj_info);
        json_out->Flush();
        out.flush();
        out << NcbiEndl;
    }
}

int CMakeBlastDBApp::Run(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostPrefix("makeblastdb");

    int status = 0;
    try { x_BuildDatabase(); }
    CATCH_ALL(status)
    x_AddCmdOptions();
    m_UsageReport.AddParam(CBlastUsageReport::eExitStatus, status);
    return status;
}

void CMakeBlastDBApp::x_AddCmdOptions()
{
	const CArgs & args = GetArgs();
    if (args["input_type"].HasValue()) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eInputType, args["input_type"].AsString());
    }
    if (args[kArgDbType].HasValue()) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eSeqType, args[kArgDbType].AsString());
    }
    if(args["taxid"].HasValue() || args["taxid_map"].HasValue()) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eTaxIdList, true);
	}
    if(args["parse_seqids"].HasValue()) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eParseSeqIDs, args["parse_seqids"].AsBoolean());
    }
    if (args["gi_mask"].HasValue()) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eGIList, true);
    }
    else if(args["mask_data"].HasValue()) {
    	 m_UsageReport.AddParam(CBlastUsageReport::eMaskAlgo, true);
	}
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CMakeBlastDBApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
