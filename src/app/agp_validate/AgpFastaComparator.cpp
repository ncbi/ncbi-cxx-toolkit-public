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
 * Authors:  Mike DiCuccio, Michael Kornbluh
 *
 * File Description:
 *     Makes sure an AGP file builds the same sequence found in a FASTA
 *     file.
 *
 */

#include <ncbi_pch.hpp>

#include "AgpFastaComparator.hpp"

#include <algorithm>
#include <sstream>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiexec.hpp>

#include <util/checksum.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/lds2/lds2_dataloader.hpp>
#include <objtools/lds2/lds2.hpp>
#include <objtools/readers/agp_seq_entry.hpp>
#include <objtools/readers/agp_util.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>

#ifdef COMP_LOG
#  error COMP_LOG was already defined
#endif

// convenience macro for writing to the logfile (if it's open)
#define COMP_LOG(msg)                           \
    do {                                        \
        if( x_IsLogFileOpen() ) {    \
            *m_pLoadLogFile << msg << endl;     \
        }                                       \
    } while(false)


USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {

    // pScope can be NULL
    CRef<CSeq_id> s_CustomGetSeqIdFromStr( const string & str, CScope * pScope )
    {
        // start with parent class's default parsing
        CRef<CSeq_id> seq_id = CAgpToSeqEntry::s_DefaultSeqIdFromStr(str);

        // optimize for the (hopefully common) fast case of local IDs
        if( seq_id->IsLocal() ) {
            return seq_id;
        }

        // build what this would look like as a local ID
        CRef<CSeq_id> pLocalSeqId = CAgpToSeqEntry::s_LocalSeqIdFromStr(str);

        // reject prot-only accessions, or accessions that aren't found
        CSeq_id::EAccessionInfo fAccnInfo = seq_id->IdentifyAccession();
        const bool bAccnIsProtOnly = ( 
            (fAccnInfo & CSeq_id::fAcc_prot) &&
            ! (fAccnInfo & CSeq_id::fAcc_nuc));
        const bool bSeqIdIsFound = ( pScope ? pScope->GetBioseqHandle(*seq_id) : false );
        if( bAccnIsProtOnly || ! bSeqIdIsFound ) 
        {
            // fall back on local ID
            return pLocalSeqId;
        }

        const bool bLocalSeqIdIsfound = ( 
            pScope ? pScope->GetBioseqHandle(*pLocalSeqId) : false );
        if( bLocalSeqIdIsfound ) {
            // print a warning that a local ID was overridden
            cerr << "Warning: '" << str << "' was used as an accession, "
                "so the local component was ignored." << endl;
        }

        // everything looks fine, so return it
        return seq_id;
    }

    // slight customization to CAgpToSeqEntry:
    // if an ID can be found in GenBank, use that
    // and otherwise fall back on local ID
    class CCustomAgpToSeqEntry : public CAgpToSeqEntry {
    public:

        CCustomAgpToSeqEntry(CScope * pScope)
            : m_pScope(pScope)
        {
        }

    protected:
        virtual CRef<CSeq_id> x_GetSeqIdFromStr( const std::string & str )
        {
            return s_CustomGetSeqIdFromStr(str, m_pScope.GetPointer());
        }

    private:
        CRef<CScope> m_pScope;
    };
}

/////////////////////////////////////////////////////////////////////////////
//  CAgpFastaComparator::

CAgpFastaComparator::CAgpFastaComparator(void)
    : m_bSuccess(true)
{
}

/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


