/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <algo/align/prosplign/prosplign_exception.hpp>
#include <algo/align/prosplign/prosplign.hpp>

#include "Info.hpp"
#include "Ali.hpp"
#include "nucprot.hpp"
#include "NSeq.hpp"
#include "PSeq.hpp"
#include "AliSeqAlign.hpp"

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/alnmgr/alntext.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);

const char GAP_CHAR='-'; // used in dna and protein text
const char SPACE_CHAR=' '; // translation and protein
const char INTRON_CHAR='.'; // protein
const char INTRON_OR_GAP[] = {INTRON_CHAR,GAP_CHAR,0};

// used in match text
const char BAD_PIECE_CHAR='X';
const char MISMATCH_CHAR=' ';
const char BAD_OR_MISMATCH[] = {BAD_PIECE_CHAR,MISMATCH_CHAR,0};
const char MATCH_CHAR='|';
const char POSIT_CHAR='+';

BEGIN_SCOPE(prosplign)

CProSplignOutputOptionsExt::CProSplignOutputOptionsExt(const CProSplignOutputOptions& options) : CProSplignOutputOptions(options)
{
    drop = GetTotalPositives() - GetFlankPositives();
    splice_cost = GetFlankPositives()?((100 - GetFlankPositives())*GetMinFlankingExonLen())/GetFlankPositives():0;
}

list<CNPiece> FindGoodParts(const CProteinAlignText& alignment_text, CProSplignOutputOptionsExt m_options)
{
    
    const string& nuc = alignment_text.GetDNA();
    const string& outp = alignment_text.GetProtein();
    const string& orig_match = alignment_text.GetMatch();
    list<CNPiece> m_AliPiece;
    //init
    if(m_options.IsPassThrough()) {
        string::size_type n1 = outp.find_first_not_of(GAP_CHAR);
        string::size_type n2 = outp.find_last_not_of(GAP_CHAR);
        if (n1 <= n2) 
        m_AliPiece.push_back(CNPiece(n1, n2+1, 0, 0));
        return m_AliPiece;
    }

    string match = orig_match;
    for (size_t i = 1; i < match.size()-1; ++i) {
        if (isupper(outp[i])) {
            _ASSERT( outp[i-1]==SPACE_CHAR && outp[i+1]==SPACE_CHAR );
            match[i-1] = match[i+1] = match[i];
            ++i;
        }
    }

    m_AliPiece = FindGoodParts(CNPiece(0, match.size(), 0, 0), match, outp, m_options);
    for(list<CNPiece>::iterator it =  m_AliPiece.begin(); it !=  m_AliPiece.end(); ) {
        list<CNPiece> tmp = ExcludeBadExons(*it, match, outp, m_options);
         m_AliPiece.splice(it, tmp);
        it =  m_AliPiece.erase(it);
    }
    for(list<CNPiece>::iterator it =  m_AliPiece.begin(); it !=  m_AliPiece.end(); ) {
        list<CNPiece> tmp = FindGoodParts(*it, match, outp, m_options);
         m_AliPiece.splice(it, tmp);
        it =  m_AliPiece.erase(it);
    }

    if( !m_AliPiece.empty() && m_options.GetFillHoles() ) {
        string::size_type beg = m_AliPiece.front().beg;
        string::size_type end = m_AliPiece.back().end;
        m_AliPiece.clear();
        m_AliPiece.push_back(CNPiece(beg, end, 0, 0));
    }


    //remove trailing NNs
    if( !m_AliPiece.empty() && m_options.GetCutNs() ) {
        for(list<CNPiece>::iterator it =  m_AliPiece.begin(); it !=  m_AliPiece.end(); ) {
            int pos = it->end - 1;
            for(; pos >= it->beg && nuc[pos] == 'N' ; --pos);
            if(pos < it->beg) {
                it =  m_AliPiece.erase(it);
            } else {
                it->end = pos+1;
                ++it;
            }
        }
    }

    //trim partial codons 
    if( !m_AliPiece.empty() && m_options.GetCutFlankPartialCodons() ) {
        for(list<CNPiece>::iterator it =  m_AliPiece.begin(); it !=  m_AliPiece.end(); ) {
            int pos = it->end - 1;
            if(isupper(outp[pos])) {//case where N was trimmed
                //cout<<it->beg<<", pos: "<<pos<<", poschar: "<<<<", pos: "<<endl;
                _ASSERT( pos > it->beg 
                         && (match[pos] == POSIT_CHAR || match[pos] == MATCH_CHAR) 
                         && (match[pos-1] == POSIT_CHAR || match[pos-1] == MATCH_CHAR) );
                pos -= 2;
                if(pos < it->beg) {
                    it =  m_AliPiece.erase(it);
                } else {
                    it->end = pos+1;
                    ++it;
                }
            } else {
                ++it;
            }
        }
        for(list<CNPiece>::iterator it =  m_AliPiece.begin(); it !=  m_AliPiece.end(); ) {
            //partial codons at exon end
            int pos = it->end - 1;
            for(; pos >= it->beg && ( islower(outp[pos]) || outp[pos] == INTRON_CHAR ) ; --pos);
            if(pos < it->beg) {
                it =  m_AliPiece.erase(it);
            } else {
                it->end = pos+1;
                ++it;
            }
        }
        for(list<CNPiece>::iterator it =  m_AliPiece.begin(); it !=  m_AliPiece.end(); ) {
            //partial codons at exon end
            int pos = it->beg;
            for(; pos < it->end && ( islower(outp[pos]) || outp[pos] == INTRON_CHAR ) ; ++pos);
            if(pos == it->end) {
                it =  m_AliPiece.erase(it);
            } else {
                it->beg = pos;
                ++it;
            }
        }
            
    }//end trim partial codons 

    return  m_AliPiece;
}

