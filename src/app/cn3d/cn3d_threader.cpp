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
* Revision 1.4  2001/03/23 23:31:56  thiessen
* keep atom info around even if coords not all present; mainly for disulfide parsing in virtual models
*
* Revision 1.3  2001/03/22 00:33:16  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
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
#include "cn3d/molecule.hpp"
#include "cn3d/structure_set.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

// define to include debugging output (threader structures to files)
#define DEBUG_THREADER

// always do debugging in Debug build mode
#if (!defined(DEBUG_THREADER) && defined(_DEBUG))
#define DEBUG_THREADER
#endif


static const double SCALING_FACTOR = 100000;

static const std::string ThreaderResidues = "ARNDCQEGHILKMFPSTWYV";

// gives threader residue number for a character (-1 if non-standard aa)
static int LookupResidueNumber(char r)
{
    typedef std::map < char, int > Char2Int;
    static Char2Int charMap;

    if (charMap.size() == 0) {
        for (int i=0; i<ThreaderResidues.size(); i++)
            charMap[ThreaderResidues[i]] = i;
    }

    Char2Int::const_iterator c = charMap.find(toupper(r));
    return ((c != charMap.end()) ? c->second : -1);
}


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

Seq_Mtf * Threader::CreateSeqMtf(const BlockMultipleAlignment *multiple, double weightPSSM)
{
    // convert all sequences to Bioseqs
    CreateBioseqs(multiple);

    // create SeqAlign from this BlockMultipleAlignment
    SeqAlignPtr seqAlign = CreateSeqAlign(multiple);

#ifdef DEBUG_THREADER
    // dump for debugging
    SeqAlignPtr saChain = seqAlign;
    AsnIoPtr aip = AsnIoOpen("Seq-align.debug.txt", "w");
    while (saChain) {
        SeqAlignAsnWrite(saChain, aip, NULL);
        saChain = saChain->next;
    }
    aip = AsnIoClose(aip);
#endif

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

void Threader::CreateBioseqs(const BlockMultipleAlignment *multiple)
{
    for (int row=0; row<multiple->NRows(); row++)
        CreateBioseq(multiple->GetSequenceOfRow(row));
}

Cor_Def * Threader::CreateCorDef(const BlockMultipleAlignment *multiple, double loopLengthMultiplier)
{
    static const int MIN_LOOP_MAX = 2;
    static const int EXTENSION_MAX = 10;

    BlockMultipleAlignment::UngappedAlignedBlockList *alignedBlocks = multiple->GetUngappedAlignedBlocks();
    Cor_Def *corDef = NewCorDef(alignedBlocks->size());

    // zero loop constraints for tails
    corDef->lll.llmn[0] = corDef->lll.llmn[alignedBlocks->size()] =
    corDef->lll.llmx[0] = corDef->lll.llmx[alignedBlocks->size()] =
    corDef->lll.lrfs[0] = corDef->lll.lrfs[alignedBlocks->size()] = 0;

    // loop constraints for unligned regions between aligned blocks
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator
        b = alignedBlocks->begin(), be = alignedBlocks->end();
    int n, max;
    for (n=1, b++; b!=be; n++, b++) {
        const UnalignedBlock *uaBlock = multiple->GetUnalignedBlockBefore(*b);
        if (uaBlock) {
            max = (int) (loopLengthMultiplier * uaBlock->width);
            if (max < MIN_LOOP_MAX) max = MIN_LOOP_MAX;
        } else
            max = MIN_LOOP_MAX;
        corDef->lll.llmn[n] = 0;
        corDef->lll.llmx[n] = max;
        corDef->lll.lrfs[n] = max;
    }

    // minimum block sizes (in coordinates of master)
    const Block::Range *range;
    int mid;
    for (n=0, b=alignedBlocks->begin(); b!=be; b++, n++) {
        range = (*b)->GetRangeOfRow(0);
        mid = (range->from + range->to) / 2;
        corDef->sll.rfpt[n] = mid;
        corDef->sll.nomn[n] = mid - range->from;
        corDef->sll.comn[n] = range->to - mid;
    }

    // left extension - trim to available residues
    corDef->sll.nomx[0] = corDef->sll.nomn[0] + EXTENSION_MAX;
    if (corDef->sll.rfpt[0] - corDef->sll.nomx[0] < 0)
        corDef->sll.nomx[0] = corDef->sll.rfpt[0];

    // right extension - trim to available residues
    corDef->sll.comx[alignedBlocks->size() - 1] = corDef->sll.comn[alignedBlocks->size() - 1] + EXTENSION_MAX;
    if (corDef->sll.rfpt[alignedBlocks->size() - 1] + corDef->sll.comx[alignedBlocks->size() - 1] >=
            multiple->GetMaster()->sequenceString.size())
        corDef->sll.comx[alignedBlocks->size() - 1] =
            multiple->GetMaster()->sequenceString.size() - corDef->sll.rfpt[alignedBlocks->size() - 1] - 1;

    // extensions into unaligned areas between blocks
    const Block::Range *prevRange;
    int nUnaligned, extN;
    for (n=0, b=alignedBlocks->begin(); b!=be; b++, n++) {
        range = (*b)->GetRangeOfRow(0);
        if (n > 0) {
            nUnaligned = range->from - prevRange->to - 1;
            extN = nUnaligned / 2;  // N extension of right block gets smaller portion if nUnaligned is odd
            corDef->sll.nomx[n] = corDef->sll.nomn[n] + extN;
            corDef->sll.comx[n - 1] = corDef->sll.comn[n - 1] + nUnaligned - extN;
        }
        prevRange = range;
    }

    // no fixed segments
    corDef->fll.n = 0;

    delete alignedBlocks;
    return corDef;
}

Qry_Seq * Threader::CreateQrySeq(const BlockMultipleAlignment *multiple,
        const BlockMultipleAlignment *pairwise)
{
    const Sequence *slaveSeq = pairwise->GetSequenceOfRow(1);
    BlockMultipleAlignment::UngappedAlignedBlockList *multipleABlocks = multiple->GetUngappedAlignedBlocks();
    BlockMultipleAlignment::UngappedAlignedBlockList *pairwiseABlocks = pairwise->GetUngappedAlignedBlocks();

    // query has # constraints = # blocks in multiple alignment
    Qry_Seq *qrySeq = NewQrySeq(slaveSeq->sequenceString.size(), multipleABlocks->size());

    // fill in residue numbers
    int i;
    for (i=0; i<slaveSeq->sequenceString.size(); i++)
        qrySeq->sq[i] = LookupResidueNumber(slaveSeq->sequenceString[i]);

    // if a block in the multiple is contained in the pairwise (looking at master coords),
    // then add a constraint to keep it there
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator
        m, me = multipleABlocks->end(), p, pe = pairwiseABlocks->end();
    const Block::Range *multipleRange, *pairwiseRange;
    for (i=0, m=multipleABlocks->begin(); m!=me; i++, m++) {
        multipleRange = (*m)->GetRangeOfRow(0);
        for (p=pairwiseABlocks->begin(); p!=pe; p++) {
            pairwiseRange = (*p)->GetRangeOfRow(0);
            if (pairwiseRange->from <= multipleRange->from && pairwiseRange->to >= multipleRange->to) {
                int masterCenter = (multipleRange->from + multipleRange->to) / 2;
                // offset of master residue at center of multiple block
                int offset = masterCenter - pairwiseRange->from;
                pairwiseRange = (*p)->GetRangeOfRow(1);
                // slave residue in pairwise aligned to master residue at center of multiple block
                qrySeq->sac.mn[i] = qrySeq->sac.mx[i] = pairwiseRange->from + offset;
                break;
            }
        }
    }

    delete multipleABlocks;
    delete pairwiseABlocks;
    return qrySeq;
}

/*----------------------------------------------------------------------------
 *  stuff to read in the contact potential. (code swiped from DDV)
 *---------------------------------------------------------------------------*/
static Int4 CountWords(char* Str) {
    Int4     i, Count=0;
    Boolean  InsideStr;
    InsideStr = FALSE;
    for (i=0; i<StrLen(Str); i++) {
        if (!InsideStr && (Str[i] != ' ')) {
            Count++;
            InsideStr = TRUE;
        }
        if (Str[i] == ' ') {
            InsideStr = FALSE;
        }
    }
    return(Count);
}
static void ReadToRowOfEnergies(ifstream& InFile, int NumResTypes) {
    char  Str[1024];
    while (!InFile.eof()) {
        InFile.getline(Str, sizeof(Str));
            if (CountWords(Str) == NumResTypes) {
            break;
        }
    }
}

Rcx_Ptl * Threader::CreateRcxPtl(double weightContacts)
{
    Rcx_Ptl*  pmf;
    char      FileName[256] = "ContactPotential";
    char      ResName[32];
    char      Path[512];
    Int4      i, j, k;
    double    temp;

    static const int kNumResTypes = 21;
    static const int kNumDistances = 6;
    static const int kPeptideIndex = 20;

    /* open the contact potential for reading */
//    FindPath("ncbi", "ncbi", "data", Path, sizeof(Path));
//    StrCat(Path, FileName);
    StrCpy(Path, FileName);
    auto_ptr<CNcbiIfstream> InFile(new CNcbiIfstream(Path));
    if (!(*InFile)) {
        ERR_POST(Error << "Threader::CreateRcxPtl() - can't open " << FileName << " for reading");
        return NULL;
    }

    pmf = NewRcxPtl(kNumResTypes, kNumDistances, kPeptideIndex);

    /* read in the contact potential */
    for (i=0; i<kNumDistances; i++) {
        ReadToRowOfEnergies(*InFile, kNumResTypes);
        if (InFile->eof()) goto error;
        for (j=0; j<kNumResTypes; j++) {
            InFile->getline(ResName, sizeof(ResName), ' ');  /* skip residue name */
            if (InFile->eof()) goto error;
            for (k=0; k<kNumResTypes; k++) {
                *InFile >> temp;
                if (InFile->eof()) goto error;
                pmf->rre[i][j][k] = ThrdRound(temp*SCALING_FACTOR*weightContacts);
            }
        }
    }

    /* read in the hydrophobic energies */
    ReadToRowOfEnergies(*InFile, kNumDistances);
    for (i=0; i<kNumResTypes; i++) {
        InFile->getline(ResName, sizeof(ResName), ' ');  /* skip residue name */
        if (InFile->eof()) goto error;
        for (j=0; j<kNumDistances; j++) {
            *InFile >> temp;
            if (InFile->eof()) goto error;
            pmf->re[j][i] = ThrdRound(temp*SCALING_FACTOR*weightContacts);
        }
    }

    /* calculate sum of pair energies plus hydrophobic energies */
    for(i=0; i<kNumDistances; i++) {
        for(j=0; j<kNumResTypes; j++) {
            for(k=0; k<kNumResTypes; k++) {
                pmf->rrt[i][j][k] = pmf->rre[i][j][k] + pmf->re[i][j] + pmf->re[i][k];
            }
        }
    }

    return(pmf);

error:
    ERR_POST(Error << "Threader::CreateRcxPtl() - error parsing " << FileName);
    FreeRcxPtl(pmf);
    return NULL;
}

/*----------------------------------------------------------------------------
 *  set up the annealing parameters. (more code swiped from DDV)
 *  hard-coded for now.  later we can move these parameters to a file.
 *---------------------------------------------------------------------------*/
Gib_Scd * Threader::CreateGibScd(bool fast, int nRandomStarts)
{
    Gib_Scd*  gsp;
    int       NumTrajectoryPoints;

    static const int kNumTempSteps = 3;

    gsp = NewGibScd(kNumTempSteps);

    gsp->nrs = nRandomStarts;     /* Number of random starts */
    gsp->nts = kNumTempSteps;     /* Number of temperature steps */
    gsp->crs = 50;                /* Number of starts before convergence test */
    gsp->cfm = 20;                /* Top thread frequency convergence criterion */
    gsp->csm = 5;                 /* Top thread start convergence criterion */
    gsp->cet = 5 * SCALING_FACTOR;/* Temperature for convergence test ensemble */
    gsp->cef = 50;                /* Percent of ensemble defining top threads */
    gsp->isl = 1;                 /* Code for choice of starting locations */
    gsp->iso = 0;                 /* Code for choice of segment sample order */
    gsp->ito = 0;                 /* Code for choice of terminus sample order */
    gsp->rsd = -1;                /* Seed for random number generator -- neg for Rand01() */
    gsp->als = 0;                 /* Code for choice of alignment record style */
    gsp->trg = 0;                 /* Code for choice of trajectory record */

    if (fast) {
    //    gsp->nti[0] = 5;            /* Number of iterations per tempeature step */
    //    gsp->nti[1] = 10;
    //    gsp->nti[2] = 25;
        gsp->nti[0] = 10;           /* Number of iterations per tempeature step */
        gsp->nti[1] = 20;
        gsp->nti[2] = 40;
    } else {
        gsp->nti[0] = 10;           /* Number of iterations per tempeature step */
        gsp->nti[1] = 20;
        gsp->nti[2] = 40;
    }

    gsp->nac[0] = 4;              /* Number of alignment cycles per iteration */
    gsp->nac[1] = 4;
    gsp->nac[2] = 4;

    gsp->nlc[0] = 2;              /* Number of location cycles per iteration */
    gsp->nlc[1] = 2;
    gsp->nlc[2] = 2;

    if (fast) {
    //    gsp->tma[0] = 5 * SCALING_FACTOR;  /* Temperature steps for alignment sampling */
    //    gsp->tma[1] = 5 * SCALING_FACTOR;
    //    gsp->tma[2] = 5 * SCALING_FACTOR;
        gsp->tma[0] = 20 * SCALING_FACTOR;  /* Temperature steps for alignment sampling */
        gsp->tma[1] = 10 * SCALING_FACTOR;
        gsp->tma[2] =  5 * SCALING_FACTOR;
    } else {
        gsp->tma[0] = 20 * SCALING_FACTOR;  /* Temperature steps for alignment sampling */
        gsp->tma[1] = 10 * SCALING_FACTOR;
        gsp->tma[2] =  5 * SCALING_FACTOR;
    }

    gsp->tml[0] = 5 * SCALING_FACTOR;     /* Temperature steps for location sampling */
    gsp->tml[1] = 5 * SCALING_FACTOR;
    gsp->tml[2] = 5 * SCALING_FACTOR;

    gsp->lms[0] = 0;              /* Iterations before local minimum test */
    gsp->lms[1] = 10;
    gsp->lms[2] = 20;

    gsp->lmw[0] = 0;              /* Iterations in local min test interval */
    gsp->lmw[1] = 10;
    gsp->lmw[2] = 10;

    gsp->lmf[0] = 0;              /* Percent of top score indicating local min */
    gsp->lmf[1] = 80;
    gsp->lmf[2] = 95;

    if (gsp->als == 0) {
        NumTrajectoryPoints = 1;
    } else {
        NumTrajectoryPoints = 0;
        for (int i=0; i<kNumTempSteps; i++) {
            NumTrajectoryPoints += gsp->nti[i] * (gsp->nac[i] + gsp->nlc[i]);
        }
        NumTrajectoryPoints *= gsp->nrs;
        NumTrajectoryPoints += gsp->nrs;
    }
    gsp->ntp = NumTrajectoryPoints;

    return(gsp);
}

Fld_Mtf * Threader::CreateFldMtf(const Sequence *masterSequence)
{
    if (!masterSequence) return NULL;
    if (!masterSequence->molecule || masterSequence->molecule->parentSet->isAlphaOnly)
        return NewFldMtf(masterSequence->sequenceString.size(), 0, 0);

    const Molecule *mol = masterSequence->molecule;
    Fld_Mtf *fldMtf = NewFldMtf(mol->residues.size(), 0, 0);

    return fldMtf;
}

bool Threader::Realign(const BlockMultipleAlignment *masterMultiple,
    const AlignmentList *originalAlignments, AlignmentList *newAlignments)
{
    // will eventually be variables...
    static const double weightPSSM = 1.0;
    static const double loopLengthMultiplier = 1.5;
    static const int nRandomStarts = 1;

    // either calculate no z-scores (0), or calculate z-score for best result (1)
    static const int zscs = 1;

    // number of alignments returned per threading call
    static const int nResults = 10;

    Seq_Mtf *seqMtf = NULL;
    Cor_Def *corDef = NULL;
    Rcx_Ptl *rcxPtl = NULL;
    Gib_Scd *gibScd = NULL;
    Fld_Mtf *fldMtf = NULL;
    float *trajectory = NULL;
    bool retval = false;

    AlignmentList::const_iterator p, pe = originalAlignments->end();

#ifdef DEBUG_THREADER
    FILE *pFile;
#endif

    // create potential and Gibbs schedule
    if (!(rcxPtl = CreateRcxPtl(1.0 - weightPSSM))) goto cleanup;
    if (!(gibScd = CreateGibScd(true, nRandomStarts))) goto cleanup;
    trajectory = new float[gibScd->ntp];

    // create PSSM
    if (!(seqMtf = CreateSeqMtf(masterMultiple, weightPSSM))) goto cleanup;
#ifdef DEBUG_THREADER
    pFile = fopen("Seq_Mtf.debug.txt", "w");
    PrintSeqMtf(seqMtf, pFile);
    fclose(pFile);
#endif

    // create core definition
    if (!(corDef = CreateCorDef(masterMultiple, loopLengthMultiplier))) goto cleanup;
#ifdef DEBUG_THREADER
    pFile = fopen("Cor_Def.debug.txt", "w");
    PrintCorDef(corDef, pFile);
    fclose(pFile);
#endif

    // create contact lists
    if (weightPSSM < 1.0 && (!masterMultiple->GetMaster()->molecule ||
            masterMultiple->GetMaster()->molecule->parentSet->isAlphaOnly)) {
        ERR_POST("Can't use contact potential on non-structured master, or alpha-only (virtual bond) models!");
        goto cleanup;
    }
    if (!(fldMtf = CreateFldMtf(masterMultiple->GetMaster()))) goto cleanup;

#ifdef DEBUG_THREADER
    pFile = fopen("Fld_Mtf.debug.txt", "w");
    PrintFldMtf(fldMtf, pFile);
    fclose(pFile);
#endif

    for (p=originalAlignments->begin(); p!=pe; p++) {

        if ((*p)->NRows() != 2 || (*p)->GetMaster() != masterMultiple->GetMaster()) {
            ERR_POST(Error << "Threader::Realign() - bad pairwise alignment");
            continue;
        }

        Qry_Seq *qrySeq = NULL;
        Thd_Tbl *thdTbl = NULL;
        int success;

        // create query sequence
        if (!(qrySeq = CreateQrySeq(masterMultiple, *p))) goto cleanup2;
#ifdef DEBUG_THREADER
        pFile = fopen("Qry_Seq.debug.txt", "w");
        PrintQrySeq(qrySeq, pFile);
        fclose(pFile);
#endif

        // create results storage structure
        thdTbl = NewThdTbl(nResults, corDef->sll.n);

        // actually run the threader (finally!)
        success = atd(fldMtf, corDef, qrySeq, rcxPtl, gibScd, thdTbl, seqMtf,
            trajectory, zscs, SCALING_FACTOR, weightPSSM);

        if (success) {
#ifdef DEBUG_THREADER
            pFile = fopen("Thd_Tbl.debug.txt", "w");
            PrintThdTbl(thdTbl, pFile);
            fclose(pFile);
#endif
        }

cleanup2:
        if (qrySeq) FreeQrySeq(qrySeq);
        if (thdTbl) FreeThdTbl(thdTbl);
    }

    retval = true;

cleanup:
    if (seqMtf) FreeSeqMtf(seqMtf);
    if (corDef) FreeCorDef(corDef);
    if (fldMtf) FreeFldMtf(fldMtf);
    if (rcxPtl) FreeRcxPtl(rcxPtl);
    if (gibScd) FreeGibScd(gibScd);
    if (trajectory) delete trajectory;

    return retval;
}

END_SCOPE(Cn3D)