CAgpFastaComparator::EResult CAgpFastaComparator::Run(
    const std::list<std::string> & files,
    const std::string & loadlog,
    const std::string & agp_as_fasta_file,
    TDiffsToHide diffsToHide,
    int diffs_to_find           // how many differences to show
    )
{
    LOG_POST(Error << "" );     // newline
    LOG_POST(Error << "Starting AGP/Fasta Compare" );
    LOG_POST(Error << "" );     // newline

    // figure out which files are AGP and which are FASTA
    list<string> compAndObjFiles;
    list<string> agpFiles;
    ITERATE( std::list<std::string>, file_iter, files ) {
        const string & file = *file_iter;
        switch( x_GuessFileType(file) ) {
        case eFileType_FASTA:
        case eFileType_ASN1:
        case eFileType_Unknown: // unknown might be binary ASN.1 (we might want to fix that)
            compAndObjFiles.push_back(file);
            break;
        case eFileType_AGP:
            agpFiles.push_back(file);
            break;
        }
    }

    if( ! loadlog.empty() ) {
        m_pLoadLogFile.reset( 
            new CNcbiOfstream(loadlog.c_str() ) );
    }

    if( ! agp_as_fasta_file.empty() ) {
        m_pAgpAsFastaFile.reset(
            new CNcbiOfstream(agp_as_fasta_file.c_str()));
    }

    CRef<CObjectManager> om(CObjectManager::GetInstance());

    // quickly scan the AGP files to determine the component
    // Seq-ids
    TSeqIdSet compSeqIds;
    TSeqIdSet objSeqIds;
    if( ! x_GetCompAndObjSeqIds( compSeqIds, objSeqIds, agpFiles ) ) {
        // error message should've been printed inside x_GetCompAndObjSeqIds
        return CAgpFastaComparator::eResult_Failure;
    }
    if( x_IsLogFileOpen() ) {
        ITERATE(TSeqIdSet, seq_id_it, compSeqIds) {
            COMP_LOG("Component seq-id from AGP file(s): " 
                << seq_id_it->AsString());
        }
        ITERATE(TSeqIdSet, seq_id_it, objSeqIds) {
            COMP_LOG("Object seq-id from AGP file(s): " 
                << seq_id_it->AsString());
        }
    }

    // load local component FASTA sequences and Genbank into 
    // local scope for lookups using local data storage
    auto_ptr<CTmpFile> ldsdb_file;
    CRef<CLDS2_Manager> lds_mgr;
    ldsdb_file.reset( new CTmpFile ); // file deleted on object destruction
    lds_mgr.Reset(new CLDS2_Manager( ldsdb_file->GetFileName() ));

    // adjust FASTA flags
    // (workaround for CXX-3453 which caused WGS-246 )
    CFastaReader::TFlags fasta_flags = lds_mgr->GetFastaFlags();
    fasta_flags &= ~CFastaReader::fParseGaps;
    // component ids are always interpreted as local
    fasta_flags &= ~CFastaReader::fParseRawID; 
    fasta_flags |= CFastaReader::fAddMods;
    lds_mgr->SetFastaFlags(fasta_flags);

    list<string> objfiles;
    ITERATE( list<string>, file_iter, compAndObjFiles ) {
        // check if file is a FASTA component file

        if( eFileType_FASTA != x_GuessFileType( *file_iter ) ) {
            // we support text ASN.1 object files
            COMP_LOG("Object file: " << *file_iter);
            objfiles.push_back(*file_iter);
            continue;
        }

        ifstream file_strm( file_iter->c_str() );
        string line;
        // look at the ids in the file to try to determine what
        // 
        while( NcbiGetline(file_strm, line, "\r\n") ) {
            // extract accession
            // Get first word, trim final '|' (if any).
            if( ! NStr::StartsWith(line, ">") ) {
                continue;
            }
            SIZE_TYPE after_seq_id_pos = line.find_first_of(" \t");
            if( after_seq_id_pos == string::npos ) {
                after_seq_id_pos = line.length();
            }
            string acc_long = line.substr(1, (after_seq_id_pos - 1));
            CRef<CSeq_id> seq_id = s_CustomGetSeqIdFromStr( acc_long, NULL );
            CSeq_id_Handle acc_h = CSeq_id_Handle::GetHandle(*seq_id);

            COMP_LOG("Sample accession from " << *file_iter 
                << ": " << acc_h.AsString());
            if( compSeqIds.find(acc_h) != compSeqIds.end() ) {
                // component files go into the component object
                // temporary database
                COMP_LOG("Component file: " << *file_iter);
                lds_mgr->AddDataFile( *file_iter );
                break;
            } else if( objSeqIds.find(acc_h) != objSeqIds.end() ) {
                // object files will be remembered for later processing
                COMP_LOG("Object file: " << *file_iter);
                objfiles.push_back(*file_iter);
                break;
            }
        }

        // no seq-id in the file seems relevant
        if( ! file_strm ) {
            // none of the seq-ids seem to be used anywhere
            cerr << "Warning: This file seems to be unused: '" 
                << *file_iter << "'" << endl;
        }
    }
    lds_mgr->UpdateData();
    CLDS2_DataLoader::RegisterInObjectManager(
        *om, ldsdb_file->GetFileName(), ( fasta_flags & ~CFastaReader::fNoSeqData ),
        CObjectManager::eDefault, 1 );
    CGBDataLoader::RegisterInObjectManager(
        *om, 0, 
        CObjectManager::eDefault, 2 );

    // calculate checksum of the AGP sequences and the FASTA sequences

    // temporary dir to hold outputs so we can diff.
    // this is only used if we're showing diffs
    auto_ptr<CTmpSeqVecStorage> temp_dir;
    if( diffs_to_find > 0 ) {
        temp_dir.reset( new CTmpSeqVecStorage );
    }

    TUniqueSeqs agp_ids;
    // process every AGP file
    if( agpFiles.empty() ) {
        cerr << "error: could not find any agp files" << endl;
        return eResult_Failure;
    }
    x_ProcessAgps( agpFiles, agp_ids, temp_dir.get()  );

    TUniqueSeqs fasta_ids;
    // process every objfile 
    if( objfiles.empty() ) {
        cerr << "error: could not find any obj files" << endl;
        return eResult_Failure;
    }
    x_ProcessObjects( objfiles, fasta_ids, temp_dir.get() );

    // check for duplicate sequences
    x_CheckForDups( fasta_ids, "object file(s)" );
    x_CheckForDups( agp_ids,   "AGP file(s)" );

    // will hold ones that are only in FASTA or only in AGP.
    // Of course, if one appears in both, we should print it in a more
    // user-friendly way
    TSeqIdSet vSeqIdFASTAOnly;
    TSeqIdSet vSeqIdAGPOnly;

    TUniqueSeqs::const_iterator iter1 = fasta_ids.begin();
    TUniqueSeqs::const_iterator iter1_end = fasta_ids.end();

    TUniqueSeqs::const_iterator iter2 = agp_ids.begin();
    TUniqueSeqs::const_iterator iter2_end = agp_ids.end();

    // make sure set of sequences in obj FASTA match AGP's objects.
    // Print discrepancies.
    LOG_POST(Error << "Reporting differences...");
    for ( ;  iter1 != iter1_end  &&  iter2 != iter2_end;  ) {
        if (iter1->first < iter2->first) {
            copy( iter1->second.begin(), iter1->second.end(),
                  inserter(vSeqIdFASTAOnly, vSeqIdFASTAOnly.begin() ) );
            ++iter1;
        }
        else if (iter2->first < iter1->first) {
            copy( iter2->second.begin(), iter2->second.end(),
                  inserter(vSeqIdAGPOnly, vSeqIdAGPOnly.begin() ) );
            ++iter2;
        }
        else if( iter1->second != iter2->second ) {
            // Find the ones in FASTA but not AGP
            set_difference( iter1->second.begin(), iter1->second.end(),
                            iter2->second.begin(), iter2->second.end(),
                            inserter(vSeqIdFASTAOnly,
                                     vSeqIdFASTAOnly.begin() ) );

            // Find the ones in AGP but not FASTA
            set_difference( iter2->second.begin(), iter2->second.end(),
                            iter1->second.begin(), iter1->second.end(),
                            inserter(vSeqIdAGPOnly,
                                     vSeqIdAGPOnly.begin() ) );

            ++iter1;
            ++iter2;
        }
        else {
            ++iter1;
            ++iter2;
        }
    }

    for ( ;  iter1 != iter1_end;  ++iter1) {
        copy( iter1->second.begin(), iter1->second.end(),
              inserter(vSeqIdFASTAOnly, vSeqIdFASTAOnly.begin() ) );
    }

    for ( ;  iter2 != iter2_end;  ++iter2) {
        copy( iter2->second.begin(), iter2->second.end(),
              inserter(vSeqIdAGPOnly, vSeqIdAGPOnly.begin() ) );
    }

    // look at vSeqIdFASTAOnly and vSeqIdAGPOnly and 
    // print in user-friendly way
    // Also, fill in SeqIds that are in both
    TSeqIdSet seqIdIntersection;
    x_OutputDifferingSeqIds( vSeqIdFASTAOnly, vSeqIdAGPOnly, diffsToHide, seqIdIntersection );

    const bool bThereWereDifferences = ( 
        ( ! vSeqIdFASTAOnly.empty() &&
          ! (diffsToHide & fDiffsToHide_ObjfileOnly) ) ||
        ( ! vSeqIdAGPOnly.empty() &&
          ! (diffsToHide & fDiffsToHide_AGPOnly) ) );
    if( ! bThereWereDifferences ) {
        LOG_POST(Error << "No differences found");
    }
    if( bThereWereDifferences ) {
        m_bSuccess = false;
    }

    if( bThereWereDifferences && diffs_to_find > 0 &&
        ! seqIdIntersection.empty() ) 
    {
        x_OutputSeqDifferences( diffs_to_find,
                                seqIdIntersection,
                                *temp_dir );
    }


    return ( m_bSuccess ? eResult_Success : eResult_Failure );
}