list<CNPiece> FindGoodParts(const CNPiece pc, const string& match_all_pos, const string& protein, CProSplignOutputOptionsExt m_options)
{
    list<CNPiece> m_AliPiece;
    const string& match = match_all_pos;
    
    string::size_type n = match.find_first_not_of(BAD_OR_MISMATCH, pc.beg);
    if(n == string::npos || n >= (unsigned)pc.end) return m_AliPiece; //no matches    
    bool ism = true;
    bool isintr = false;
    string::size_type beg = n;
    int efflen = 0;
    if(match[n] == MATCH_CHAR && protein.find_first_not_of(GAP_CHAR) == n && protein[n+1] == 'M')
        efflen += m_options.GetStartBonus();
    for(; n<(unsigned)pc.end; ++n) {
        if(match[n] == POSIT_CHAR || match[n] == MATCH_CHAR) {
            if(!ism) {
                ism = true;
                m_AliPiece.push_back(CNPiece(beg, n, 0, efflen));
                beg = n;
                efflen = 0;
            } 
        } else {
            if(ism) {
                    ism = false;
                    m_AliPiece.push_back(CNPiece(beg, n, efflen, efflen));
                    beg = n;
                    efflen = 0;
            }
        }
        if(protein[n] != INTRON_CHAR) {//inside exon
            efflen ++;  
            isintr = false;
        }
        else {//intron
            if(!isintr) {
                efflen += m_options.splice_cost;
                isintr = true;
            }
        }
    }
    if(ism) {//endpoint is a match
        m_AliPiece.push_back(CNPiece(beg, n, efflen, efflen));
    }
    //join pieces
    list<CNPiece>::iterator itb, ite, itc;
    list<CNPiece>::size_type pnum = m_AliPiece.size() + 1;
    while(pnum > m_AliPiece.size()) {
        pnum = m_AliPiece.size();
        //forward
        for(itb = m_AliPiece.begin(); ; ) {
            ite = itb;
            int slen = 0, spos = 0;
            itc = itb;
            ++itc;
            while(itc != m_AliPiece.end()) {
                if(m_options.Bad(itc)) break;//really bad
                slen += itc->efflen;
                spos += itc->posit;
                if(m_options.Dropof(slen, spos, itb)) break;
                ++itc;//points to  'good'
                if(m_options.Perc(itc, slen, spos, itb)) {
                    if(m_options.BackCheck(itb, itc)) ite = itc;//join!
                }
                slen += itc->efflen;
                spos += itc->posit;
                ++itc;//points to  ' bad' or end
            }
            if(ite != itb) {
                m_options.Join(itb, ite);
                m_AliPiece.erase(itb, ite);
                itb = ite;
            }
            ++itb;
            if(itb == m_AliPiece.end()) break; 
            ++itb;
        }
        //backward
        itb = m_AliPiece.end();
        --itb;
        while(itb != m_AliPiece.begin()) {
            ite = itb;
            int slen = 0, spos = 0;
            itc = itb;
            while(itc != m_AliPiece.begin()) {
                --itc;//points to  ' bad'
                if(m_options.Bad(itc)) break;//really bad
                slen += itc->efflen;
                spos += itc->posit;
                if(m_options.Dropof(slen, spos, itb)) break;
                --itc;//points to  'good'
                if(m_options.Perc(itc, slen, spos, itb)) {
                    if(m_options.ForwCheck(itc, itb)) ite = itc;//join!
                }
                slen += itc->efflen;
                spos += itc->posit;
            }
            if(ite != itb) {
                m_options.Join(ite, itb);
                m_AliPiece.erase(ite, itb);
            }
            if(itb == m_AliPiece.begin()) break;
            --itb;
            --itb;
        }
    }   
	//throw out bad pieces
    for(list<CNPiece>::iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ) {
		if(it->posit == 0) it = m_AliPiece.erase(it);
        else if (it->efflen < m_options.GetMinGoodLen()) it = m_AliPiece.erase(it);
		else ++it;
    }
    return m_AliPiece;
}


