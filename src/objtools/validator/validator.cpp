/* $Id:
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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko.......
 *
 * File Description:
 *   Validates CSeq_entries and CSeq_submits
 *
 */
#include <corelib/ncbistd.hpp>
#include <objects/validator/validator.hpp>
#include "validatorp.hpp"

#include <serial/serialbase.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


// *********************** CValidError implementation **********************


CValidError::CValidError
(CObjectManager&   objmgr,
 const CSeq_entry& se,
 unsigned int      options)
{
    CValidError_imp imp(objmgr, m_ErrItems, options);
    imp.Validate(se);
}


CValidError::CValidError
(CObjectManager&    objmgr,
 const CSeq_submit& ss,
 unsigned int       options)
{

    CValidError_imp imp(objmgr, m_ErrItems, options);
    imp.Validate(ss);
}


CValidError::~CValidError()
{
}


// *********************** CValidErrItem implementation ********************


CValidErrItem::CValidErrItem
(EDiagSev sev,
 unsigned int         ei,
 const string&        msg,
 const CSerialObject& obj)
  : m_Severity (sev),
    m_ErrIndex (ei),
    m_Message (msg),
    m_Object (&obj, obj.GetThisTypeInfo())
{
}

CValidErrItem::~CValidErrItem(void)
{
}


EDiagSev CValidErrItem::GetSeverity(void) const
{
    return m_Severity;
}


const string& CValidErrItem::GetErrCode(void) const
{
    if (m_ErrIndex <= eErr_UNKNOWN) {
        return sm_Terse [m_ErrIndex];
    }
    return sm_Terse [eErr_UNKNOWN];
}


const string& CValidErrItem::GetMsg(void) const
{
    return m_Message;
}


const string& CValidErrItem::GetVerbose(void) const
{
    if (m_ErrIndex <= eErr_UNKNOWN) {
        return sm_Verbose [m_ErrIndex];
    }
    return sm_Verbose [eErr_UNKNOWN];
}


const CConstObjectInfo& CValidErrItem::GetObject(void) const
{
    return m_Object;
}


// ************************ CValidError_CI implementation **************


CValidError_CI::CValidError_CI(void) :
    m_Validator(0),
    m_ErrCodeFilter(eErr_ALL), // eErr_UNKNOWN
    m_MinSeverity(eDiag_Info),
    m_MaxSeverity(eDiag_Critical)
{
}


CValidError_CI::CValidError_CI
(const CValidError& ve,
 string             errcode,
 EDiagSev           minsev,
 EDiagSev           maxsev) :
    m_Validator(&ve),
    m_ErrIter(ve.m_ErrItems.begin()),
    m_MinSeverity(minsev),
    m_MaxSeverity(maxsev)
{
    if (errcode.empty()) {
        m_ErrCodeFilter = eErr_ALL;
    }

    for (unsigned int i = eErr_ALL; i < eErr_UNKNOWN; i++) {
        if (errcode == CValidErrItem::sm_Terse[i]) {
            m_ErrCodeFilter = i;
            return;
        }
    }
    m_ErrCodeFilter = eErr_ALL; // eErr_UNKNOWN
}


CValidError_CI::CValidError_CI(const CValidError_CI& iter) :
    m_Validator(iter.m_Validator),
    m_ErrIter(iter.m_ErrIter),
    m_ErrCodeFilter(iter.m_ErrCodeFilter),
    m_MinSeverity(iter.m_MinSeverity),
    m_MaxSeverity(iter.m_MaxSeverity)
{
}


CValidError_CI::~CValidError_CI(void)
{
}


CValidError_CI& CValidError_CI::operator=(const CValidError_CI& iter)
{
    if (this == &iter) {
        return *this;
    }

    m_Validator = iter.m_Validator;
    m_ErrIter = iter.m_ErrIter;
    m_ErrCodeFilter = iter.m_ErrCodeFilter;
    m_MinSeverity = iter.m_MinSeverity;
    m_MaxSeverity = iter.m_MaxSeverity;
    return *this;
}


