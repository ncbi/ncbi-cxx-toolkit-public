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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

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
#include <objtools/readers/agp_util.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CAgpToSeqEntry::

namespace {

CRef<CSeq_id> s_SeqIdFromStr( const string & str ) {
    CRef<CSeq_id> seq_id;
    try { 
        seq_id.Reset( new CSeq_id( str ) );
    } catch(...) {
        // couldn't create real seq-id.  fall back on local seq-id
        seq_id.Reset( new CSeq_id );
        seq_id->SetLocal().SetStr( str );
    }
    return seq_id;
}

// This class is used to turn an AGP file into a Seq-entry
class CAgpToSeqEntry : public CAgpReader {
public:

    // When reading, loads results into "entries",
    CAgpToSeqEntry( vector< CRef<CSeq_entry> > & entries ) :
      CAgpReader( eAgpVersion_2_0 ),
      m_entries(entries)
      { }

protected:
    virtual void OnGapOrComponent() {
        if( ! m_bioseq || 
            m_prev_row->GetObject() != m_this_row->GetObject() ) 
        {
            x_FinishedBioseq();

            // initialize new bioseq
            CRef<CSeq_inst> seq_inst( new CSeq_inst );
            seq_inst->SetRepr(CSeq_inst::eRepr_delta);
            seq_inst->SetMol(CSeq_inst::eMol_dna);
            seq_inst->SetLength(0);

            m_bioseq.Reset( new CBioseq );
            m_bioseq->SetInst(*seq_inst);

            CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Local,
                m_this_row->GetObject(), m_this_row->GetObject() ));
            m_bioseq->SetId().push_back(id);
        }

        CRef<CSeq_inst> seq_inst( & m_bioseq->SetInst() );

        CRef<CDelta_seq> delta_seq( new CDelta_seq );
        seq_inst->SetExt().SetDelta().Set().push_back(delta_seq);