list<CNPiece> ExcludeBadExons(const CNPiece pc, const string& match_all_pos, const string& protein, CProSplignOutputOptionsExt m_options)
{
    const string& match = match_all_pos;
    list<CNPiece> alip;
    vector<pair<int, int> > exons; //exons inside pc, format [exon_beg, exon_end)

    bool in_exon = false;
    for(int n=pc.beg; n<pc.end; ) {
        if( ( protein[n] != INTRON_CHAR) && !in_exon ) {
            in_exon = true;
            exons.push_back(make_pair(n, 0));//mark beg. of exon
        }
        ++n;
        if( ( (protein[n] == INTRON_CHAR) || (n == pc.end) ) && in_exon ) {
            in_exon = false;
            exons.back().second = n;//mark end. of exon
        }
    }

    int cur_beg = pc.beg;
    for(vector<pair<int, int> >::iterator eit = exons.begin(); eit != exons.end(); ++eit) {
        int pos = 0;
        int id = 0;
        int len = eit->second - eit->first;
        for(int i = eit->first; i < eit->second; ++i) {
            if(match[i] == POSIT_CHAR) ++pos;
            else if(match[i] == MATCH_CHAR) {
                ++id;
                ++pos;
            }
        }
        if(( 100*pos < len*m_options.GetMinExonPos() ) || ( 100*id < m_options.GetMinExonId() * len)) {//throw out bad exon
            //find prev match
            int n;
            for(n = eit->first - 1; n > cur_beg; --n) {
                if(match[n] == POSIT_CHAR || match[n] == MATCH_CHAR) break;
            }
            ++n;
            if(n > cur_beg) alip.push_back(CNPiece(cur_beg, n, 0, 0));
            //find next match
            for(n = eit->second; n < pc.end; ++n) {
                if(match[n] == POSIT_CHAR || match[n] == MATCH_CHAR) break;
            }
            cur_beg = n;            
        }
    }
    if(cur_beg < pc.end) alip.push_back(CNPiece(cur_beg, pc.end, 0, 0));    
    return alip;    
}



//CNPiece implementation

CNPiece::CNPiece(string::size_type obeg, string::size_type oend, int oposit, int oefflen)
    {
        beg = (int)obeg;
        end = (int)oend;
        posit = oposit;
        efflen = oefflen;
    }

bool CProSplignOutputOptionsExt::Bad(list<prosplign::CNPiece>::iterator it)
{
    if(it->efflen > GetMaxBadLen()) return true;
    return false;
}

bool CProSplignOutputOptionsExt::Dropof(int efflen, int posit, list<prosplign::CNPiece>::iterator it)
{
    if((GetTotalPositives()-drop)*(efflen+it->efflen) > 100*(posit+it->posit)) return true;
    return false;
}

bool CProSplignOutputOptionsExt::Perc(list<prosplign::CNPiece>::iterator add, int efflen, int posit, list<prosplign::CNPiece>::iterator cur)
{
    if(Dropof(efflen, posit, add)) return false;
    if(GetTotalPositives()*(efflen+cur->efflen+add->efflen) > 100*(posit+cur->posit+add->posit)) return false;
    return true;
}

void CProSplignOutputOptionsExt::Join(list<prosplign::CNPiece>::iterator it, list<prosplign::CNPiece>::iterator last)
{
    int posit = last->posit;
    int efflen = last->efflen;
    for(list<prosplign::CNPiece>::iterator it1 = it; it1 != last; ++it1) {
        posit += it1->posit;
        efflen += it1->efflen;
    }
    last->posit = posit;
    last->efflen = efflen;
    last->beg = it->beg;
}

