/* $Id$
*===========================================================================
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
* File Name:  thrddecl.h
*
* Author:  Stephen Bryant
*
* Initial Version Creation Date: 08/16/2000
*
* $Revision$
*
* File Description: threader
*
* Modifications:
* --------------------------------------------------------------------------
* $Log$
* Revision 1.4  2006/08/09 19:04:13  lanczyck
* add export macros for ncbi_algo_structure.dll
*
* Revision 1.3  2006/01/09 12:52:44  thiessen
* tweaks to preclude MSVC warnings, mainly making void returns and removing unused vars
*
* Revision 1.2  2005/10/31 21:57:16  thiessen
* MSVC fix
*
* Revision 1.1  2005/10/31 21:26:38  thiessen
* check in threader to C++ toolkit, with no C toolkit dependencies
*
* Revision 1.14  2001/04/26 17:23:38  thiessen
* fix bug in updating aligned residue types
*
* Revision 1.13  2001/04/25 15:43:29  hurwitz
* initial check-in of Anna's code to fix problem of duplicate alignments with different scores
*
* Revision 1.12  2001/03/02 23:14:12  hurwitz
* run threading faster for PSSM weight=1, bug fix
*
* Revision 1.11  2001/01/16 17:17:58  hurwitz
* changed Int4s to ints, added PrintSeqMtf
*
* Revision 1.10  2000/12/20 18:56:41  hurwitz
* new random num gen, more debug printing
*
* Revision 1.9  2000/12/14 21:07:58  hurwitz
* adding debugging routines, scaling-factor fixes
*
* Revision 1.8  2000/12/06 23:36:50  hurwitz
* sort the results by score in atd
*
* Revision 1.7  2000/12/05 20:52:21  hurwitz
* put threading results in order
*
* Revision 1.6  2000/11/02 20:54:16  hurwitz
* added options for z-score calculations, fixed initialization prob
*
* Revision 1.5  2000/09/22 22:31:33  hurwitz
* added memory management of ThdTbl (results structure)
*
* Revision 1.4  2000/09/14 22:23:39  hurwitz
* adding memory management for contact-potential, annealing-schedule, and folding-motif
*
* Revision 1.3  2000/08/30 21:33:55  hurwitz
* added new and free functions for Seq_Mtf and Qry_Seq
*
* Revision 1.2  2000/08/23 20:10:42  hurwitz
* added memory management functions for Cor_Def and extern C in .h files
*
* Revision 1.1  2000/08/16 20:45:21  hurwitz
* initial check in of threading routines
*
* ==========================================================================
*/

#if !defined(THRDDECL_H)
#define THRDDECL_H

#include <stdio.h>
#include <corelib/mswin_export.h>

