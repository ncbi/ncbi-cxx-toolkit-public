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

#include <objects/seqloc/seqloc__.hpp>
#include <objects/general/general__.hpp>

#include <algo/align/prosplign/prosplign.hpp>
#include <algo/align/prosplign/prosplign_exception.hpp>

#include "Ali.hpp"
#include "NSeq.hpp"
#include "Info.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)
using namespace objects;

CAli::CAli(CNSeq& nseq, CPSeq& pseq)
{
    cnseq = &nseq;
    cpseq = &pseq;
}

//CPosAli::CPosAli(const CAli& ali, const string& nuc, const string& prot) : CAli(ali), m_protid(prot), m_nucid(nuc)
CPosAli::CPosAli(const CAli& ali, const CSeq_id& protein, const CSeq_loc& genomic, const CProSplignOutputOptions& output_options, const CSubstMatrix& matrix) : 
    CAli(ali), m_genomic(genomic), m_protein(protein), m_output_options(output_options), m_has_stop_on_nuc(false)
{
    m_IsFull = m_output_options.IsPassThrough();
    CInfo info(ali,m_output_options);
    info.InitAlign(matrix);
    info.Cut();
    AddPostProcInfo(info.m_AliPiece);

    SetHasStartOnNuc(info);
    SetHasStopOnNuc(info);
}

bool CPosAli::HasStartOnNuc(void) const
{
    return m_has_start_on_nuc;
}
void CPosAli::SetHasStartOnNuc(const CInfo& info)
{
    m_has_start_on_nuc = false;

    if (info.m_AliPiece.empty())
        return;

    int nulpos = info.NextNucNullBased(m_pcs.front().beg-1);
    if(!cnseq->ValidPos(nulpos) || !cnseq->ValidPos(nulpos+2)) return;
    char start[] = "---";
    start[0] = cnseq->Upper(nulpos);
    ++nulpos;
    start[1] = cnseq->Upper(nulpos);
    ++nulpos;
    start[2] = cnseq->Upper(nulpos);
    m_has_start_on_nuc = strcmp(start, "ATG")==0;
}

bool CPosAli::HasStopOnNuc() const
{
    return m_has_stop_on_nuc;
}
void CPosAli::SetHasStopOnNuc(const CInfo& info)
{
    m_has_stop_on_nuc = false;

    if (info.m_AliPiece.empty())
        return;

    int nulpos = info.NextNucNullBased(m_pcs.back().end-1);
    if(!cnseq->ValidPos(nulpos) || !cnseq->ValidPos(nulpos+2)) return;

    char stop[] = "---";
    stop[0] = cnseq->Upper(nulpos);
    ++nulpos;
    stop[1] = cnseq->Upper(nulpos);
    ++nulpos;
    stop[2] = cnseq->Upper(nulpos);
    m_has_stop_on_nuc = (!strcmp(stop, "TGA") || !strcmp(stop, "TAG") || !strcmp(stop, "TAA"));
}
    

// CPosAli::CPosAli(CSeq_alignHandle& hali, const CSeq_id& protein, const CSeq_loc& genomic, const CProSplignOutputOptions& output_options, CNSeq& nseq, CPSeq& pseq, CNSeq& nseq1)
// : CAli(nseq, pseq), m_genomic(genomic), m_protein(protein), m_output_options(output_options)
// {

//     int& nuc_from = local_param.nuc_from;
//     int& nuc_to = local_param.nuc_to;
//     bool& strand = local_param.strand;

//     m_IsFull = output_options.IsPassThrough();

//     strand = hali.GetStrand();
//     score = 0;

//     EAliPieceType type;
//     CAliPiece alip;

//     int nuc_cur = -1;
//     CDense_seg::TStarts::const_iterator tis = hali.m_sali->GetSegs().GetDenseg().GetStarts().begin();
//     ITERATE(CDense_seg::TLens, it, hali.m_sali->GetSegs().GetDenseg().GetLens()) {
//         if(*tis == -1) type = eHP;
//         else if(*tis == -2) type = eSP;
//         else type = eMP;
//         ++tis;   
//         if(*tis == -1) type = eVP;
//         else {
//             if(nuc_cur == -1) {//init
//                 nuc_cur = *tis;
//                 if(strand) nuc_from = nuc_cur + 1;
//                 else nuc_to = nuc_cur + *it;
//             }
//             //move 
//             if(strand) {
//                 if(nuc_cur != *tis) NCBI_THROW(CProSplignException, eFormat, "Dense-seg format error");
//                 nuc_cur += *it;
//             }
//             else nuc_from = *tis + 1;
//         }
//         ++tis;
//         alip.Init(type, *it);
//         m_ps.push_back(alip);
//     }
//     if(nuc_cur < 0) NCBI_THROW(CProSplignException, eFormat, "not ProSplign Seq-align");
//     if(strand) nuc_to = nuc_cur;
//     if(nuc_to > nseq1.size()) NCBI_THROW(CProSplignException, eFormat, "alignment coordinate exceeds sequence length");