        if( m_this_row->is_gap ) {
            delta_seq->SetLiteral().SetLength(m_this_row->gap_length);
            if( m_this_row->component_type == 'U' ) {
                delta_seq->SetLiteral().SetFuzz().SetLim();
            }
            seq_inst->SetLength() += m_this_row->gap_length;
        } else {
            CSeq_loc& loc = delta_seq->SetLoc();

            CRef<CSeq_id> comp_id =
                s_SeqIdFromStr( m_this_row->GetComponentId() );
            loc.SetInt().SetId(*comp_id);

            loc.SetInt().SetFrom( m_this_row->component_beg - 1 );
            loc.SetInt().SetTo(   m_this_row->component_end - 1 );
            seq_inst->SetLength() += ( m_this_row->component_end - m_this_row->component_beg + 1 );
            
            switch( m_this_row->orientation ) {
            case CAgpRow::eOrientationPlus:
                loc.SetInt().SetStrand( eNa_strand_plus );
                break;
            case CAgpRow::eOrientationMinus:
                loc.SetInt().SetStrand( eNa_strand_minus );
                break;
            case CAgpRow::eOrientationUnknown:
                loc.SetInt().SetStrand( eNa_strand_unknown );
                break;
            case CAgpRow::eOrientationIrrelevant:
                loc.SetInt().SetStrand( eNa_strand_other );
                break;
            default:
                throw runtime_error("unknown orientation " + NStr::IntToString(m_this_row->orientation));
            }
        }
    }

    virtual int Finalize(void)
    {
        // First, do real finalize
        const int return_val = CAgpReader::Finalize();
        // Then, our own finalization
        x_FinishedBioseq();

        return return_val;
    }

    void x_FinishedBioseq(void)
    {
        if( m_bioseq ) {
            CRef<CSeq_entry> entry( new CSeq_entry );
            entry->SetSeq(*m_bioseq);
            m_entries.push_back( entry );

            m_bioseq.Reset();
        }
    }

    CRef<CBioseq> m_bioseq;
    vector< CRef<CSeq_entry> > & m_entries;
};

} // anonymous namespace


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
    TDiffsToHide diffsToHide )
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

    CRef<CObjectManager> om(CObjectManager::GetInstance());

    // quickly scan the AGP files to determine the component
    // Seq-ids
    TSeqIdSet compSeqIds;
    x_GetCompSeqIds( compSeqIds, agpFiles );

    // load local component FASTA sequences and Genbank into 
    // local scope for lookups using local data storage
    auto_ptr<CTmpFile> ldsdb_file;
    CRef<CLDS2_Manager> lds_mgr;
    ldsdb_file.reset( new CTmpFile ); // file deleted on object destruction
    lds_mgr.Reset(new CLDS2_Manager( ldsdb_file->GetFileName() ));
    list<string> objfiles;
    ITERATE( list<string>, file_iter, compAndObjFiles ) {
        // check if file is a FASTA component file

        if( eFileType_FASTA != x_GuessFileType( *file_iter ) ) {
            // we support text ASN.1 object files
            objfiles.push_back(*file_iter);
            continue;
        }
        ifstream file_strm( file_iter->c_str() );
        string line;
        NcbiGetline(file_strm, line, "\r\n");
        // extract accession
        // Get first word, trim final '|' (if any).
        SIZE_TYPE pos1=line.find(' ' , 1);
        SIZE_TYPE pos2=line.find('\t', 1);
        if(pos2<pos1) pos1 = pos2;
        if(pos1!=NPOS) {
            pos1--;
            if(pos1>0 && line[pos1]=='|') pos1--;
        }
        string acc_long = line.substr(1, pos1);
        CRef<CSeq_id> comp_id = s_SeqIdFromStr( acc_long );
        CSeq_id_Handle acc_h = CSeq_id_Handle::GetHandle(*comp_id);
        if( compSeqIds.find(acc_h) != compSeqIds.end() ) {
            // component files go into the component object
            // temporary database
            lds_mgr->AddDataFile( *file_iter );
        } else {
            // object files will be remembered for later processing
            objfiles.push_back(*file_iter);
        }
    }
    lds_mgr->UpdateData();
    CLDS2_DataLoader::RegisterInObjectManager(
        *om, ldsdb_file->GetFileName(), -1,
        CObjectManager::eDefault, 1 );
    CGBDataLoader::RegisterInObjectManager(
        *om, 0, 
        CObjectManager::eDefault, 2 );

    // calculate checksum of the AGP sequences and the FASTA sequences

    TUniqueSeqs agp_ids;
    // process every AGP file
    if( agpFiles.empty() ) {
        cerr << "error: could not find any agp files" << endl;
        return eResult_Failure;
    }
    x_ProcessAgps( agpFiles, agp_ids );

    TUniqueSeqs fasta_ids;
    // process every objfile 
    if( objfiles.empty() ) {
        cerr << "error: could not find any obj files" << endl;
        return eResult_Failure;
    }
    x_ProcessObjects( objfiles, fasta_ids );

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
            vSeqIdFASTAOnly.insert( iter1->second );
            ++iter1;
        }
        else if (iter2->first < iter1->first) {
            vSeqIdAGPOnly.insert( iter2->second );
            ++iter2;
        }
        else {
            ++iter1;
            ++iter2;
        }
    }

    for ( ;  iter1 != iter1_end;  ++iter1) {
        vSeqIdFASTAOnly.insert( iter1->second );
    }

    for ( ;  iter2 != iter2_end;  ++iter2) {
        vSeqIdAGPOnly.insert( iter2->second );
    }

    // look at vSeqIdFASTAOnly and vSeqIdAGPOnly and 
    // print in user-friendly way
    x_OutputDifferences( vSeqIdFASTAOnly, vSeqIdAGPOnly, diffsToHide );

    const bool bThereWereDifferences = ( 
        ( ! vSeqIdFASTAOnly.empty() &&
          ! (diffsToHide & fDiffsToHide_ObjfileOnly) ) ||
        ( ! vSeqIdAGPOnly.empty() &&
          ! (diffsToHide & fDiffsToHide_AGPOnly) ) );
    if( ! bThereWereDifferences ) {
        LOG_POST(Error << "Success: No differences found");
    }
    if( bThereWereDifferences ) {
        m_bSuccess = false;
    }

    return ( m_bSuccess ? eResult_Success : eResult_Failure );
}


