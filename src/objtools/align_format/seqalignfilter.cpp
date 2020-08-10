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
 * Authors:  Vahram Avagyan
 *
 */

/// @file seqalignfilter.cpp
/// Implementation of the alignment filtering class.
///
#include <ncbi_pch.hpp>

#include <objects/general/User_object.hpp>
#include <objtools/align_format/align_format_util.hpp>
#include <objtools/align_format/seqalignfilter.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/iterator.hpp>

#include <list>
#include <algorithm>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(align_format)

/////////////////////////////////////////////////////////////////////////////

CSeqAlignFilter::CSeqAlignFilter(EResultsFormat eFormat)
: m_eFormat(eFormat)
{
}

CSeqAlignFilter::~CSeqAlignFilter(void)
{
}

/////////////////////////////////////////////////////////////////////////////

void CSeqAlignFilter::FilterSeqaligns(const string& fname_in_seqaligns,
                                      const string& fname_out_seqaligns,
                                      const string& fname_gis_to_filter)
{
    CSeq_align_set full_aln;
    ReadSeqalignSet(fname_in_seqaligns, full_aln);

    CSeq_align_set filtered_aln;
    FilterByGiListFromFile(full_aln, fname_gis_to_filter, filtered_aln);

    WriteSeqalignSet(fname_out_seqaligns, filtered_aln);
}

void CSeqAlignFilter::FilterSeqalignsExt(const string& fname_in_seqaligns,
                                            const string& fname_out_seqaligns,
                                            CRef<CSeqDB> db)
{
    CSeq_align_set full_aln;
    ReadSeqalignSet(fname_in_seqaligns, full_aln);

    CSeq_align_set filtered_aln;
    FilterBySeqDB(full_aln, db, filtered_aln);

    WriteSeqalignSet(fname_out_seqaligns, filtered_aln);
}

/////////////////////////////////////////////////////////////////////////////

void CSeqAlignFilter::FilterByGiListFromFile(const CSeq_align_set& full_aln,
                                                const string& fname_gis_to_filter,
                                                CSeq_align_set& filtered_aln)
{
    CRef<CSeqDBFileGiList> seqdb_gis(new CSeqDBFileGiList(fname_gis_to_filter));

    CConstRef<CSeq_id> id_aligned_seq;
    filtered_aln.Set().clear();

    ITERATE(CSeq_align_set::Tdata, iter, full_aln.Get()) { 
        if (!((*iter)->GetSegs().IsDisc())) {

            // process a single alignment

            id_aligned_seq = &((*iter)->GetSeq_id(1));
            TGi gi = id_aligned_seq->GetGi();

            if (seqdb_gis->FindGi(gi)) {
                filtered_aln.Set().push_back(*iter);
            }
        }
        else {

            // recursively process a set of alignments

            CRef<CSeq_align_set> filtered_sub_aln(new CSeq_align_set);
            FilterByGiListFromFile((*iter)->GetSegs().GetDisc(), fname_gis_to_filter, *filtered_sub_aln);

            CRef<CSeq_align> aln_disc(new CSeq_align);
            aln_disc->Assign(**iter);
            aln_disc->SetSegs().SetDisc(*filtered_sub_aln);

            filtered_aln.Set().push_back(aln_disc);
        }
    }
}


void CSeqAlignFilter::FilterByGiList(const CSeq_align_set& full_aln,
                                        const list<TGi>& list_gis,
                                        CSeq_align_set& filtered_aln)
{
    CConstRef<CSeq_id> id_aligned_seq;
    filtered_aln.Set().clear();

    ITERATE(CSeq_align_set::Tdata, iter, full_aln.Get()) { 
        if (!((*iter)->GetSegs().IsDisc())) {

            // process a single alignment

            id_aligned_seq = &((*iter)->GetSeq_id(1));
            TGi gi = id_aligned_seq->GetGi();

            if (find(list_gis.begin(), list_gis.end(), gi) != list_gis.end()) {
                filtered_aln.Set().push_back(*iter);
            }
        }
        else {

            // recursively process a set of alignments

            CRef<CSeq_align_set> filtered_sub_aln(new CSeq_align_set);
            FilterByGiList((*iter)->GetSegs().GetDisc(), list_gis, *filtered_sub_aln);

            CRef<CSeq_align> aln_disc(new CSeq_align);
            aln_disc->Assign(**iter);
            aln_disc->SetSegs().SetDisc(*filtered_sub_aln);

            filtered_aln.Set().push_back(aln_disc);
        }
    }
}