//     /* not needed, three letters are incorporated into CNSeq class
//     //try to add 3 bases at the end for stop check
//     if(strand) {
//         int added;
//         for(added = 0;added<3; ++added) {
//             if(nuc_to >= nseq1.size()) break;
//             nuc_to++;
//         }
//         if(added) {
//             alip.Init(eHP, added);
//             m_ps.push_back(alip);
//         }
//     } else {
//         int added;
//         for(added = 0;added<3; ++added) {
//             if(nuc_from <= 1) break;
//             --nuc_from;
//         }
//         if(added) {
//             alip.Init(eHP, added);
//             m_ps.push_back(alip);
//         }
//     }
//     */
//     nseq.Init(nseq1, local_param);
// }

CRef<CSeq_align> CPosAli::ToSeq_align(int comp_id)
{
	CRef<CSeq_align> topl(new CSeq_align);
    //set comp num
    if(comp_id != -1) {
        CRef<CScore> sc(new CScore);
        sc->SetId().SetStr("compartment_id");
        sc->SetValue().SetInt(comp_id);
        topl->SetScore().push_back(sc);
    }
	if(m_IsFull) topl->SetType(CSeq_align::eType_global);
	else topl->SetType(CSeq_align::eType_disc);
	topl->SetDim(2);
	//strand setup
	ENa_strand nstrand = m_genomic.GetStrand();

	if(m_IsFull) {
		CDense_seg &ds = topl->SetSegs().SetDenseg();

		int nulpos = 0;
		vector<CAliPiece>::const_iterator spit, epit, pit;
		spit = m_ps.begin();
		epit = m_ps.end();
/*
        //throw end gaps
		--epit;
		while(spit->m_type == eHP)  {
			nulpos += spit->m_len;
	        ++spit;
		}
		while(epit->m_type == eHP)  {
			--epit;
		}
		++epit;
*/
		PopulateDense_seg(ds, spit, 0, nulpos, 0, epit, 0, nstrand);
		

	} else {//m_IsFull is false
		CSeq_align_set::Tdata& sas = topl->SetSegs().SetDisc().Set();
		list<CNPiece>::iterator it;
		for(it = m_pcs.begin(); it != m_pcs.end(); ++it) {
			vector<CAliPiece>::const_iterator spit, epit;
			int sshift, eshift;
			int nulpos, nultripos;
			FindPos(spit, sshift, nulpos, nultripos, it->beg);
			FindPos(epit, eshift, it->end);
			CRef<CSeq_align> lol(new CSeq_align);
			lol->SetType(CSeq_align::eType_partial);
			PopulateDense_seg(lol->SetSegs().SetDenseg(), spit, sshift, nulpos, nultripos, epit, eshift, nstrand);
			sas.push_back(lol);
		}
	}

	return topl;
}

void CPosAli::PopulateDense_seg(CDense_seg& ds, vector<CAliPiece>::const_iterator& spit, int sshift, int nulpos, int nultripos, vector<CAliPiece>::const_iterator& epit, int eshift, ENa_strand nstrand)
{
    CRef<CSeq_id> ps(new CSeq_id);
    ps->Assign(m_protein);
    CRef<CSeq_id> ns(new CSeq_id);
    ns->Assign(*m_genomic.GetId());

    int numseg = 0;

    vector<CAliPiece>::const_iterator eepit = epit;
	if(eshift) ++eepit;
	vector<CAliPiece>::const_iterator pit;
	for(pit = spit; pit != eepit; ++pit) {
        //set up piece length
        int len;
        if(pit == epit) len = eshift;        
        else len = pit->m_len;
        if(pit == spit) len -= sshift;        
		ds.SetLens().push_back(len);
        
		ds.SetStrands().push_back(eNa_strand_plus);
		ds.SetStrands().push_back(nstrand);
		switch(pit->m_type) {
			case eMP:
				ds.SetStarts().push_back(nultripos);
				nultripos += len;
				if(IsForward(nstrand)) ds.SetStarts().push_back(NucPosOut(nulpos));
				else ds.SetStarts().push_back(NucPosOut(nulpos + len-1));
				nulpos += len;
				break;
			case eHP:
				ds.SetStarts().push_back(-1);
				if(IsForward(nstrand)) ds.SetStarts().push_back(NucPosOut(nulpos));
				else ds.SetStarts().push_back(NucPosOut(nulpos + len-1));
				nulpos += len;
				break;
			case eVP:
				ds.SetStarts().push_back(nultripos);
				nultripos += len;
				ds.SetStarts().push_back(-1);
				break;
			case eSP:
				ds.SetStarts().push_back(-2);
				if(IsForward(nstrand)) ds.SetStarts().push_back(NucPosOut(nulpos));
				else ds.SetStarts().push_back(NucPosOut(nulpos + len-1));
				nulpos += len;
				break;
			default: 
				break;
		}
		++numseg;
	}
	ds.SetNumseg(numseg);
}

