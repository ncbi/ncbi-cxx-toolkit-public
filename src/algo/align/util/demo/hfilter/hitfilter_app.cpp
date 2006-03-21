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
#include <util/util_exception.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static const string g_m8("m8"), g_AsnTxt("asntxt"), g_AsnBin("asnbin");

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

    argdescr->AddDefaultKey("fmt_in", "fmt_in", "Input format",
                            CArgDescriptions::eString, g_m8);

    argdescr->AddOptionalKey("file_in", "file_in", "Input file (stdin otherwise)",
                             CArgDescriptions::eInputFile,
                             CArgDescriptions::fBinary);

    argdescr->AddOptionalKey("file_out", "file_out", "Output file (stdout otherwise)",
                             CArgDescriptions::eOutputFile,
                             CArgDescriptions::fBinary);    
    
    argdescr->AddDefaultKey("fmt_out", "fmt_out", "Output format",
                            CArgDescriptions::eString, g_m8);
    
    argdescr->AddDefaultKey("hits_per_chunk", "hits_per_chunk", 
                            "Input is split into chunks with the number of hits "
                            "per chunk limited by this parameter.",
                            CArgDescriptions::eInteger, 
                            "1000000");

    argdescr->AddOptionalKey("ids", "ids", "Table to rename sequence IDs.",
                             CArgDescriptions::eInputFile);

    CArgAllow_Strings* constrain_format = new CArgAllow_Strings;
    constrain_format->Allow(g_m8)->Allow(g_AsnTxt)->Allow(g_AsnBin);
    argdescr->SetConstraint("fmt_in", constrain_format);
    argdescr->SetConstraint("fmt_out", constrain_format);

    SetupArgDescriptions(argdescr.release());
}


void CAppHitFilter::x_LoadIDs(CNcbiIstream& istr)
{
    m_IDs.clear();
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
    }
}

const CAppHitFilter::THit::TCoord kMinOutputHitLen = 75;

void CAppHitFilter::x_ReadInputHits(THitRefs* phitrefs)
{
    const CArgs& args = GetArgs();
    const string fmt_in = args["fmt_in"].AsString();
    const string fmt_out = args["fmt_out"].AsString();
    const THit::TCoord min_len = args["min_len"].AsInteger();
    const double min_idty = args["min_idty"].AsDouble();

    CNcbiIstream& istr = args["file_in"]? args["file_in"].AsInputFile(): cin;

    size_t count = 0;
    const size_t kReport = 100000;
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
                if(++count % kReport == 0) {
                    //cerr << "Read " << count << " hits" << endl;
                }
            }
        }
    }
    else {
        const ESerialDataFormat sfmt ();

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
                if(++count % kReport == 0) {
                    //cerr << "Read " << count << " hits" << endl;
                }
            }
        }
    }
    cerr << "Total accepted hits = " << phitrefs->size() << endl;
}


void CAppHitFilter::x_DumpOutput(const THitRefs& hitrefs)
{    
    const CArgs& args = GetArgs();
    const string fmt = args["fmt_out"].AsString();

    CNcbiOstream& ostr = args["file_out"]? args["file_out"].AsOutputFile(): cout;

    if(fmt == g_m8) {
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
            //cerr << h << endl;

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
                CRef<CSeq_id> id;
                const string strid (h.GetId(where)->GetSeqIdString(true));
                TMapIds::const_iterator im = m_IDs.find(strid), ime = m_IDs.end();
                if(im == ime) {
                    id.Reset(new CSeq_id());
                    id->Assign(*h.GetId(where));
                }
                else {
                    id.Reset(new CSeq_id(im->second));
                }
                ids.push_back(id);
            }            

            CRef<CSeq_align> seq_align (new CSeq_align);
            seq_align->SetType(CSeq_align::eType_disc);
            seq_align->SetSegs().SetDenseg(*ds);
            align_list.push_back(seq_align);
        }

        if(fmt_txt) {
            ostr << MSerial_AsnText << *seq_annot << endl;
        }
        else {
            ostr << MSerial_AsnBinary << *seq_annot;
        }
    }
}


template<class TRef>
bool s_PNullRef(const TRef& hr) {
    return hr.IsNull();
}


bool s_PHitRefScore(const CAppHitFilter::THitRef& lhs, 
                    const CAppHitFilter::THitRef& rhs) {
    return lhs->GetScore() > rhs->GetScore();
}

int CAppHitFilter::Run()
{    
    const CArgs& args = GetArgs();
    const string fmt_in = args["fmt_in"].AsString();
    const string fmt_out = args["fmt_out"].AsString();

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

    THitRefs all;
    x_ReadInputHits(&all);
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
        CStopWatch sw (CStopWatch::eStart);
        cerr << " cur set size = " << (ii_hi - ii_beg) 
             << " total processed = " << (ii - ii_beg) << " ... ";
        CHitFilter<THit>::s_RunGreedy(ii_beg, ii_hi, kMinOutputHitLen);
        ii_hi = remove_if(ii_beg, ii_hi, s_PNullRef<THitRef>);
        cerr << "done in " << sw.Elapsed() << " seconds" << endl;
    }
    all.erase(ii_hi, ii_end);
    
    x_DumpOutput(all);

    return 0;
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
