/* nucprot.h
 *
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
 * File Name:  nucprot.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */

#ifndef _NUCPROT_
#define _NUCPROT_

#define Seq_descr_GIBB_mod_dna          0
#define Seq_descr_GIBB_mod_rna          1
#define Seq_descr_GIBB_mod_extrachr     2
#define Seq_descr_GIBB_mod_plasmid      3
#define Seq_descr_GIBB_mod_mito         4
#define Seq_descr_GIBB_mod_chlo         5
#define Seq_descr_GIBB_mod_kinet        6
#define Seq_descr_GIBB_mod_cyane        7
#define Seq_descr_GIBB_mod_synth        8
#define Seq_descr_GIBB_mod_recomb       9
#define Seq_descr_GIBB_mod_partial      10
#define Seq_descr_GIBB_mod_complete     11
#define Seq_descr_GIBB_mod_mutagen      12
#define Seq_descr_GIBB_mod_natmut       13
#define Seq_descr_GIBB_mod_transposon   14
#define Seq_descr_GIBB_mod_insertion    15
#define Seq_descr_GIBB_mod_noleft       16
#define Seq_descr_GIBB_mod_noright      17
#define Seq_descr_GIBB_mod_macronuclear 18
#define Seq_descr_GIBB_mod_proviral     19
#define Seq_descr_GIBB_mod_est          20
#define Seq_descr_GIBB_mod_sts          21
#define Seq_descr_GIBB_mod_gss          22

struct GeneRefFeats
{
    bool valid;
    TSeqFeatList::iterator first;
    TSeqFeatList::iterator last;

    GeneRefFeats() :
        valid(false)
    {}
};

void CheckDupDates(TEntryList& seq_entries);
void ProcNucProt(ParserPtr pp, TEntryList& seq_entries, GeneRefFeats& gene_refs);
void ExtractDescrs(TSeqdescList& descrs_from, TSeqdescList& descrs_to, ncbi::objects::CSeqdesc::E_Choice choice);

#endif
