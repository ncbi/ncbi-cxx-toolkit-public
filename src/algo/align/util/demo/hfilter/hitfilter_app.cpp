/* $Id$
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
 * Author:  Yuri Kapustin
 *
 * File Description:  HitFilter application
 *                   
*/

#include <ncbi_pch.hpp>

#include "hitfilter_app.hpp"

#include <algo/align/util/hit_filter.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <util/util_exception.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static const string g_m8("m8"), g_AsnTxt("asntxt"), g_AsnBin("asnbin");
static const CAppHitFilter::THit::TCoord kMinOutputHitLen = 75;

void CAppHitFilter::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramName(),
                              "HitFilter v.2.0.0");

    argdescr->AddDefaultKey("min_idty", "min_idty", 
                            "Minimal input hit identity",
                            CArgDescriptions::eDouble, "0.95");
    
    argdescr->AddDefaultKey("min_len", "min_len", 
                            "Minimal input hit length",
                            CArgDescriptions::eInteger, "500");
    
    argdescr->AddDefaultKey("retain_overlap", "retain_overlap", 
                            "Min overlap to retain in kilobases (0=OFF)",
                            CArgDescriptions::eInteger, "0");

    argdescr->AddDefaultKey("fmt_in", "fmt_in", "Input format",
                            CArgDescriptions::eString, g_m8);

    argdescr->AddOptionalKey("file_in", "file_in", "Input file (stdin otherwise)",
                             CArgDescriptions::eInputFile,
                             CArgDescriptions::fBinary);

    argdescr->AddOptionalKey("constraints", "constraints",
                             "Binary ASN file with constraining alignments",
                             CArgDescriptions::eInputFile,
                             CArgDescriptions::fBinary);

    argdescr->AddOptionalKey("file_out", "file_out", "Output file (stdout otherwise)",
                             CArgDescriptions::eOutputFile,
                             CArgDescriptions::fBinary);    
    
    argdescr->AddOptionalKey("m", "m","Text description/comment to add to the output",
                             CArgDescriptions::eString);
    
    argdescr->AddDefaultKey("fmt_out", "fmt_out", "Output format",
                            CArgDescriptions::eString, g_m8);
    
    argdescr->AddDefaultKey("hits_per_chunk", "hits_per_chunk", 
                            "Input is split into chunks with the number of hits "
                            "per chunk limited by this parameter.",
                            CArgDescriptions::eInteger, 
                            "5000000");
    
    argdescr->AddDefaultKey("coord_margin", "coord_margin", 
                            "Larger values of this argument will result in less "
                            "RAM used but longer running times.",
                            CArgDescriptions::eInteger, 
                            "1");

    argdescr->AddOptionalKey("ids", "ids", "Table to rename sequence IDs.",
                             CArgDescriptions::eInputFile);

    CArgAllow_Strings* constrain_format = new CArgAllow_Strings;
    constrain_format->Allow(g_m8)->Allow(g_AsnTxt)->Allow(g_AsnBin);
    argdescr->SetConstraint("fmt_in", constrain_format);
    argdescr->SetConstraint("fmt_out", constrain_format);
    
    CArgAllow* constrain_minlen = new CArgAllow_Integers(kMinOutputHitLen, 1000000);
    argdescr->SetConstraint("min_len", constrain_minlen);
    
    CArgAllow* constrain_minidty = new CArgAllow_Doubles(0.0, 1.0);
    argdescr->SetConstraint("min_idty", constrain_minidty);

    SetupArgDescriptions(argdescr.release());
}


void CAppHitFilter::x_LoadIDs(CNcbiIstream& istr)
{
    m_IDs.clear();
    m_IDRevs.clear();
    while(istr) {
        string ctgid, accver;
        istr >> ctgid;
        if(ctgid.size() == 0) {
            break;
        }
        istr >> accver;
        if(accver.size() == 0) {
            break;
        }
        m_IDs[ctgid] = accver;

        TMapIdPairs::iterator ie = m_IDRevs.end(), im = m_IDRevs.find(accver);
        if(im == ie) {
            SBuildIDs build_ids;
            build_ids.m_id[0] = build_ids.m_id[1] = ctgid;
            m_IDRevs[accver] = build_ids;
        }
        else {
            SBuildIDs& bi = im->second;
            bi.m_id[1] = ctgid;
        }
    }
}


