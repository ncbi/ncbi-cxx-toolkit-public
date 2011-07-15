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

#include <serial/iterator.hpp>
#include <objmgr/util/create_defline.hpp>

#include <objects/blastdb/Blast_db_mask_info.hpp>
#include <objects/blastdb/Blast_mask_list.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_writer/writedb.hpp>
#include <objtools/blast/seqdb_writer/writedb_error.hpp>
#include <util/format_guess.hpp>
#include <util/util_exception.hpp>
#include <objtools/blast/seqdb_writer/build_db.hpp>

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
#endif /* SKIP_DOXYGEN_PROCESSING */

/// The main application class
class CMakeBlastDBApp : public CNcbiApplication {
public:
    /// Convenience typedef
    typedef CFormatGuess::EFormat TFormat;
    
    enum ESupportedInputFormats {
        eFasta,
        eXml,
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
    
    // Data
    
    CNcbiOstream * m_LogFile;
    
    CRef<CBuildDatabase> m_DB;
    
    CRef<CMaskedRangeSet> m_Ranges;

    bool m_IsModifyMode;
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
        
    case CFormatGuess::eXml:
        file >> MSerial_Xml >> *obj;
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
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eXml);
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
    
    arg_desc->AddDefaultKey(kArgDbType, "molecule_type",
                            "Molecule type of input",
                            CArgDescriptions::eString, "prot");
    arg_desc->SetConstraint(kArgDbType, &(*new CArgAllow_Strings,
                                        "nucl", "prot"));

    arg_desc->SetCurrentGroup("Configuration options");
    arg_desc->AddOptionalKey(kArgDbTitle, "database_title",
                             "Title for BLAST database\n" + dflt,
                             CArgDescriptions::eString);
    
