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
* Author:  Kevin Bealer
*
*/

/** @file build_db.cpp 
  Code to build a database given various sources of sequence data.
*/
#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>

// Blast databases

#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#include <objtools/blast/seqdb_writer/writedb.hpp>
#include <objtools/readers/fasta.hpp>

// Object Manager

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/readers/reader_exception.hpp> // for CObjReaderParseException

// Other utilities

#include <util/sequtil/sequtil_convert.hpp>

// Local

#include <objtools/blast/seqdb_writer/build_db.hpp>
#include <objtools/blast/seqdb_writer/multisource_util.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
#endif

int debug_mode = 0;

void CBuildDatabase::x_ResolveRemoteId(CRef<objects::CSeq_id> & seqid, int & gi)
{
    CScope::TIds ids = x_GetScope().GetIds(*seqid);
    
    bool have_seqid = false;
    bool have_gi = false;
    
    gi = 0;
    
    ITERATE(CScope::TIds, iter, ids) {
        CConstRef<CSeq_id> id = iter->GetSeqId();
        if (debug_mode > 5)
            m_LogFile << "Seq-id " << seqid->AsFastaString()
                      << " contains id " << id->AsFastaString() << endl;
        
        if (id->IsGi()) {
            if (gi > 0) {
                if (debug_mode > 5)
                    m_LogFile << "WARNING: multiple GIs discovered; gi[0] = "
                              << gi << endl;
            } else {
                if (debug_mode > 5)
                    m_LogFile << "Seq-id " << seqid->AsFastaString()
                              << " resolved to "
                              << id->GetGi() << endl;
                gi = GI_TO(int, id->GetGi());
                have_gi = true;
            }
        } else if ((! have_seqid) && (id->Which() == seqid->Which())) {
            m_LogFile << "Remote: Resolving <" << seqid->AsFastaString()
                      << "> to <" << id->AsFastaString() << ">" << endl;
            
            if (id->GetTextseq_Id() == NULL ||
                id->GetTextseq_Id()->IsSetVersion() == false) {
                
                m_LogFile
                    << "Warning: Resolution still does not provide version."
                    << endl;
            } else {
                seqid.Reset(const_cast<CSeq_id*>(id.GetPointer()));
                have_seqid = true;
            }
        }
        
        if (have_gi)
            break;
    }
}

// Resolve all ids to GIs, storing them in a GI list.

CRef<CInputGiList> CBuildDatabase::x_ResolveGis(const vector<string> & ids)
{
    CRef<CInputGiList> gi_list(new CInputGiList);
    
    ITERATE(vector<string>, id, ids) {
        // There are three possibilities:
        //
        // 1. Numbers are added to the list as GIs.
        // 2. Remote services may be called to determine the most
        //    recent version.
        // 3. Non-numerical types are added to the list as Seq-ids.
        //
        // For #2, the remote service call is only made if:
        //
        // A. Remote services are enabled.
        // B. The Seq-id can have a version (only CTextseq_id types.)
        // C. The version is not present.
        
        int gi(0);
        bool specific = false;
        CRef<CSeq_id> seqid;
        
        bool worked = CheckAccession(*id, gi, seqid, specific);
        
        // If a source database is specified, try that as a backup
        // resolution mechanism.
        
        if (! worked) {
            if (m_SourceDb.NotEmpty()) {
                worked = x_ResolveFromSource(*id, seqid);
            }
        }
        
        if (! worked) {
            m_LogFile << "Did not recognize id: \"" << *id << "\"" << endl;
            continue;
        }
        
        // 1. Numeric GI
        
        if (gi != 0) {
            if (debug_mode > 5)
                m_LogFile << "Found numerical GI:" << gi << endl;
            
            gi_list->AppendGi(gi);
            continue;
        }
        
        // 2. Possible remote resolution.  We look for a GI and if
        // that is not found, try to find a Seq-id of the same type
        // (but with a version).
        
        if (m_UseRemote && (! specific)) {
            x_ResolveRemoteId(seqid, gi);
            
            if (gi != 0) {
                gi_list->AppendGi(gi);
                continue;
            }
        }
        
        // 3. Just add the Seq-id as a Seq-id.
        
        gi_list->AppendSi(*id);
    }
    
    return gi_list;
}

bool CBuildDatabase::x_ResolveFromSource(const string  & acc,
                                         CRef<objects::CSeq_id> & id)
{
    if (m_SourceDb.Empty()) {
        return false;
    }
    
    vector<int> oids;
    m_SourceDb->AccessionToOids(acc, oids);
    
    bool found(false), done(false);
    
    ITERATE(vector<int>, oid, oids) {
        list< CRef<CSeq_id> > ids = m_SourceDb->GetSeqIDs(*oid);
        
        ITERATE(list< CRef<CSeq_id> >, seqid, ids) {
            CRef<CSeq_id> s = *seqid;
            
            string S = s->AsFastaString();
            size_t pos = S.find(acc);
            
            if (pos != string::npos) {
                size_t endpos = pos + acc.size();
                
                bool start_okay = (pos == 0 || S[pos-1] == '|');
                bool end_okay = ((endpos == S.size()) ||
                                 (S[endpos] == '.' ||
                                  S[endpos] == '|'));
                
                if (start_okay && end_okay) {
                    done = true;
                }
                
                if (done || (! found)) {
                    found = true;
                    id = s;
                }
            }
            
            if (done)
                break;
        }
        
        if (done)
            break;
    }
    
    return found;
}