void CAppHitFilter::x_ReadInputHits(THitRefs* phitrefs)
{
    const CArgs& args = GetArgs();

    const string        fmt_in       = args["fmt_in"].AsString();
    const string        fmt_out      = args["fmt_out"].AsString();
    const THit::TCoord  min_len      = args["min_len"].AsInteger();
    const double        min_idty     = args["min_idty"].AsDouble();

    CNcbiIstream& istr = args["file_in"]? args["file_in"].AsInputFile(): cin;

    if(fmt_in == g_m8) {
        while(istr) {
            char line [1024];
            istr.getline(line, sizeof line, '\n');
            string s (NStr::TruncateSpaces(line));
            if(s.size()) {

                THitRef hit (new THit(s.c_str()));
                if(hit->GetIdentity() >= min_idty && hit->GetLength() >= min_len) {
                    phitrefs->push_back(hit);
                }
            }
        }
    }
    else {
        const bool  parse_aln = fmt_out != g_m8;

        CObjectIStream* in_ptr =  CObjectIStream::Open(fmt_in == g_AsnTxt? 
                                  eSerial_AsnText: eSerial_AsnBinary, istr);
        auto_ptr<CObjectIStream> in (in_ptr);

        while (!in->EndOfData()) {

            CRef<CSeq_annot> sa (new CSeq_annot);
            *in >> *sa;

            typedef list<CRef<CSeq_align> > TSeqAlignList;
            const TSeqAlignList& sa_list = sa->GetData().GetAlign();
            ITERATE(TSeqAlignList, ii, sa_list) {
                    
                CRange<TSeqPos> r = (*ii)->GetSeqRange(0);
                if(r.GetTo() - r.GetFrom() >= min_len) {

                    THitRef hit (new THit(**ii, parse_aln));
                    if(hit->GetIdentity() >= min_idty) {
                        if(hit->GetQueryStrand() == false) {
                            hit->FlipStrands();
                        }
                        phitrefs->push_back(hit);
                    }
                }
            }
        }
    }
}


void CAppHitFilter::x_DumpOutput(const THitRefs& hitrefs)
{    
    const CArgs& args = GetArgs();
    const string fmt = args["fmt_out"].AsString();

    CNcbiOstream& ostr = args["file_out"]? args["file_out"].AsOutputFile(): cout;

    string comment (args["m"]? args["m"].AsString(): "");

    if(fmt == g_m8) {

        if(comment.size() > 0) {
            ostr << "# " << comment << endl;
        }

        ITERATE(THitRefs, ii, hitrefs) {
            const THit& hit = **ii;
            ostr << hit << endl;
        }
    }
    else {

        CRef<CSeq_annot> seq_annot (new CSeq_annot);
        CSeq_annot::TData::TAlign& align_list = seq_annot->SetData().SetAlign();

        const bool fmt_txt (fmt == g_AsnTxt);
        ITERATE(THitRefs, ii, hitrefs) {
            
            const THit& h = **ii;

            CRef<CDense_seg> ds (new CDense_seg);
            const ENa_strand query_strand = h.GetQueryStrand()? eNa_strand_plus: 
                eNa_strand_minus;
            const ENa_strand subj_strand = h.GetSubjStrand()? eNa_strand_plus: 
                eNa_strand_minus;
            const string xcript (CAlignShadow::s_RunLengthDecode(h.GetTranscript()));

            ds->FromTranscript(h.GetQueryStart(), query_strand,
                              h.GetSubjStart(), subj_strand,
                              xcript);

            if(query_strand == eNa_strand_plus  && subj_strand == eNa_strand_plus) {
                ds->ResetStrands();
            }

            vector< CRef< CSeq_id > > &ids = ds->SetIds();
            for(Uint1 where = 0; where < 2; ++where) {

                CRef<CSeq_id> id (new CSeq_id);
                id->Assign(*h.GetId(where));
                
                /*
                const string strid (h.GetId(where)->GetSeqIdString(true));
                TMapIds::const_iterator im = m_IDs.find(strid), ime = m_IDs.end();
                if(im == ime) {
                    id.Reset(new CSeq_id());
                    id->Assign(*h.GetId(where));
                }
                else {
                    id.Reset(new CSeq_id(im->second));
                }
                */

                ids.push_back(id);
            }

            CDense_seg::TScores& scores = ds->SetScores();
            CRef<CScore> score (new CScore);
            score->SetValue().SetReal(h.GetScore());
            scores.push_back(score);

            CRef<CSeq_align> seq_align (new CSeq_align);
            seq_align->SetType(CSeq_align::eType_partial);
            seq_align->SetSegs().SetDenseg(*ds);
            align_list.push_back(seq_align);
        }

        if(comment.size() > 0) {
            seq_annot->AddComment(comment);
        }

        if(fmt_txt) {
            ostr << MSerial_AsnText << *seq_annot << endl;
        }
        else {
            ostr << MSerial_AsnBinary << *seq_annot;
        }
    }
}