bool CProSplignOutputOptionsExt::ForwCheck(list<prosplign::CNPiece>::iterator it1, list<prosplign::CNPiece>::iterator it2)
{
    int efflen = it1->efflen;
    int pos = it1->posit;
    for(;it1!= it2;){
        it1++;
        if(Dropof(efflen, pos, it1)) return false;
        efflen += it1->efflen;
        pos += it1->posit;
        it1++;
        efflen += it1->efflen;
        pos += it1->posit;
    }
    return true;
}

bool CProSplignOutputOptionsExt::BackCheck(list<prosplign::CNPiece>::iterator it1, list<prosplign::CNPiece>::iterator it2)
{
    int efflen = it2->efflen;
    int pos = it2->posit;
    for(;it1!= it2;){
        it2--;
        if(Dropof(efflen, pos, it2)) return false;
        efflen += it2->efflen;
        pos += it2->posit;
        it2--;
        efflen += it2->efflen;
        pos += it2->posit;
    }
    return true;
}

END_SCOPE(prosplign)

namespace {
int GetCompNum(const CSeq_align& sa)
{
    if (sa.CanGetExt()) {
        ITERATE(CSeq_align::TExt, e, sa.GetExt()) {
            const CUser_object& uo = **e;
            if (uo.CanGetData()) {
                ITERATE(CUser_object::TData, field, uo.GetData()) {
                    if ((*field)->CanGetLabel() && (*field)->GetLabel().IsStr() && (*field)->GetLabel().GetStr()=="CompartmentId") {
                        return (*field)->GetData().GetInt();
                    }
                }
            }
        }
    }
    return -1;
}
}

void CProSplignText::Output(const CSeq_align& seqalign, CScope& scope, ostream& out, int width, const string& matrix_name)
{
    int compartment_id = GetCompNum(seqalign);
    string contig_name = seqalign.GetSegs().GetSpliced().GetGenomic_id().GetSeqIdString(true);
    string prot_id = seqalign.GetSegs().GetSpliced().GetProduct_id().GetSeqIdString(true);
    TSeqRange bounds = CProteinAlignText::GetGenomicBounds(scope, seqalign)->GetTotalRange();
    int nuc_from = bounds.GetFrom()+1;
    int nuc_to = bounds.GetTo()+1;
    bool is_plus_strand = seqalign.GetSegs().GetSpliced().GetGenomic_strand()==eNa_strand_plus;

    out<<endl<<"************************************************************************"<<endl;
    out<<"************************************************************************"<<endl;
    out<<"************************************************************************"<<endl;
    out<<compartment_id<<"\t"<<contig_name<<"\t"<<prot_id<<"\t"<<nuc_from<<"\t"<<nuc_to<<"\t";
    out<<(is_plus_strand?'+':'-')<<endl;

    CProteinAlignText align_text(scope, seqalign, matrix_name);
    const string& dna = align_text.GetDNA();
    const string& translation = align_text.GetTranslation();
    string match = align_text.GetMatch();
    const string& protein = align_text.GetProtein();
    string good_parts;
    good_parts.append(match.size(),'*');
    for (size_t i = 0; i < match.size(); ++i) {
        if (match[i] == BAD_PIECE_CHAR)
            match[i] = good_parts[i] = ' ';
    }

    int npos1 = is_plus_strand?nuc_from:nuc_to;

    int prot_beg_pos = protein.find_first_not_of(GAP_CHAR);
    int prot_end_pos = protein.find_last_not_of(GAP_CHAR);

    for(int i=0;i<prot_end_pos; i+=width) {
        int apos = i+width-1;
        if (apos >= (int)dna.length()) {
            apos = (int)dna.length() - 1;
            width = apos-i+1;
        }

#ifdef NCBI_COMPILER_WORKSHOP
        int gaps = 0;
        count(dna.begin()+i, dna.begin()+(i+width), GAP_CHAR, gaps);
        int real_bases = width-gaps;
#else
        int real_bases = width-count(dna.begin()+i, dna.begin()+(i+width), GAP_CHAR);
#endif

        int npos2 = is_plus_strand?npos1+real_bases-1:npos1-(real_bases-1);

        //throw out head and tail lines with a complete row gap
        if (apos > prot_beg_pos) {
            out.setf(IOS_BASE::left, IOS_BASE::adjustfield);
            if (real_bases>0) {
                out<<setw(12)<<npos1<<dna.substr(i, width)<<"   "<<npos2<<endl;
            } else { //complete row is a nucleotide gap
                out<<setw(12)<<"-"<<dna.substr(i, width)<<"   "<<"-"<<endl;
            }
            out<<setw(12)<<" "<<translation.substr(i, width)<<endl;
            out<<setw(12)<<" "<<match.substr(i, width)<<endl;
            out<<setw(12)<<" "<<protein.substr(i, width)<<endl;
            out<<setw(12)<<" "<<good_parts.substr(i, width)<<endl;
        }

        npos1 = is_plus_strand?npos2+1:npos2-1;
    }
}