CAgpFastaComparator::CTmpSeqVecStorage::CTmpSeqVecStorage(void) :
    m_dir( x_GetTmpDir() )
{
    if( m_dir.Exists() ) {
        throw std::runtime_error("Temp dir already exists: " + m_dir.GetPath() );
    }

    if( ! m_dir.Create() ) {
        throw std::runtime_error("Could not create temp dir: " + m_dir.GetPath() );
    }
}

CAgpFastaComparator::CTmpSeqVecStorage::~CTmpSeqVecStorage(void)
{
    if( ! m_dir.Remove() ) {
        cerr << "Warning: could not delete temporary dir "
             << m_dir.GetPath() << endl;
    }
}

void CAgpFastaComparator::CTmpSeqVecStorage::WriteData( EType type, CSeq_entry_Handle seh )
{
    for (CBioseq_CI bioseq_it(seh, CSeq_inst::eMol_na);  bioseq_it;  ++bioseq_it) 
    {
        CSeq_id_Handle idh = sequence::GetId(*bioseq_it,
                                             sequence::eGetId_Best);
        ofstream output_stream( GetFileName(type, idh).c_str() );

        // write raw sequence, but have a newline every 60 residues.
        // newlines are important for the "diff" command
        CSeqVector vec(*bioseq_it, CBioseq_Handle::eCoding_Iupac);
        CSeqVector::const_iterator iter = vec.begin();
        int bytes_copied = 0;
        for( ; iter != vec.end(); ++iter, ++bytes_copied ) {
            if( bytes_copied > 0 && (bytes_copied % 60) == 0 ) {
                // use '\n' instead of endl to avoid flushing
                output_stream << '\n'; 
            }
            output_stream << *iter;
        }
        output_stream << endl;
    }
}