CValidError_CI& CValidError_CI::operator++(void)
{
    if (m_ErrIter != m_Validator->m_ErrItems.end()) {
        ++m_ErrIter;
    }
    
    for (; m_ErrIter != m_Validator->m_ErrItems.end(); ++m_ErrIter) {
        if ((m_ErrCodeFilter == eErr_ALL  ||
            (**m_ErrIter).GetErrCode() == CValidErrItem::sm_Terse[m_ErrCodeFilter])  &&
            (**m_ErrIter).GetSeverity() <= m_MaxSeverity  &&
            (**m_ErrIter).GetSeverity() >= m_MinSeverity) {
            break;
        }
    }
    return *this;
}


CValidError_CI::operator bool (void) const
{
    return m_ErrIter != m_Validator->m_ErrItems.end();
}


const CValidErrItem& CValidError_CI::operator*(void) const
{
    return **m_ErrIter;
}


const CValidErrItem* CValidError_CI::operator->(void) const
{
    return &(**m_ErrIter);
}


// External terse error type explanation
const string CValidErrItem::sm_Terse [] = {
    "UNKNOWN",

    "SEQ_INST_ExtNotAllowed",
    "SEQ_INST_ExtBadOrMissing",
    "SEQ_INST_SeqDataNotFound",
    "SEQ_INST_SeqDataNotAllowed",
    "SEQ_INST_ReprInvalid",
    "SEQ_INST_CircularProtein",
    "SEQ_INST_DSProtein",
    "SEQ_INST_MolNotSet",
    "SEQ_INST_MolOther",
    "SEQ_INST_FuzzyLen",
    "SEQ_INST_InvalidLen",
    "SEQ_INST_InvalidAlphabet",
    "SEQ_INST_SeqDataLenWrong",
    "SEQ_INST_SeqPortFail",
    "SEQ_INST_InvalidResidue",
    "SEQ_INST_StopInProtein",
    "SEQ_INST_PartialInconsistent",
    "SEQ_INST_ShortSeq",
    "SEQ_INST_NoIdOnBioseq",
    "SEQ_INST_BadDeltaSeq",
    "SEQ_INST_LongHtgsSequence",
    "SEQ_INST_LongLiteralSequence",
    "SEQ_INST_SequenceExceeds350kbp",
    "SEQ_INST_ConflictingIdsOnBioseq",
    "SEQ_INST_MolNuclAcid",
    "SEQ_INST_ConflictingBiomolTech",
    "SEQ_INST_SeqIdNameHasSpace",
    "SEQ_INST_IdOnMultipleBioseqs",
    "SEQ_INST_DuplicateSegmentReferences",
    "SEQ_INST_TrailingX",
    "SEQ_INST_BadSeqIdFormat",
    "SEQ_INST_PartsOutOfOrder",
    "SEQ_INST_BadSecondaryAccn",
    "SEQ_INST_ZeroGiNumber",
    "SEQ_INST_RnaDnaConflict",
    "SEQ_INST_HistoryGiCollision",
    "SEQ_INST_GiWithoutAccession",
    "SEQ_INST_MultipleAccessions",
    "SEQ_INST_HistAssemblyMissing",

    "SEQ_DESCR_BioSourceMissing",
    "SEQ_DESCR_InvalidForType",
    "SEQ_DESCR_FileOpenCollision",
    "SEQ_DESCR_Unknown",
    "SEQ_DESCR_NoPubFound",
    "SEQ_DESCR_NoOrgFound",
    "SEQ_DESCR_MultipleBioSources",
    "SEQ_DESCR_NoMolInfoFound",
    "SEQ_DESCR_BadCountryCode",
    "SEQ_DESCR_NoTaxonID",
    "SEQ_DESCR_InconsistentBioSources",
    "SEQ_DESCR_MissingLineage",
    "SEQ_DESCR_SerialInComment",
    "SEQ_DESCR_BioSourceNeedsFocus",
    "SEQ_DESCR_BadOrganelle",
    "SEQ_DESCR_MultipleChromosomes",
    "SEQ_DESCR_BadSubSource",
    "SEQ_DESCR_BadOrgMod",
    "SEQ_DESCR_InconsistentProteinTitle",
    "SEQ_DESCR_Inconsistent",
    "SEQ_DESCR_ObsoleteSourceLocation",
    "SEQ_DESCR_ObsoleteSourceQual",
    "SEQ_DESCR_StructuredSourceNote",
    "SEQ_DESCR_MultipleTitles",

    "GENERIC_NonAsciiAsn",
    "GENERIC_Spell",
    "GENERIC_AuthorListHasEtAl",
    "GENERIC_MissingPubInfo",
    "GENERIC_UnnecessaryPubEquiv",
    "GENERIC_BadPageNumbering",

    "SEQ_PKG_NoCdRegionPtr",
    "SEQ_PKG_NucProtProblem",
    "SEQ_PKG_SegSetProblem",
    "SEQ_PKG_EmptySet",
    "SEQ_PKG_NucProtNotSegSet",
    "SEQ_PKG_SegSetNotParts",
    "SEQ_PKG_SegSetMixedBioseqs",
    "SEQ_PKG_PartsSetMixedBioseqs",
    "SEQ_PKG_PartsSetHasSets",
    "SEQ_PKG_FeaturePackagingProblem",
    "SEQ_PKG_GenomicProductPackagingProblem",
    "SEQ_PKG_InconsistentMolInfoBiomols",

    "SEQ_FEAT_InvalidForType",
    "SEQ_FEAT_PartialProblem",
    "SEQ_FEAT_InvalidType",
    "SEQ_FEAT_Range",
    "SEQ_FEAT_MixedStrand",
    "SEQ_FEAT_SeqLocOrder",
    "SEQ_FEAT_CdTransFail",
    "SEQ_FEAT_StartCodon",
    "SEQ_FEAT_InternalStop",
    "SEQ_FEAT_NoProtein",
    "SEQ_FEAT_MisMatchAA",
    "SEQ_FEAT_TransLen",
    "SEQ_FEAT_NoStop",
    "SEQ_FEAT_TranslExcept",
    "SEQ_FEAT_NoProtRefFound",
    "SEQ_FEAT_NotSpliceConsensus",
    "SEQ_FEAT_OrfCdsHasProduct",
    "SEQ_FEAT_GeneRefHasNoData",
    "SEQ_FEAT_ExceptInconsistent",
    "SEQ_FEAT_ProtRefHasNoData",
    "SEQ_FEAT_GenCodeMismatch",
    "SEQ_FEAT_RNAtype0",
    "SEQ_FEAT_UnknownImpFeatKey",
    "SEQ_FEAT_UnknownImpFeatQual",
    "SEQ_FEAT_WrongQualOnImpFeat",
    "SEQ_FEAT_MissingQualOnImpFeat",
    "SEQ_FEAT_PsuedoCdsHasProduct",
    "SEQ_FEAT_IllegalDbXref",
    "SEQ_FEAT_FarLocation",
    "SEQ_FEAT_DuplicateFeat",
    "SEQ_FEAT_UnnecessaryGeneXref",
    "SEQ_FEAT_TranslExceptPhase",
    "SEQ_FEAT_TrnaCodonWrong",
    "SEQ_FEAT_BothStrands",
    "SEQ_FEAT_CDSgeneRange",
    "SEQ_FEAT_CDSmRNArange",
    "SEQ_FEAT_OverlappingPeptideFeat",
    "SEQ_FEAT_SerialInComment",
    "SEQ_FEAT_MultipleCDSproducts",
    "SEQ_FEAT_FocusOnBioSourceFeature",
    "SEQ_FEAT_PeptideFeatOutOfFrame",
    "SEQ_FEAT_InvalidQualifierValue",
    "SEQ_FEAT_MultipleMRNAproducts",
    "SEQ_FEAT_mRNAgeneRange",
    "SEQ_FEAT_TranscriptLen",
    "SEQ_FEAT_TranscriptMismatches",
    "SEQ_FEAT_CDSproductPackagingProblem",
    "SEQ_FEAT_DuplicateInterval",
    "SEQ_FEAT_PolyAsiteNotPoint",
    "SEQ_FEAT_ImpFeatBadLoc",
    "SEQ_FEAT_LocOnSegmentedBioseq",
    "SEQ_FEAT_UnnecessaryCitPubEquiv",
    "SEQ_FEAT_ImpCDShasTranslation",
    "SEQ_FEAT_ImpCDSnotPseudo",
    "SEQ_FEAT_MissingMRNAproduct",
    "SEQ_FEAT_AbuttingIntervals",
    "SEQ_FEAT_CollidingGeneNames",
    "SEQ_FEAT_MultiIntervalGene",
    "SEQ_FEAT_FeatContentDup",
    "SEQ_FEAT_BadProductSeqId",
    "SEQ_FEAT_RnaProductMismatch",

    "SEQ_ALIGN_SeqIdProblem",
    "SEQ_ALIGN_StrandRev",
    "SEQ_ALIGN_DensegLenStart",
    "SEQ_ALIGN_StartLessthanZero",
    "SEQ_ALIGN_StartMorethanBiolen",
    "SEQ_ALIGN_EndLessthanZero",
    "SEQ_ALIGN_EndMorethanBiolen",
    "SEQ_ALIGN_LenLessthanZero",
    "SEQ_ALIGN_LenMorethanBiolen",
    "SEQ_ALIGN_SumLenStart",
    "SEQ_ALIGN_AlignDimSeqIdNotMatch",
    "SEQ_ALIGN_SegsDimSeqIdNotMatch",
    "SEQ_ALIGN_FastaLike",
    "SEQ_ALIGN_NullSegs",
    "SEQ_ALIGN_SegmentGap",
    "SEQ_ALIGN_SegsDimOne",
    "SEQ_ALIGN_AlignDimOne",
    "SEQ_ALIGN_Segtype",
    "SEQ_ALIGN_BlastAligns",

    "SEQ_GRAPH_GraphMin",
    "SEQ_GRAPH_GraphMax",
    "SEQ_GRAPH_GraphBelow",
    "SEQ_GRAPH_GraphAbove",
    "SEQ_GRAPH_GraphByteLen",
    "SEQ_GRAPH_GraphOutOfOrder",
    "SEQ_GRAPH_GraphBioseqLen",
    "SEQ_GRAPH_GraphSeqLitLen",
    "SEQ_GRAPH_GraphSeqLocLen",
    "SEQ_GRAPH_GraphStartPhase",
    "SEQ_GRAPH_GraphStopPhase",
    "SEQ_GRAPH_GraphDiffNumber",
    "SEQ_GRAPH_GraphACGTScore",
    "SEQ_GRAPH_GraphNScore",
    "SEQ_GRAPH_GraphGapScore",
    "SEQ_GRAPH_GraphOverlap",

    "UNKONWN"
};


