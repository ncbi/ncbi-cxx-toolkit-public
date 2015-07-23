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
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/general/Object_id.hpp>
#include <util/util_exception.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace {
    const string g_m8("m8"), g_AsnTxt("asntxt"), g_AsnBin("asnbin");

    const string kMode_Pairwise ("pairwise");
    const string kMode_Multiple ("multiple");

    const CAppHitFilter::THit::TCoord kMinHitLen (10);

    const double kBigDbl(0.5 * numeric_limits<float>::max());
    const string kBoth("strict");
    const string kQuery("query");
    const string kSubj("subject");
}

void CAppHitFilter::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramName(),
                              "HitFilter v.2.0.2");

    argdescr->AddDefaultKey("mode", "mode", 
                            "Specify whether the hits should be resolved in pairs "
                            "or as a single set.",
                            CArgDescriptions::eString, 
                            kMode_Multiple);

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

    argdescr->AddFlag("sas", "Assume seq-align-set as the top-level structure "
                      "for the input ASN hits", true);

    argdescr->AddDefaultKey("merge", "merge",
                            "Merge abutting alignments unless the merged "
                            "alignment overlap length ratio is greater "
                            "than this parameter. Any negative value will "
                            "turn merging off.",
                            CArgDescriptions::eDouble,
                            "-1.0");

    argdescr->AddOptionalKey("constraints", "constraints",
                             "Binary ASN file with constraining alignments",
                             CArgDescriptions::eInputFile,
                             CArgDescriptions::fBinary);

    argdescr->AddOptionalKey("file_out", "file_out", "Output file (stdout otherwise)",
                             CArgDescriptions::eOutputFile,
                             CArgDescriptions::fBinary);    

    argdescr->AddOptionalKey("m", "m",
                             "Text description/comment to add to the output",
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

    argdescr->AddDefaultKey("ut", "uniqueness_type", 
                            "uniqueness type (strict, query, or subject)",
                            CArgDescriptions::eString, 
                            "strict");
    CArgAllow_Strings* unique_type = new CArgAllow_Strings;
    unique_type->Allow("strict")->Allow("query")->Allow("subject");
    argdescr->SetConstraint("ut", unique_type);

    argdescr->AddFlag("keep_strands",
                      "Keep plus-plus strands"
                     );

    argdescr->AddFlag("no_output_constraint",
                      "Do not output constraints"
                     );
    CArgAllow_Strings* constrain_format = new CArgAllow_Strings;
    constrain_format->Allow(g_m8)->Allow(g_AsnTxt)->Allow(g_AsnBin);
    argdescr->SetConstraint("fmt_in", constrain_format);
    argdescr->SetConstraint("fmt_out", constrain_format);

    CArgAllow* constrain_minlen = new CArgAllow_Integers(kMinHitLen, 1000000);
    argdescr->SetConstraint("min_len", constrain_minlen);

    CArgAllow* constrain_minidty = new CArgAllow_Doubles(0.0, 1.0);
    argdescr->SetConstraint("min_idty", constrain_minidty);

    CArgAllow* constrain_merge = new CArgAllow_Doubles(-1.0, 1.0);
    argdescr->SetConstraint("merge", constrain_merge);

    CArgAllow_Strings* constrain_mode = new CArgAllow_Strings;
    constrain_mode->Allow(kMode_Pairwise)->Allow(kMode_Multiple);
    argdescr->SetConstraint("mode", constrain_mode);

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


void CAppHitFilter::x_ReadInputHits(THitRefs* phitrefs, bool one_pair)
{
    const CArgs& args = GetArgs();

    const string        fmt_in       = args["fmt_in"].AsString();
    const string        fmt_out      = args["fmt_out"].AsString();
    const THit::TCoord  min_len      = args["min_len"].AsInteger();
    const double        min_idty     = args["min_idty"].AsDouble();

    CNcbiIstream& istr = args["file_in"]? args["file_in"].AsInputFile(): cin;

    phitrefs->clear();

    if(fmt_in == g_m8) {

        static string firstline;
        THit::TId id_query, id_subj;

        if(one_pair && firstline.size()) {

            THitRef hit (new THit(firstline.c_str()));
            if(hit->GetIdentity() >= min_idty && hit->GetLength() >= min_len) {
                phitrefs->push_back(hit);
                id_query = hit->GetQueryId();
                id_subj = hit->GetSubjId();
            }
            firstline.resize(0);
        }

        while(istr) {

            string line;
            getline(istr, line);
            string s (NStr::TruncateSpaces(line));

            if(s.size()) {

                THitRef hit (new THit(s.c_str()));

                if(one_pair) {

                    if(id_query.IsNull()) {
                        id_query = hit->GetQueryId();
                        id_subj = hit->GetSubjId();
                    }
                    else if( false == id_query -> Match(*(hit->GetQueryId()))
                             || false == id_subj  -> Match(*(hit->GetSubjId())) )
                    {
                        if(phitrefs->size()) {
                            firstline = s;
                            break;
                        }
                        else {
                            id_query = hit->GetQueryId();
                            id_subj = hit->GetSubjId();
                        }
                    }
                }

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

        const bool assume_sas (args["sas"]);

        while (!in->EndOfData()) {

            if(assume_sas) {

                CRef<CSeq_align_set> sas (new CSeq_align_set);
                *in >> *sas;
                const TSeqAlignList& sa_list (sas->Get());
                ITERATE(TSeqAlignList, ii, sa_list) {
                    CRef<CSeq_align> seq_align (*ii);
                    x_IterateSeqAlignList(seq_align->GetSegs().GetDisc().Get(), 
                                          phitrefs, parse_aln, min_len, min_idty);
                }
            }
            else {

                CRef<CSeq_annot> seq_annot(new CSeq_annot);
                *in >> *seq_annot;
                const TSeqAlignList& sa_list (seq_annot->GetData().GetAlign());
                x_IterateSeqAlignList(sa_list, phitrefs, parse_aln, 
                                      min_len, min_idty);
            }
        }
    }
    if(one_pair && phitrefs->size()) {

        // check input validity

        typedef set<string> TStringSet;
        static TStringSet idtags;

        const string strid_query (phitrefs->front()->GetId(0)->GetSeqIdString(true));
        const string strid_subj (phitrefs->front()->GetId(1)->GetSeqIdString(true));
        const string tag (strid_subj + "$_#_&" + strid_query);
        if(idtags.end() != idtags.find(tag)) {
            NCBI_THROW(CException, eUnknown, 
                       "In pairwise mode input hits must be collated "
                       "by query and subject.");
        }
        else {
            idtags.insert(tag);
        }
    }
}


void CAppHitFilter::x_IterateSeqAlignList(const TSeqAlignList& sa_list,
                                          THitRefs* phitrefs, 
                                          bool parse_aln,
                                          const THit::TCoord& min_len,
                                          const double& min_idty) const

{
    ITERATE(TSeqAlignList, ii, sa_list) {

        const CRange<TSeqPos> r ((*ii)->GetSeqRange(0));
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
        ostr << "#"
             << "QueryId"
             << "\tTargetId"
             << "\tPercentIdent"
             << "\tAlignLen"
             << "\tNumMismatches"
             << "\tNumGapOpenings"
             << "\tQrySeqStart"
             << "\tQrySeqStop"
             << "\tTgtSeqStart"
             << "\tTgtSeqStop"
             << "\te-value"
             << "\tbit score"
             << endl;

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

            bool no_output_constraint = args["no_output_constraint"].HasValue();
            if (no_output_constraint && h.GetScore() >= kBigDbl) {
                continue;
            }

            CRef<CDense_seg> ds (new CDense_seg);
            const ENa_strand query_strand = h.GetQueryStrand()? eNa_strand_plus: 
                eNa_strand_minus;
            const ENa_strand subj_strand = h.GetSubjStrand()? eNa_strand_plus: 
                eNa_strand_minus;
            const string xcript (CAlignShadow::s_RunLengthDecode(h.GetTranscript()));

            ds->FromTranscript(h.GetQueryStart(), query_strand,
                              h.GetSubjStart(), subj_strand,
                              xcript);
                
            bool is_gap = false;
            if (ds->GetNumseg() == 1) {
                for (int i = 0; i < ds->GetDim(); i++) {
                    if (ds->GetStarts()[i] == -1) {
                        is_gap = true;
                        break;
                    }
                }
                if (is_gap) {
                    continue;
                }
            }
            else if (ds->GetNumseg() == 0) {
                continue;
            }
                

            bool keep_strands = args["keep_strands"].HasValue();
            if(!keep_strands && query_strand == eNa_strand_plus  && subj_strand == eNa_strand_plus) {
                ds->ResetStrands();
            }

            vector< CRef< CSeq_id > > &ids = ds->SetIds();
            for(Uint1 where = 0; where < 2; ++where) {

                CRef<CSeq_id> id (new CSeq_id);
                id->Assign(*h.GetId(where));
                ids.push_back(id);
            }


            CRef<CSeq_align> seq_align (new CSeq_align());

            // add reciprocity
            CRef<CScore> score(new CScore());
            score->SetId().SetStr("reciprocity");
            try {
                if (h.GetScore() > kBigDbl || args["ut"].AsString() == kBoth) 
                {
                    // derived from constraint alignment or 
                    // uniquify query and subject specified
                    score->SetValue().SetInt((int)e_ReciprocalBest);
                } else if (args["ut"].AsString() == kQuery) {
                    score->SetValue().SetInt((int)e_SubjectDuplication);
                } else {
                    score->SetValue().SetInt((int)e_QueryDuplication);
                }
            }
            catch (CException &e) {
                cerr << "Error adding reciprocity" << endl;
                throw e;
            }
            seq_align->SetScore().push_back(score);

            seq_align->SetType(CSeq_align::eType_partial);
            seq_align->SetSegs().SetDenseg(*ds);

            align_list.push_back(seq_align);
        }

        if(comment.size() > 0) {
            seq_annot->AddComment(comment);
        }

        try {
            if(fmt_txt) {
                ostr << MSerial_AsnText << *seq_annot << endl;
            }
            else {
                ostr << MSerial_AsnBinary << *seq_annot << flush;
            }
        }
        catch (CException &e) {
                cerr << "Error writing output file" << endl;
                throw e;
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

    const bool   mode_multiple ( args["mode"].AsString() == kMode_Multiple );
    const string fmt_in        ( args["fmt_in"].AsString() );
    const string fmt_out       ( args["fmt_out"].AsString() );
    const double maxlenfr      (args["merge"].AsDouble());

    if((fmt_out == g_AsnTxt || fmt_out == g_AsnBin) &&
       (fmt_in != g_AsnTxt && fmt_in != g_AsnBin)) 
    {
        NCBI_THROW(CAppHitFilterException, 
                   eGeneral, 
                   "For ASN output, input must also be in ASN");
    }

    if( mode_multiple == false && (args["ids"] || args["constraints"]
                                   || fmt_in == g_AsnTxt || fmt_in == g_AsnBin ))
    {

        NCBI_THROW(CException, eUnknown, 
                   "Invalid parameter combination - "
                   "some options are not yet supported in pairwise mode.");
    }

    THitRefs all;

    if(mode_multiple) {
        x_DoMultiple(&all);
    }
    else {
        x_DoPairwise(&all);
    }

    if(maxlenfr >= 0) {
        THitRefs merged;
        CHitFilter<THit>::s_MergeAbutting(all.begin(), all.end(), maxlenfr, &merged);
        all = merged;
    }

    x_DumpOutput(all);

    return 0;
}


void CAppHitFilter::x_DoPairwise(THitRefs* pall)
{
    THitRefs& all (*pall);

    const CArgs & args (GetArgs());

    const THit::TCoord min_len (args["min_len"].AsInteger());
    const double min_idty      (args["min_idty"].AsDouble());
    const size_t margin        (args["coord_margin"].AsInteger());
    const THit::TCoord retain_overlap (1024 * args["retain_overlap"].AsInteger());

    CHitFilter<THit>::EUnique_type unique_type = CHitFilter<THit>::e_Strict;
    if (args["ut"].AsString() == "query") {
        unique_type = CHitFilter<THit>::e_Query;
    } else if (args["ut"].AsString() == "subject") {
        unique_type = CHitFilter<THit>::e_Subject;
    }

    try {
        THitRefs hits;
        for(x_ReadInputHits(&hits, true); hits.size(); x_ReadInputHits(&hits, true)) {

            THitRefs hits_new;
            CHitFilter<THit>::s_RunGreedy(
                hits.begin(), hits.end(),
                &hits_new, min_len, 
                min_idty, margin,
                retain_overlap,
                unique_type
                );
            sort(hits_new.begin(), hits_new.end(), s_PHitRefScore);
            hits.resize(remove_if(hits.begin(), hits.end(), CHitFilter<THit>::s_PNullRef) 
                        - hits.begin());
            copy(hits.begin(), hits.end(), back_inserter(all));
            copy(hits_new.begin(), hits_new.end(), back_inserter(all));
            hits.clear();
        }
    }
    catch (CException &e) {
        cerr << "Error running x_DoPairwise" << endl;
        throw e;
    }
}


void CAppHitFilter::x_DoMultiple(THitRefs* pall)
{   
    THitRefs& all (*pall);

    const CArgs & args (GetArgs());

    const string fmt_in                   = args["fmt_in"].AsString();
    const string fmt_out                  = args["fmt_out"].AsString();
    const THit::TCoord min_len            = args["min_len"].AsInteger();
    const double min_idty                 = args["min_idty"].AsDouble();
    const THit::TCoord retain_overlap     = 1024 * args["retain_overlap"].AsInteger();
    const size_t margin (args["coord_margin"].AsInteger());

    CHitFilter<THit>::EUnique_type unique_type = CHitFilter<THit>::e_Strict;    
    if (args["ut"].AsString() == "query") {
        unique_type = CHitFilter<THit>::e_Query;
    } else if (args["ut"].AsString() == "subject") {
        unique_type = CHitFilter<THit>::e_Subject;
    }

    if(args["ids"]) {
        x_LoadIDs(args["ids"].AsInputFile());
    }

    THitRefs restraint;
    if(args["constraints"]) {
        x_LoadConstraints(args["constraints"].AsInputFile(), restraint);
    }

    x_ReadInputHits(&all);
    copy(restraint.begin(), restraint.end(), back_inserter(all));

    sort(all.begin(), all.end(), s_PHitRefScore);

    const size_t M = args["hits_per_chunk"].AsInteger();
    const size_t dim = all.size();
    size_t m = min(dim, M);

    const THitRefs::iterator ii_beg = all.begin(), ii_end = all.end();
    THitRefs::iterator ii_hi = ii_beg, ii = ii_beg;

    try {    
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
            CHitFilter<THit>::s_RunGreedy(
                ii_beg, ii_hi, 
                &hits_new, min_len, 
                min_idty, margin,
                retain_overlap,
                unique_type
            );
            sort(hits_new.begin(), hits_new.end(), s_PHitRefScore);
            THitRefs::iterator ii_hi0 = ii_hi;
            ii_hi = remove_if(ii_beg, ii_hi, CHitFilter<THit>::s_PNullRef);
            THitRefs::iterator jj = hits_new.begin(), jje = hits_new.end();
            for(;jj != jje && ii_hi != ii_hi0; *ii_hi++ = *jj++);
            if(jj != jje) {
                LOG_POST("Warning: space from eliminated alignments "
                         "not enough for all splits.");
            }            
        }
    }
    catch (CException &e) {
        cerr << "Error in x_DoMultiple" << endl;
        throw e;
    }
    all.erase(ii_hi, ii_end);
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
                TGi gi = id0->GetGi();
                accver = sequence::GetAccessionForGi(gi, *scope);
            }
            else {
                const string seqidstr = id0->AsFastaString();
                TGi gi = sequence::GetGiForAccession(seqidstr, *scope);
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

        hit->SetScore(kBigDbl);

        all.push_back(hit);

        if(hit->GetLength() > maxlen) {
            maxlen = hit->GetLength();
        }
    }
    float score_factor = 0.25 / maxlen;
    const CArgs& args = GetArgs();    
    NON_CONST_ITERATE(THitRefs, ii, all) {
        THitRef& h = *ii;
        h->SetScore(h->GetScore() * (1 + score_factor * h->GetLength()));

        if (args["no_output_constraint"].HasValue()) {
            h->SetIdentity(1.0);
        }
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