string
CAgpFastaComparator::CTmpSeqVecStorage::GetFileName( EType type, CSeq_id_Handle idh )
{
    std::stringstream file_name_strm;
    file_name_strm << m_dir.GetPath() << CDirEntry::GetPathSeparator();

    switch( type ) {
    case eType_AGP:
        file_name_strm << "agp";
        break;
    case eType_Obj:
        file_name_strm << "obj";
        break;
    default:
        _TROUBLE;
        // in case _TROUBLE falls through, do the best we can:
        file_name_strm << "UNKNOWN";
        break;
    }

    file_name_strm << '.';

    // get cleaned version of seqid without any
    // illegal characters
    {
        const string initial_seq_id = idh.AsString();
        std::stringstream final_seq_id;
        ITERATE(string, ch_iter, initial_seq_id) {
            const unsigned char ch = *ch_iter;
            if( isalnum(ch) ) {
                final_seq_id << ch;
            } else {
                final_seq_id << '_' << setfill('0') << setw(3) << ch;
            }
        }
        file_name_strm << final_seq_id.str();
    }

    return file_name_strm.str();
}

string CAgpFastaComparator::CTmpSeqVecStorage::x_GetTmpDir(void)
{
    std::stringstream dir_strm;
    dir_strm << CDir::GetTmpDir() << '/'
             << "AgpFastaComparator." << CProcess::GetCurrentPid()
             << "."
             << CTime(CTime::eCurrent).AsString("YMDTh:m:s.l");
    return dir_strm.str();
}

void CAgpFastaComparator::x_Process(const CSeq_entry_Handle seh,
                                    TUniqueSeqs& seqs,
                                    int * in_out_pUniqueBioseqsLoaded,
                                    int * in_out_pBioseqsSkipped,
                                    CNcbiOfstream *pDataOutFile )
{
    _ASSERT( 
        in_out_pUniqueBioseqsLoaded != NULL && 
        in_out_pBioseqsSkipped != NULL );

    // skipped is total minus loaded.
    int total = 0;

    for (CBioseq_CI bioseq_it(seh, CSeq_inst::eMol_na);  bioseq_it;  ++bioseq_it) {
        ++total;
        CSeqVector vec(*bioseq_it, CBioseq_Handle::eCoding_Iupac);
        CSeq_id_Handle idh = sequence::GetId(*bioseq_it,
                                             sequence::eGetId_Best);
        string data;
        if( ! vec.CanGetRange(0, bioseq_it->GetBioseqLength()) ) {
            LOG_POST(Error << "  Skipping one: could not load due to error "
                     "in AGP file "
                     "(length issue or does not include range [1, "
                     << bioseq_it->GetBioseqLength() << "] or "
                     "doesn't exist) for " << idh 
                     << " (though issue could be due to failure to resolve "
                     "one of the contigs.  "
                     "Are all necessary components in GenBank or in files "
                     "specified on the command-line?)." );

            // try to figure out where the length error is
            x_PrintDetailsOfLengthIssue( *bioseq_it );
            m_bSuccess = false;
            continue;
        }
        try {
            vec.GetSeqData(0, bioseq_it->GetBioseqLength(), data);
        } catch(CSeqVectorException ex) {
            LOG_POST(Error << "  Skipping one: could not load due to error, "
                "probably in AGP file, possibly a length issue, for " 
                << idh << Endl() << Endl() 
                << "Raw technical information about error: " << ex.what() );
            m_bSuccess = false;
            continue;
        }

        if( pDataOutFile != NULL ) {
            x_WriteDataAsFasta( *pDataOutFile, idh, data );
        }

        CChecksum cks(CChecksum::eMD5);
        cks.AddLine(data);

        string md5;
        cks.GetMD5Digest(md5);

        TKey key(md5, bioseq_it->GetBioseqLength());
        pair<TSeqIdSet::iterator, bool> insert_result =
            seqs[key].insert(idh);
        if( ! insert_result.second ) {
            LOG_POST(Error << "  Error: skipping sequence with same name and values: " << idh);
            m_bSuccess = false;
            continue;
        }

        if( x_IsLogFileOpen() ) {
            CNcbiOstrstream os;
            ITERATE (string, i, key.first) {
                os << setw(2) << setfill('0') << hex << (int)((unsigned char)*i);
            }
            COMP_LOG("  " << idh << ": "
                << string(CNcbiOstrstreamToString(os))
                << " / " << key.second);
        }

        ++*in_out_pUniqueBioseqsLoaded;
    }

    *in_out_pBioseqsSkipped = ( total -  *in_out_pUniqueBioseqsLoaded);
}