void CAgpFastaComparator::x_Process(const CSeq_entry_Handle seh,
                                            TUniqueSeqs& seqs,
                                            int * in_out_pUniqueBioseqsLoaded,
                                            int * in_out_pBioseqsSkipped )
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
        if( ! vec.CanGetRange(0, bioseq_it->GetBioseqLength() - 1) ) {
            LOG_POST(Error << "  Skipping one: could not load due to error "
                     "in AGP file "
                     "(length issue or doesn't exist) for " << idh 
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
            vec.GetSeqData(0, bioseq_it->GetBioseqLength() - 1, data);
        } catch(CSeqVectorException ex) {
            LOG_POST(Error << "  Skipping one: could not load due to error, probably in AGP file, possibly a length issue, for " << idh);
            m_bSuccess = false;
            continue;
        }

        CChecksum cks(CChecksum::eMD5);
        cks.AddLine(data);

        string md5;
        cks.GetMD5Digest(md5);

        TKey key(md5, bioseq_it->GetBioseqLength());
        pair<TUniqueSeqs::iterator, bool> insert_result =
            seqs.insert(TUniqueSeqs::value_type(key, idh));
        if( ! insert_result.second ) {
            LOG_POST(Error << "  Error: duplicate sequence " << idh );
            m_bSuccess = false;
            continue;
        }

        if( m_pLoadLogFile.get() != NULL ) {
            CNcbiOstrstream os;
            ITERATE (string, i, key.first) {
                os << setw(2) << setfill('0') << hex << (int)((unsigned char)*i);
            }

            *m_pLoadLogFile << "  " << idh << ": "
                << string(CNcbiOstrstreamToString(os))
                << " / " << key.second << endl;
        }

        ++*in_out_pUniqueBioseqsLoaded;
    }

    *in_out_pBioseqsSkipped = ( total -  *in_out_pUniqueBioseqsLoaded);
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

            CBioseq_Handle inner_bioseq_h = scope.GetBioseqHandle(seq_id_h);
            if( ! inner_bioseq_h ) {
                LOG_POST(Error << "    Could not find bioseq for "
                         << seq_id_h
                         << ".  Maybe you need to specify component file(s)." );
                continue;
            }

            if( ! inner_bioseq_h.IsSetInst_Length() ) {
                LOG_POST(Error << "    Could not get length of bioseq for "
                         << seq_id_h );
                continue;
            }

            const TSeqPos bioseq_len = inner_bioseq_h.GetInst_Length();
            if( highest_pnt >= bioseq_len ) {
                LOG_POST(Error << "    For "
                         << seq_id_h
                         << " length is " << bioseq_len
                         << " but user tries to access the point "
                         << (highest_pnt+1) ); // "+1" because user sees 1-based
                continue;
            }
        }
    } catch(...) {
        CNcbiOstrstream bioseq_strm;
        bioseq_strm << MSerial_AsnText << *bioseq_h.GetCompleteBioseq();
        LOG_POST(Error << kBugInAgpFastaCompare << ": "
                 << (string)CNcbiOstrstreamToString(bioseq_strm) );
        return;
    }
}

void CAgpFastaComparator::x_GetCompSeqIds(
    TSeqIdSet & out_compSeqIds,
    const std::list<std::string> & agpFiles )
{
    // for speed, we do the parsing ourselves, avoiding some
    // of the weight of a full AGP parser
    ITERATE( std::list<std::string>, file_iter, agpFiles ) {
        ifstream file_strm(file_iter->c_str());
        string line;
        while( NcbiGetline(file_strm, line, "\r\n") ) {
            // skip comment lines
            if( line.empty() || line[0] == '#' ) {
                continue;
            }

            // find 6th column (tab-separated)
            SIZE_TYPE pos = 0;
            for( int  ii = 0;
                 pos != NPOS && pos < (line.length() - 1) && ii < 5 ;
                 ++ii )
            {
                pos = line.find('\t', pos + 1);
            }
            if( pos != NPOS ) {
                // pos points to tab just before the column we want
                // find the tab that's after the column we want
                SIZE_TYPE close_tab_pos = line.find('\t', pos + 1 );
                if( close_tab_pos != NPOS ) {
                    // we have the column we want.  Make sure it's
                    // not numeric (which would indicate it's just
                    // a gap)
                    if( ! isdigit(line[pos+1]) ) {
                        CRef<CSeq_id> seq_id =
                            s_SeqIdFromStr( line.substr(pos+1, close_tab_pos - pos - 1 ) );
                        out_compSeqIds.insert(
                            CSeq_id_Handle::GetHandle(*seq_id) );
                    }
                }
            }
        }
    }
}

