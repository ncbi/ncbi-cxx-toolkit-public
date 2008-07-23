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

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/blastdb/Blast_db_mask_info.hpp>
#include <objects/blastdb/Blast_mask_list.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include <objtools/readers/seqdb/seqdbcommon.hpp>
#include <objtools/writers/writedb/writedb.hpp>
#include <objtools/writers/writedb/writedb_error.hpp>
#include <util/format_guess.hpp>
#include <util/regexp.hpp>
#include <util/util_exception.hpp>
#include <objtools/writers/writedb/build_db.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <algo/blast/blastinput/blast_input.hpp>
#include "../blast/blast_app_util.hpp"
#include "masked_range_set.hpp"

#include <iostream>
#include <sstream>
#include <fstream>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

/// The main application class
class CMakeBlastDBApp : public CNcbiApplication {
public:
    typedef CFormatGuess::EFormat TFormat;
    
    /** @inheritDoc */
    CMakeBlastDBApp()
        : m_LogFile(NULL)
    {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
    
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();
    
    void x_AddSequenceData(CNcbiIstream & input);
    
    void x_BuildDatabase();
    
    void x_AddFasta(CNcbiIstream & data);
    
    void x_AddSeqEntries(CNcbiIstream & data, TFormat fmt);
    
    void x_ProcessMaskData();
    
    void x_ProcessInputData(const string & paths, bool is_protein);
    
    // Data
    
    CNcbiOstream * m_LogFile;
    
    CRef<CObjectManager> m_ObjMgr;

    CRef<CBuildDatabase> m_DB;
    
    CRef<CMaskedRangeSet> m_Ranges;
};

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
        
    case CFormatGuess::eXml:
        file >> MSerial_Xml >> *obj;
        break;
        
    default:
        throw runtime_error(string("Unknown encoding for ") + msg);
    }
}

template<class TObj>
void s_ReadObject(CNcbiIstream & file,
                  CRef<TObj>    & obj,
                  const string  & msg)
{
    s_ReadObject(file, CFormatGuess().Format(file), obj, msg);
}

static const string kInput("in");
static const string kInputSeparators(" ");
static const string kOutput("out");

void CMakeBlastDBApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                  "Application to create BLAST databases, version " 
                  + CBlastVersion().Print());

    string dflt("Default = input file name provided to -");
    dflt += kInput + " argument";

    arg_desc->SetCurrentGroup("Input options");
    arg_desc->AddDefaultKey(kInput, "input_file", 
                            "Input file/database name; the data type is "
                            "automatically detected, it may be "
                            "any of the following:\n"
                            "\tFASTA file(s) and/or \n"
                            "\tBLAST database(s)\n",
                            CArgDescriptions::eInputFile, "-");
    
    arg_desc->AddDefaultKey("dbtype", "molecule_type",
                            "Molecule type of input",
                            CArgDescriptions::eString, "prot");
    arg_desc->SetConstraint("dbtype", &(*new CArgAllow_Strings,
                                        "nucl", "prot"));

    arg_desc->SetCurrentGroup("Configuration options");
    arg_desc->AddOptionalKey("title", "database_title",
                             "Title for BLAST database\n" + dflt,
                             CArgDescriptions::eString);
    
    arg_desc->AddFlag("parse_seqids",
                      "Parse Seq-ids in FASTA input", true);
    
    arg_desc->AddFlag("hash_index",
                      "Create index of sequence hash values.",
                      true);
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    arg_desc->SetCurrentGroup("Sequence masking options");
    arg_desc->AddOptionalKey("mask_data", "mask_data_files",
                             "Comma-separated list of input files containing "
                             "masking data as produced by NCBI masking "
                             "applications (e.g. dustmasker, segmasker, "
                             "windowmasker)",
                             CArgDescriptions::eString);
#endif
    
    arg_desc->SetCurrentGroup("Output options");
    arg_desc->AddOptionalKey(kOutput, "database_name",
                             "Name of BLAST database to be created\n" + dflt +
                             "Required if multiple file(s)/database(s) are "
                             "provided as input",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("max_file_sz", "number_of_bytes",
                            "Maximum file size for BLAST database files",
                            CArgDescriptions::eString, "1GB");
    SetupArgDescriptions(arg_desc.release());
}

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

class CSeqEntrySource : public IBioseqSource {
public:
    typedef CFormatGuess::EFormat TFormat;
    
    CSeqEntrySource(CNcbiIstream & is, TFormat fmt)
        : m_Input(is), m_Format(fmt), m_Done(false)
    {
    }
    