void CAgpFastaComparator::x_WriteDataAsFasta(
    CNcbiOfstream & dataOutFile,
    const CSeq_id_Handle & idh,
    const std::string & data )
{
    const static SIZE_TYPE kFastaWidth = 60;

    dataOutFile << '>' << idh << endl;

    const SIZE_TYPE data_len = data.length();
    SIZE_TYPE next_idx = 0;
    for( ; next_idx < data_len ; next_idx += kFastaWidth ) {
        SIZE_TYPE chars_to_copy = min( kFastaWidth, (data_len - next_idx) );
        dataOutFile.write( data.c_str() + next_idx, chars_to_copy );
        dataOutFile << '\n';
    }
}

void CAgpFastaComparator::x_PrintDetailsOfLengthIssue(
    CBioseq_Handle bioseq_h )
{
    const static string kBugInAgpFastaCompare(
        "    This is probably a bug in agp_fasta_compare: could not get "
        "information on the bioseq with an error" );

    const CDelta_ext::Tdata *p_delta_data = NULL;
    try {
        CScope &scope = bioseq_h.GetScope();

        p_delta_data = &bioseq_h.GetCompleteBioseq()->GetInst().GetExt().GetDelta().Get();

        if( p_delta_data == NULL ) {
            LOG_POST(Error << kBugInAgpFastaCompare);
            return;
        }


        // put it in a reference to make it easier to work with
        const CDelta_ext::Tdata &delta_data = *p_delta_data;

        ITERATE( CDelta_ext::Tdata, delta_iter, delta_data ) {
            if( (*delta_iter)->IsLiteral() ) {
                continue;
            }

            const CSeq_interval & seq_int = (*delta_iter)->GetLoc().GetInt();

            const TSeqPos highest_pnt =
                max( seq_int.GetFrom(), seq_int.GetTo() );
            CSeq_id_Handle seq_id_h =
                CSeq_id_Handle::GetHandle(seq_int.GetId());

            CBioseq_Handle inner_bioseq_h;
            try {
                inner_bioseq_h = scope.GetBioseqHandle(seq_id_h);
                if( ! inner_bioseq_h ) {
                    LOG_POST(Error << "    Couldn't find bioseq for "
                         << seq_id_h
                         << ".  Maybe you need to specify component file(s)." );
                } else if( ! inner_bioseq_h.IsSetInst_Length() ) {
                    LOG_POST(Error << "    Could not get length of bioseq for "
                        << seq_id_h );
                } else {
                    const TSeqPos bioseq_len = inner_bioseq_h.GetInst_Length();
                    if( highest_pnt >= bioseq_len ) {
                        LOG_POST(Error << "    For "
                            << seq_id_h
                            << " length is " << bioseq_len
                            << " but user tries to access the point "
                            << (highest_pnt+1) ); // "+1" because user sees 1-based
                    }
                }
            } catch(...) {
                LOG_POST(Error << "    Could not find bioseq for "
                         << seq_id_h
                         << ".  Maybe you need to specify component file(s)." );
            }
        }
    } catch(std::exception & ex) {
        CNcbiOstrstream bioseq_strm;
        bioseq_strm << MSerial_AsnText << *bioseq_h.GetCompleteBioseq();
        LOG_POST(Error << kBugInAgpFastaCompare << ": "
            << Endl() << Endl()
            << "Raw technical information about error: " << Endl()
            << ex.what()
            << Endl()
            << " Bioseq ASN.1: " << (string)CNcbiOstrstreamToString(bioseq_strm) );
        return;
    } catch(...) {
        CNcbiOstrstream bioseq_strm;
        bioseq_strm << MSerial_AsnText << *bioseq_h.GetCompleteBioseq();
        LOG_POST(Error << kBugInAgpFastaCompare << ": "
            << "(unknown error)"
            << " Bioseq ASN.1: " << (string)CNcbiOstrstreamToString(bioseq_strm) );
        return;
    }
}