bool s_PHitRefScore(const CAppHitFilter::THitRef& lhs, 
                    const CAppHitFilter::THitRef& rhs) {
    return lhs->GetScore() > rhs->GetScore();
}

int CAppHitFilter::Run()
{    
    const CArgs& args = GetArgs();
    const string fmt_in                   = args["fmt_in"].AsString();
    const string fmt_out                  = args["fmt_out"].AsString();
    const THit::TCoord min_len            = args["min_len"].AsInteger();
    const double min_idty                 = args["min_idty"].AsDouble();
    const THit::TCoord retain_overlap     = 1024 * args["retain_overlap"].AsInteger();

    if((fmt_out == g_AsnTxt || fmt_out == g_AsnBin) &&
       (fmt_in != g_AsnTxt && fmt_in != g_AsnBin)) 
    {
        NCBI_THROW(CAppHitFilterException, 
                   eGeneral, 
                   "For ASN output, input must also be in ASN");
    }

    if(args["ids"]) {
        x_LoadIDs(args["ids"].AsInputFile());
    }

    const size_t margin = args["coord_margin"].AsInteger();

    THitRefs restraint;
    if(args["constraints"]) {
        x_LoadConstraints(args["constraints"].AsInputFile(), restraint);
    }

    THitRefs all;
    x_ReadInputHits(&all);

    copy(restraint.begin(), restraint.end(), back_inserter(all));
    sort(all.begin(), all.end(), s_PHitRefScore);

    const size_t M = args["hits_per_chunk"].AsInteger();
    const size_t dim = all.size();
    size_t m = min(dim, M);
    const THitRefs::iterator ii_beg = all.begin(), ii_end = all.end();
    THitRefs::iterator ii_hi = ii_beg, ii = ii_beg;
    while(ii < ii_end) {

        THitRefs::iterator ii_dst = ii + m;
        if(ii_dst > ii_end) {
            ii_dst = ii_end;
        }

        if(ii_hi < ii) {
            copy(ii, ii_dst, ii_hi);
            ii_hi += ii_dst - ii;
            ii = ii_dst;
        }
        else {
            ii_hi = ii = ii_dst;
        }
        THitRefs hits_new;
        CHitFilter<THit>::s_RunGreedy(ii_beg, ii_hi, 
                                      &hits_new, min_len, 
                                      min_idty, margin,
                                      retain_overlap);
        sort(hits_new.begin(), hits_new.end(), s_PHitRefScore);
        THitRefs::iterator ii_hi0 = ii_hi;
        ii_hi = remove_if(ii_beg, ii_hi, CHitFilter<THit>::s_PNullRef);
        THitRefs::iterator jj = hits_new.begin(), jje = hits_new.end();
        for(;jj != jje && ii_hi != ii_hi0; *ii_hi++ = *jj++);
        if(jj != jje) {
            cerr << "Warning: space from eliminated alignments "
                 << "not enough for all splits. " << endl;
        }
    }
    all.erase(ii_hi, ii_end);
    
    x_DumpOutput(all);

    return 0;
}