    arg_desc->AddFlag("parse_seqids",
                      "Parse Seq-ids in FASTA input", true);
    arg_desc->AddOptionalKey("input_type", "type",
                             "Type of the data specified in input_file "
                             "(if not specified it will be guessed)",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("input_type", &(*new CArgAllow_Strings, 
                                            "fasta", "blastdb",
                                            "xml", "asn1_bin",
                                            "asn1_txt"));
    
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

    arg_desc->AddFlag("gi_mask",
                      "Create GI indexed masking data.", true);
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
    arg_desc->AddDefaultKey("max_file_sz", "number_of_bytes",
                            "Maximum file size for BLAST database files",
                            CArgDescriptions::eString, "1GB");
#if _BLAST_DEBUG
    arg_desc->AddFlag("verbose", "Produce verbose output", true);
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

class CSeqEntrySource : public IBioseqSource {
public:
    /// Convenience typedef
    typedef CFormatGuess::EFormat TFormat;
    
    CSeqEntrySource(CNcbiIstream & is, TFormat fmt)
        :m_objmgr(CObjectManager::GetInstance()),
         m_scope(new CScope(*m_objmgr)),
         m_entry(new CSeq_entry)   
    {
        char ch=is.peek();

        // Get rid of white spaces 
        while(!is.eof() && (ch==' ' || ch=='\t' || ch=='\n' || ch=='\r')) {
            is.read(&ch, 1);
            ch=is.peek();
        }

        // If input is a Bioseq_set
        if(ch == 'B' || ch == '0') {
            CRef<CBioseq_set> obj(new CBioseq_set);
            s_ReadObject(is, fmt, obj, "bioseq");
            m_entry->SetSet(*obj);
        } else {
            s_ReadObject(is, fmt, m_entry, "bioseq");
        }

        m_bio = Begin(*m_entry);
        m_scope->AddTopLevelSeqEntry(*m_entry);
    }
    
    virtual CConstRef<CBioseq> GetNext()
    {
        CConstRef<CBioseq> rv;
        
       if (m_bio ) {
            if (! m_bio ->CanGetDescr()) {
                 sequence::CDeflineGenerator gen;
                 const string & title = gen.GenerateDefline(*m_bio , *m_scope);
                 CRef<CSeqdesc> des(new CSeqdesc);
                 des->SetTitle(title);
                 CSeq_descr& desr(m_bio ->SetDescr());
                 desr.Set().push_back(des);
            }
            rv.Reset(&(*m_bio ));
            ++m_bio ;
        }
      
        return rv;
    }
    
private:
    CRef<CObjectManager>    m_objmgr;
    CRef<CScope>            m_scope;
    CRef<CSeq_entry>        m_entry;
    CTypeIterator <CBioseq> m_bio;
};

void CMakeBlastDBApp::x_AddSeqEntries(CNcbiIstream & data, TFormat fmt)
{
    CSeqEntrySource src(data, fmt);
    m_DB->AddSequences(src);
}

class CRawSeqDBSource : public IRawSequenceSource {
public:
    CRawSeqDBSource(const string & name, bool protein, CBuildDatabase * outdb);
    
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
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    vector<CBlastDbBlob> m_Blobs;
    vector<int> m_ColumnIds;
    vector<string> m_ColumnNames;
    vector<int> m_MaskIds;
    map<int, int> m_MaskIdMap;
#endif
};

CRawSeqDBSource::CRawSeqDBSource(const string & name, bool protein, CBuildDatabase * outdb)
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
        algo_name += NStr::IntToString(*algo_id);
        m_MaskIdMap[*algo_id] = outdb->RegisterMaskingAlgorithm(algo, algo_opts, algo_name); 
    }              
    // Process columns
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
                         vector<SBlastDbMaskData>  & mask_range,
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
    // process masks
    ITERATE(vector<int>, algo_id, m_MaskIds) { 

        CSeqDB::TSequenceRanges ranges;
        m_Source->GetMaskData(m_Oid, *algo_id, ranges);

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
        m_Source->GetColumnBlob(column_ids[i], m_Oid, m_Blobs[i]);
        column_blobs[i] = m_Blobs[i].Str();
    }
#endif
    
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
    case eXml:          retval = CFormatGuess::eXml; break;
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
    case CFormatGuess::eXml:          retval = eXml; break;
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
        } else if (input_type == "xml") {
            retval = eXml;
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

vector<CMakeBlastDBApp::ESupportedInputFormats>
CMakeBlastDBApp::x_GuessInputType(const vector<CTempString>& filenames,
                                  vector<string>& blastdbs)
{
    vector<ESupportedInputFormats> retval(filenames.size(), eUnsupported);
    // Guess the input data type
    for (size_t i = 0; i < filenames.size(); i++) {
        // input_file could be FASTA, ASN.1 or XML, or if it doesn't match
        // exactly, a BLAST database
        const string & seq_file = filenames[i];

        if (seq_file == "-") {
            retval[i] = x_ConvertToSupportedType(x_GuessFileType(cin));
        } else {
            CFile input_file(seq_file);
            if ( !input_file.Exists() ) {
                blastdbs.push_back(seq_file);
                retval[i] = eBlastDb;
                continue;
            }
            if (input_file.GetLength() == 0) {
                ERR_POST(Error << "Ignoring sequence input file '"
                               << seq_file << "' as it is empty.");
                continue;
            }
            CNcbiIfstream f(seq_file.c_str(), ios::binary);
            retval[i] = x_ConvertToSupportedType(x_GuessFileType(f));
        }
    }
    return retval;
}

CMakeBlastDBApp::TFormat
CMakeBlastDBApp::x_GuessFileType(CNcbiIstream & input)
{
    CFormatGuess fg(input);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eXml);
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
    case CFormatGuess::eXml:
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
    
    const CArgValue & files = args["mask_data"];
    
    if (! files.HasValue()) {
        return;
    }
    
    vector<string> mask_list;

    NStr::Tokenize(NStr::TruncateSpaces(files.AsString()), ",", mask_list,
                   NStr::eNoMergeDelims);

    if (! mask_list.size()) {
        NCBI_THROW(CInvalidDataException, eInvalidInput, 
                "mask_data option found, but no files were specified.");
    }
    
    vector<string> gi_mask_names;
    const CArgValue & gi_names = args["gi_mask_name"];
    if (gi_names.HasValue()) {
        NStr::Tokenize(NStr::TruncateSpaces(gi_names.AsString()), ",", gi_mask_names,
                   NStr::eNoMergeDelims);
        if (mask_list.size() != gi_mask_names.size()) {
            NCBI_THROW(CInvalidDataException, eInvalidInput, 
                "gi_mask list does not correspond to mask_data list.");
        }
        // TODO optionally we need check to make sure the names are unique...
    } 
    
    for (unsigned int i = 0; i < mask_list.size(); ++i) {
        if ( !CFile(mask_list[i]).Exists() ) {
            ERR_POST(Error << "Ignoring mask file '" << mask_list[i] 
                           << "' as it does not exist.");
            continue;
        }

        CNcbiIfstream mask_file(mask_list[i].c_str(), ios::binary);
        CFormatGuess::EFormat mask_file_format = CFormatGuess::eUnknown;
        {{
             CFormatGuess fg(mask_file);
             fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
             fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
             fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eXml);
             fg.GetFormatHints().DisableAllNonpreferred();
             mask_file_format = fg.GuessFormat();
         }}
        
        int algo_id = -1;
        while (true) {
            CRef<CBlast_db_mask_info> first_obj;
        
            try {
                s_ReadObject(mask_file, mask_file_format, first_obj, "mask data in '" + mask_list[i] + "'");
            }
            catch (CEofException&) {
                // must be end of file
                break;
            }
        
            if (algo_id < 0) {
                *m_LogFile << "Mask file: " << mask_list[i] << endl;
                EBlast_filter_program prog_id = 
                    static_cast<EBlast_filter_program>(first_obj->GetAlgo_program());
                string opts = first_obj->GetAlgo_options();
                string name = gi_mask_names.size() ? gi_mask_names[i] : mask_list[i];

                algo_id = m_DB->RegisterMaskingAlgorithm(prog_id, opts, name);
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

void CMakeBlastDBApp::x_ProcessInputData(const string & paths,
                                         bool           is_protein)
{
    bool has_fasta_data = false;
    vector<CTempString> names;
    SeqDB_SplitQuoted(paths, names);
    vector<string> blastdb;

    vector<ESupportedInputFormats> input_types;
    ESupportedInputFormats hint = eUnsupported;
    if ( (hint = x_GetUserInputTypeHint()) != eUnsupported) {
        input_types.assign(names.size(), hint);
        if (hint == eBlastDb) {
            copy(names.begin(), names.end(), back_inserter(blastdb));
        }
    } else {
        input_types = x_GuessInputType(names, blastdb);
    }
    _ASSERT(input_types.size() == names.size());

    for (size_t i = 0; i < names.size(); i++) {
        // input_file could be FASTA, ASN.1 or XML.
        const string & seq_file = names[i];

        if (seq_file == "-") {
            _ASSERT(input_types[i] != eBlastDb);
            x_AddSequenceData(cin, x_ConvertToCFormatGuessType(input_types[i]));
            has_fasta_data = TRUE;
        } else {
            CFile input_file(seq_file);
            if ( !input_file.Exists() ) {
                // this is a BLAST database
                _ASSERT(find(blastdb.begin(), blastdb.end(), seq_file) 
                        != blastdb.end());
                continue;
            }
            if (input_file.GetLength() == 0) {
                ERR_POST(Error << "Ignoring sequence input file '"
                               << seq_file << "' as it is empty.");
                continue;
            }
            CNcbiIfstream f(seq_file.c_str(), ios::binary);
            _ASSERT(input_types[i] != eBlastDb);
            x_AddSequenceData(f, x_ConvertToCFormatGuessType(input_types[i]));
            has_fasta_data = TRUE;
        }
    }

    if (blastdb.size() > 0)
    {

        CSeqDB::ESeqType seqtype =
            (is_protein
             ? CSeqDB::eProtein : CSeqDB::eNucleotide);

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
                                     << s << " as either FASTA file or BLAST db");
                        continue;
                }
                final_blastdb.push_back(s);
            }
        }

        if (final_blastdb.size()) {
            string quoted;
            SeqDB_CombineAndQuote(final_blastdb, quoted);
            if (has_fasta_data) {
                ERR_POST(Error << "Ignoring sequence input file '"
                               << quoted << "' as it mixes with FASTA files.");
            } else {
                CRef<IRawSequenceSource> raw(new CRawSeqDBSource(quoted, is_protein, m_DB));
                m_DB->AddSequences(*raw);
            }
        } else {
            NCBI_THROW(CInvalidDataException, eInvalidInput, 
                "No valid input FASTA file or BLAST db is found.");
        }
    }
}

