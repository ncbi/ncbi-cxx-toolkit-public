#ifndef _SEMANTIC_ACTIONS_HPP_
#define _SEMANTIC_ACTIONS_HPP_

#include <corelib/ncbistd.hpp>
#include <objects/varrep/varrep__.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

void AssignRefSeqIdentifier(const string& identifier, CRef<CVariantExpression>& result);

void TagAsChimera(CRef<CSequenceVariant>& seq_var);

void TagAsMosaic(CRef<CSequenceVariant>& seq_var);

void AssignSequenceVariant(CRef<CSequenceVariant>& variant, CRef<CVariantExpression>& result);

void AssignMissense(CRef<CAaSite>& initial, 
                    const CProteinSub::TFinal& final, 
                    CRef<CSimpleVariant>& result);

void AssignNonsense(CRef<CAaSite>& initial, CRef<CSimpleVariant>& result);

void AssignUnknownSub(CRef<CAaSite>& initial, CRef<CSimpleVariant>& result);

void AssignAaDup(CRef<CAaLocation>& aa_loc, CRef<CSimpleVariant>& result);

void AssignAaDel(CRef<CAaLocation>& aa_loc, CRef<CSimpleVariant>& result);

void AssignNtermExtension(CRef<CAaSite>& initial_start_site, CRef<CCount>& new_start_site, CRef<CSimpleVariant>& result);

void AssignNtermExtension(CRef<CAaSite>& initial_start_site, const string& new_aa, CRef<CCount>& new_start_site, CRef<CSimpleVariant>& result);

void AssignCtermExtension(const string& initial_stop_site, const string& aa, CRef<CCount>& new_stop_site, CRef<CSimpleVariant>& result); 

void AssignAaIntervalLocation(CRef<CAaInterval>& aa_interval, CRef<CAaLocation>& result); 

void AssignAaSiteLocation(CRef<CAaSite>& aa_site, CRef<CAaLocation>& result);

void AssignAaInterval(CRef<CAaSite>& start, CRef<CAaSite>& stop, CRef<CAaInterval>& result);

void AssignAaSite(const string& aa, const string& pos, CRef<CAaSite>& result);

void AssignCount(const string& count, CRef<CCount>& result);

void AssignFuzzyCount(const string& count, CRef<CCount>& result);

void AssignCountRange(const string& start, const string& stop, CRef<CCount>& result);

void AssignMinCount(const string& min_count, CRef<CCount>& result);

void AssignMaxCount(const string& max_count, CRef<CCount>& result);

void AssignAaSSR(CRef<CAaLocation>& aa_loc, CRef<CCount>& count, CRef<CSimpleVariant>& result);

void AssignAaInsertion(CRef<CAaInterval>& aa_interval, 
                       const CInsertion::TSeqinfo::TRaw_seq& raw_seq, 
                       CRef<CSimpleVariant>& result);

void AssignAaInsertionSize(CRef<CAaInterval>& aa_interval, CRef<CCount>& seq_size, CRef<CSimpleVariant>& result);

void AssignFrameshift(CRef<CAaSite>& aa_site, CRef<CSimpleVariant>& result);

void AssignAaDelins(CRef<CAaLocation>& aa_loc, const string& raw_seq, CRef<CSimpleVariant>& result);

void AssignAaDelinsSize(CRef<CAaLocation>& aa_loc, CRef<CCount> seq_size, CRef<CSimpleVariant>& result); 

void AssignFuzzyLocalVariation(CRef<CSimpleVariant>& input, CRef<CSimpleVariant>& result);

void AssignFuzzyLocalVariantSeq(CRef<CSimpleVariantSeq>& in, CRef<CSimpleVariantSeq>& result);

void AssignSimpleVariant(CRef<CSimpleVariant>& simple_variant, CRef<CVariant>& result);

void AssignSpecialVariant(ESpecialVariant special_variant, CRef<CVariant>& result);

void AssignSingleVariation(CRef<CVariant>& variant, CRef<CSequenceVariant>& result);

void AssignSingleLocalVariation(CRef<CSimpleVariant>& local_var, CRef<CSequenceVariant>& result);

/*
void AssignUnknownChromosomeVariant(CRef<CSimpleVariantSeq>& variant_seq, CRef<CSequenceVariant>& result);

void AssignSecondChromosomeVariant(CRef<CSimpleVariantSeq>& variant_seq, CRef<CSequenceVariant>& result);

void AssignSecondChromosomeSpecialVariant(const ESpecialVariant& special_variant, CRef<CSequenceVariant>& result);

void AssignChromosomeVariant(CRef<CSimpleVariantSeq>& variant_seq, CRef<CSequenceVariant>& result);
*/