void CAppHitFilter::x_LoadConstraints(CNcbiIstream& istr, THitRefs& all)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope (new CScope (*om));
    scope->AddDefaults();

    CRef<CSeq_annot> seq_annot (new CSeq_annot);
    istr >> MSerial_AsnBinary >> *seq_annot;
    //istr >> MSerial_AsnText >> *seq_annot;

    typedef list<CRef<CSeq_align> > TSeqAlignList;
    TSeqAlignList& sa_list = seq_annot->SetData().SetAlign();
    THit::TCoord maxlen = 0;
    NON_CONST_ITERATE(TSeqAlignList, ii, sa_list) {
                    
        CRef<CSeq_align> seq_align = *ii;

        THitRef hit (new THit(*seq_align, true));

        for(Uint1 where = 0; where < 2; ++where) {

            CRef<CSeq_id> id;

            CConstRef<CSeq_id> id0 (hit->GetId(where)); 
            string accver;
            if(id0->IsGi()) {
                const int gi = id0->GetGi();
                accver = sequence::GetAccessionForGi(gi, *scope);
            }
            else {
                const string seqidstr = id0->AsFastaString();
                const int gi = sequence::GetGiForAccession(seqidstr, *scope);
                accver = sequence::GetAccessionForGi(gi, *scope);
            }

            TMapIdPairs::const_iterator im = m_IDRevs.find(accver), 
                ime = m_IDRevs.end();
            if(im == ime) {
                id.Reset(new CSeq_id());
                id->Assign(*(hit->GetId(where)));
            }
            else {
                const string ctgid = string("lcl|") + im->second.m_id[where];
                id.Reset(new CSeq_id(ctgid));
            }
            hit->SetId(where, id);
        }            

        if(hit->GetQueryStrand() == false) {
            hit->FlipStrands();
        }

        hit->SetScore(0.5 * numeric_limits<float>::max());

        all.push_back(hit);

        if(hit->GetLength() > maxlen) {
            maxlen = hit->GetLength();
        }
    }

    float score_factor = 0.25 / maxlen;
    NON_CONST_ITERATE(THitRefs, ii, all) {
        THitRef& h = *ii;
        h->SetScore(h->GetScore() * (1 + score_factor * h->GetLength()));
    }
}


void CAppHitFilter::Exit()
{
    return;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CAppHitFilter().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2006/10/16 16:15:20  kapustin
 * ASN element output type changed from disc to partial
 *
 * Revision 1.13  2006/06/27 14:25:21  kapustin
 * Introduce retain_overlap (min overlap to retain) parameter
 *
 * Revision 1.12  2006/06/01 19:51:36  kapustin
 * Introduce coordinate margin argument to control RAM/speed balance
 *
 * Revision 1.11  2006/05/22 15:33:14  kapustin
 * Apply length and identity cutoffs on the output
 *
 * Revision 1.10  2006/04/17 19:33:23  kapustin
 * Advance hfilter application
 *
 * Revision 1.9  2006/03/23 22:01:53  kapustin
 * Support external alignment constraints
 *
 * Revision 1.8  2006/03/22 13:54:55  kapustin
 * Use non-templated predicate to work around some intractable compilers
 *
 * Revision 1.7  2006/03/21 16:19:12  kapustin
 * Add multiple greedy reconciliation algorithm for pairwise alignment filtering
 *
 * Revision 1.6  2005/07/28 12:29:35  kapustin
 * Convert to non-templatized classes where causing compilation incompatibility
 *
 * Revision 1.5  2005/07/27 18:55:46  kapustin
 * Advance the demo to print query and subj coverages
 *
 * Revision 1.4  2005/04/18 15:24:48  kapustin
 * Split CAlignShadow into core and blast tabular representation
 *
 * Revision 1.3  2004/12/22 21:26:18  kapustin
 * Move friend template definition to the header. Declare explicit 
 * specialization.
 *
 * Revision 1.2  2004/12/21 22:45:19  kapustin
 * Temporarily comment out the code
 *
 * Revision 1.1  2004/12/21 20:07:47  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