void CPosAli::AddPostProcInfo(const list<CNPiece>& pcs) 
{
	m_IsFull = false;
    m_pcs.clear();
	m_pcs.insert(m_pcs.end(), pcs.begin(), pcs.end());
}

int CPosAli::NucPosOut(int pos) const
{
    if(IsForward(m_genomic.GetStrand()))
        return pos + m_genomic.GetStart(eExtreme_Positional);
    else
        return m_genomic.GetStop(eExtreme_Positional) - pos;
}

// CPosAli::CPosAli(const CAli& ali, const string& nuc_id, const string& prot_id, const list<CNPiece>& pcs) : CAli(ali), m_protid(prot_id), m_nucid(nuc_id)
// {
//     AddPostProcInfo(pcs);
// }

CAliCreator::CAliCreator(CAli& ali) : m_ali(ali), m_CurType(eMP)
{
    m_CurLen = 0; 
}

CAliCreator::~CAliCreator(void)
{
}

//finds protein strand by alignment position (0, 1 or 2)
int CAli::FindFrame(int alipos) const
{
    vector<CAliPiece>::const_iterator pit;
    int shift, nulpos, nultripos;
    FindPos(pit, shift, nulpos, nultripos, alipos);
    return nultripos%3;
}

//finds piece (pit) and shift from the beginning of piece by alignment position
void CAli::FindPos(vector<CAliPiece>::const_iterator& pit, int& shift, int alipos) const
{
    int pos = 0;
    pit = m_ps.begin();
    while(pos <= alipos) {
        if(pit == m_ps.end()) {
            if(pos == alipos) {//alipos is right behind the end of the alignment
                shift = 0;
                return;
            }
            NCBI_THROW(CProSplignException, eOuputError, "wrong alignment position");
        }
        pos += pit->m_len;
        ++pit;
    }
    --pit;
    pos -= pit->m_len;
    shift = alipos - pos;
}

	//finds piece (pit) and shift from the beginning of piece by alignment position
	//AND initial (0-based) protein(multiplied by 3) and nucleotide positions
void CAli::FindPos(vector<CAliPiece>::const_iterator& pit, int& shift, int& nulpos, int& nultripos, int alipos) const
{
    int pos = 0, len;
	nulpos = 0;
	nultripos = 0;
    pit = m_ps.begin();
    while(pos <= alipos) {
        if(pit == m_ps.end()) NCBI_THROW(CProSplignException, eOuputError, "position must be inside alignment");
		if(pos + pit->m_len > alipos) len = alipos - pos;
		else len = pit->m_len;
		switch(pit->m_type) {
			case eMP:
				nultripos += len;
				nulpos += len;
				break;
			case eHP:
				nulpos += len;
				break;
			case eVP:
				nultripos += len;
				break;
			case eSP:
				nulpos += len;
				break;
			default: 
				break;
		}
        pos += pit->m_len;
        ++pit;
    }
    --pit;
    pos -= pit->m_len;
    shift = alipos - pos;
}