USING_SCOPE(prosplign);

namespace {

struct CAliChunk {
    CAliChunk(TSeqPos ali_pos, TSeqPos nuc_pos, TSeqPos prot_pos, CSpliced_seg::TExons::iterator exon_iter, CSpliced_exon::TParts::iterator chunk_iter) :
        m_nuc_pos(nuc_pos), m_prot_pos(prot_pos), m_exon_iter(exon_iter), m_chunk_iter(chunk_iter), m_bad(false)
    {
        const CSpliced_exon_chunk& chunk = **chunk_iter;

        m_nuc_len = 0;
        m_prot_len = 0;
        if (chunk.IsDiag() || chunk.IsMatch() || chunk.IsMismatch()) {
            int len = 0;
            if (chunk.IsDiag()) {
                len = chunk.GetDiag();
            } else if (chunk.IsMatch()) {
                len = chunk.GetMatch();
            } else if (chunk.IsMismatch()) {
                len = chunk.GetMismatch();
            }

            m_nuc_len = len;
            m_prot_len = len;
        } else if (chunk.IsProduct_ins()) {
            m_prot_len = chunk.GetProduct_ins();
        } else if (chunk.IsGenomic_ins()) {
            m_nuc_len = chunk.GetGenomic_ins();
        }

        m_ali_range = TSeqRange(ali_pos, ali_pos + max(m_nuc_len,m_prot_len)-1);
    }