bool CAgpFastaComparator::x_GetCompAndObjSeqIds(
    TSeqIdSet & out_compSeqIds,
    TSeqIdSet & out_objSeqIds,
    const std::list<std::string> & agpFiles )
{
    const static CTempString kDelim("\t");

    const static CTempString kNotAGPErr(
        "This file is not in a recognized AGP format: ");

    // what is held in some of the AGP columns
    const static int kObjSeqIdCol = 0;
    const static int kCompTypeCol = 4;
    const static int kCompSeqIdCol = 5;
    const static int kMaxColUsed = kCompSeqIdCol;

    vector<CTempString> vecLineTokens;

    // for speed, we do the parsing ourselves with only very minimal
    // error-checking
    ITERATE( std::list<std::string>, file_iter, agpFiles ) {
        ifstream file_strm(file_iter->c_str());
        string line;
        while( NcbiGetline(file_strm, line, "\r\n") ) {
            // skip comment lines
            if( line.empty() || line[0] == '#' ) {
                continue;
            }

            vecLineTokens.clear();
            NStr::Tokenize(line, kDelim, vecLineTokens);

            // are there enough columns for an AGP file?
            if( vecLineTokens.size() <= kMaxColUsed ){
                cerr << kNotAGPErr << *file_iter << endl;
                return false;
            }

            // skip gaps
            CTempString sComponentType = vecLineTokens[kCompTypeCol];
            if( sComponentType.length() != 1 ) {
                cerr << kNotAGPErr << *file_iter << endl;
                return false;
            }
            const char chCompType = toupper(sComponentType[0]);
            if( chCompType == 'N' || chCompType == 'U' ) 
            {
                // skip gaps
                continue;
            }

            // get object Seq-id
            CRef<CSeq_id> objSeqId = s_CustomGetSeqIdFromStr(
                vecLineTokens[kObjSeqIdCol], NULL);
            out_objSeqIds.insert(
                CSeq_id_Handle::GetHandle(*objSeqId));

            // get component Seq-id
            CRef<CSeq_id> comp_seq_id =
                    s_CustomGetSeqIdFromStr(
                    vecLineTokens[kCompSeqIdCol], NULL);
            out_compSeqIds.insert(
                CSeq_id_Handle::GetHandle(*comp_seq_id) );
        }
    }

    return true;
}

void CAgpFastaComparator::x_ProcessObjects(
    const list<string> & filenames,
    TUniqueSeqs& fasta_ids,
    CTmpSeqVecStorage *temp_dir )
{
    int iNumLoaded = 0;
    int iNumSkipped = 0;

    LOG_POST(Error << "Processing object file(s)...");
    COMP_LOG("Processing object file(s)...");
    ITERATE( list<string>, file_iter, filenames ) {
        const string &filename = *file_iter;
        try {
            CFormatGuess guesser( filename );
            const CFormatGuess::EFormat format = 
                guesser.GuessFormat();

            if( format == CFormatGuess::eFasta ) {
                CNcbiIfstream file_istrm( filename.c_str(), ios::binary );
                CFastaReader reader(file_istrm, CFastaReader::fAddMods);
                while (file_istrm) {
                    CRef<CSeq_entry> entry = reader.ReadOneSeq();

                    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
                    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
                    x_Process(seh, fasta_ids, &iNumLoaded, &iNumSkipped, NULL );
                    if( temp_dir ) {
                        temp_dir->WriteData( CTmpSeqVecStorage::eType_Obj, seh );
                    }
                }
            } else if( format == CFormatGuess::eBinaryASN || 
                       format == CFormatGuess::eTextASN )
            {
                // see if it's a submit
                CRef<CSeq_submit> submit( new CSeq_submit );
                {
                    CNcbiIfstream file_istrm( filename.c_str(), ios::binary );
                    x_SetBinaryVsText( file_istrm, format );
                    file_istrm >> *submit;
                }

                if( submit ) {

                    if( ! submit->IsEntrys() ) {
                        LOG_POST(Error << "Seq-submits must have 'entrys'.");
                        m_bSuccess = false;
                        return;
                    }

                    ITERATE( CSeq_submit::C_Data::TEntrys, entry_iter, 
                             submit->GetData().GetEntrys() ) 
                    {
                        const CSeq_entry &entry = **entry_iter;

                        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
                        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
                        x_Process(seh, fasta_ids, &iNumLoaded, &iNumSkipped, NULL );
                        if( temp_dir ) {
                            temp_dir->WriteData( CTmpSeqVecStorage::eType_Obj, seh );
                        }
                    }
                } 
                else
                {
                    CRef<CSeq_entry> entry( new CSeq_entry );

                    CNcbiIfstream file_istrm( filename.c_str(), ios::binary );
                    x_SetBinaryVsText( file_istrm, format );
                    file_istrm >> *entry;

                    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
                    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
                    x_Process(seh, fasta_ids, &iNumLoaded, &iNumSkipped, NULL );
                    if( temp_dir ) {
                        temp_dir->WriteData( CTmpSeqVecStorage::eType_Obj, seh );
                    }
                }
            } else {
                LOG_POST(Error << "Could not determine format of " << filename 
                         << ", best guess is: " << CFormatGuess::GetFormatName(format) );
                m_bSuccess = false;
                return;
            }
        }
        catch(CObjReaderParseException & ex ) {
            if( ex.GetErrCode() == CObjReaderParseException::eEOF ) {
                // end of file; no problem
            } else {
                LOG_POST(Error << "Error reading object file: " << ex.what() );
                m_bSuccess = false;
                return;
            }
        }
        catch (CException& ex ) {
            LOG_POST(Error << "Error reading object file: " << ex.what() );
            m_bSuccess = false;
            return;
        }
    }

    LOG_POST(Error << "Loaded " << iNumLoaded << " object file sequence(s).");
    if( iNumSkipped > 0 ) {
        LOG_POST(Error << "  Skipped " << iNumSkipped << " FASTA sequence(s).");
    }
}