static void s_GetFilteredRedundantGis(CRef<CSeqDB> db,
                                      int oid,
                                      vector<TGi>& gis)
{
    // Note: copied from algo/blast/api to avoid dependencies

    gis.resize(0);
    if (!db->GetGiList()) {
        return;
    }
    
    list< CRef<CSeq_id> > seqid_list = db->GetSeqIDs(oid);
    gis.reserve(seqid_list.size());
    
    ITERATE(list< CRef<CSeq_id> >, id, seqid_list) {
        if ((**id).IsGi()) {
            gis.push_back((**id).GetGi());
        }
    }

	sort(gis.begin(), gis.end());
}

void CSeqAlignFilter::FilterBySeqDB(const CSeq_align_set& full_aln,
                                    CRef<CSeqDB> db,
                                    CSeq_align_set& filtered_aln)
{
    filtered_aln.Set().clear();

    ITERATE(CSeq_align_set::Tdata, iter_aln, full_aln.Get()) { 
        if (!((*iter_aln)->GetSegs().IsDisc())) {

            // process a single alignment

            // get the gi of the aligned sequence
            CConstRef<CSeq_id> id_aligned_seq;
            id_aligned_seq = &((*iter_aln)->GetSeq_id(1));
            TGi gi_aligned_seq = id_aligned_seq->GetGi();

            // get the corresponding oid from the db (!!! can we rely on this? !!!)
            int oid_aligned_seq = -1;
            db->GiToOid(gi_aligned_seq, oid_aligned_seq);

            // retrieve the filtered list of gi's corresponding to this oid
            vector<TGi> vec_gis_from_DB;

            if (oid_aligned_seq > 0)
                s_GetFilteredRedundantGis(db, oid_aligned_seq, vec_gis_from_DB);

            // if that list is non-empty, add seq-align's with those gi's to the filtered alignment set
            if (!vec_gis_from_DB.empty()) {

                x_CreateOusputSeqaligns(*iter_aln, gi_aligned_seq, filtered_aln, vec_gis_from_DB);
            }
        }
        else {

            // recursively process a set of alignments

            CRef<CSeq_align_set> filtered_sub_aln(new CSeq_align_set);
            FilterBySeqDB((*iter_aln)->GetSegs().GetDisc(), db, *filtered_sub_aln);

            CRef<CSeq_align> aln_disc(new CSeq_align);
            aln_disc->Assign(**iter_aln);
            aln_disc->SetSegs().SetDisc(*filtered_sub_aln);

            filtered_aln.Set().push_back(aln_disc);
        }
    }    
}




/////////////////////////////////////////////////////////////////////////////