    TSeqRange m_ali_range;
    TSeqPos m_nuc_pos;
    TSeqPos m_prot_pos;
    int m_nuc_len;
    int m_prot_len;
    CSpliced_seg::TExons::iterator m_exon_iter;
    CSpliced_exon::TParts::iterator m_chunk_iter;
    bool m_bad;
};

typedef list<CAliChunk> TAliChunkCollection;
typedef TAliChunkCollection::iterator TAliChunkIterator;


TAliChunkCollection ExtractChunks(CScope& scope, CSeq_align& seq_align)
{
    CSpliced_seg& sps = seq_align.SetSegs().SetSpliced();
    ENa_strand strand = sps.GetGenomic_strand();
    TSeqRange bounds = CProteinAlignText::GetGenomicBounds(scope, seq_align)->GetTotalRange();
    int nuc_from = bounds.GetFrom();
    int nuc_to = bounds.GetTo();
    int prot_from = 0;

    int alignment_pos = 0;

    TAliChunkCollection chunks;

    NON_CONST_ITERATE (CSpliced_seg::TExons, e_it, sps.SetExons()) {
        CSpliced_exon& exon = **e_it;
        int prot_cur_start = exon.GetProduct_start().AsSeqPos();
        int nuc_cur_start = exon.GetGenomic_start();
        int nuc_cur_end = exon.GetGenomic_end();

        if (strand == eNa_strand_plus) {
            alignment_pos += max(prot_cur_start-prot_from, nuc_cur_start-nuc_from);
            nuc_from = nuc_cur_start;
        } else {
            alignment_pos += max(prot_cur_start-prot_from, nuc_to-nuc_cur_end);
            nuc_to = nuc_cur_end;
        }
        prot_from = prot_cur_start;

        NON_CONST_ITERATE (CSpliced_exon::TParts, p_it, exon.SetParts()) {
            CAliChunk chunk(alignment_pos, strand == eNa_strand_plus?nuc_from:nuc_to, prot_from, e_it, p_it);
            alignment_pos = chunk.m_ali_range.GetTo()+1;
            prot_from += chunk.m_prot_len;
            if (strand == eNa_strand_plus) {
                nuc_from += chunk.m_nuc_len;
            } else {
                nuc_to -= chunk.m_nuc_len;
            }

            chunks.push_back(chunk);
        }

        _ASSERT( int(exon.GetProduct_end().AsSeqPos()+1) == prot_from );
        _ASSERT( (strand==eNa_strand_plus && nuc_cur_end+1==nuc_from) || (strand!=eNa_strand_plus && nuc_cur_start-1==nuc_to) );
    }
    return chunks;
}

list<TSeqRange> InvertPartList(const list<CNPiece>& good_parts, TSeqRange total_range)
{
    list<TSeqRange> bad_parts;
    
    int tail_beg = total_range.GetFrom();
    int tail_end = total_range.GetTo();
    ITERATE(list<CNPiece>, i, good_parts) {
        if (tail_beg < i->beg)
            bad_parts.push_back(TSeqRange(tail_beg,i->beg-1));
        tail_beg = i->end;
    }
    if (tail_beg <= tail_end)
        bad_parts.push_back(TSeqRange(tail_beg,tail_end));

    return bad_parts;
}
    
void TestExonLength(const CSpliced_exon& exon)
{
        int nuc_len = 0;
        int prot_len = 0;
        ITERATE (CSpliced_exon::TParts, p_it, exon.GetParts()) {
            const CSpliced_exon_chunk& chunk = **p_it;

            if (chunk.IsDiag() || chunk.IsMatch() || chunk.IsMismatch()) {
                int len = 0;
                if (chunk.IsDiag()) {
                    len = chunk.GetDiag();
                } else if (chunk.IsMatch()) {
                    len = chunk.GetMatch();
                } else if (chunk.IsMismatch()) {
                    len = chunk.GetMismatch();
                }
                
                nuc_len += len;
                prot_len += len;
            } else if (chunk.IsProduct_ins()) {
                prot_len += chunk.GetProduct_ins();
            } else if (chunk.IsGenomic_ins()) {
                nuc_len += chunk.GetGenomic_ins();
            }
        }

#ifdef _DEBUG
                int prot_cur_start = exon.GetProduct_start().AsSeqPos();
                int prot_cur_end = exon.GetProduct_end().AsSeqPos();
                int nuc_cur_end = exon.GetGenomic_end();
                int nuc_cur_start = exon.GetGenomic_start();
#endif

        _ASSERT( nuc_cur_end-nuc_cur_start+1 == nuc_len );
        _ASSERT( prot_cur_end-prot_cur_start+1 == prot_len );
}

void SplitChunk(TAliChunkCollection& chunks, TAliChunkIterator iter, TSeqPos start_of_second_chunk, bool genomic_plus)
{
    _DEBUG_CODE( TestExonLength(**iter->m_exon_iter); );
    _ASSERT( iter->m_ali_range.GetFrom() < start_of_second_chunk );
    _ASSERT( start_of_second_chunk <= iter->m_ali_range.GetTo());
    _ASSERT( iter->m_nuc_len == iter->m_prot_len ); // not an insertion

    CRef< CSpliced_exon_chunk > new_chunk( new CSpliced_exon_chunk);
    new_chunk->Assign(**iter->m_chunk_iter);
    int first_len = start_of_second_chunk - iter->m_ali_range.GetFrom();
    int second_len = iter->m_ali_range.GetTo() - start_of_second_chunk+1;

    TAliChunkIterator first_iter = chunks.insert(iter, *iter);

    if (genomic_plus) {
        iter->m_nuc_pos += first_len;
    } else {
        iter->m_nuc_pos -= first_len;
    }
    iter->m_prot_pos += first_len;

    if (new_chunk->IsDiag()) {
        new_chunk->SetDiag(first_len);
        (*iter->m_chunk_iter)->SetDiag(second_len);
    } else if (new_chunk->IsMatch()) {
        new_chunk->SetMatch(first_len);
        (*iter->m_chunk_iter)->SetMatch(second_len);
    } else if (new_chunk->IsMismatch()) {
        new_chunk->SetMismatch(first_len);
        (*iter->m_chunk_iter)->SetMismatch(second_len);
    }

    first_iter->m_ali_range.SetTo(start_of_second_chunk-1);
    iter->m_ali_range.SetFrom(start_of_second_chunk);

    first_iter->m_nuc_len = first_iter->m_prot_len = first_len;
    iter->m_nuc_len = iter->m_prot_len = second_len;

    first_iter->m_chunk_iter = (*iter->m_exon_iter)->SetParts().insert(iter->m_chunk_iter, new_chunk);

    _DEBUG_CODE( TestExonLength(**iter->m_exon_iter); );
}

void DropExon(CSpliced_seg::TExons& exons, CSpliced_seg::TExons::iterator& exon_iter)
{
    exons.erase(exon_iter);
	exon_iter = exons.end();
}

void DropExonHead(TAliChunkIterator chunk_iter, bool genomic_plus)
{
    CSpliced_exon* cur_exon = *chunk_iter->m_exon_iter;

    _DEBUG_CODE( TestExonLength(*cur_exon); );
#ifdef _DEBUG
    size_t chunks_count = cur_exon->GetParts().size();
#endif

    cur_exon->SetParts().erase(cur_exon->SetParts().begin(), chunk_iter->m_chunk_iter);

    if (genomic_plus) {
        cur_exon->SetGenomic_start(chunk_iter->m_nuc_pos);
    } else {
        cur_exon->SetGenomic_end(chunk_iter->m_nuc_pos);
    }
    cur_exon->SetProduct_start(*NultriposToProduct_pos(chunk_iter->m_prot_pos));
    cur_exon->SetPartial(true);
    if (cur_exon->IsSetAcceptor_before_exon())
        cur_exon->ResetAcceptor_before_exon();

    _ASSERT( 0 < cur_exon->GetParts().size() );
    _ASSERT( cur_exon->GetParts().size() <  chunks_count );

    _DEBUG_CODE( TestExonLength(*cur_exon); );
}

void SplitExon(CSpliced_seg::TExons& exons, TAliChunkIterator chunk_iter, bool genomic_plus)
{
    CSpliced_exon* cur_exon = *chunk_iter->m_exon_iter;
    
    _DEBUG_CODE( TestExonLength(*cur_exon); );
#ifdef _DEBUG
    size_t chunks_count = cur_exon->GetParts().size();
#endif

    CRef< CSpliced_exon > new_exon( new CSpliced_exon );
    new_exon->Assign(*cur_exon);

    CSpliced_exon::TParts::iterator new_exon_chunk = new_exon->SetParts().begin();
    ITERATE(CSpliced_exon::TParts, old_exon_chunk, cur_exon->GetParts()) {
        if (old_exon_chunk==chunk_iter->m_chunk_iter)
            break;
        ++new_exon_chunk;
    }

    new_exon->SetParts().erase(new_exon_chunk, new_exon->SetParts().end());

    if (genomic_plus) {
        new_exon->SetGenomic_end(chunk_iter->m_nuc_pos-1);
    } else {
        new_exon->SetGenomic_start(chunk_iter->m_nuc_pos+1);
    }
    new_exon->SetProduct_end(*NultriposToProduct_pos(chunk_iter->m_prot_pos-1));
    new_exon->SetPartial(true);
    if (new_exon->IsSetDonor_after_exon())
        new_exon->ResetDonor_after_exon();

    _ASSERT( new_exon->GetGenomic_start() <= new_exon->GetGenomic_end() );
    _ASSERT( new_exon->GetProduct_start().AsSeqPos() <= new_exon->GetProduct_end().AsSeqPos() );

    exons.insert(chunk_iter->m_exon_iter, new_exon);

    DropExonHead(chunk_iter, genomic_plus);

    _ASSERT( 0 < new_exon->GetParts().size() && 0 < cur_exon->GetParts().size() );
    _ASSERT( new_exon->GetParts().size()+cur_exon->GetParts().size() == chunks_count );

    _DEBUG_CODE( TestExonLength(*new_exon); );
    _DEBUG_CODE( TestExonLength(*cur_exon); );
}

}