void CMakeBlastDBApp::x_BuildDatabase()
{
    const CArgs & args = GetArgs();
    
    // Get arguments to the CBuildDatabase constructor.
    
    bool is_protein = (args[kArgDbType].AsString() == "prot");
    
    // 1. title option if present
    // 2. otherwise, kInput
    string title = (args[kArgDbTitle].HasValue()
                    ? args[kArgDbTitle]
                    : args[kInput]).AsString();
    
    // 1. database name option if present
    // 2. else, kInput
    string dbname = (args[kOutput].HasValue()
                     ? args[kOutput]
                     : args[kInput]).AsString();
    
    vector<string> input_files;
    NStr::Tokenize(dbname, kInputSeparators, input_files);
    if (dbname == "-" || input_files.size() > 1) {
        NCBI_THROW(CInvalidDataException, eInvalidInput, 
            "Please provide a database name using -" + kOutput);
    }

    if (args[kInput].AsString() == dbname) {
        m_IsModifyMode = true;
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
    
    bool parse_seqids = args["parse_seqids"];
    bool hash_index = args["hash_index"];
    bool use_gi_mask = args["gi_mask"];
    
    CWriteDB::TIndexType indexing = CWriteDB::eNoIndex;
    indexing |= (hash_index ? CWriteDB::eAddHash : 0);
    indexing |= (parse_seqids ? CWriteDB::eFullIndex : 0);

    m_DB.Reset(new CBuildDatabase(dbname,
                                  title,
                                  is_protein,
                                  indexing,
                                  use_gi_mask,
                                  m_LogFile));

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
    
    m_DB->SetLinkouts(no_bits, true);
    m_DB->SetMembBits(no_bits, true);
    
    // Max file size
    
    Uint8 bytes = NStr::StringToUInt8_DataSize(args["max_file_sz"].AsString());
    *m_LogFile << "Maximum file size: " 
               << Uint8ToString_DataSize(bytes) << endl;
    
    m_DB->SetMaxFileSize(bytes);

    if (args["taxid"].HasValue()) {
        _ASSERT( !args["taxid_map"].HasValue() );
        CRef<CTaxIdSet> taxids(new CTaxIdSet(args["taxid"].AsInteger()));
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