void CAgpFastaComparator::x_ProcessAgps(const list<string> & filenames,
                                       TUniqueSeqs& agp_ids,
                                       CTmpSeqVecStorage *temp_dir )
{
    int iNumLoaded = 0;
    int iNumSkipped = 0;

    LOG_POST(Error << "Processing AGP...");
    COMP_LOG("Processing AGP...");

    CRef<CScope> pAgpToSeqEntryScope(new CScope(*CObjectManager::GetInstance()));
    pAgpToSeqEntryScope->AddDefaults();

    ITERATE( list<string>, file_iter, filenames ) {
        const string &filename = *file_iter;
        CNcbiIfstream istr( filename.c_str() );
        while (istr) {
            CCustomAgpToSeqEntry agp_reader(pAgpToSeqEntryScope.GetPointer());
            int err_code = agp_reader.ReadStream( istr ); // loads entries
            if( err_code != 0 ) {
                LOG_POST(Error << "Error occurred reading AGP file: "
                         << agp_reader.GetErrorMessage() );
                m_bSuccess = false;
                return;
            }
            ITERATE (vector< CRef<CSeq_entry> >, it, agp_reader.GetResult() ) {
                CRef<CSeq_entry> entry = *it;

                CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
                CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
                scope->AddDefaults();

                x_Process(seh, agp_ids, &iNumLoaded, &iNumSkipped, m_pAgpAsFastaFile.get() );
                if( temp_dir ) {
                    temp_dir->WriteData( CTmpSeqVecStorage::eType_AGP, seh );
                }
            }
        }
    }
    LOG_POST(Error << "Loaded " << iNumLoaded << " AGP sequence(s).");
    if( iNumSkipped > 0 ) {
        LOG_POST(Error << "  Skipped " << iNumSkipped << " AGP sequence(s).");
    }
}

void CAgpFastaComparator::x_OutputDifferingSeqIds(
    const TSeqIdSet & vSeqIdFASTAOnly,
    const TSeqIdSet & vSeqIdAGPOnly,
    TDiffsToHide diffs_to_hide,
    TSeqIdSet & out_seqIdIntersection )
{
    // find the ones in both
    set_intersection( 
        vSeqIdFASTAOnly.begin(), vSeqIdFASTAOnly.end(),
        vSeqIdAGPOnly.begin(), vSeqIdAGPOnly.end(),
        inserter(out_seqIdIntersection, out_seqIdIntersection.begin()) );
    if( ! out_seqIdIntersection.empty() ) {
        LOG_POST(Error << "  These " << out_seqIdIntersection.size()
                 << " differ between object file and AGP:");
        ITERATE( TSeqIdSet, id_iter, out_seqIdIntersection ) {
            LOG_POST(Error << "    " << *id_iter);
        }
    }

    // find the ones in FASTA only
    TSeqIdSet vSeqIdTempSet;
    set_difference( 
        vSeqIdFASTAOnly.begin(), vSeqIdFASTAOnly.end(),
        vSeqIdAGPOnly.begin(), vSeqIdAGPOnly.end(),
        inserter(vSeqIdTempSet, vSeqIdTempSet.begin()) );
    if( ! vSeqIdTempSet.empty() && ! (diffs_to_hide & fDiffsToHide_ObjfileOnly) ) {
        LOG_POST(Error << "  These " << vSeqIdTempSet.size()
                 << " are in Object file only: " << "\n"
                 << "  (Check above: were some AGP sequences skipped due "
                 << "to errors?)");
        ITERATE( TSeqIdSet, id_iter, vSeqIdTempSet ) {
            LOG_POST(Error << "    " << *id_iter);
        }
    }

    // find the ones in AGP only
    vSeqIdTempSet.clear();
    set_difference( 
        vSeqIdAGPOnly.begin(), vSeqIdAGPOnly.end(),
        vSeqIdFASTAOnly.begin(), vSeqIdFASTAOnly.end(),
        inserter(vSeqIdTempSet, vSeqIdTempSet.begin()) );
    if( ! vSeqIdTempSet.empty() && ! (diffs_to_hide & fDiffsToHide_AGPOnly) ) {
        LOG_POST(Error << "  These " << vSeqIdTempSet.size()
                 << " are in AGP only: " << "\n"
                 << "  (Check above: were some FASTA sequences skipped due "
                 << "to errors?)");
        ITERATE( TSeqIdSet, id_iter, vSeqIdTempSet ) {
            LOG_POST(Error << "    " << *id_iter);
        }
    }
}