void prosplign::SetScores(objects::CSeq_align& seq_align, objects::CScope& scope, const string& matrix_name) {
    CProteinAlignText pro_text(scope, seq_align, matrix_name);
    const string& prot = pro_text.GetProtein();
    const string& dna = pro_text.GetDNA();
    const string& match = pro_text.GetMatch();
    // const string& trans = pro_text.GetTranslation();
    int pos = 0, ident = 0, len = 0, neg = 0, pgap = 0, ngap = 0;
    for(string::size_type i=0;i<match.size(); ++i) {
        if( (prot[i] != '.') && (match[i] != 'X') ) {//skip introns and bad parts
            ++len;
            if(prot[i] == '-') {
                ++pgap;
            } else if(dna[i] == '-') {
                ++ngap;
            } else if(isalpha(prot[i])) {
                bool triple = isupper(prot[i]);
                switch(match[i]) {
                case '|':
                    if(triple) ident +=3;
                    else ++ident;
                case '+':
                    if(triple) pos +=3;
                    else ++pos;
                    break;
                default://mismatch
                    if(triple) neg +=3;
                    else ++neg;
                    break;
                }
            }
        }
    }
    seq_align.SetNamedScore("num_ident", ident);
    seq_align.SetNamedScore("num_positives", pos);
    seq_align.SetNamedScore("num_negatives", neg);
    seq_align.SetNamedScore("product_gap_length", pgap);
    seq_align.SetNamedScore("genomic_gap_length", ngap);
    seq_align.SetNamedScore("align_length", len);
    //internal protein gap length for full alignment quality measure
    int ipgap = 0; 
    int ibeg, iend;
    for(ibeg = 0; ibeg<(int)(prot.size()) && ( (prot[ibeg] == '.') || (match[ibeg] == 'X') || (prot[ibeg] == '-' ) ); ++ibeg) {}
    for(iend = prot.size() - 1; iend >=0 && ( (prot[iend] == '.') || (match[iend] == 'X') || (prot[iend] == '-' ) ); --iend) {}
    for(int i=ibeg;i<=iend; ++i) {
        if( (prot[i] != '.') && (match[i] != 'X') ) {//skip introns and bad parts
            if(prot[i] == '-') {
                ++ipgap;
            }
        }
    }
    seq_align.SetNamedScore("product_internal_gap_length", ipgap);
}