// External verbose error type explanation
const string CValidErrItem::sm_Verbose [] = {
"UNKNOWN",

/* SEQ_INST */

"A Bioseq 'extension' is used for special classes of Bioseq. This class \
of Bioseq should not have one but it does. This is probably a software \
error.",

"This class of Bioseq requires an 'extension' but it is missing or of \
the wrong type. This is probably a software error.",

"No actual sequence data was found on this Bioseq. This is probably a \
software problem.",

"The wrong type of sequence data was found on this Bioseq. This is \
probably a software problem.",

"This Bioseq has an invalid representation class. This is probably a \
software error.",

"This protein Bioseq is represented as circular. Circular topology is \
normally used only for certain DNA molecules, for example, plasmids.",

"This protein Bioseq has strandedness indicated. Strandedness is \
normally a property only of DNA sequences. Please unset the \
strandedness.",

"It is not clear whether this sequence is nucleic acid or protein. \
Please set the appropriate molecule type (Bioseq.mol).",

"Most sequences are either nucleic acid or protein. However, the \
molecule type (Bioseq.mol) is set to 'other'. It should probably be set \
to nucleic acid or a protein.",
"This sequence is marked as having an uncertain length, but the length \
is known exactly.",

"The length indicated for this sequence is invalid. This is probably a \
software error.",

"This Bioseq has an invalid alphabet (e.g. protein codes on a nucleic \
acid or vice versa). This is probably a software error.",

"The length of this Bioseq does not agree with the length of the actual \
data. This is probably a software error.",

"Something is very wrong with this entry. The validator cannot open a \
SeqPort on the Bioseq. Further testing cannot be done.",

"Invalid residue codes were found in this Bioseq.",

"Stop codon symbols were found in this protein Bioseq.",

"This segmented sequence is described as complete or incomplete in \
several places, but these settings are inconsistent.",

"This Bioseq is unusually short (less than 4 amino acids or less than 11 \
nucleic acids). GenBank does not usually accept such short sequences.",

"No SeqIds were found on this Bioseq. This is probably a software \
error.",

"Delta sequences should only be HTGS-1 or HTGS-2.",

"HTGS-1 or HTGS-2 sequences must be < 350 KB in length.",

"Delta literals must be < 350 KB in length.",

"Individual sequences must be < 350 KB in length, unless they represent \
a single gene.",

"Two SeqIds of the same class was found on this Bioseq. This is probably \
a software error.",

"The specific type of this nucleic acid (DNA or RNA) is not set.",

"HTGS/STS/GSS records should be genomic DNA. There is a conflict between \
the technique and expected molecule type.",

"The Seq-id.name field should be a single word without any whitespace. \
This should be fixed by the database staff.",

"There are multiple occurrences of the same Seq-id in this record. \
Sequence identifiers must be unique within a record.",

"The segmented sequence refers multiple times to the same Seq-id. This \
may be due to a software error. Please consult with the database staff \
to fix this record.",

"The protein sequence ends with one or more X (unknown) amino acids.",

"A nucleotide sequence identifier should be 1 letter plus 5 digits or 2 \
letters plus 6 digits, and a protein sequence identifer should be 3 \
letters plus 5 digits.",

"The parts inside a segmented set should correspond to the seq_ext of \
the segmented bioseq. A difference will affect how the flatfile is \
displayed.",

"A secondary accession usually indicates a record replaced or subsumed \
by the current record. In this case, the current accession and \
secondary are the same.",

"GI numbers are assigned to sequences by NCBI's sequence tracking \
database. 0 is not a legal value for a gi number.",

"The MolInfo biomol field is inconsistent with the Bioseq molecule type \
field.",

"The Bioseq history gi refers to this Bioseq, not to its predecessor or \
successor.",

"The Bioseq has a gi identifier but no GenBank/EMBL/DDBJ accession \
identifier.",

"The Bioseq has a gi identifier and more than one GenBank/EMBL/DDBJ \
accession identifier.",

"The Bioseq has a TPA identifier but does not have a Seq-hist.assembly alignment. \
This should be annotated or calculated by the database, resulting in a PRIMARY \
block visible in the flatfile.",

/* SEQ_DESCR */

"The biological source of this sequence has not been described \
correctly. A Bioseq must have a BioSource descriptor that covers the \
entire molecule. Additional BioSource features may also be added to \
recombinant molecules, natural or otherwise, to designate the parts of \
the molecule. Please add the source information.",

"This descriptor cannot be used with this Bioseq. A descriptor placed at \
the BioseqSet level applies to all of the Bioseqs in the set. Please \
make sure the descriptor is consistent with every sequence to which it \
applies.",

"FileOpen is unable to find a local file. This is normal, and can be \
ignored.",

"An unknown or 'other' modifier was used.",

"No publications were found in this entry which refer to this Bioseq. If \
a publication descriptor is added to a BioseqSet, it will apply to all \
of the Bioseqs in the set. A publication feature should be used if the \
publication applies only to a subregion of a sequence.",

"This entry does not specify the organism that was the source of the \
sequence. Please name the organism.",

"There are multiple BioSource or OrgRef descriptors in the same chain \
with the same taxonomic name. Their information should be combined into \
a single BioSource descriptor.",

"This sequence does not have a Mol-info descriptor applying to it. This \
indicates genomic vs. message, sequencing technique, and whether the \
sequence is incomplete.",

"The country code (up to the first colon) is not on the approved list of \
countries.",

"The BioSource is missing a taxonID database identifier. This will be \
inserted by the automated taxonomy lookup called by Clean Up Record.",

"This population study has BioSource descriptors with different \
taxonomic names. All members of a population study should be from the \
same organism.",

"A BioSource should have a taxonomic lineage, which can be obtained from \
the taxonomy network server.",

"Comments that refer to the conclusions of a specific reference should \
not be cited by a serial number inside brackets (e.g., [3]), but should \
instead be attached as a REMARK on the reference itself.",

"Focus must be set on a BioSource descriptor in records where there is a \
BioSource feature with a different organism name.",

"Note that only Kinetoplastida have kinetoplasts, and that only \
Chlorarchniophyta and Cryptophyta have nucleomorphs.",

"There are multiple chromosome qualifiers on this Bioseq. With the \
exception of some pseudoautosomal genes, this is likely to be a \
biological annotation error.",

"Unassigned SubSource subtype.",

"Unassigned OrgMod subtype.",

"An instantiated protein title descriptor should normally be the same as \
the automatically generated title. This may be a curated exception, or \
it may be out of synch with the current annotation.",

"There are two descriptors of the same type which are inconsistent with \
each other. Please make them consistent.",

"There is a source location that is no longer legal for use in GenBank \
records.",

"There is a source qualifier that is no longer legal for use in GenBank \
records.",

"The name of a structured source field is present as text in a note. \
The data should probably be put into the appropriate field instead.",

"There are multiple title descriptors in the same chain.",

/* SEQ_GENERIC */

"There is a non-ASCII type character in this entry.",

"There is a potentially misspelled word in this entry.",

"The author list contains et al, which should be replaced with the \
remaining author names.",

"The publication is missing essential information, such as title or \
authors.",

"A nested Pub-equiv is not normally expected in a publication. This may \
prevent proper display of all publication information.",

"The publication page numbering is suspect.",

/* SEQ_PKG */

"A protein is found in this entry, but the coding region has not been \
described. Please add a CdRegion feature to the nucleotide Bioseq.",

"Both DNA and protein sequences were expected, but one of the two seems \
to be missing. Perhaps this is the wrong package to use.",

"A segmented sequence was expected, but it was not found. Perhaps this \
is the wrong package to use.",

"No Bioseqs were found in this BioseqSet. Is that what was intended?",

"A nuc-prot set should not contain any other BioseqSet except segset.",

"A segset should not contain any other BioseqSet except parts.",

"A segset should not contain both nucleotide and protein Bioseqs.",

"A parts set should not contain both nucleotide and protein Bioseqs.",

"A parts set should not contain BioseqSets.",

"A feature should be packaged on its bioseq, or on a set containing the \
Bioseq.",

"The product of an mRNA feature in a genomic product set should point to \
a cDNA Bioseq packaged in the set, perhaps within a nuc-prot set. \
RefSeq records may however be referenced remotely.",

"Mol-info.biomol is inconsistent within a segset or parts set.",

/* SEQ_FEAT */

"This feature type is illegal on this type of Bioseq.",

"There are several places in an entry where a sequence can be described \
as either partial or complete. In this entry, these settings are \
inconsistent. Make sure that the location and product Seq-locs, the \
Bioseqs, and the SeqFeat partial flag all agree in describing this \
SeqFeat as partial or complete.",

"A feature with an invalid type has been detected. This is most likely a \
software problem.",

"The coordinates describing the location of a feature do not fall within \
the sequence itself. A feature location or a product Seq-loc is out of \
range of the Bioseq it points to.",

"Mixed strands (plus and minus) have been found in the same location. \
While this is biologically possible, it is very unusual. Please check \
that this is really what you mean.",

"This location has intervals that are out of order. While whis is \
biologically possible, it is very unusual. Please check that this is \
really what you mean.",

"A fundamental error occurred in software while attempting to translate \
this coding region. It is either a software problem or sever data \
corruption.",

"An illegal start codon was used. Some possible explanations are: (1) \
the wrong genetic code may have been selected; (2) the wrong reading \
frame may be in use; or (3) the coding region may be incomplete at the \
5' end, in which case a partial location should be indicated.",

"Internal stop codons are found in the protein sequence. Some possible \
explanations are: (1) the wrong genetic code may have been selected; (2) \
the wrong reading frame may be in use; (3) the coding region may be \
incomplete at the 5' end, in which case a partial location should be \
indicated; or (4) the CdRegion feature location is incorrect.",

"Normally a protein sequence is supplied. This sequence can then be \
compared with the translation of the coding region. In this entry, no \
protein Bioseq was found, and the comparison could not be made.",

"The protein sequence that was supplied is not identical to the \
translation of the coding region. Mismatching amino acids are found \
between these two sequences.",

"The protein sequence that was supplied is not the same length as the \
translation of the coding region. Please determine why they are \
different.",

"A coding region that is complete should have a stop codon at the 3'end. \
 A stop codon was not found on this sequence, although one was \
expected.",

"An unparsed transl_except qualifier was found. This indicates a parser \
problem.",

"The name and description of the protein is missing from this entry. \
Every protein Bioseq must have one full-length Prot-ref feature to \
provide this information.",

"Splice junctions typically have GT as the first two bases of the intron \
(splice donor) and AG as the last two bases of the intron (splice \
acceptor). This intron does not conform to that pattern.",

"A coding region flagged as orf has a protein product. There should be \
no protein product bioseq on an orf.",

"A gene feature exists with no locus name or other fields filled in.",

"A coding region has an exception gbqual but the excpt flag is not \
set.",

"A protein feature exists with no name or other fields filled in.",

"The genetic code stored in the BioSource is different than that for \
this CDS.",

"RNA type 0 (unknown RNA) should be type 255 (other).",

"An import feature has an unrecognized key.",

"An import feature has an unrecognized qualifier.",

"This qualifier is not legal for this feature.",

"An essential qualifier for this feature is missing.",

"A coding region flagged as pseudo has a protein product. There should \
be no protein product bioseq on a pseudo CDS.",

"The database in a cross-reference is not on the list of officially \
recognized database abbreviations.",

"The location has a reference to a bioseq that is not packaged in this \
record.",

"The intervals on this feature are identical to another feature of the \
same type, but the label or comment are different.",

"This feature has a gene xref that is identical to the overlapping gene. \
This is redundant, and probably should be removed.",

"A /transl_except qualifier was not on a codon boundary.",

"The tRNA codon recognized does not code for the indicated amino acid \
using the specified genetic code.",

"Feature location indicates that it is on both strands. This is not \
biologically possible for this kind of feature. Please indicate the \
correct strand (plus or minus) for this feature.",

"A CDS is overlapped by a gene feature, but is not completely contained \
by it. This may be an annotation error.",

"A CDS is overlapped by an mRNA feature, but the mRNA does not cover all \
intervals (i.e., exons) on the CDS. This may be an annotation error.",

"The intervals on this processed protein feature overlap another protein \
feature. This may be caused by errors in originally annotating these \
features on DNA coordinates, where start or stop positions do not occur \
in between codon boundaries. These then appear as errors when the \
features are converted to protein coordinates by mapping through the \
CDS.",

"Comments that refer to the conclusions of a specific reference should \
not be cited by a serial number inside brackets (e.g., [3]), but should \
instead be attached as a REMARK on the reference itself.",

"More than one CDS feature points to the same protein product. This can \
happen with viral long terminal repeats (LTRs), but GenBank policy is to \
have each equivalent CDS point to a separately accessioned protein \
Bioseq.",

"The /focus flag is only appropriate on BioSource descriptors, not \
BioSource features.",

"The start or stop positions of this processed peptide feature do not \
occur in between codon boundaries. This may incorrectly overlap other \
peptides when the features are converted to protein coordinates by \
mapping through the CDS.",

"The value of this qualifier is constrained to a particular vocabulary \
of style. This value does not conform to those constraints. Please see \
the feature table documentation",

"for more information.",

"More than one mRNA feature points to the same cDNA product. This is an \
error in the genomic product set. Each mRNA feature should have a \
unique product Bioseq.",

"An mRNA is overlapped by a gene feature, but is not completely \
contained by it. This may be an annotation error.",

"The mRNA sequence that was supplied is not the same length as the \
transcription of the mRNA feature. Please determine why they are \
different.",

"The mRNA sequence and the transcription of the mRNA feature are \
different. If the number is large, it may indicate incorrect intron/exon \
boundaries.",

"The nucleotide location and protein product of the CDS are not packaged \
together in the same nuc-prot set. This may be an error in the software \
used to create",

"the record.",

"The location has identical adjacent intervals, e.g., a duplicate exon \
reference.",

"A polyA_site should be at a single nucleotide position.",

"An import feature loc field does not equal the feature location. This \
should be corrected, and then the loc field should be cleared.",

"Feature locations traditionally go on the individual parts of a \
segmented bioseq, not on the segmented sequence itself. These features \
are invisible in asn2ff reports, and are now being flagged for \
correction.",

"A set of citations on a feature should not normally have a nested \
Pub-equiv construct. This may prevent proper matching to the correct \
publication.",

"A CDS that has known translation errors cannot have a /translation \
qualifier.",

"A CDS that has known translation errors must be marked as pseudo to \
suppress the",

"translation.",

"The mRNA feature points to a cDNA product that is not packaged in the \
record. This is an error in the genomic product set.",

"The start of one interval is next to the stop of another. A single \
interval may be desirable in this case.",

"Two gene features should not have the same name.",

"A gene feature on a single Bioseq should have a single interval \
spanning everything considered to be under that gene.",

"The intervals on this feature are identical to another feature of the \
same type, and the label and comment are also identical. This is likely \
to be an error in annotating the record. Note that GenBank format \
suppresses duplicate features, so use of Graphic view is recommended.",

"The feature product refers to a database ID that has a locus name \
but no accession. This is probably an error in parsing of a submission.",

"The RNA feature product type does not correspond to the RNA feature type. \
 These need to be consistent.",

/* SEQ_ALIGN */

"The seqence referenced by an alignment SeqID is not packaged in the \
record.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"BLAST alignments are not desired in records submitted to the sequence \
database.",

/* SEQ_GRAPH */

"The graph minimum value is outside of the 0-100 range.",

"The graph maximum value is outside of the 0-100 range.",

"Some quality scores are below the stated graph minimum value.",

"Some quality scores are above the stated graph maximum value.",

"The number of bytes in the quality graph does not correspond to the",

"stated length of the graph.",

"The quality graphs are not packaged in order - may be due to an old \
fa2htgs bug.",

"The length of the quality graph does not correspond to the length of \
the Bioseq.",

"The length of the quality graph does not correspond to the length of \
the delta Bioseq literal component.",

"The length of the quality graph does not correspond to the length of \
the delta Bioseq location component.",

"The quality graph does not start or stop on a sequence segment \
boundary.",

"The quality graph does not start or stop on a sequence segment \
boundary.",

"The number quality graph does not equal the number of sequence \
segments.",

"Quality score values for known bases should be above 0.",

"Quality score values for unknown bases should not be above 0.",

"Gap positions should not have quality scores above 0.",

"Quality graphs overlap - may be due to an old fa2htgs bug.",

"UNKNOWN"
};



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

 
/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2003/01/24 21:17:19  shomrat
* Added missing verbose strings
*
* Revision 1.5  2003/01/07 20:00:57  shomrat
* Member function GetMessage() changed to GetMsg() due o conflict
*
* Revision 1.4  2002/12/26 16:34:43  shomrat
* Typo
*
* Revision 1.3  2002/12/24 16:51:41  shomrat
* Changes to include directives
*
* Revision 1.2  2002/12/23 20:19:22  shomrat
* Redundan character removed
*
*
* ===========================================================================
*/