#ifdef __cplusplus
extern "C" {
#endif

NCBI_THREADER_EXPORT 
int atd(Fld_Mtf* mtf, Cor_Def* cdf, Qry_Seq* qsq, Rcx_Ptl* pmf,
        Gib_Scd* gsp, Thd_Tbl* ttb, Seq_Mtf* psm, float* trg, int zscs,
        double ScalingFactor, float PSSM_Weight);

/*int*/ void cprl(Fld_Mtf* mtf, Cor_Def* cdf, Rcx_Ptl* pmf, Cxl_Los** cpr, int ct);

/*int*/ void ttb0(Thd_Tbl* ttb);

/*int*/ void sgoi(int is, int it, Rnd_Smp* pvl, Seg_Ord* sgo);

int slo0(Fld_Mtf* mtf, Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli,
         int cs, int ct, int* mn, int* mx);

int rsmp(Rnd_Smp* pvl);
float Rand01(int* idum);

int sal0(Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli, Cur_Aln* sai,
         int cs, int* mn, int* mx);

int spci(Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli, Cur_Aln* sai,
         int n, Seg_Cmp* spc);

/*int*/ void spni(Cxl_Los** cpl, Cur_Loc* sli, int n, Seg_Nsm* spn);

/*int*/ void cxei(Seg_Nsm* spn, Seg_Cmp* spc, Rcx_Ptl* pmf, Cur_Loc* sli, Seq_Mtf* psm, Cor_Def* cdf, Thd_Cxe* cxe);

/*int*/ void cpll(Cor_Def* cdf, Rcx_Ptl* pmf, Qry_Seq* qsq, Cxl_Los** cpr,
         Cur_Aln* sai, Cxl_Los** cpl);

/*int*/ void spel(Cxl_Los** cpl, Cur_Aln* sai, Cur_Loc* sli, int cs, Seg_Gsm* spe,
         Cor_Def* cdf, Seq_Mtf* psm, Seg_Cmp* spc);

int ttbi(Cur_Aln* sai, Cur_Loc* sli, Thd_Gsm* tdg, Thd_Tbl* ttb,
         int nrs, int o);

/*int*/ void cpal(Rcx_Ptl* pmf, Cxl_Los** cpr, Cur_Loc* sli, Cxl_Als** cpa);

/*int*/ void spea(Cor_Def* cdf, Cxl_Als** cpa, Cur_Aln* sai, Cur_Loc* sli,
         int n, int h, Seg_Gsm* spe, Seq_Mtf* psm, Seg_Cmp* spc);

int salr(Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli, Cur_Aln* sai,
         int cs, int* mn, int* mx);

/*int*/ void salu(Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli,
         int cs, int al, Cur_Aln* sai);

int algs(Rnd_Smp* pvl, int tm);

int slor(Fld_Mtf* mtf, Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli, Cur_Aln* sai,
         int cs, int ct, int* mn, int*mx);

/*int*/ void slou(Fld_Mtf* mtf, Cor_Def* cdf, int cs, int ct, int of, Cur_Loc* sli,
	Cur_Aln* sai, Qry_Seq* qsq);

/*float*/ void bwfi(Thd_Tbl* ttb, Gib_Scd* gsp, Thd_Tst* tts);

void zsc(Thd_Tbl* ttb, Seq_Mtf* psm, Qry_Seq* qsq, Cxl_Los** cpr,
          Cor_Def* cdf, Rcx_Ptl* pmf, Seg_Gsm* spe, Cur_Aln* sai, Rnd_Smp* pvl,
          double ScalingFactor);

int g(Seg_Gsm* spe, Seq_Mtf* psm, Thd_Gsm* tdg);

float g0(Seg_Nsm* spn, Thd_Cxe* cxe);

float dgri(Seg_Gsm* spe, Seg_Nsm* spn, Thd_Cxe* cxe, Thd_Gsm* tdg,
           Seq_Mtf* psm, Seg_Cmp* spc);

NCBI_THREADER_EXPORT 
Cor_Def*  NewCorDef(int NumBlocks);
NCBI_THREADER_EXPORT 
Cor_Def*  FreeCorDef(Cor_Def* cdf);
NCBI_THREADER_EXPORT 
void      PrintCorDef(Cor_Def* cdf, FILE* pFile);

NCBI_THREADER_EXPORT 
Seq_Mtf*  NewSeqMtf(int NumResidues, int AlphabetSize);
NCBI_THREADER_EXPORT 
Seq_Mtf*  FreeSeqMtf(Seq_Mtf* psm);
NCBI_THREADER_EXPORT 
void      PrintSeqMtf(Seq_Mtf* psm, FILE* pFile);

NCBI_THREADER_EXPORT 
Qry_Seq*  NewQrySeq(int NumResidues, int NumBlocks);
NCBI_THREADER_EXPORT 
Qry_Seq*  FreeQrySeq(Qry_Seq* qsq);
NCBI_THREADER_EXPORT 
void      PrintQrySeq(Qry_Seq* qsq, FILE* pFile);

NCBI_THREADER_EXPORT 
Rcx_Ptl*  NewRcxPtl(int NumResTypes, int NumDistances, int PeptideIndex);
NCBI_THREADER_EXPORT 
Rcx_Ptl*  FreeRcxPtl(Rcx_Ptl* pmf);

NCBI_THREADER_EXPORT 
Gib_Scd*  NewGibScd(int NumTempSteps);
NCBI_THREADER_EXPORT 
Gib_Scd*  FreeGibScd(Gib_Scd* gsp);

NCBI_THREADER_EXPORT 
Fld_Mtf*  NewFldMtf(int NumResidues, int NumResResContacts, int NumResPepContacts);
NCBI_THREADER_EXPORT 
Fld_Mtf*  FreeFldMtf(Fld_Mtf* mtf);
NCBI_THREADER_EXPORT 
void      PrintFldMtf(Fld_Mtf* mtf, FILE* pFile);

NCBI_THREADER_EXPORT 
Thd_Tbl*  NewThdTbl(int NumResults, int NumCoreElements);
NCBI_THREADER_EXPORT 
Thd_Tbl*  FreeThdTbl(Thd_Tbl* ttb);
void      OrderThdTbl(Thd_Tbl* pResults);
int       CopyResult(Thd_Tbl* pFromResults, Thd_Tbl* pToResults, int from, int to);
NCBI_THREADER_EXPORT 
void      PrintThdTbl(Thd_Tbl* ttb, FILE* pFile);
NCBI_THREADER_EXPORT 
void      ScaleThdTbl(Thd_Tbl* ttb, double ScalingFactor);

NCBI_THREADER_EXPORT 
int       ThrdRound(double Num);

#ifdef __cplusplus
}
#endif

#endif