void prosplign::RefineAlignment(CScope& scope, CSeq_align& seq_align, const list<CNPiece>& good_parts)
{
    CSpliced_seg& sps = seq_align.SetSegs().SetSpliced();
    TAliChunkCollection chunks = ExtractChunks(scope, seq_align);

    if (chunks.empty())
        return;

    bool genomic_plus = sps.GetGenomic_strand()==eNa_strand_plus;

    list<TSeqRange> bad_parts = InvertPartList(good_parts, TSeqRange(chunks.front().m_ali_range.GetFrom(),chunks.back().m_ali_range.GetTo()));

    TAliChunkIterator chunk_iter = chunks.begin();

    ITERATE(list<TSeqRange>, bad_part, bad_parts) {
        while (chunk_iter != chunks.end() && chunk_iter->m_ali_range.GetTo() < bad_part->GetFrom()) {
            ++chunk_iter;
        }

        if (chunk_iter == chunks.end())
            break;
        if (bad_part->GetTo() < chunk_iter->m_ali_range.GetFrom())
            continue;

        if (chunk_iter->m_ali_range.GetFrom() < bad_part->GetFrom())
            SplitChunk(chunks, chunk_iter, bad_part->GetFrom(), genomic_plus);

        while (chunk_iter != chunks.end() && chunk_iter->m_ali_range.GetTo() <= bad_part->GetTo())
            chunk_iter++->m_bad = true;

        if (chunk_iter != chunks.end() && chunk_iter->m_ali_range.GetFrom() <= bad_part->GetTo()) {
            chunk_iter->m_bad = true;
            SplitChunk(chunks, chunk_iter, bad_part->GetTo()+1, genomic_plus);
            chunk_iter->m_bad = false;
        }
    }
    
    CSpliced_seg::TExons::iterator prev_exon_iter = sps.SetExons().end();

    NON_CONST_ITERATE(TAliChunkCollection, chunk_iter, chunks) {
        while(chunk_iter != chunks.end() && !chunk_iter->m_bad) {
            prev_exon_iter = chunk_iter->m_exon_iter;
            ++chunk_iter;
        }
        if (chunk_iter == chunks.end())
            break;
        if (prev_exon_iter != chunk_iter->m_exon_iter) {
            if ((*chunk_iter->m_exon_iter)->IsSetAcceptor_before_exon())
                (*chunk_iter->m_exon_iter)->ResetAcceptor_before_exon();
        } else {
            SplitExon(sps.SetExons(),chunk_iter, genomic_plus);
        }

        prev_exon_iter = chunk_iter->m_exon_iter;
        TAliChunkIterator next_chunk_iter = chunk_iter;
        ++next_chunk_iter;
        while (next_chunk_iter != chunks.end() && next_chunk_iter->m_bad && next_chunk_iter->m_exon_iter==prev_exon_iter) {
            chunk_iter = next_chunk_iter++;
        }

        if (next_chunk_iter == chunks.end() || next_chunk_iter->m_exon_iter!=prev_exon_iter) {
            DropExon(sps.SetExons(), prev_exon_iter);
        } else {
            DropExonHead(next_chunk_iter, genomic_plus);
        }
    }

#ifdef _DEBUG
    ITERATE (CSpliced_seg::TExons, e_it, sps.GetExons()) {
        TestExonLength(**e_it);
    }
    
#endif
}

END_NCBI_SCOPE