void CBuildDatabase::x_DupLocal()
{
    TIdToBits bitset;
    
    // Get sequence, deflines, ambiguities, and sometimes pigs.  The
    // simplest route (for WriteDB) is raw data + asn deflines, so we
    // use that when possible.
    
    CStopWatch sw(CStopWatch::eStart);
    int count = 0;
    
    for(int oid = 0; m_SourceDb->CheckOrFindOID(oid); oid++) {
        // Raw data.
        
        const char * buffer (0);
        int          slength(0);
        int          alength(0);
        
        m_SourceDb->GetRawSeqAndAmbig(oid, & buffer, & slength, & alength);
        
        CSequenceReturn seqret(*m_SourceDb, buffer);
        
        CTempString sequence(buffer, slength);
        CTempString ambig(buffer + slength, alength);
        
        // Deflines
        
        CRef<CBlast_def_line_set> headers = m_SourceDb->GetHdr(oid);
        m_DeflineCount += headers->Get().size();
        m_OIDCount ++;
        
        x_SetLinkAndMbit(headers);
        
        // Always include the taxid; although OPTIONAL, some programs
        // expect it, since the C ASN.1 loaders always emit integers.
        
        m_Taxids->FixTaxId(headers);
        
        // Now, add the sequence to the WriteDB database.
        
        m_OutputDb->AddSequence(sequence, ambig);
        m_OutputDb->SetDeflines(*headers);
        count ++;
    }
    
    if (count) {
        double t = sw.Elapsed();
        
        m_LogFile << "Duplication from source DB; duplicated "
                  << count << " sequences in " << t << " seconds." << endl;
    }
}

// This could be moved to writedb once it is tested and working.

static CConstRef<CBioseq> s_FixBioseqDeltas(CConstRef<objects::CBioseq> bs)
{
    if ((! bs->CanGetInst()) || bs->GetInst().CanGetSeq_data() ||
    	 ! bs->GetInst().CanGetExt() || ! bs->GetInst().GetExt().IsDelta()) {
        return bs;
    }
    
    if (bs->GetInst().CanGetMol() &&
        !CSeq_inst::IsNa(bs->GetInst().GetMol())) {
        CConstRef<CSeq_id> id = FindBestChoice(bs->GetId(), CSeq_id::BestRank);
        CNcbiOstrstream oss;
        oss << id->AsFastaString() << ": Protein delta sequences are not supported.";
        string msg = CNcbiOstrstreamToString(oss);
        NCBI_THROW(CMultisourceException, eArg, msg);
    }

    try {
        const CDelta_ext & dext = bs->GetInst().GetExt().GetDelta();
        
        if(dext.Get().front()->Which() != CDelta_seq::e_Literal)
        	return bs;

        typedef list< CRef< CDelta_seq > > TItems;
        
        // Don't really want to use na4, because a half byte at the
        // end of a string would require that string to be manually
        // adjusted before appending.
        
        string seq8na;
        if (bs->GetInst().CanGetLength()) {
            seq8na.reserve(bs->GetInst().GetLength());
        }
        
        string na8;
        
        ITERATE(TItems, item, dext.Get()) {
        	if(((**item).IsLoc()) && ((**item).GetLoc().IsNull())) {
                seq8na.append(1, 0x0f);
        		continue;
        	}

            const CSeq_literal & L = (**item).GetLiteral();

            if (!L.CanGetSeq_data()) {
                if (L.CanGetLength()){
                   seq8na.append(L.GetLength(), 0x0f);
                   continue;
                } else {
                   NCBI_THROW(CMultisourceException, eArg,
                      "Part of the delta sequence, including its length, is un-available.");
                }
            }
            
            if (L.GetSeq_data().IsNcbi2na()) {
                CSeqConvert::Convert(L.GetSeq_data().GetNcbi2na(),
                                     CSeqUtil::e_Ncbi2na,
                                     0,
                                     L.GetLength(),
                                     na8,
                                     CSeqUtil::e_Ncbi8na);
            } else if (L.GetSeq_data().IsNcbi4na()) {
                CSeqConvert::Convert(L.GetSeq_data().GetNcbi4na(),
                                     CSeqUtil::e_Ncbi4na,
                                     0,
                                     L.GetLength(),
                                     na8,
                                     CSeqUtil::e_Ncbi8na);
	    } else if (L.GetSeq_data().IsGap()) {
                seq8na.append(L.GetLength(), 0x0f);
            } else {
                NCBI_THROW(CMultisourceException, eArg,
                           "Unhandled type of sequence data encountered.");
            }
            
            seq8na += na8;
            na8.resize(0);
        }
        
        // Now convert back to 4na, since WriteDB does not yet handle
        // 8na sequences.
        
        int length = seq8na.size();
        vector<char> seq4na;
        CSeqConvert::Convert(seq8na,
                             CSeqUtil::e_Ncbi8na,
                             0,
                             length,
                             seq4na,
                             CSeqUtil::e_Ncbi4na);
        
        // Copy the needed fields of the CBioseq (but remove the delta
        // sequence) and add a Seq-data.
        
        CRef<CBioseq> bs2(new CBioseq);
        
        if (bs->IsSetId()) {
            bs2->SetId() = bs->GetId();
        }
        
        if (bs->IsSetDescr()) {
            bs2->SetDescr(const_cast<CSeq_descr&>(bs->GetDescr()));
        }
        
        CRef<CSeq_inst> inst(new CSeq_inst);
        
        inst->SetSeq_data().SetNcbi4na().Set().swap(seq4na);
        inst->SetMol(CSeq_inst::eMol_na);
        inst->SetLength(length);
        inst->SetRepr(CSeq_inst::eRepr_raw);
        
        bs2->SetInst(*inst);
        
        if (bs->IsSetAnnot()) {
            bs2->SetAnnot() = bs->GetAnnot();
        }
        
        bs = bs2;
    }
    catch(CInvalidChoiceSelection &) {
        NCBI_THROW(CMultisourceException, eArg,
                   "Bioseq must have Seq-data or "
                   "Delta containing only literals.");
    }
    
    return bs;
}