void CAgpFastaComparator::x_CheckForDups( TUniqueSeqs & unique_ids,
                                          const string & file_type )
{
    ITERATE( TUniqueSeqs, unique_id_iter, unique_ids ) {
        const TSeqIdSet & id_set = unique_id_iter->second;
        if( id_set.size() > 1 ) {
            CNcbiOstrstream errmsg;
            errmsg << "WARNING: Identical sequences in " << file_type << ":";
            ITERATE( TSeqIdSet, id_iter, id_set ) {
                errmsg << " '" << *id_iter << "'";
            }
            LOG_POST( Error << (string)CNcbiOstrstreamToString(errmsg) );
        }
    }
}

void CAgpFastaComparator::x_OutputSeqDifferences(
        int diffs_to_find,
        const TSeqIdSet & seqIdIntersection,
        CTmpSeqVecStorage & temp_dir )
{
    const static string kDiff = "/usr/bin/diff";
    if( ! CExec::IsExecutable(kDiff) ) {
        cerr << "No differences shown because cannot run " << kDiff << endl;
        return;
    }

    const static string kAwk = "/usr/bin/awk";
    if( ! CExec::IsExecutable(kAwk) ) {
        cerr << "No differences shown because cannot run " << kAwk << endl;
        return;
    }

    ITERATE( TSeqIdSet, id_iter, seqIdIntersection ) {
        const CSeq_id_Handle & idh = *id_iter;
        const string agp_file = temp_dir.GetFileName( CTmpSeqVecStorage::eType_AGP, idh );
        const string obj_file = temp_dir.GetFileName( CTmpSeqVecStorage::eType_Obj, idh );

        cout << endl;        
        cout << "##### Comparing " << idh << " for AGP ('<') and Obj ('>'):" << endl;
        cout << endl;

        // This is a suboptimal implementation for multiple reasons:
        // - It won't work in Windows
        // - CExec::System is prone to exploits (though since agp_validate
        //   is not setuid or setgid, this is less severe an issue than
        //   it could be).
        //   - Similarly, building a command-line from a stringstream 
        //     could also be dangerous.
        // I'm awaiting JIRA CXX-3145 to see if a superior
        // solution is possible.  In particular, I would like the NCBI
        // C++ toolkit to have a diff library.
        std::stringstream cmd_strm;
        cmd_strm << kDiff << " '" << agp_file << "' '" << obj_file << "' 2> /dev/null | " << kAwk << " 'BEGIN { max_lines = " << diffs_to_find << "; left_seen = 0; right_seen = 0; } "
                 << "/^</ { left_seen += 1; if( left_seen <= max_lines ) { print } } "
                 << "/^>/ { right_seen += 1; if( right_seen <= max_lines ) { print } } "
                 << "/^[0-9]/ {  if( left_seen > right_seen ) { right_seen = left_seen } else { left_seen = right_seen }  if( left_seen >= max_lines && right_seen >= max_lines) { exit } ; print } "
                 << "/^-/ { print }'";
        CExec::System( cmd_strm.str().c_str() );
    }
}

void CAgpFastaComparator::x_SetBinaryVsText( CNcbiIstream & file_istrm, 
                                                    CFormatGuess::EFormat guess_format )
{
    // set binary vs. text
    switch( guess_format ) {
        case CFormatGuess::eBinaryASN:
            file_istrm >> MSerial_AsnBinary;
            break;
        case CFormatGuess::eTextASN:
            file_istrm >> MSerial_AsnText;
            break;
        default:
            break;
            // a format where binary vs. text is irrelevant
    }
}

CAgpFastaComparator::EFileType CAgpFastaComparator::x_GuessFileType( const string & filename )
{
    // To prevent us from reading huge files
    int iterations_remaining = 100;

    ifstream file_strm(filename.c_str());
    string line;

    // find first non-blank line
    while( file_strm && line.empty() &&
           iterations_remaining-- > 0 )
    {
        // get line and trim it
        NcbiGetline(file_strm, line, "\r\n");
        NStr::TruncateSpacesInPlace( line );
    }

    if( line.empty() ) {
        return eFileType_Unknown;
    }

    if( line[0] == '>' ) {
        return eFileType_FASTA;
    }

    if( line.find("::=") != NPOS ) {
        return eFileType_ASN1;
    }

    if( line[0] == '#' ) {
        return eFileType_AGP;
    }

    int num_tabs = 0;
    // did not use std::count because Sun WorkShop compiler defines it in
    // a non-standard way and this is cleaner than preprocessor directives
    ITERATE( string, str_iter, line ) {
        if( *str_iter == '\t' ) {
            ++num_tabs;
        }
    }
    if( num_tabs >= 7 ) {
        return eFileType_AGP;
    }

    return eFileType_Unknown;
}