void CAgpFastaComparator::x_ProcessObjects( const list<string> & filenames,
                                                   TUniqueSeqs& fasta_ids )
{
    int iNumLoaded = 0;
    int iNumSkipped = 0;

    LOG_POST(Error << "Processing object file(s)...");
    if( m_pLoadLogFile.get() != NULL ) {
        *m_pLoadLogFile << "Processing object file(s)..." << endl;
    }
    ITERATE( list<string>, file_iter, filenames ) {
        const string &filename = *file_iter;
        try {
            CFormatGuess guesser( filename );
            const CFormatGuess::EFormat format = 
                guesser.GuessFormat();

            if( format == CFormatGuess::eFasta ) {
                CNcbiIfstream file_istrm( filename.c_str(), ios::binary );
                CFastaReader reader(file_istrm);
                while (file_istrm) {
                    CRef<CSeq_entry> entry = reader.ReadOneSeq();

                    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
                    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
                    x_Process(seh, fasta_ids, &iNumLoaded, &iNumSkipped );
                }
            } else if( format == CFormatGuess::eBinaryASN || 
                       format == CFormatGuess::eTextASN )
            {
                // see if it's a submit
                CRef<CSeq_submit> submit( new CSeq_submit );
                {
                    CNcbiIfstream file_istrm( filename.c_str(), ios::binary );
                    x_SetBinaryVsText( file_istrm, format );
                    try {
                        file_istrm >> *submit;
                    } catch(...) {
                        // didn't work
                    }
                }

                if( submit ) {

                    if( ! submit->IsEntrys() ) {
                        LOG_POST(Error << "Seq-submits must have 'entrys'.");
                        return;
                    }

                    ITERATE( CSeq_submit::C_Data::TEntrys, entry_iter, 
                             submit->GetData().GetEntrys() ) 
                    {
                        const CSeq_entry &entry = **entry_iter;

                        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
                        CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
                        x_Process(seh, fasta_ids, &iNumLoaded, &iNumSkipped );
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
                    x_Process(seh, fasta_ids, &iNumLoaded, &iNumSkipped );
                }
            } else {
                LOG_POST(Error << "Could not determine format of " << filename 
                         << ", best guess is: " << CFormatGuess::GetFormatName(format) );
                return;
            }
        }
        catch(CObjReaderParseException & ex ) {
            if( ex.GetErrCode() == CObjReaderParseException::eEOF ) {
                // end of file; no problem
            } else {
                LOG_POST(Error << "Error reading object file: " << ex.what() );
            }
        }
        catch (CException& ex ) {
            LOG_POST(Error << "Error reading object file: " << ex.what() );
        }
    }

    LOG_POST(Error << "Loaded " << iNumLoaded << " object file sequence(s).");
    if( iNumSkipped > 0 ) {
        LOG_POST(Error << "  Skipped " << iNumSkipped << " FASTA sequence(s).");
    }
}


void CAgpFastaComparator::x_ProcessAgps(const list<string> & filenames,
                                        TUniqueSeqs& agp_ids )
{
    int iNumLoaded = 0;
    int iNumSkipped = 0;

    LOG_POST(Error << "Processing AGP...");
    if( m_pLoadLogFile.get() != NULL ) {
        *m_pLoadLogFile << "Processing AGP..." << endl;
    }
    ITERATE( list<string>, file_iter, filenames ) {
        const string &filename = *file_iter;
        CNcbiIfstream istr( filename.c_str() );
        while (istr) {
            vector< CRef<CSeq_entry> > entries;
            CAgpToSeqEntry agp_reader( entries );
            int err_code = agp_reader.ReadStream( istr ); // loads var entries
            if( err_code != 0 ) {
                LOG_POST(Error << "Error occurred reading AGP file: "
                         << agp_reader.GetErrorMessage() );
                m_bSuccess = false;
                return;
            }
            ITERATE (vector< CRef<CSeq_entry> >, it, entries) {
                CRef<CSeq_entry> entry = *it;

                CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
                CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
                scope->AddDefaults();

                x_Process(seh, agp_ids, &iNumLoaded, &iNumSkipped );
            }
        }
    }
    LOG_POST(Error << "Loaded " << iNumLoaded << " AGP sequence(s).");
    if( iNumSkipped > 0 ) {
        LOG_POST(Error << "  Skipped " << iNumSkipped << " AGP sequence(s).");
    }
}

void CAgpFastaComparator::x_OutputDifferences(
    const TSeqIdSet & vSeqIdFASTAOnly,
    const TSeqIdSet & vSeqIdAGPOnly,
    TDiffsToHide diffs_to_hide )
{
    // find the ones in both
    TSeqIdSet vSeqIdTempSet;
    set_intersection( 
        vSeqIdFASTAOnly.begin(), vSeqIdFASTAOnly.end(),
        vSeqIdAGPOnly.begin(), vSeqIdAGPOnly.end(),
        inserter(vSeqIdTempSet, vSeqIdTempSet.begin()) );
    if( ! vSeqIdTempSet.empty() ) {
        LOG_POST(Error << "  These differ between object file and AGP:");
        ITERATE( TSeqIdSet, id_iter, vSeqIdTempSet ) {
            LOG_POST(Error << "    " << *id_iter);
        }
    }

    // find the ones in FASTA only
    vSeqIdTempSet.clear();
    set_difference( 
        vSeqIdFASTAOnly.begin(), vSeqIdFASTAOnly.end(),
        vSeqIdAGPOnly.begin(), vSeqIdAGPOnly.end(),
        inserter(vSeqIdTempSet, vSeqIdTempSet.begin()) );
    if( ! vSeqIdTempSet.empty() && ! (diffs_to_hide & fDiffsToHide_ObjfileOnly) ) {
        LOG_POST(Error << "  These are in Object file only: " << "\n"
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
        LOG_POST(Error << "  These are in AGP only: " << "\n"
            << "  (Check above: were some FASTA sequences skipped due "
            << "to errors?)");
        ITERATE( TSeqIdSet, id_iter, vSeqIdTempSet ) {
            LOG_POST(Error << "    " << *id_iter);
        }
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