    virtual CConstRef<CBioseq> GetNext()
    {
        CRef<CSeq_entry> obj(new CSeq_entry);
        CConstRef<CBioseq> rv;
        
        if (m_Done)
            return rv;
        
        try {
            s_ReadObject(m_Input, m_Format, obj, "bioseq");
        }
        catch(const CEofException &) {
            obj.Reset();
        }
        
        if (obj.Empty()) {
            m_Done = true;
        } else {
            if (! obj->IsSeq()) {
                throw runtime_error("Seq-entry does not contain CBioseq");
            }
            
            rv.Reset(& obj->GetSeq());
        }
        
        return rv;
    }
    
private:
    CNcbiIstream & m_Input;
    TFormat        m_Format;
    bool           m_Done;
};

void CMakeBlastDBApp::x_AddSeqEntries(CNcbiIstream & data, TFormat fmt)
{
    CSeqEntrySource src(data, fmt);
    m_DB->AddSequences(src);
}

class CRawSeqDBSource : public IRawSequenceSource {
public:
    CRawSeqDBSource(const string & name, bool protein);
    
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
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    vector<CBlastDbBlob> m_Blobs;
    
    vector<int> m_ColumnIds;
    vector<string> m_ColumnNames;
#endif
};

CRawSeqDBSource::CRawSeqDBSource(const string & name, bool protein)
    : m_Sequence(NULL), m_Oid(0)
{
    CSeqDB::ESeqType seqtype =
        protein ? CSeqDB::eProtein : CSeqDB::eNucleotide;
    
    m_Source.Reset(new CSeqDBExpert(name, seqtype));
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    m_Source->ListColumns(m_ColumnNames);
    
    for(int i = 0; i < (int)m_ColumnNames.size(); i++) {
        m_ColumnIds.push_back(m_Source->GetColumnId(m_ColumnNames[i]));
    }
#endif
}

bool
CRawSeqDBSource::GetNext(CTempString               & sequence,
                         CTempString               & ambiguities,
                         CRef<CBlast_def_line_set> & deflines,
                         vector<int>               & column_ids,
                         vector<CTempString>       & column_blobs)
{
    if (! m_Source->CheckOrFindOID(m_Oid))
        return false;
    
    if (m_Sequence) {
        m_Source->RetSequence(& m_Sequence);
        m_Sequence = NULL;
    }
    
    int slength(0), alength(0);
    
    m_Source->GetRawSeqAndAmbig(m_Oid, & m_Sequence, & slength, & alength);
    
    sequence    = CTempString(m_Sequence, slength);
    ambiguities = CTempString(m_Sequence + slength, alength);
    
    deflines = m_Source->GetHdr(m_Oid);
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // The column IDs will be the same each time; another approach is
    // to only send back the IDs for those columns that are non-empty.
    
    column_ids = m_ColumnIds;
    column_blobs.resize(column_ids.size());
    m_Blobs.resize(column_ids.size());
    
    for(int i = 0; i < (int)column_ids.size(); i++) {
        m_Source->GetColumnBlob(column_ids[i], m_Oid, m_Blobs[i]);
        column_blobs[i] = m_Blobs[i].Str();
    }
#endif
    
    m_Oid ++;
    
    return true;
}

void CMakeBlastDBApp::x_AddSequenceData(CNcbiIstream & input)
{
    TFormat fmt = CFormatGuess().Format(input);
    
    switch(fmt) {
    case CFormatGuess::eFasta:
        x_AddFasta(input);
        break;
        
    case CFormatGuess::eTextASN:
    case CFormatGuess::eBinaryASN:
    case CFormatGuess::eXml:
        x_AddSeqEntries(input, fmt);
        break;
        
    default:
        throw runtime_error("Input format not recognized");
    }
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
void CMakeBlastDBApp::x_ProcessMaskData()
{
    const CArgs & args = GetArgs();
    
    const CArgValue & files = args["mask_data"];
    
    if (! files.HasValue()) {
        return;
    }
    
    vector<string> mask_list;
    NStr::Tokenize(NStr::TruncateSpaces(files.AsString()), ",", mask_list,
                   NStr::eNoMergeDelims);
    
    if (! mask_list.size()) {
        throw runtime_error("mask_data option found, "
                            "but no files were specified.");
    }
    
    ITERATE(vector<string>, iter, mask_list) {
        CNcbiIfstream mask_file(iter->c_str(), ios::binary);
        
        CRef<CBlast_db_mask_info> first_obj;
        
        s_ReadObject(mask_file, first_obj, "mask data in '" + *iter + "'");
        *m_LogFile << "Mask file: " << *iter << endl;
        
        EBlast_filter_program prog_id = 
            static_cast<EBlast_filter_program>(first_obj->GetAlgo_program());
        string opts = first_obj->GetAlgo_options();
        int algo_id = m_DB->RegisterMaskingAlgorithm(prog_id, opts);
        
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
                    throw runtime_error("Cannot get masked range Seq-id");
                }
                
                m_Ranges->Insert(algo_id, *seqid, **iter);
            }
            
            if (! masks->GetMore())
                break;
            
            s_ReadObject(mask_file, masks, "mask data (continuation)");
        }
    }
}
#endif