void AssignSequenceType(CRef<CSequenceVariant>& result);


void AssignFuzzySimpleNtSite(const string& site_index, CRef<CNtSite>& result);

void AssignSimpleNtSite(const string& site_index, CRef<CNtSite>& result);

void Assign3primeUTRSite(CRef<CNtSite>& nt_site, CRef<CNtSite>& result);

void Assign5primeUTRSite(CRef<CNtSite>& nt_site, CRef<CNtSite>& result);

void AssignIntronSite(const string& base, const string& offset, CRef<CNtSite>& result);

void AssignNtSiteRange(CRef<CNtSite>& start, CRef<CNtSite>& stop, CRef<CNtLocation>& result);

void AssignNtSite(CRef<CNtSite>& local_site, CRef<CNtLocation>& result);

void AssignFuzzyNtSite(CRef<CNtSite>& center_site, CRef<CNtSite>& result);

void AssignNtInterval(CRef<CNtLocation>& start, CRef<CNtLocation>& stop, CRef<CNtLocation>& result);

void AssignNtSiteLocation(CRef<CNtSite>& nt_site, CRef<CNtLocation>& result);

void AssignNtIntervalLocation(CRef<CNtInterval>& nt_int, CRef<CNtLocation>& result);

void AssignNtRemoteLocation(const string& seq_id, const string& seq_type, CRef<CNtLocation>& nt_loc, CRef<CNtLocation>& result);

void AssignNtRemoteLocation(const string& seq_id, CRef<CNtLocation>& nt_loc, CRef<CNtLocation>& result);

void AssignNtSSR(CRef<CNtLocation>& nt_loc, const CRepeat::TRaw_seq& raw_seq, CRef<CCount>& count, CRef<CSimpleVariant>& result);

void AssignNtSSR(CRef<CNtLocation>& nt_loc, CRef<CCount>& count, CRef<CSimpleVariant>& result);

void AssignNtInv(CRef<CNtLocation>& nt_int, CRef<CSimpleVariant>& result);

void AssignNtInv(CRef<CNtLocation>& nt_int, const CInversion::TRaw_seq& raw_seq, CRef<CSimpleVariant>& result);

void AssignNtInvSize(CRef<CNtLocation>& nt_int, string size, CRef<CSimpleVariant>& result);

void AssignNtConversion(CRef<CNtLocation>& src_int, CRef<CNtLocation>& dest_int, CRef<CSimpleVariant>& result);

void AssignNtInsertion(CRef<CNtLocation>& nt_int, const CInsertion::TSeqinfo::TRaw_seq& raw_seq, CRef<CSimpleVariant>& result);

void AssignNtDeletion(CRef<CNtLocation>& nt_loc, CRef<CSimpleVariant>& result);

void AssignNtDeletion(CRef<CNtLocation>& nt_loc, const CDeletion::TRaw_seq& raw_seq, CRef<CSimpleVariant>& result);

void AssignNtDelins(CRef<CNtLocation>& nt_loc, const CInsertion::TSeqinfo::TRaw_seq& inserted_seq, CRef<CSimpleVariant>& result);

void AssignNtDelins(CRef<CNtLocation>& nt_loc,
                    const CDeletion::TRaw_seq& deleted_seq, 
                    const CInsertion::TSeqinfo::TRaw_seq& inserted_seq,
                    CRef<CSimpleVariant>& result);

void AssignNtDup(CRef<CNtLocation>& nt_loc, CRef<CSimpleVariant>& result);

void AssignNtDup(CRef<CNtLocation>& nt_loc, const CDuplication::TRaw_seq& raw_seq, CRef<CSimpleVariant>& result);

void AssignNtSub(CRef<CNtLocation>& nt_loc, 
                 const CNaSub::TInitial& initial_nt,
                 const CNaSub::TFinal& final_nt,
                 CRef<CSimpleVariant>& result);

void AssignNtIdentity(CRef<CNtLocation>& nt_loc,
                      const CNaIdentity::TNucleotide& nucleotide,
                      CRef<CSimpleVariant>& result);

void AssignNtIdentity(CRef<CNtLocation>& nt_loc,
                      CRef<CSimpleVariant>& result);

void AssignSequenceType(const string& type, CRef<CSequenceVariant>& result);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