void CSeqAlignFilter::x_CreateOusputSeqaligns(CConstRef<CSeq_align> in_aln, TGi in_gi,
                                            CSeq_align_set& out_aln, const vector<TGi>& out_gi_vec)
{
    if (out_gi_vec.size() == 0)
        return;

    if (m_eFormat == eMultipleSeqaligns)
    {
        for (vector<TGi>::const_iterator it_gi_out = out_gi_vec.begin();
                it_gi_out != out_gi_vec.end(); it_gi_out++)
        {
            // get a copy of the input seq-align and change the gi of
            // the aligned sequence to the gi that must go into the output

            bool success = false;
            CRef<CSeq_align> sa_copy = x_UpdateGiInSeqalign(in_aln, 1,
                                                            in_gi, *it_gi_out, success);

            // if the update was successful, add the new seq-align to the results
            if (success)
            {
                // remove any "use_this_gi" entries as the selected format option requires
                x_RemoveExtraGis(sa_copy);

                out_aln.Set().push_back(sa_copy);
            }
        }
    }
    else if (m_eFormat == eCombined)
    {
        // update the main gi of the aligned sequence & add any extra gi's as "use this gi" entries

        vector<TGi> vec_old_extra_gis;
        x_ReadExtraGis(in_aln, vec_old_extra_gis);

        TGi main_new_gi;
        vector<TGi> vec_new_extra_gis;
        x_GenerateNewGis(in_gi, vec_old_extra_gis, out_gi_vec, main_new_gi, vec_new_extra_gis);

        bool success = false;
        CRef<CSeq_align> sa_copy = x_UpdateGiInSeqalign(in_aln, 1, in_gi,
                                                        main_new_gi, success);

        if (success)
        {
            x_RemoveExtraGis(sa_copy);
            x_WriteExtraGis(sa_copy, vec_new_extra_gis);

            out_aln.Set().push_back(sa_copy);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

void CSeqAlignFilter::x_GenerateNewGis(
                    TGi main_old_gi,                        // in: main gi stored before filtering
                    const vector<TGi>& vec_old_extra_gis,    // in: extra gi's stored before filtering
                    const vector<TGi>& vec_out_gis,            // in: list of all gi's after filtering
                    TGi& main_new_gi,                        // out: main gi after filtering
                    vector<TGi>& vec_new_extra_gis)            // out: extra gi's after filtering
{
    if (vec_out_gis.empty())
        return;

    int i_out_gi = 0, i_old_gi = 0, i_new_gi = 0;

    // set the main gi

    if (find(vec_out_gis.begin(), vec_out_gis.end(), main_old_gi) != vec_out_gis.end())
        main_new_gi = main_old_gi;
    else
        main_new_gi = vec_out_gis[0];  //main_new_gi = vec_out_gis[i_out_gi++];

    int num_gis_left = vec_out_gis.size();  //int num_gis_left = vec_out_gis.size() - 1;
    if (num_gis_left > 0)
    {
        // set the extra gi's (copy & filter the old ones, then add the new ones)
        // we do not copy the vec_out_gis directly to preserve the original order
        // (older gi's will appear before newly added gi's)

        vec_new_extra_gis.resize(num_gis_left);

        for (; i_old_gi < (int)(vec_old_extra_gis.size()); i_old_gi++)
        {
            TGi old_gi = vec_old_extra_gis[i_old_gi];
            if (find(vec_out_gis.begin(), vec_out_gis.end(), old_gi) != vec_out_gis.end())
                vec_new_extra_gis[i_new_gi++] = old_gi;
        }

        for (; i_out_gi < (int)(vec_out_gis.size()); i_out_gi++)
        {
            TGi out_gi = vec_out_gis[i_out_gi];
            if (find(vec_old_extra_gis.begin(), vec_old_extra_gis.end(), out_gi)
                == vec_old_extra_gis.end())    // not one of the old gis (already copied)
            {
                // if (out_gi != main_new_gi)    // not the main gi (already set)
                     vec_new_extra_gis[i_new_gi++] = out_gi;
            }
        }
    }
    else
    {
        // no extra gi's to copy

        vec_new_extra_gis.clear();
    }
}

/////////////////////////////////////////////////////////////////////////////

CRef<CSeq_align> CSeqAlignFilter::x_UpdateGiInSeqalign(CConstRef<CSeq_align> sa, unsigned int n_row,
                                                     TGi old_gi, TGi new_gi, bool& success)
{
    // create a copy of the given alignment

    CRef<CSeq_align> sa_copy(new CSeq_align);
    sa_copy->Assign(*(sa.GetNonNullPointer()));

    // update the gi of sequence #n_row in the copied alignment structure

    bool gi_changed = false;

    if (sa_copy->GetSegs().IsDendiag())
    {
        // find and update gi's in every appropriate diag entry

        CSeq_align::C_Segs::TDendiag& dendiag = sa_copy->SetSegs().SetDendiag();
        NON_CONST_ITERATE(CSeq_align::C_Segs::TDendiag, iter_diag, dendiag)
        {
            if ((*iter_diag)->IsSetIds() && n_row < (*iter_diag)->GetIds().size())
            {
                const CSeq_id& id_to_change = *((*iter_diag)->GetIds()[n_row]);
                if (id_to_change.IsGi() &&
                    id_to_change.GetGi() == old_gi)
                {
                    (*iter_diag)->SetIds()[n_row]->SetGi(new_gi);
                    gi_changed = true;
                }
            }
        }
    }
    else if (sa_copy->GetSegs().IsDenseg())
    {
        // update the gi in the dense-seg entry

        CSeq_align::C_Segs::TDenseg& denseg = sa_copy->SetSegs().SetDenseg();
        if (denseg.IsSetIds() && n_row < denseg.GetIds().size())
        {
            const CSeq_id& id_to_change = *(denseg.GetIds()[n_row]);
            if (id_to_change.IsGi() &&
                id_to_change.GetGi() == old_gi)
            {
                denseg.SetIds()[n_row]->SetGi(new_gi);
                gi_changed = true;
            }
        }
    }
    else if (sa_copy->GetSegs().IsStd())
    {
        // find and update gi's in every appropriate seq-loc entry in the std-segs

        CSeq_align::C_Segs::TStd& stdsegs = sa_copy->SetSegs().SetStd();
        NON_CONST_ITERATE(CSeq_align::C_Segs::TStd, iter_std, stdsegs)
        {
            if ((*iter_std)->IsSetLoc() && n_row < (*iter_std)->GetLoc().size())
            {
                CSeq_loc& loc_to_change = *((*iter_std)->SetLoc()[n_row]);

                // question: do seq-locs ever contain parts of different sequences?

                const CSeq_id* p_id_to_change = loc_to_change.GetId();
                if (p_id_to_change)        // one and only one id is associated with this seq-loc
                {
                    if (p_id_to_change->IsGi() &&
                        p_id_to_change->GetGi() == old_gi)
                    {
                        CRef<CSeq_id> id_updated(new CSeq_id(CSeq_id::e_Gi, new_gi));
                        loc_to_change.SetId(*id_updated);
                        gi_changed = true;
                    }
                }
            }
        }
    }
    else
    {
        // these alignment types are not supported here
    }

    success = gi_changed;
    return sa_copy;
}

void CSeqAlignFilter::x_ReadExtraGis(CConstRef<CSeq_align> sa, vector<TGi>& vec_extra_gis)
{
    vec_extra_gis.clear();

    CSeq_align::TScore score_entries = sa->GetScore();
    ITERATE(CSeq_align::TScore, iter_score, score_entries)
    {
        CRef<CScore> score_entry = *iter_score;

        if (score_entry->CScore_Base::IsSetId())
            if (score_entry->GetId().IsStr())
            {
                string str_id = score_entry->GetId().GetStr();
                if (str_id == "use_this_gi")
                {
                    TGi gi = GI_FROM(CScore::C_Value::TInt, score_entry->GetValue().GetInt());
                    vec_extra_gis.push_back(gi);
                }
            }
    }
}

void CSeqAlignFilter::x_WriteExtraGis(CRef<CSeq_align> sa, const vector<TGi>& vec_extra_gis)
{
    for (int i_gi = 0; i_gi < (int)(vec_extra_gis.size()); i_gi++)
        x_AddUseGiEntryInSeqalign(sa, vec_extra_gis[i_gi]);
}

void CSeqAlignFilter::x_RemoveExtraGis(CRef<CSeq_align> sa)
{
    CSeq_align::TScore& score_entries = sa->SetScore();

    CSeq_align::TScore::iterator iter_score = score_entries.begin();
    while (iter_score != score_entries.end())
    {
        CRef<CScore> score_entry = *iter_score;
        bool erase_entry = false;

        if (score_entry->IsSetId())
            if (score_entry->GetId().IsStr())
            {
                string str_id = score_entry->GetId().GetStr();
                erase_entry = (str_id == "use_this_gi");
            }

        if (erase_entry)
            iter_score = score_entries.erase(iter_score);
        else
            iter_score++;
    }
}

bool CSeqAlignFilter::x_AddUseGiEntryInSeqalign(CRef<CSeq_align> sa, TGi new_gi)
{
    // add a "use this gi" entry with the new gi to the score section of the alignment

    CRef<CScore> score_entry(new CScore);
    score_entry->SetId().SetStr("use_this_gi");
    score_entry->SetValue().SetInt(GI_TO(CScore::C_Value::TInt, new_gi));

    sa->SetScore().push_back(score_entry);

    return true;
}

CRef<CSeqDB> CSeqAlignFilter::PrepareSeqDB(const string& fname_db, bool is_prot,
                                            const string& fname_gis)
{
    CRef<CSeqDBFileGiList> seqdb_gis;
    seqdb_gis = new CSeqDBFileGiList(fname_gis);

    CRef<CSeqDB> seqdb;
    seqdb = new CSeqDB(fname_db,
                        is_prot? CSeqDB::eProtein : CSeqDB::eNucleotide,
                        seqdb_gis);
    return seqdb;
}

void CSeqAlignFilter::ReadSeqalignSet(const string& fname, CSeq_align_set& aln)
{
    auto_ptr<CObjectIStream> asn_in(CObjectIStream::Open(fname, eSerial_AsnText));
    *asn_in >> aln;
}

void CSeqAlignFilter::WriteSeqalignSet(const string& fname, const CSeq_align_set& aln)
{
    auto_ptr<CObjectOStream> asn_out(CObjectOStream::Open(fname, eSerial_AsnText));
    *asn_out << aln;
}

void CSeqAlignFilter::ReadGiList(const string& fname, list<TGi>& list_gis, bool sorted)
{
    CRef<CSeqDBFileGiList> seqdb_gis;
    seqdb_gis = new CSeqDBFileGiList(fname);

    vector<TGi> vec_gis;
    seqdb_gis->GetGiList(vec_gis);

    if (sorted)
        sort(vec_gis.begin(), vec_gis.end());

    list_gis.clear();
    for (vector<TGi>::iterator it = vec_gis.begin(); it != vec_gis.end(); it++)
        list_gis.push_back(*it);
}

void CSeqAlignFilter::ReadGiVector(const string& fname, vector<TGi>& vec_gis, bool sorted)
{
    CRef<CSeqDBFileGiList> seqdb_gis;
    seqdb_gis = new CSeqDBFileGiList(fname);

    seqdb_gis->GetGiList(vec_gis);
    if (sorted)
        sort(vec_gis.begin(), vec_gis.end());
}

static CRef<CSeq_align> s_UpdateSubjectInSeqalign(CRef<CSeq_align> &in_align, CRef<CSeq_id> &newSeqID)
                                                     
{
    // create a copy of the given alignment
    CRef<CSeq_align> sa_copy(new CSeq_align);
    sa_copy->Assign(*(in_align.GetNonNullPointer()));

    const CSeq_id& subjid = in_align->GetSeq_id(1);
    if(!subjid.Match(*newSeqID)) {
        if (sa_copy->GetSegs().IsDenseg())
        {
            CSeq_align::C_Segs::TDenseg& denseg = sa_copy->SetSegs().SetDenseg();
            if (denseg.IsSetIds() && denseg.GetIds().size() == 2)
            {
                denseg.SetIds()[1] = newSeqID;            
            }
        }
    }    
    return sa_copy;
}


CRef<CSeq_align> s_UpdateSeqAlnWithFilteredSeqIDs(CRef<CSeqDB> filteredDB,
                                              int oid,
                                              CRef<CSeq_align> &in_align)                                      
{
    CRef<CSeq_align> sa_copy;    
    CRef<CSeq_id> newSubjectID;

    const CSeq_id& subjid = in_align->GetSeq_id(1);
        
    vector< CRef<CSeq_id> > seqids;
    list< CRef<CSeq_id> > seqid_list = filteredDB->GetSeqIDs(oid);
    seqids.reserve(seqid_list.size());
    
    ITERATE(list< CRef<CSeq_id> >, id, seqid_list) {    
        if(subjid.IsGi() && (**id).IsGi()) {        
            seqids.push_back(*id);
        }
        else if(!subjid.IsGi() && !(**id).IsGi()) {
            seqids.push_back(*id);
        }
    }

   if(!seqids.empty()) {
        //update main subject and and add use_this_seq                
        newSubjectID = seqids[0];
        sa_copy = s_UpdateSubjectInSeqalign(in_align,newSubjectID);
        vector <string> useThisSeqs;
        for(size_t i = 0; i < seqids.size(); i++) {
            string textSeqID;
            CAlignFormatUtil::GetTextSeqID(seqids[i], &textSeqID);
            if(seqids[0]->IsGi()) {
                useThisSeqs.push_back("gi:" + textSeqID);
             }
             else {
                useThisSeqs.push_back("seqid:" + textSeqID);
             }
         }          
         CRef<CUser_object> userObject(new CUser_object());
	     userObject->SetType().SetStr("use_this_seqid");
	     userObject->AddField("SEQIDS", useThisSeqs);
         sa_copy->ResetExt();
	     sa_copy->SetExt().push_back(userObject);      
    }     
    return sa_copy;
}


CRef<CSeq_align_set> CSeqAlignFilter::FilterBySeqDB(const CSeq_align_set& seqalign, //CRef<CSeq_align_set> &seqalign
                                                    CRef<CSeqDB> &filteredDB,
                                                    vector<int>& oid_vec)
{
    CConstRef<CSeq_id> previous_id, subjid;    
    size_t i = 0;
    bool success = false;
    CRef<CSeq_id> newSubjectID;

    CRef<CSeq_align_set> new_aln(new CSeq_align_set);

    ITERATE(CSeq_align_set::Tdata, iter, seqalign.Get()){ 
        CRef<CSeq_align> currAlign = *iter;
        CRef<CSeq_align> filtered_aln;

        subjid = &(currAlign->GetSeq_id(1));    
        if(previous_id.Empty() || !subjid->Match(*previous_id))
        {
            success = false;
            if (oid_vec[i] > 0) { 
                // retrieve the filtered list of gi's corresponding to this oid
                filtered_aln = s_UpdateSeqAlnWithFilteredSeqIDs(filteredDB, oid_vec[i], currAlign);
                if(!filtered_aln.Empty()) { //found sequence with this oid oid_vec[i] in filtered seqdb
                    newSubjectID.Reset(const_cast<CSeq_id*>(&filtered_aln->GetSeq_id(1)));                    
                    success = true;
                }                    
            }
            i++;
        }
        else {
            if(success) {
                filtered_aln = s_UpdateSubjectInSeqalign(currAlign,newSubjectID);
            }
        }
        previous_id = subjid;

        if(success && !filtered_aln.Empty()) {                
            new_aln->Set().push_back(filtered_aln);
        }            
    }    
    return new_aln;
}

bool static s_IncludeDeflineTaxid(const CBlast_def_line & def, const set<TTaxId> & user_tax_ids)
{
    CBlast_def_line::TTaxIds tax_ids;
    if (def.IsSetTaxid()) {
        tax_ids.insert(def.GetTaxid());
    }
    if(def.IsSetLinks()) {
        CBlast_def_line::TLinks leaf_ids = def.GetLinks();
#ifdef NCBI_STRICT_TAX_ID
        ITERATE(CBlast_def_line::TLinks, it, leaf_ids) tax_ids.insert(TAX_ID_FROM(int, *it));
#else
        tax_ids.insert(leaf_ids.begin(), leaf_ids.end());
#endif
    }

    if(user_tax_ids.size() > tax_ids.size()) {
        ITERATE(CBlast_def_line::TTaxIds, itr, tax_ids) {
          if(user_tax_ids.find(*itr) != user_tax_ids.end()) {
            return true;
          }
        }
    }
    else {
        ITERATE(set<TTaxId>, itr, user_tax_ids) {
            if(tax_ids.find(*itr) != tax_ids.end()) {
                return true;
            }
        }
    }
    return false;
}


static CRef<CSeq_align> s_ModifySeqAlnWithFilteredSeqIDs(CRef<CBlast_def_line_set>  &bdlRef,
                                                         const set<TTaxId>& taxids,
                                                         CRef<CSeq_align> &in_align)
{
    CRef<CSeq_align> sa_copy;

    const list< CRef< CBlast_def_line > >& bdlSet = bdlRef->Get();
    vector <string> useThisSeqs;
    ITERATE(list<CRef<CBlast_def_line> >, iter, bdlSet) {
        const CBlast_def_line & defline = **iter;
        bool has_match = s_IncludeDeflineTaxid(defline, taxids);
        CRef<CSeq_id> seqID;
        string textSeqID;
        if(has_match) {
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            seqID = FindBestChoice(cur_id, CSeq_id::WorstRank);
            CAlignFormatUtil::GetTextSeqID(seqID, &textSeqID);

            list<string> use_this_seq;
            CAlignFormatUtil::GetUseThisSequence(*in_align,use_this_seq);  
            if(use_this_seq.size() > 0) {
                has_match = CAlignFormatUtil::MatchSeqInUseThisSeqList(use_this_seq, textSeqID);          
            }
        }
        if(has_match) {
            if(sa_copy.Empty()) {                
                sa_copy = s_UpdateSubjectInSeqalign(in_align,seqID);
            }
            if(seqID->IsGi()) {
                useThisSeqs.push_back("gi:" + textSeqID);
            }
            else {
                useThisSeqs.push_back("seqid:" + textSeqID);
            }   
            CRef<CUser_object> userObject(new CUser_object());
	        userObject->SetType().SetStr("use_this_seqid");
	        userObject->AddField("SEQIDS", useThisSeqs);
            sa_copy->ResetExt();
	        sa_copy->SetExt().push_back(userObject);      
        }                
    }
    return sa_copy;
}

        

CRef<CSeq_align_set> CSeqAlignFilter::FilterByTaxonomy(const CSeq_align_set& seqalign, //CRef<CSeq_align_set> &seqalign                                                    
                                                        CRef<CSeqDB> &seqdb,
                                                        const set<TTaxId>& taxids)
{
    CConstRef<CSeq_id> previous_id, subjid;    
    bool success = false;
    CRef<CSeq_id> newSubjectID;

    CRef<CSeq_align_set> new_aln(new CSeq_align_set);

    ITERATE(CSeq_align_set::Tdata, iter, seqalign.Get()){ 
        CRef<CSeq_align> currAlign = *iter;
        CRef<CSeq_align> filtered_aln;

        subjid = &(currAlign->GetSeq_id(1));    
        if(previous_id.Empty() || !subjid->Match(*previous_id))
        {
            success = false;
            int oid;
            seqdb->SeqidToOid(*subjid, oid);    
            CRef<CBlast_def_line_set> bdlSet = seqdb->GetHdr(oid);               
            filtered_aln = s_ModifySeqAlnWithFilteredSeqIDs(bdlSet, taxids, currAlign);                
            if(!filtered_aln.Empty()) { //found sequence with this oid oid_vec[i] in filtered seqdb
                newSubjectID.Reset(const_cast<CSeq_id*>(&filtered_aln->GetSeq_id(1)));                    
                success = true;
            }                                
        }
        else {
            if(success) {
                filtered_aln = s_UpdateSubjectInSeqalign(currAlign,newSubjectID);
            }
        }
        previous_id = subjid;

        if(success && !filtered_aln.Empty()) {                
            new_aln->Set().push_back(filtered_aln);
        }            
    }    
    return new_aln;
}

static CRef<CSeq_align> s_ModifySeqAlnWithFilteredSeqIDs(CRef <CScope> &scope,
                                                         CSeq_id::EAccessionInfo accType,                                                         
                                                         CRef<CSeq_align> &in_align)
{
    CRef<CSeq_align> sa_copy;

    list<string> use_this_seq;
    CAlignFormatUtil::GetUseThisSequence(*in_align,use_this_seq);  
    bool modifyAlignment = false;
    if(use_this_seq.size() > 0) {
        modifyAlignment = CAlignFormatUtil::RemoveSeqsOfAccessionTypeFromSeqInUse(use_this_seq, accType);            
    }
    else { //go through blast deflines               
        const CSeq_id &subjid = in_align->GetSeq_id(1);                 
        const CBioseq_Handle& handle = scope->GetBioseqHandle(subjid);
        CRef<CBlast_def_line_set> bdlRef = CSeqDB::ExtractBlastDefline(handle);
        const list< CRef< CBlast_def_line > >& bdlSet = bdlRef->Get();  
        ITERATE(list<CRef<CBlast_def_line> >, iter, bdlSet) {
            string textSeqID;            
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            CRef<CSeq_id> seqID = FindBestChoice(cur_id, CSeq_id::WorstRank);                        
            CSeq_id::EAccessionInfo accInfo = seqID->IdentifyAccession();                                    
            if(accInfo != accType) {//not CSeq_id::eAcc_refseq_prot        
                CAlignFormatUtil::GetTextSeqID(seqID, &textSeqID);                
                use_this_seq.push_back(textSeqID);             
            }
            else {
                modifyAlignment = true;
            }            
        }
    }    
    if(!modifyAlignment) {            
        sa_copy = in_align;
    }        
    if(modifyAlignment && use_this_seq.size() > 0) { 
        vector <string> useThisSeqs;    
        CRef<CSeq_id> seqID (new CSeq_id(*(use_this_seq.begin())));        
        sa_copy = s_UpdateSubjectInSeqalign(in_align,seqID);            
        ITERATE(list<string>, iter_seq, use_this_seq){                        
            if(seqID->IsGi()) {
                useThisSeqs.push_back("gi:" + *iter_seq);
            }
            else {
                useThisSeqs.push_back("seqid:" + *iter_seq);
            }   
            CRef<CUser_object> userObject(new CUser_object());
	        userObject->SetType().SetStr("use_this_seqid");
	        userObject->AddField("SEQIDS", useThisSeqs);
            sa_copy->ResetExt();
	        sa_copy->SetExt().push_back(userObject);      
        }                    
    }
    return sa_copy;
}            

CRef<CSeq_align_set> CSeqAlignFilter::FilterByAccessionType(const CSeq_align_set& seqalign, //CRef<CSeq_align_set> &seqalign                                                    
                                                            CRef <CScope> &scope,
                                                            vector <CSeq_id::EAccessionInfo> accTypes)                                                        
{
    CConstRef<CSeq_id> previous_id, subjid;    
    bool success = false;
    CRef<CSeq_id> newSubjectID;

    CRef<CSeq_align_set> new_aln(new CSeq_align_set);

    ITERATE(CSeq_align_set::Tdata, iter, seqalign.Get()){ 
        CRef<CSeq_align> currAlign = *iter;
        CRef<CSeq_align> filtered_aln;

        subjid = &(currAlign->GetSeq_id(1));    
        if(previous_id.Empty() || !subjid->Match(*previous_id))
        {
            success = false;
            for (size_t i = 0; i < accTypes.size(); i++) {
                if(!currAlign.Empty()) {
                    filtered_aln = s_ModifySeqAlnWithFilteredSeqIDs(scope, accTypes[i], currAlign);                
                    currAlign = filtered_aln;
                }
            }
            if(!filtered_aln.Empty()) { 
                newSubjectID.Reset(const_cast<CSeq_id*>(&filtered_aln->GetSeq_id(1)));                
                success = true;
            }                                
        }
        else {
            if(success) {
                filtered_aln = s_UpdateSubjectInSeqalign(currAlign,newSubjectID);
            }            
        }
        previous_id = subjid;

        if(success && !filtered_aln.Empty()) {                
            new_aln->Set().push_back(filtered_aln);
        }            
    }    
    return new_aln;
}
END_SCOPE(align_format)
END_NCBI_SCOPE