void CMakeBlastDBApp::x_ProcessInputData(const string & paths,
                                         bool           is_protein)
{
    vector<CTempString> names;
    SeqDB_SplitQuoted(paths, names);
    
    vector<string> blastdb;
    vector<string> fasta;
    
    CSeqDB::ESeqType seqtype =
        (is_protein
         ? CSeqDB::eProtein
         : CSeqDB::eNucleotide);
    
    ITERATE(vector<CTempString>, iter, names) {
        bool is_blastdb = false;
        const string & s = *iter;
        
        if (s != "-") {
            try {
                CSeqDB db(s, seqtype);
                is_blastdb = true;
            }
            catch(const CSeqDBException &) {
                is_blastdb = false; // necessary?
            }
        }
        
        if (is_blastdb) {
            blastdb.push_back(s);
        } else {
            fasta.push_back(s);
        }
    }
    
    if (blastdb.size()) {
        string quoted;
        SeqDB_CombineAndQuote(blastdb, quoted);
        
        CRef<IRawSequenceSource> raw(new CRawSeqDBSource(quoted, is_protein));
        m_DB->AddSequences(*raw);
    }
    
    ITERATE(vector<string>, file, fasta) {
        const string & fasta_file = *file;
        
        if (fasta_file == "-") {
            x_AddSequenceData(cin);
        } else {
            CNcbiIfstream f(fasta_file.c_str(), ios::binary);
            x_AddSequenceData(f);
        }
    }
}

void CMakeBlastDBApp::x_BuildDatabase()
{
    const CArgs & args = GetArgs();
    
    // Get arguments to the CBuildDatabase constructor.
    
    bool is_protein = (args["dbtype"].AsString() == "prot");
    
    // 1. title option if present
    // 2. otherwise, kInput
    string title = (args["title"].HasValue()
                    ? args["title"]
                    : args[kInput]).AsString();
    
    // 1. database name option if present
    // 2. else, kInput
    string dbname = (args[kOutput].HasValue()
                     ? args[kOutput]
                     : args[kInput]).AsString();
    
    vector<string> input_files;
    NStr::Tokenize(dbname, kInputSeparators, input_files);
    if (dbname == "-" || input_files.size() > 1) {
        throw runtime_error("Please provide a database name using " + kOutput);
    }
    _ASSERT(NStr::Find(dbname, kInputSeparators) == NPOS);
    // N.B.: Source database(s) in the current working directory will
    // be overwritten (as in formatdb)
    
    if (title == "-") {
        throw runtime_error("Please provide a title using -title");
    }
    
    m_LogFile = & (args["logfile"].HasValue()
                   ? args["logfile"].AsOutputFile()
                   : cout);
    
    // Do we need this option?
    // 1. if so, implement it.
    // 2. if not, get rid of it.
    //bool parse_seqids = args["parse_seqids"];
    bool hash_index = args["hash_index"];
    
    CWriteDB::EIndexType indexing = (CWriteDB::EIndexType)
        (CWriteDB::eDefault | (hash_index ? CWriteDB::eAddHash : 0));

    if (m_ObjMgr.Empty()) {
        m_ObjMgr.Reset(CObjectManager::GetInstance());
        try {
            CGBLoaderParams params("id2");
            params.SetPreopenConnection(CGBLoaderParams::ePreopenNever);
            CGBDataLoader::RegisterInObjectManager(*m_ObjMgr,
                                                params,
                                                CObjectManager::eDefault);
        } catch (const CException& e) {
            ERR_POST(Warning << e.GetMsg());
        }
    }
        
    m_DB.Reset(new CBuildDatabase(dbname,
                                  title,
                                  is_protein,
                                  indexing,
                                  m_LogFile));
    
    // Should we keep the linkout and membership bits?  Sure.
    
    // Create empty linkout bit table in order to call these methods;
    // however, in the future it would probably be good to populate
    // this from a user provided option as multisource does.  Also, it
    // might be wasteful to copy membership bits, as the resulting
    // database will most likely not have corresponding mask files;
    // but until there is a way to configure membership bits with this
    // tool, I think it is better to keep, than to toss.
    
    TLinkoutMap no_bits;
    
    m_DB->SetLinkouts(no_bits, true);
    m_DB->SetMembBits(no_bits, true);
    
    // Max file size
    
    Uint8 bytes = NStr::StringToUInt8_DataSize(args["max_file_sz"].AsString());
    *m_LogFile << "Maximum file size: " 
               << Uint8ToString_DataSize(bytes) << endl;
    
    m_DB->SetMaxFileSize(bytes);
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    x_ProcessMaskData();
#endif
    x_ProcessInputData(args[kInput].AsString(), is_protein);
}

int CMakeBlastDBApp::Run(void)
{
    int status = 0;
    try { x_BuildDatabase(); } 
    CATCH_ALL(status)
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CMakeBlastDBApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