void CBuildDatabase::x_AddPig(CRef<objects::CBlast_def_line_set> headers)
{
	int pig = 0;
	const CBlast_def_line  &  defline = *(headers->Get().front());
	if (defline.IsSetOther_info())
		pig = defline.GetOther_info().front();

	m_OutputDb->SetPig(pig);
}

void CBuildDatabase::x_EditHeaders(CRef<objects::CBlast_def_line_set> headers)
{
    // Always include the taxid; although OPTIONAL, some programs
    // expect it, since the C ASN.1 loaders always emit integers.
    
    m_Taxids->FixTaxId(headers);
    
    // Edit the linkouts
    
    x_SetLinkAndMbit(headers);
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
void
CBuildDatabase::x_AddMasksForSeqId(const list< CRef<CSeq_id> >& ids) 
{
    if (m_MaskData.Empty()) {
        return;
    }
    
    const CMaskedRangesVector& rng = m_MaskData->GetRanges(ids);
    if (rng.empty()) {
        return;
    }

    vector <int> gis;
    ITERATE(list< CRef<CSeq_id> >, id, ids) {
        if ((*id)->IsGi()) {
            gis.push_back(GI_TO(int, (*id)->GetGi()));
        }
    }
    m_OutputDb->SetMaskData(rng, gis);
    m_FoundMatchingMasks = true;
}
#endif

bool CBuildDatabase::x_EditAndAddBioseq(CConstRef<objects::CBioseq>   bs,
                                        objects::CSeqVector         * sv,
                                        bool						  add_pig)
{
    CRef<CBlast_def_line_set> headers =
        CWriteDB::ExtractBioseqDeflines(*bs, m_ParseIDs);
    
    x_EditHeaders(headers);
    
    // Add the sequence
    if (sv) {
        m_OutputDb->AddSequence(*bs, *sv);
    } else {
        bs = s_FixBioseqDeltas(bs);
        if(bs->GetInst().CanGetSeq_data())
        	m_OutputDb->AddSequence(*bs);
        else
        	return false;
    }

    m_DeflineCount += headers->Get().size();
    m_OIDCount ++;

    if(add_pig) {
    	x_AddPig(headers);
    }
    
    m_OutputDb->SetDeflines(*headers);
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    const list< CRef<CSeq_id> > & ids = bs->GetId();
    x_AddMasksForSeqId(ids);
#endif
    return true;
}

void CBuildDatabase::x_AddOneRemoteSequence(const objects::CSeq_id & seqid,
                                            bool          & found_all,
                                            bool          & error)
{
    // Get handle and bioseq
    
    CConstRef<CBioseq> bs;
    CBioseq_Handle bsh;
    
    try {
        bsh = x_GetScope().GetBioseqHandle(seqid);
        bs = bsh.GetCompleteBioseq();
        
        if (debug_mode > 5) m_LogFile << MSerial_AsnText << *bs << endl;
    }
    catch (const CException & e) {
        m_LogFile << "Caught exception for query: "
                  << seqid.AsFastaString() << endl
                  << e.what() << endl;
        found_all = false;
        error = true;
    }
    
    if (bsh.GetState() & CBioseq_Handle::fState_not_found) {
        error = true;
    }
    
    

    CSeqVector sv(bsh);
    
    if(!x_EditAndAddBioseq(bs, & sv))
    	error = true;

    if (error) {
            if (debug_mode > 5)
                m_LogFile << "Could not find entry for: "
                          << seqid.AsFastaString() << endl;

            found_all = false;
            return;
    }
    
    if (debug_mode > 5)
        m_LogFile << "-- REMOTE: Found sequence "
                  << seqid.AsFastaString() << endl;
}

bool CBuildDatabase::x_AddRemoteSequences(CInputGiList & gi_list)
{
    CStopWatch sw(CStopWatch::eStart);
    int count = 0;
    
    bool found_all = true;
    
    int num_gis = gi_list.GetNumGis();
    int i = 0;
    
    for(i = 0; i < num_gis; i++) {
        if (m_Verbose)
            m_LogFile << "GI " << gi_list.GetKey<int>(i);
        
        // We only need to fetch here for those cases where the SeqDB
        // attempt could not translate the GI.
        
        if (gi_list.GetGiOid(i).oid == -1) {
            if (m_Verbose)
                m_LogFile << " not found locally; adding remotely." << endl;
            
            CRef<CSeq_id> id(new CSeq_id);
            id->SetGi(GI_FROM(int, gi_list.GetKey<int>(i)));
            
            bool error = false;
            
            x_AddOneRemoteSequence(*id, found_all, error);
            count++;
        } else {
            if (m_Verbose)
                m_LogFile << " found locally; not adding remotely." << endl;
        }
    }
    
    int num_seqids = gi_list.GetNumSis();
    
    for(i = 0; i < num_seqids; i++) {
        if (m_Verbose)
            m_LogFile << "Seq-id "
                      << gi_list.GetKey<string>(i);
        
        // We only need to fetch here for those cases where the SeqDB
        // attempt could not translate the GI.
        
        if (gi_list.GetSiOid(i).oid == -1) {
            if (m_Verbose)
                m_LogFile << " not found locally; adding remotely." << endl;
            
            bool error = false;
            
            string acc = gi_list.GetKey<string>(i);
            CRef<CSeq_id> id(new CSeq_id(acc));
            x_AddOneRemoteSequence(*id, found_all, error);
            count++;
        } else {
            if (m_Verbose)
                m_LogFile << " found locally; not adding remotely." << endl;
        }
    }
    
    if (count) {
        double t = sw.Elapsed();
        
        m_LogFile << "Adding sequences from remote source; added "
                  << count << " sequences in " << t << " seconds." << endl;
    }
    
    return found_all;
}

bool
CBuildDatabase::x_ReportUnresolvedIds(const CInputGiList & gi_list) const
{
    bool success = true;
    
    int num_gis = gi_list.GetNumGis();
    
    int unresolved = 0;
    
    int i;
    for(i = 0; i < num_gis; i++) {
        // We only need to fetch here for those cases where the SeqDB
        // attempt could not translate the GI.
        
        if (gi_list.GetGiOid(i).oid == -1) {
            if (m_Verbose)
                m_LogFile << "GI " << gi_list.GetKey<int>(i)
                          << " was not resolvable." << endl;
            
            success = false;
            unresolved ++;
        } else {
            if (m_Verbose)
                m_LogFile << "GI " << gi_list.GetKey<int>(i)
                          << " found locally." << endl;
        }
    }
    
    int num_seqids = gi_list.GetNumSis();
    
    for(i = 0; i < num_seqids; i++) {
        // We only need to fetch here for those cases where the SeqDB
        // attempt could not translate the GI.
        
        if (gi_list.GetSiOid(i).oid == -1) {
            if (m_Verbose)
                m_LogFile << "Seq-id "
                          << gi_list.GetKey<string>(i)
                          << " was not resolvable." << endl;
            
            unresolved ++;
            success = false;
        } else {
            if (m_Verbose)
                m_LogFile << "Seq-id "
                          << gi_list.GetKey<string>(i)
                          << " found locally." << endl;
        }
    }
    
    if (unresolved) {
        m_LogFile << "Could not resolve " << unresolved << " IDs." << endl;
    }
    
    success = false;
    unresolved ++;
    
    return success;
}

class CFastaBioseqSource : public IBioseqSource {
public:
    CFastaBioseqSource(CNcbiIstream & fasta_file,
                       bool is_protein,
                       bool parse_ids);
    
    ~CFastaBioseqSource();
    
    virtual CConstRef<CBioseq> GetNext();
    
private:
    CRef<ILineReader> m_LineReader;
    CFastaReader* m_FastaReader;
};

CFastaBioseqSource::CFastaBioseqSource(CNcbiIstream & fasta_file,
                                       bool is_protein,
                                       bool parse_ids)
    : m_FastaReader(NULL)
{
    m_LineReader.Reset(new CBufferedLineReader(fasta_file));
    
    typedef CFastaReader::EFlags TFlags;
    
    int iflags = CFastaReader::fAllSeqIds |
                 CFastaReader::fForceType |
                 CFastaReader::fParseGaps;
    
    if (is_protein) {
        iflags |= CFastaReader::fAssumeProt;
    } else {
        iflags |= CFastaReader::fAssumeNuc;
    }
    
    if (parse_ids) {
        iflags |= CFastaReader::fAllSeqIds | CFastaReader::fRequireID;
    } else {
        iflags |= CFastaReader::fNoParseID;
    }
    
    TFlags flags = (TFlags) iflags;
    
    m_FastaReader = new CFastaReader(*m_LineReader, flags);
}

CFastaBioseqSource::~CFastaBioseqSource()
{
    delete m_FastaReader;
}

CConstRef<CBioseq> CFastaBioseqSource::GetNext()
{
    CConstRef<CBioseq> rv;
    
    if (m_LineReader.NotEmpty() && ! m_LineReader->AtEOF()) {
        CRef<CSeq_entry> entry;
        try { entry = m_FastaReader->ReadOneSeq(); }
        catch (const CObjReaderParseException& e) { 
            static const string kKeyword("m_Pos = ");
            SIZE_TYPE start = NStr::Find(e.what(), kKeyword);
            SIZE_TYPE end = NStr::Find(e.what(), ")", start);
            string pos("unknown");
            if (start != NPOS && end != NPOS) {
                start += kKeyword.size();
                pos = string(e.what()).substr(start, end-start);
            }
            string msg = e.GetMsg();
            const string extra_string("CFastaReader: ");
            if (NStr::StartsWith(msg, extra_string)) {
                msg.erase(0, extra_string.size());
            }
            NCBI_THROW(CWriteDBException, eFileErr, msg);
        }
        
        if (entry.NotEmpty()) {
            _ASSERT(entry->IsSeq());
            rv.Reset(& entry->GetSeq());
        }
    }
    
    // Any failure to read a Bioseq is considered an EOF.
    
    if (rv.Empty()) {
        m_LineReader.Reset();
    }
    
    return rv;
}

bool CBuildDatabase::AddSequences(IBioseqSource & src, bool add_pig)
{
    bool found = false;
    
    CStopWatch sw(CStopWatch::eStart);
    int count = 0;
    
    CConstRef<CBioseq> bs = src.GetNext();
    
    while(bs.NotEmpty()) {
        string bioseq_id("Unknown");

        if (bs->CanGetId()) {
            const list< CRef<CSeq_id> > & ids = bs->GetId();
            if (! ids.empty() && ids.front().NotEmpty()) {
                bioseq_id.assign(ids.front()->AsFastaString());
            }
        }

        if(bs->IsAa() != m_IsProtein ){
                    bs = src.GetNext();
                    continue;
       }

        if ((bs->GetLength() == 0) || (!x_EditAndAddBioseq(bs, NULL, add_pig))){
            m_LogFile << "Ignoring sequence '" << bioseq_id
                      << "' as it has no sequence data" << endl;
            bs = src.GetNext();
            continue;
        }

        if (m_Verbose) {
            m_LogFile << "Adding bioseq from fasta; first id is: '" << bioseq_id
                << "'" << endl;
        }
        
        // No linkouts or memberships here (yet).

        found = true;
        
        count++;
        
        if (debug_mode > 5) m_LogFile << "-- FASTA: Found sequence." << endl;
        
        bs = src.GetNext();
    }
    
    if (count) {
        double t = sw.Elapsed();
        
        m_LogFile << "Adding sequences from FASTA; added "
                  << count << " sequences in " << t << " seconds." << endl;
    }
    
    return found;
}

bool CBuildDatabase::AddSequences(IRawSequenceSource & src)
{
    CStopWatch sw(CStopWatch::eStart);
    
    bool done = false;
    bool rv = false;
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // Get all column names.
    
    vector<string> all_names;
    map<int, int> in2out;
    int mask_id = -1;
    
    src.GetColumnNames(all_names);
    
    for(int i = 0; i < (int) all_names.size(); i++) {
        string name = all_names[i];
        int in_id = src.GetColumnId(name);

        // skip masking data column
        if (name == "BlastDb/MaskData") {
            mask_id = in_id;
            continue;
        }
        int out_id = m_OutputDb->FindColumn(name);
        
        if (out_id < 0) {
            out_id = m_OutputDb->CreateUserColumn(name);
        }
        
        typedef map<string,string> StringPairMap;
        const StringPairMap & meta = src.GetColumnMetaData(in_id);
        
        ITERATE(StringPairMap, iter, meta) {
            m_OutputDb->AddColumnMetaData(out_id, iter->first, iter->second);
        }
        
        in2out[in_id] = out_id;
    }
#endif    
    // Copy all data.
    
    vector<CTempString> column_blobs;
    vector<int> column_ids;
    
    int count = 0;
    
    while(! done) {
        CTempString sequence, ambiguities;
        CRef<CBlast_def_line_set> deflines;
        CMaskedRangesVector  mask_data;
        
        if (src.GetNext(sequence,
                        ambiguities,
                        deflines,
                        mask_data,
                        column_ids,
                        column_blobs)) {
            
            // Copy data
            
            _ASSERT(column_blobs.size() == column_ids.size());
            
            if (sequence.empty()) {
                NCBI_THROW(CMultisourceException, eArg,
                           "Error in raw data: no sequence");
            }
            
            if ((! ambiguities.empty()) && m_IsProtein) {
                NCBI_THROW(CMultisourceException, eArg,
                           "Error in raw data: "
                           "protein db cannot with ambiguities");
            }
            
            if (deflines.Empty()) {
                NCBI_THROW(CMultisourceException, eArg,
                           "Error in raw data: no headers provided");
            }
            
            x_EditHeaders(deflines);
            
            m_OutputDb->AddSequence(sequence, ambiguities);
            x_AddPig(deflines);
            m_OutputDb->SetDeflines(*deflines);
            
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
            for(int i = 0; i < (int)column_ids.size(); i++) {
                int in_id = column_ids[i];
                if (in_id == mask_id) continue;
                
                if (column_blobs[i].size() == 0)
                    continue;
                
                _ASSERT(in2out.find(in_id) != in2out.end());
                
                int out_id = in2out[in_id];
                
                CTempString blob_in = column_blobs[i];
                CBlastDbBlob & blob_out = m_OutputDb->SetBlobData(out_id);
                
                blob_out.Clear();
                blob_out.WriteRaw(& blob_in.data()[0], blob_in.size());
            }
            // Don't forget about the IMaskDataSource!
            vector <int> gis;  // GIs associated with this sequence
            if (!mask_data.empty() || !m_MaskData.Empty()) {
                ITERATE(CBlast_def_line_set::Tdata, defline, deflines->Get()) {
                    const list< CRef<CSeq_id> > & ids = (*defline)->GetSeqid();
                    ITERATE(list< CRef<CSeq_id> >, id, ids) {
                        if ((*id)->IsGi()) {
                            gis.push_back(GI_TO(int, (*id)->GetGi()));
                        }
                    }
                    if (!m_MaskData.Empty()) {
                        const CMaskedRangesVector rng = m_MaskData->GetRanges(ids);
                        if (!rng.empty()) {
                            mask_data.insert(mask_data.end(), rng.begin(), rng.end());  
                            m_FoundMatchingMasks = true;
                        }
                    }
                }
            }
            if (!mask_data.empty()) {
                m_OutputDb->SetMaskData(mask_data, gis);
            }
#endif
            
            rv = true;
            count ++;
        } else {
            done = true;
        }
    }
    
    if (count) {
        double t = sw.Elapsed();
        
        m_LogFile << "Adding sequences from raw db source; added "
                  << count << " sequences in " << t << " seconds." << endl;
    }
    
    return rv;
}

static void s_CreateDirectories(const string& dbname)
{
    CDirEntry dir_entry(dbname);
    string dir_name = dir_entry.GetDir(CDirEntry::eIfEmptyPath_Empty);
    if (dir_name.empty()) {
        return;
    }

    CDir d(dir_name);
    if ( !d.Exists() ) {
        if ( !d.CreatePath() ) {
            string msg("Failed to create directory '" + d.GetName() + "'");
            NCBI_THROW(CMultisourceException, eOutputFileError, msg);
        }
    }
    if (!d.CheckAccess(CDirEntry::fWrite)) {
        string msg("You do not have write permissions on '" + 
                   d.GetName() + "'");
        NCBI_THROW(CMultisourceException, eOutputFileError, msg);
    }
}

CBuildDatabase::CBuildDatabase(const string         & dbname,
                               const string         & title,
                               bool                   is_protein,
                               CWriteDB::TIndexType   indexing,
                               bool                   use_gi_mask,
                               ostream              * logfile)
    : m_IsProtein    (is_protein),
      m_KeepLinks    (false),
      m_KeepMbits    (false),
      m_Taxids       (new CTaxIdSet()),
      m_LogFile      (*logfile),
      m_UseRemote    (true),
      m_DeflineCount (0),
      m_OIDCount     (0),
      m_Verbose      (false),
      m_ParseIDs     (((indexing & CWriteDB::eFullIndex) != 0 ? true : false)),
      m_FoundMatchingMasks(false)
{
    s_CreateDirectories(dbname);
    m_LogFile << "\n\nBuilding a new DB, current time: "
              << CTime(CTime::eCurrent).AsString() << endl;
    
    m_LogFile << "New DB name:   " << dbname << endl;
    m_LogFile << "New DB title:  " << title << endl;
    const string mol_type(is_protein ? "Protein" : "Nucleotide");
    m_LogFile << "Sequence type: " << mol_type << endl;
    if (DeleteBlastDb(dbname, ParseMoleculeTypeString(mol_type))) {
        m_LogFile << "Deleted existing BLAST database with identical name." << endl;
    }
    
    CWriteDB::ESeqType seqtype =
        (is_protein ? CWriteDB::eProtein : CWriteDB::eNucleotide);
    
    m_OutputDb.Reset(new CWriteDB(dbname,
                                  seqtype,
                                  title,
                                  indexing,
                                  m_ParseIDs,
                                  use_gi_mask));
    
    // Standard 1 GB limit
    
    m_OutputDb->SetMaxFileSize(1000*1000*1000);
}

CBuildDatabase::CBuildDatabase(const string & dbname,
                               const string & title,
                               bool           is_protein,
                               bool           sparse,
                               bool           parse_seqids,
                               bool           use_gi_mask,
                               ostream      * logfile)
    : m_IsProtein    (is_protein),
      m_KeepLinks    (false),
      m_KeepMbits    (false),
      m_Taxids       (new CTaxIdSet()),
      m_LogFile      (*logfile),
      m_UseRemote    (true),
      m_DeflineCount (0),
      m_OIDCount     (0),
      m_Verbose      (false),
      m_ParseIDs     (parse_seqids),
      m_FoundMatchingMasks(false)
{
    s_CreateDirectories(dbname);
    m_LogFile << "\n\nBuilding a new DB, current time: "
              << CTime(CTime::eCurrent).AsString() << endl;
    
    m_LogFile << "New DB name:   " << dbname << endl;
    m_LogFile << "New DB title:  " << title << endl;
    const string mol_type(is_protein ? "Protein" : "Nucleotide");
    m_LogFile << "Sequence type: " << mol_type << endl;
    if (DeleteBlastDb(dbname, ParseMoleculeTypeString(mol_type))) {
        m_LogFile << "Deleted existing BLAST database with identical name." << endl;
    }
    
    CWriteDB::ESeqType seqtype =
        (is_protein ? CWriteDB::eProtein : CWriteDB::eNucleotide);
    
    CWriteDB::EIndexType ix = (sparse
                               ? CWriteDB::eSparseIndex
                               : CWriteDB::eDefault);
    
    m_OutputDb.Reset(new CWriteDB(dbname,
                                  seqtype,
                                  title,
                                  ix,
                                  m_ParseIDs,
                                  use_gi_mask));

    // Standard 1 GB limit
    
    m_OutputDb->SetMaxFileSize(1000*1000*1000);
}

CBuildDatabase::~CBuildDatabase()
{
    if (m_MaskData.NotEmpty() && !m_FoundMatchingMasks) {
        ERR_POST(Error << "No sequences matched any of the masks provided.\n"
                       << "Please ensure that the -parse_seqids option is used "
                       << "in the\nfiltering program as well as makeblastdb.");
    }
    if (!m_Taxids->HasEverFixedId()) {
        ERR_POST(Error << "No sequences matched any of the taxids provided.");
    }
}

void CBuildDatabase::SetTaxids(CTaxIdSet & taxids)
{
    m_Taxids.Reset(& taxids);
}

void CBuildDatabase::SetMaskLetters(const string & letters)
{
    m_OutputDb->SetMaskedLetters(letters);
}

CScope & CBuildDatabase::x_GetScope()
{
    if (m_Scope.Empty()) {
        if (m_ObjMgr.Empty()) {
            m_ObjMgr.Reset(CObjectManager::GetInstance());
        }
        
        m_Scope.Reset(new CScope(*m_ObjMgr));
        
        // Add default loaders (GB loader in this demo) to the scope.
        m_Scope->AddDefaults();
    }
    
    return *m_Scope;
}

void CBuildDatabase::SetSourceDb(CRef<CSeqDBExpert> seqdb)
{
    m_LogFile << "Configured source DB: " << seqdb->GetDBNameList() << endl;
    m_LogFile << "Source DB has title:  " << seqdb->GetTitle() << endl;
    m_LogFile << "Source DB time stamp: " << seqdb->GetDate() << endl;
    m_SourceDb = seqdb;
}

void CBuildDatabase::SetSourceDb(const string & src_db_name)
{
    _ASSERT(src_db_name.size());
    CRef<CSeqDBExpert> src_db(new CSeqDBExpert(src_db_name,
                                               m_IsProtein
                                               ? CSeqDB::eProtein
                                               : CSeqDB::eNucleotide));
    
    SetSourceDb(src_db);
}

void CBuildDatabase::SetLinkouts(const TLinkoutMap & linkouts,
                                 bool                keep_links)
{
    m_LogFile << "Keep Linkouts: " << (keep_links ? "T" : "F") << endl;
    MapToLMBits(linkouts, m_Id2Links);
    m_KeepLinks = keep_links;
}

void CBuildDatabase::SetMembBits(const TLinkoutMap & membbits,
                                 bool                keep_mbits)
{
    m_LogFile << "Keep MBits: " << (keep_mbits ? "T" : "F") << endl;
    MapToLMBits(membbits, m_Id2Mbits);
    m_KeepMbits = keep_mbits;
}

bool
CBuildDatabase::Build(const vector<string> & ids,
                      CNcbiIstream         * fasta_file)
{
    CStopWatch sw(CStopWatch::eStart);
    
    StartBuild();
    
    bool success = AddIds(ids);
    
    if (success) {
        success = AddFasta(*fasta_file);
    }
    
    bool success2 = EndBuild();
    
    success = success || success2;
    
    double t = sw.Elapsed();
    
    m_LogFile << "Total sequences stored: " << m_OIDCount << endl;
    m_LogFile << "Total deflines stored: " << m_DeflineCount << endl;
    
    m_LogFile << "Total time to build database: "
              << t << " seconds.\n" << endl;
    
    return success;
}

void CBuildDatabase::StartBuild()
{
}

bool CBuildDatabase::AddIds(const vector<string> & ids)
{
    
    bool success = true;
    
    // Resolve all ids to GIs, storing them in a GI list.
    
    CRef<CInputGiList> gi_list;
    
    if (m_SourceDb.NotEmpty() && ! ids.empty()) {
        gi_list = x_ResolveGis(ids);
    }
    
    // Translate the GI list.
    
    if (gi_list.NotEmpty() &&
        (gi_list->GetNumGis() || gi_list->GetNumSis())) {
        
        // The process of constructing a SeqDB object with a user GI
        // list causes translation of the User GI list, and is the
        // fastest way of performing such a translation in bulk.  It
        // is possible to iterate the list afterwards to determine
        // what subset of it that has been translated; non-translated
        // GIs will need to be fetched using a data loader.
        //
        // It is not necessary, however, to iterate the GI list to
        // find OIDs that correspond to the filtered DB; these can be
        // found using OID iteration over SeqDB, which produces a
        // better ordering inasmuch as the reads from the source
        // sequence data will be sequential on disk.
        
        _ASSERT(m_SourceDb.NotEmpty());
        
        CRef<CSeqDBExpert> filtered
            (new CSeqDBExpert(m_SourceDb->GetDBNameList(),
                              m_SourceDb->GetSequenceType(),
                              &* gi_list));
        
        m_SourceDb = filtered;
        
        // Add all local database sequences to the output DB.
        
        x_DupLocal();
        
        if (m_Verbose) {
            // Map oid to gi.
            map<int,int> seen_it;
            
            for(int i = 0; i < gi_list->GetNumGis(); i++) {
                int this_oid = gi_list->GetGiOid(i).oid;
                int this_gi = gi_list->GetGiOid(i).gi;
            
                if (this_oid != -1) {
                    if (seen_it.find(this_oid) == seen_it.end()) {
                        seen_it[this_oid] = this_gi;
                    } else {
                        m_LogFile << "GI " << this_gi
                                  << " is duplicate of GI "
                                  << seen_it[this_oid]
                                  << endl;
                    }
                }
            }
        }
    }
    
    if (gi_list.NotEmpty()) {
        if (m_UseRemote) {
            success = x_AddRemoteSequences(*gi_list);
        } else {
            success = x_ReportUnresolvedIds(*gi_list);
        }
    }
    
    return success;
}

bool CBuildDatabase::AddFasta(CNcbiIstream & fasta_file)
{
    // Add any fasta sequences as well.
    bool success = true;
    
    if (fasta_file) {
        CFastaBioseqSource fbs(fasta_file,
                               m_IsProtein,
                               m_ParseIDs);
        
        try {
            success = AddSequences(fbs);
        }
        catch (...) {
            EndBuild(true);
            throw;
        }
    }
    return success;
}

bool CBuildDatabase::EndBuild(bool erase)
{
    try {
        m_OutputDb->Close();
        return x_EndBuild(erase, NULL);
    } catch (const CException& e) {
        return x_EndBuild(true, erase ? NULL : &e);
    } catch (exception& e) {
        NCBI_EXCEPTION_VAR(ex, CException, eUnknown, e.what());
        return x_EndBuild(true, erase ? NULL : &ex);
    } catch (...) {
        NCBI_EXCEPTION_VAR(ex, CException, eUnknown, "Non-standard exception");
        return x_EndBuild(true, erase ? NULL : &ex);
    }
}

bool CBuildDatabase::x_EndBuild(bool erase, const CException * close_exception)
{
    bool success = false;

    vector<string> vols;
    vector<string> files;
    
    m_OutputDb->ListVolumes(vols);
    m_OutputDb->ListFiles(files);
    
    m_LogFile << endl;
    
    _ASSERT(vols.empty() == files.empty());
    
    if (vols.empty()) {
        m_LogFile << "No volumes were created because no sequences were found."
                  << endl;
        
        success = false;
    } else {
        ITERATE(vector<string>, iterv, vols) {
            m_LogFile << "volume: " << *iterv << endl;
        }
        
        m_LogFile << endl;
        ITERATE(vector<string>, iterf, files) {
            m_LogFile << "file: " << *iterf << endl;
            if (erase) {
                CFile(*iterf).Remove();
            }
        }
    }
    
    m_LogFile << endl;

    if (close_exception) {
        NCBI_RETHROW(*close_exception, CWriteDBException, eArgErr,
                     "Can not close files.");
    }
    
    return success;
}


static void
s_SetDeflineBits(objects::CBlast_def_line & defline,
                 TIdToBits       & bitmap,
                 bool              keep_old,
                 bool              is_memb,
                 vector<string>  & keys)
{
    bool found = false;
    int value = 0;
    
    ITERATE(vector<string>, key, keys) {
        if (! key->size())
            continue;
        
        TIdToBits::iterator item = bitmap.find(*key);
        
        if (item != bitmap.end()) {
            found = true;
            value |= item->second;
        }
    }
    
    if (found) {
        list<int> & linkv = (is_memb
                             ? defline.SetMemberships()
                             : defline.SetLinks());
            
        if (! keep_old) {
            linkv.clear();
        }
            
        if (linkv.empty()) {
            linkv.push_back(value);
        } else {
            linkv.front() |= value;
        }
    } else {
        if (! keep_old) {
            if (is_memb) {
                defline.ResetMemberships();
            } else {
                defline.ResetLinks();
            }
        }
    }
}

void 
CBuildDatabase::x_SetLinkAndMbit(CRef<objects::CBlast_def_line_set> headers)
{
    vector<string> keys;
    
    NON_CONST_ITERATE(CBlast_def_line_set::Tdata, iter, headers->Set()) {
        CBlast_def_line & defline = **iter;
        GetDeflineKeys(defline, keys);
        
        s_SetDeflineBits(defline, m_Id2Links, m_KeepLinks, false, keys);
        s_SetDeflineBits(defline, m_Id2Mbits, m_KeepMbits, true, keys);
    }
}

void CBuildDatabase::SetMaxFileSize(Uint8 max_file_size)
{
    m_OutputDb->SetMaxFileSize(max_file_size);
}

int
CBuildDatabase::RegisterMaskingAlgorithm(EBlast_filter_program program, 
                                         const string        & options,
                                         const string        & name)
{
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    return m_OutputDb->RegisterMaskAlgorithm(program, options, name);
#else
    return 0;
#endif
}

int
CBuildDatabase::RegisterMaskingAlgorithm(const string        & program, 
                                         const string        & description,
                                         const string        & options)
{
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    return m_OutputDb->RegisterMaskAlgorithm(program, description, options);
#else
    return 0;
#endif
}

void CBuildDatabase::SetMaskDataSource(IMaskDataSource & ranges)
{
    m_MaskData.Reset(& ranges);
}

END_NCBI_SCOPE
