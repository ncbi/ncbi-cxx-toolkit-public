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
* Authors:  Paul Thiessen
*
* File Description:
*       class to isolate and run the threader
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/02/13 01:03:56  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.1  2001/02/08 23:01:49  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* ===========================================================================
*/

// C-toolkit stuff
#include <corelib/ncbistd.hpp> // must come first to avoid NCBI type clashes
#include <ctools/ctools.h>
#include <objseq.h>
#include <objalign.h>
#include <thrdatd.h>
#include <thrddecl.h>
#include <cddutil.h>
#include <ncbistr.h>

#include "cn3d/cn3d_threader.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/sequence_set.hpp"


USING_NCBI_SCOPE;
BEGIN_SCOPE(Cn3D)

static const double SCALING_FACTOR = 100000;

// local function prototypes
static void AddSeqId(const Sequence *sequence, SeqIdPtr *id, bool addAllTypes);
static SeqAlignPtr CreateSeqAlign(const BlockMultipleAlignment *multiple);


Threader::Threader(void)
{
    SetupCToolkitErrPost(); // reroute C-toolkit err messages to C++ err streams
}

Threader::~Threader(void)
{
    BioseqMap::iterator i, ie = bioseqs.end();
    for (i=bioseqs.begin(); i!=ie; i++) BioseqFree(i->second);
}

static void AddSeqId(const Sequence *sequence, SeqIdPtr *id, bool addAllTypes)
{
    if (sequence->pdbID.size() > 0) {
        PDBSeqIdPtr pdbid = PDBSeqIdNew();
        pdbid->mol = StrSave(sequence->pdbID.c_str());
        pdbid->chain = (Uint1) sequence->pdbChain;
        ValNodeAddPointer(id, SEQID_PDB, pdbid);
        if (!addAllTypes) return;
    }
    if (sequence->gi != Sequence::VALUE_NOT_SET) {
        ValNodeAddInt(id, SEQID_GI, sequence->gi);
        if (!addAllTypes) return;
    }
    if (sequence->accession.size() > 0) {
        TextSeqIdPtr gbid = TextSeqIdNew();
        gbid->accession = StrSave(sequence->accession.c_str());
        ValNodeAddPointer(id, SEQID_GENBANK, gbid);
        if (!addAllTypes) return;
    }
}

// creates a SeqAlign from a BlockMultipleAlignment
static SeqAlignPtr CreateSeqAlign(const BlockMultipleAlignment *multiple)
{
    // one SeqAlign (chained into a linked list) for each slave row
    SeqAlignPtr prevSap = NULL, firstSap = NULL;
    for (int row=1; row<multiple->NRows(); row++) {

        SeqAlignPtr sap = SeqAlignNew();
        if (prevSap) prevSap->next = sap;
        prevSap = sap;
		if (!firstSap) firstSap = sap;

        sap->type = SAT_PARTIAL;
        sap->dim = 2;
        sap->segtype = SAS_DENDIAG;

        DenseDiagPtr prevDd = NULL;
        auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList>
            blocks(multiple->GetUngappedAlignedBlocks());
        BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.get()->end();

        for (b=blocks.get()->begin(); b!=be; b++) {
            DenseDiagPtr dd = DenseDiagNew();
            if (prevDd) prevDd->next = dd;
            prevDd = dd;
            if (b == blocks.get()->begin()) sap->segs = dd;

            dd->dim = 2;
            AddSeqId(multiple->GetSequenceOfRow(0), &(dd->id), false);      // master
            AddSeqId(multiple->GetSequenceOfRow(row), &(dd->id), false);    // slave
            dd->len = (*b)->width;

            dd->starts = (Int4Ptr) MemNew(2 * sizeof(Int4));
            const Block::Range *range = (*b)->GetRangeOfRow(0);
            dd->starts[0] = range->from;
            range = (*b)->GetRangeOfRow(row);
            dd->starts[1] = range->from;
        }
    }

	return firstSap;
}

void Threader::Test(const BlockMultipleAlignment* multiple)
{
    const double weightPSSM = 1.0;

    Seq_Mtf*        seqMtf = CreateSeqMtf(multiple, weightPSSM);
    FILE *pFile = fopen("Seq_Mtf.txt", "w");
    PrintSeqMtf(seqMtf, pFile);
    fclose(pFile);

/*
    Fld_Mtf*        fldMtf = NewFldMtf(0, 0, 0);
    Cor_Def*        corDef = NewCorDef(0);
    Qry_Seq*        qrySeq = NewQrySeq(0, 0);
    Rcx_Ptl*        rcxPtl = NewRcxPtl(0, 0, 0);
    Gib_Scd*        gibScd = NewGibScd(0);
    Thd_Tbl*        thdTbl = NewThdTbl(0, 0);
    float*          trajectory = new float[1];

    // either calculate no z-scores (0), or calculate z-score for best result (1)
    const int zscs = 1;

    int RetVal =
        atd(fldMtf, corDef, qrySeq, rcxPtl, gibScd, thdTbl, seqMtf, trajectory, zscs, SCALING_FACTOR);

    FreeFldMtf(fldMtf);
    FreeCorDef(corDef);
    FreeQrySeq(qrySeq);
    FreeRcxPtl(rcxPtl);
    FreeGibScd(gibScd);
    FreeThdTbl(thdTbl);
    delete trajectory;
*/
    FreeSeqMtf(seqMtf);
}

Seq_Mtf * Threader::CreateSeqMtf(const BlockMultipleAlignment* multiple, double weightPSSM)
{
    // convert all sequences to Bioseqs
    CreateBioseqs(multiple);

    // create SeqAlign from this BlockMultipleAlignment
    SeqAlignPtr seqAlign = CreateSeqAlign(multiple);

    // dump for debugging
    SeqAlignPtr saChain = seqAlign;
    AsnIoPtr aip = AsnIoOpen("Seq-align.txt", "w");
    while (saChain) {
        SeqAlignAsnWrite(saChain, aip, NULL);
        saChain = saChain->next;
    }
    aip = AsnIoClose(aip);

    Seq_Mtf *seqMtf = CddDenDiagCposComp2(
        GetBioseq(multiple->GetMaster()),
        7,
        seqAlign,
        NULL,
        NULL,
        weightPSSM,
        SCALING_FACTOR,
        NULL
    );

    SeqAlignFree(seqAlign);
	return seqMtf;
}

void Threader::CreateBioseq(const Sequence *sequence)
{
    if (!sequence || !sequence->isProtein) {
        ERR_POST(Error << "Threader::CreateBioseq() - got non-protein or NULL sequence");
        return;
    }

    // skip if already done
    if (bioseqs.find(sequence) != bioseqs.end()) return;

    // create new Bioseq and fill it in from Sequence data
    BioseqPtr bioseq = BioseqNew();

    bioseq->mol = Seq_mol_aa;
    bioseq->seq_data_type = Seq_code_ncbieaa;
    bioseq->repr = Seq_repr_raw;

    bioseq->length = sequence->sequenceString.size();
    bioseq->seq_data = BSNew(bioseq->length);
    BSWrite(bioseq->seq_data, const_cast<char*>(sequence->sequenceString.c_str()), bioseq->length);

    // create Seq-id
    AddSeqId(sequence, &(bioseq->id), true);

    // store Bioseq
    bioseqs[sequence] = bioseq;
}

void Threader::CreateBioseqs(const BlockMultipleAlignment* multiple)
{
    for (int row=0; row<multiple->NRows(); row++)
        CreateBioseq(multiple->GetSequenceOfRow(row));
}

END_SCOPE(Cn3D)