CAli::CAli(CNSeq& nseq, CPSeq& pseq, const vector<pair<int, int> >& igi, bool lgap, bool rgap, const CAli& frali)
{
    cnseq = &nseq;
    cpseq = &pseq;
    vector<CAliPiece>::const_iterator pit = frali.m_ps.begin();
    vector<pair<int, int> >::const_iterator igit = igi.begin();
    int curpos = 0;
    if(lgap) {//beginning gap
        if(igit == igi.end() || igit->first != 0) throw runtime_error("beginning gap not found");
        CAliPiece intron;
        intron.Init(eHP);
        if(pit != frali.m_ps.end() && pit->m_type == eHP) {//join beginning gaps
            curpos = igit->second + pit->m_len;
            ++pit;
        } else {//add beginning gap
            curpos = igit->second;
        }
        intron.m_len = curpos;
        m_ps.push_back(intron);
        ++igit;
    }
    while(pit != frali.m_ps.end()) {
        if(pit->m_type == eVP) {
            if(pit+1 == frali.m_ps.end()) {//the last V-gap
                if(pit == frali.m_ps.begin()) throw runtime_error("failed to insert introns, second stage alignment is artificial (V-gap)");
                if(rgap) {
                    if(igit == igi.end()) throw runtime_error("the last intron not found");
                } else if(igit != igi.end()) {//intron goes before the last V-gap
                    if(curpos != igit->first) throw runtime_error("the last intron is misplaced");
                    CAliPiece intron;
                    intron.Init(eSP);
                    intron.m_len = igit->second;
                    m_ps.push_back(intron);
                    curpos = igit->first + igit->second;
                    ++igit;
                }
            }
            m_ps.push_back(*pit);
      }
      else {
        if(igit != igi.end() && curpos > igit->first) throw runtime_error("failed to insert introns into second stage alignment");
        CAliPiece ps;
        ps.Init(pit->m_type);
        int rest = pit->m_len;
        while(igit != igi.end() && curpos + rest > igit->first) {
            ps.m_len = igit->first - curpos;
            if(ps.m_len > 0) {
                m_ps.push_back(ps);
                rest -= ps.m_len;
            }
            CAliPiece intron;
            intron.Init(eSP);
            intron.m_len = igit->second;
            m_ps.push_back(intron);
            curpos = igit->first + igit->second;
            ++igit;
        }
        if(rest > 0) {
            ps.m_len = rest;
            m_ps.push_back(ps);
            curpos += rest;
        }
      }
      ++pit;      
    }
    //the end gap if exists
    if(igit != igi.end()) {
        if(curpos != igit->first) throw runtime_error("misplaced end gap");
        //if(!rgap) throw runtime_error("wrong resulting alignment, intron at the end");
        if(!rgap) {//intron at the end. Artefact?
            CAliPiece intron;
            intron.Init(eSP);
            intron.m_len = igit->second;
            m_ps.push_back(intron);
        } else {//end gap
            if(!m_ps.empty() && m_ps.back().m_type == eHP) {//join end gaps
                m_ps.back().m_len += igit->second;
            } else {// add end gap
                CAliPiece intron;
                intron.Init(eHP);
                intron.m_len = igit->second;
                m_ps.push_back(intron);
            }
        }
        curpos = igit->first + igit->second;
    }
    //checks
    if(curpos != nseq.size()) throw runtime_error("wrong resulting alignment");
}

string CSeq_alignHandle::GetProtId(void)
{
    try {
        const CRef<CSeq_id> psid = m_sali->GetSegs().GetDenseg().GetIds().front();
        return psid->GetSeqIdString(true);
    } catch(...) {
        NCBI_THROW(CProSplignException, eFormat, "not ProSplign Seq-align");
    }
    return "";
}

string CSeq_alignHandle::GetNucId(void)
{
    try {
        const CRef<CSeq_id> psid = m_sali->GetSegs().GetDenseg().GetIds().back();
        return psid->GetSeqIdString(true);
    } catch(...) {
        NCBI_THROW(CProSplignException, eFormat, "not ProSplign Seq-align");
    }
    return "";
}

bool CSeq_alignHandle::GetStrand(void)
{
    try {
        if(eNa_strand_plus == m_sali->GetSegs().GetDenseg().GetStrands().back()) return true;
    } catch(...) {
        NCBI_THROW(CProSplignException, eFormat, "not ProSplign Seq-align");
    }
    return false;
}

int CSeq_alignHandle::GetCompNum(void)
{

    if(m_sali->IsSetScore()) {
        ITERATE(CSeq_align::TScore, it, m_sali->GetScore()) {
            if((*it)->IsSetId() && (*it)->GetId().IsStr() && (*it)->GetId().GetStr() == "compartment_id") {
                return (*it)->GetValue().GetInt();
            }
        }
    }
    return -1;
}

END_SCOPE(prosplign)
END_NCBI_SCOPE
