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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko.......
 *
 * File Description:
 *   Validates CSeq_entries and CSeq_submits
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/validator/validator.hpp>
#include <util/static_map.hpp>

#include "validatorp.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


// *********************** CValidator implementation **********************


CValidator::CValidator(CObjectManager& objmgr) :
    m_ObjMgr(&objmgr),
    m_PrgCallback(0),
    m_UserData(0)
{
}


CValidator::~CValidator(void)
{
}


CConstRef<CValidError> CValidator::Validate
(const CSeq_entry& se,
 CScope* scope,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError());
    CValidError_imp imp(*m_ObjMgr, &(*errors), options);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.Validate(se, 0, scope) ) {
        errors.Reset();
    }
    return errors;
}


CConstRef<CValidError> CValidator::Validate
(const CSeq_submit& ss,
 CScope* scope,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError());
    CValidError_imp imp(*m_ObjMgr, &(*errors), options);
    imp.Validate(ss, scope);
    return errors;
}


CConstRef<CValidError> CValidator::Validate
(const CSeq_annot& sa,
 CScope* scope,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError());
    CValidError_imp imp(*m_ObjMgr, &(*errors), options);
    imp.Validate(sa, scope);
    return errors;
}


void CValidator::SetProgressCallback(TProgressCallback callback, void* user_data)
{
    m_PrgCallback = callback;
    m_UserData = user_data;
}


// *********************** CValidError implementation **********************


CValidError::CValidError(void)
{
}

void CValidError::AddValidErrItem
(EDiagSev             sev,
 unsigned int         ec,
 const string&        msg,
 const string&        desc,
 const CSerialObject& obj)
{
    CConstRef<CValidErrItem> item(new CValidErrItem(sev, ec, msg, desc, obj));
    m_ErrItems.push_back(item);
    m_Stats[item->GetSeverity()]++;
}


CValidError::~CValidError()
{
}


// ************************ CValidError_CI implementation **************

CValidError_CI::CValidError_CI(void) :
    m_Validator(0),
    m_ErrCodeFilter(kEmptyStr), // eErr_UNKNOWN
    m_MinSeverity(eDiagSevMin),
    m_MaxSeverity(eDiagSevMax)
{
}


CValidError_CI::CValidError_CI
(const CValidError& ve,
 const string& errcode,
 EDiagSev           minsev,
 EDiagSev           maxsev) :
    m_Validator(&ve),
    m_Current(ve.m_ErrItems.begin()),
    m_ErrCodeFilter(errcode),
    m_MinSeverity(minsev),
    m_MaxSeverity(maxsev)
{
    if ( !Filter(**m_Current) ) {
        Next();
    }
}


CValidError_CI::CValidError_CI(const CValidError_CI& other)
{
    if ( this != &other ) {
        *this = other;
    }
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
    m_Current = iter.m_Current;
    m_ErrCodeFilter = iter.m_ErrCodeFilter;
    m_MinSeverity = iter.m_MinSeverity;
    m_MaxSeverity = iter.m_MaxSeverity;
    return *this;
}


CValidError_CI& CValidError_CI::operator++(void)
{
    Next();
    return *this;
}


CValidError_CI::operator bool (void) const
{
    return m_Current != m_Validator->m_ErrItems.end();
}


const CValidErrItem& CValidError_CI::operator*(void) const
{
    return **m_Current;
}


const CValidErrItem* CValidError_CI::operator->(void) const
{
    return &(**m_Current);
}


bool CValidError_CI::Filter(const CValidErrItem& item) const
{
    EDiagSev item_sev = (*m_Current)->GetSeverity();
    if ( (m_ErrCodeFilter.empty()  ||  
          NStr::StartsWith(item.GetErrCode(), m_ErrCodeFilter))  &&
         ((item_sev >= m_MinSeverity)  &&  (item_sev <= m_MaxSeverity)) ) {
        return true;;
    }
    return false;
}


void CValidError_CI::Next(void)
{
    if ( AtEnd() ) {
        return;
    }

    do {
        ++m_Current;
    } while ( !AtEnd()  &&  !Filter(**m_Current) );
}


bool CValidError_CI::AtEnd(void) const
{
    return m_Current == m_Validator->m_ErrItems.end();
}


// *********************** CValidErrItem implementation ********************

/*
// constant data associated with each error type
struct SErrorData
{
    SErrorInfo(const string& group, const string& terse, const string& verbose) :
        m_Group(group), m_Terse(terse), m_Verbose(verbose)
    {}

    const string& m_Group;      // group of errors it belongs to
    const string  m_Terse;      // short error decription
    const string  m_Verbose;    // long error explenation
};
typedef SErrorInfo                  TErrInfo;
typedef pair<EErrType, TErrInfo>    TErrPair;

static const string kErrGroupAll      = "All";
static const string kErrGroupInst     = "SEQ_INST";
static const string kErrGroupGENERIC  = "GENERIC";
static const string kErrGroupPkg      = "SEQ_PKG";
static const string kErrGroupFeat     = "SEQ_FEAT";
static const string kErrGroupAlign    = "SEQ_ALIGN";
static const string kErrGroupGraph    = "SEQ_GRAPH";
static const string kErrGroupDescr    = "SEQ_DESCR";
static const string kErrGroupInternal = "INTERNAL";
*/

CValidErrItem::CValidErrItem
(EDiagSev             sev,
 unsigned int         ec,
 const string&        msg,
 const string&        desc,
 const CSerialObject& obj)
  : m_Severity(sev),
    m_ErrIndex(ec),
    m_Message(msg),
    m_Desc(desc),
    m_Object(&obj)
{
}


CValidErrItem::~CValidErrItem(void)
{
}


EDiagSev CValidErrItem::GetSeverity(void) const
{
    return m_Severity;
}


const string& CValidErrItem::ConvertSeverity(EDiagSev sev)
{
    static const string str_sev[] = {
        "Info", "Warning", "Error", "Critical", "Fatal", "Trace"
    };

    return str_sev[sev];
}


const string& CValidErrItem::GetErrCode(void) const
{
    if (m_ErrIndex <= eErr_UNKNOWN) {
        return sm_Terse [m_ErrIndex];
    }
    return sm_Terse [eErr_UNKNOWN];
}



const string& CValidErrItem::GetErrGroup (void) const
{
    static const string kSeqInst  = "SEQ_INST";
    static const string kSeqDescr = "SEQ_DESCR";
    static const string kGeneric  = "GENERIC";
    static const string kSeqPkg   = "SEQ_PKG";
    static const string kSeqFeat  = "SEQ_FEAT";
    static const string kSeqAlign = "SEQ_ALIGN";
    static const string kSeqGraph = "SEQ_GRAPH";
    static const string kInternal = "INTERNAL";
    static const string kUnknown   = "UNKNOWN";

#define IS_IN(x) (m_ErrIndex >= ERR_CODE_BEGIN(x))  &&  (m_ErrIndex <= ERR_CODE_END(x))

    if ((m_ErrIndex < eErr_UNKNOWN)  &&  (m_ErrIndex > 0)) {
        if ( IS_IN(SEQ_INST) ) {
            return kSeqInst;
        } else if ( IS_IN(SEQ_DESCR) ) {
            return kSeqDescr;
        } else if ( IS_IN(GENERIC) ) {
            return kGeneric;
        } else if ( IS_IN(SEQ_PKG) ) {
            return kSeqPkg;
        } else if ( IS_IN(SEQ_FEAT) ) {
            return kSeqFeat;
        } else if ( IS_IN(SEQ_ALIGN) ) {
            return kSeqAlign;
        } else if ( IS_IN(SEQ_GRAPH) ) {
            return kSeqGraph;
        } else if ( IS_IN(INTERNAL) ) {
            return kInternal;
        }
    }

#undef IS_IN

    return kUnknown;
}


const string& CValidErrItem::GetMsg(void) const
{
    return m_Message;
}


const string& CValidErrItem::GetObjDesc(void) const
{
    return m_Desc;
}


const string& CValidErrItem::GetVerbose(void) const
{
    if (m_ErrIndex <= eErr_UNKNOWN) {
        return sm_Verbose [m_ErrIndex];
    }
    return sm_Verbose [eErr_UNKNOWN];
}


//const CConstObjectInfo& CValidErrItem::GetObject(void) const
const CSerialObject& CValidErrItem::GetObject(void) const
{
    return *m_Object;
}


#define BEGIN(x) ""
#define END(x) ""

// External terse error type explanation
const string CValidErrItem::sm_Terse [] = {
    "UNKNOWN",

    BEGIN(SEQ_INST),
    "ExtNotAllowed",
    "ExtBadOrMissing",
    "SeqDataNotFound",
    "SeqDataNotAllowed",
    "ReprInvalid",
    "CircularProtein",
    "DSProtein",
    "MolNotSet",
    "MolOther",
    "FuzzyLen",
    "InvalidLen",
    "InvalidAlphabet",
    "SeqDataLenWrong",
    "SeqPortFail",
    "InvalidResidue",
    "StopInProtein",
    "PartialInconsistent",
    "ShortSeq",
    "NoIdOnBioseq",
    "BadDeltaSeq",
    "LongHtgsSequence",
    "LongLiteralSequence",
    "SequenceExceeds350kbp",
    "ConflictingIdsOnBioseq",
    "MolNuclAcid",
    "ConflictingBiomolTech",
    "SeqIdNameHasSpace",
    "IdOnMultipleBioseqs",
    "DuplicateSegmentReferences",
    "TrailingX",
    "BadSeqIdFormat",
    "PartsOutOfOrder",
    "BadSecondaryAccn",
    "ZeroGiNumber",
    "RnaDnaConflict",
    "HistoryGiCollision",
    "GiWithoutAccession",
    "MultipleAccessions",
    "HistAssemblyMissing",
    "TerminalNs",
    "UnexpectedIdentifierChange",
    "InternalNsInSeqLit",
    "SeqLitGapLength0",
    "TpaAssmeblyProblem",
    "SeqLocLength",
    "MissingGaps",
    "CompleteTitleProblem",
    "CompleteCircleProblem",
    END(SEQ_INST),

    BEGIN(SEQ_DESCR),
    "BioSourceMissing",
    "InvalidForType",
    "FileOpenCollision",
    "Unknown",
    "NoPubFound",
    "NoOrgFound",
    "MultipleBioSources",
    "NoMolInfoFound",
    "BadCountryCode",
    "NoTaxonID",
    "InconsistentBioSources",
    "MissingLineage",
    "SerialInComment",
    "BioSourceNeedsFocus",
    "BadOrganelle",
    "MultipleChromosomes",
    "BadSubSource",
    "BadOrgMod",
    "InconsistentProteinTitle",
    "Inconsistent",
    "ObsoleteSourceLocation",
    "ObsoleteSourceQual",
    "StructuredSourceNote",
    "MultipleTitles",
    "Obsolete",
    "UnnecessaryBioSourceFocus",
    "RefGeneTrackingWithoutStatus",
    "UnwantedCompleteFlag",
    "CollidingPublications",
    "TransgenicProblem",
    END(SEQ_DESCR),

    BEGIN(GENERIC),
    "NonAsciiAsn",
    "Spell",
    "AuthorListHasEtAl",
    "MissingPubInfo",
    "UnnecessaryPubEquiv",
    "BadPageNumbering",
    "MedlineEntryPub",
    END(GENERIC),

    BEGIN(SEQ_PKG),
    "NoCdRegionPtr",
    "NucProtProblem",
    "SegSetProblem",
    "EmptySet",
    "NucProtNotSegSet",
    "SegSetNotParts",
    "SegSetMixedBioseqs",
    "PartsSetMixedBioseqs",
    "PartsSetHasSets",
    "FeaturePackagingProblem",
    "GenomicProductPackagingProblem",
    "InconsistentMolInfoBiomols",
    "GraphPackagingProblem",
    END(SEQ_PKG),

    BEGIN(SEQ_FEAT),
    "InvalidForType",
    "PartialProblem",
    "PartialsInconsistent",
    "InvalidType",
    "Range",
    "MixedStrand",
    "SeqLocOrder",
    "CdTransFail",
    "StartCodon",
    "InternalStop",
    "NoProtein",
    "MisMatchAA",
    "TransLen",
    "NoStop",
    "TranslExcept",
    "NoProtRefFound",
    "NotSpliceConsensus",
    "OrfCdsHasProduct",
    "GeneRefHasNoData",
    "ExceptInconsistent",
    "ProtRefHasNoData",
    "GenCodeMismatch",
    "RNAtype0",
    "UnknownImpFeatKey",
    "UnknownImpFeatQual",
    "WrongQualOnImpFeat",
    "MissingQualOnImpFeat",
    "PsuedoCdsHasProduct",
    "IllegalDbXref",
    "FarLocation",
    "DuplicateFeat",
    "UnnecessaryGeneXref",
    "TranslExceptPhase",
    "TrnaCodonWrong",
    "BadTrnaAA",
    "BothStrands",
    "CDSgeneRange",
    "CDSmRNArange",
    "OverlappingPeptideFeat",
    "SerialInComment",
    "MultipleCDSproducts",
    "FocusOnBioSourceFeature",
    "PeptideFeatOutOfFrame",
    "InvalidQualifierValue",
    "MultipleMRNAproducts",
    "mRNAgeneRange",
    "TranscriptLen",
    "TranscriptMismatches",
    "CDSproductPackagingProblem",
    "DuplicateInterval",
    "PolyAsiteNotPoint",
    "ImpFeatBadLoc",
    "LocOnSegmentedBioseq",
    "UnnecessaryCitPubEquiv",
    "ImpCDShasTranslation",
    "ImpCDSnotPseudo",
    "MissingMRNAproduct",
    "AbuttingIntervals",
    "CollidingGeneNames",
    "CollidingLocusTags",
    "MultiIntervalGene",
    "FeatContentDup",
    "BadProductSeqId",
    "RnaProductMismatch",
    "DifferntIdTypesInSeqLoc",
    "MissingCDSproduct",
    "MissingLocation",
    "OnlyGeneXrefs",
    "UTRdoesNotAbutCDS",
    "MultipleCdsOnMrna",
    "BadConflictFlag",
    "ConflictFlagSet",
    "LocusTagProblem",
    "AltStartCodon",
    "GenesInconsistent",
    "DuplicateTranslExcept",
    "TranslExceptAndRnaEditing",
    "NoNameForProtein",
    "TaxonDbxrefOnFeature",
    END(SEQ_FEAT),

    BEGIN(SEQ_ALIGN),
    "SeqIdProblem",
    "StrandRev",
    "DensegLenStart",
    "StartMorethanBiolen",
    "EndMorethanBiolen",
    "LenMorethanBiolen",
    "SumLenStart",
    "SegsDimMismatch",
    "SegsNumsegMismatch",
    "SegsStartsMismatch",
    "SegsPresentMismatch",
    "SegsPresentStartsMismatch",
    "SegsPresentStrandsMismatch",
    "FastaLike",
    "SegmentGap",
    "SegsInvalidDim",
    "Segtype",
    "BlastAligns",
    END(SEQ_ALIGN),

    BEGIN(SEQ_GRAPH),
    "GraphMin",
    "GraphMax",
    "GraphBelow",
    "GraphAbove",
    "GraphByteLen",
    "GraphOutOfOrder",
    "GraphBioseqLen",
    "GraphSeqLitLen",
    "GraphSeqLocLen",
    "GraphStartPhase",
    "GraphStopPhase",
    "GraphDiffNumber",
    "GraphACGTScore",
    "GraphNScore",
    "GraphGapScore",
    "GraphOverlap",
    END(SEQ_GRAPH),

    BEGIN(INTERNAL),
    "Exception",
    END(INTERNAL),

    "UNKONWN"
};



// External verbose error type explanation
const string CValidErrItem::sm_Verbose [] = {
"UNKNOWN",

/* SEQ_INST */
BEGIN(SEQ_INST),

//  SEQ_INST_ExtNotAllowed
"A Bioseq 'extension' is used for special classes of Bioseq. This class \
of Bioseq should not have one but it does. This is probably a software \
error.",
//  SEQ_INST_ExtBadOrMissing
"This class of Bioseq requires an 'extension' but it is missing or of \
the wrong type. This is probably a software error.",
//  SEQ_INST_SeqDataNotFound
"No actual sequence data was found on this Bioseq. This is probably a \
software problem.",
//  SEQ_INST_SeqDataNotAllowed
"The wrong type of sequence data was found on this Bioseq. This is \
probably a software problem.",
//  SEQ_INST_ReprInvalid
"This Bioseq has an invalid representation class. This is probably a \
software error.",
//  SEQ_INST_CircularProtein
"This protein Bioseq is represented as circular. Circular topology is \
normally used only for certain DNA molecules, for example, plasmids.",
//  SEQ_INST_DSProtein
"This protein Bioseq has strandedness indicated. Strandedness is \
normally a property only of DNA sequences. Please unset the \
strandedness.",
//  SEQ_INST_MolNotSet
"It is not clear whether this sequence is nucleic acid or protein. \
Please set the appropriate molecule type (Bioseq.mol).",
//  SEQ_INST_MolOther
"Most sequences are either nucleic acid or protein. However, the \
molecule type (Bioseq.mol) is set to 'other'. It should probably be set \
to nucleic acid or a protein.",
//  SEQ_INST_FuzzyLen
"This sequence is marked as having an uncertain length, but the length \
is known exactly.",
//  SEQ_INST_InvalidLen
"The length indicated for this sequence is invalid. This is probably a \
software error.",
//  SEQ_INST_InvalidAlphabet
"This Bioseq has an invalid alphabet (e.g. protein codes on a nucleic \
acid or vice versa). This is probably a software error.",
//  SEQ_INST_SeqDataLenWrong
"The length of this Bioseq does not agree with the length of the actual \
data. This is probably a software error.",
//  SEQ_INST_SeqPortFail
"Something is very wrong with this entry. The validator cannot open a \
SeqPort on the Bioseq. Further testing cannot be done.",
//  SEQ_INST_InvalidResidue
"Invalid residue codes were found in this Bioseq.",
//  SEQ_INST_StopInProtein
"Stop codon symbols were found in this protein Bioseq.",
//  SEQ_INST_PartialInconsistent
"This segmented sequence is described as complete or incomplete in \
several places, but these settings are inconsistent.",
//  SEQ_INST_ShortSeq
"This Bioseq is unusually short (less than 4 amino acids or less than 11 \
nucleic acids). GenBank does not usually accept such short sequences.",
//  SEQ_INST_NoIdOnBioseq
"No SeqIds were found on this Bioseq. This is probably a software \
error.",
//  SEQ_INST_BadDeltaSeq
"Delta sequences should only be HTGS-1 or HTGS-2.",
//  SEQ_INST_LongHtgsSequence
"HTGS-1 or HTGS-2 sequences must be < 350 KB in length.",
//  SEQ_INST_LongLiteralSequence
"Delta literals must be < 350 KB in length.",
//  SEQ_INST_SequenceExceeds350kbp
"Individual sequences must be < 350 KB in length, unless they represent \
a single gene.",
//  SEQ_INST_ConflictingIdsOnBioseq
"Two SeqIds of the same class was found on this Bioseq. This is probably \
a software error.",
//  SEQ_INST_MolNuclAcid
"The specific type of this nucleic acid (DNA or RNA) is not set.",
//  SEQ_INST_ConflictingBiomolTech
"HTGS/STS/GSS records should be genomic DNA. There is a conflict between \
the technique and expected molecule type.",
//  SEQ_INST_SeqIdNameHasSpace
"The Seq-id.name field should be a single word without any whitespace. \
This should be fixed by the database staff.",
//  SEQ_INST_IdOnMultipleBioseqs
"There are multiple occurrences of the same Seq-id in this record. \
Sequence identifiers must be unique within a record.",
//  SEQ_INST_DuplicateSegmentReferences
"The segmented sequence refers multiple times to the same Seq-id. This \
may be due to a software error. Please consult with the database staff \
to fix this record.",
//  SEQ_INST_TrailingX
"The protein sequence ends with one or more X (unknown) amino acids.",
//  SEQ_INST_BadSeqIdFormat
"A nucleotide sequence identifier should be 1 letter plus 5 digits or 2 \
letters plus 6 digits, and a protein sequence identifer should be 3 \
letters plus 5 digits.",
//  SEQ_INST_PartsOutOfOrder
"The parts inside a segmented set should correspond to the seq_ext of \
the segmented bioseq. A difference will affect how the flatfile is \
displayed.",
//  SEQ_INST_BadSecondaryAccn
"A secondary accession usually indicates a record replaced or subsumed \
by the current record. In this case, the current accession and \
secondary are the same.",
//  SEQ_INST_ZeroGiNumber
"GI numbers are assigned to sequences by NCBI's sequence tracking \
database. 0 is not a legal value for a gi number.",
//  SEQ_INST_RnaDnaConflict
"The MolInfo biomol field is inconsistent with the Bioseq molecule type \
field.",
//  SEQ_INST_HistoryGiCollision
"The Bioseq history gi refers to this Bioseq, not to its predecessor or \
successor.",
//  SEQ_INST_GiWithoutAccession
"The Bioseq has a gi identifier but no GenBank/EMBL/DDBJ accession \
identifier.",
//  SEQ_INST_MultipleAccessions
"The Bioseq has a gi identifier and more than one GenBank/EMBL/DDBJ \
accession identifier.",
//  SEQ_INST_HistAssemblyMissing
"The Bioseq has a TPA identifier but does not have a Seq-hist.assembly alignment. \
This should be annotated or calculated by the database, resulting in a PRIMARY \
block visible in the flatfile.",
//  SEQ_INST_TerminalNs
"The Bioseq has one or more N bases at the end.",
//  SEQ_INST_UnexpectedIdentifierChange
"The set of sequence identifiers on a Bioseq are not consistent with the \
previous version of the record in the database.",
//  SEQ_INST_InternalNsInSeqLit
"There are runs of many Ns inside the SeqLit component of a delta Bioseq.",
//  SEQ_INST_SeqLitGapLength0
"A SeqLit component of a delta Bioseq can specify a gap, but it should \
not be a gap of 0 length.",
//  SEQ_INST_TpaAssmeblyProblem
"Third party annotation records should have a TpaAssembly user object and a \
Seq-hist.assembly alignment for the PRIMARY block.",
//  SEQ_INST_SeqLocLength
"A SeqLoc component of a delta Bioseq is suspiciously small.",
//  SEQ_INST_MissingGaps
"HTGS delta records should have gaps between each sequence segment.",
//  SEQ_INST_CompleteTitleProblem
"The sequence title has complete genome in it, but it is not marked as complete.",
//  SEQ_INST_CompleteCircleProblem
"This sequence has a circular topology, but it is not marked as complete.",

END(SEQ_INST),

/* SEQ_DESCR */
BEGIN(SEQ_DESCR),

//  SEQ_DESCR_BioSourceMissing
"The biological source of this sequence has not been described \
correctly. A Bioseq must have a BioSource descriptor that covers the \
entire molecule. Additional BioSource features may also be added to \
recombinant molecules, natural or otherwise, to designate the parts of \
the molecule. Please add the source information.",
//  SEQ_DESCR_InvalidForType
"This descriptor cannot be used with this Bioseq. A descriptor placed at \
the BioseqSet level applies to all of the Bioseqs in the set. Please \
make sure the descriptor is consistent with every sequence to which it \
applies.",
//  SEQ_DESCR_FileOpenCollision
"FileOpen is unable to find a local file. This is normal, and can be \
ignored.",
//  SEQ_DESCR_Unknown
"An unknown or 'other' modifier was used.",
//  SEQ_DESCR_NoPubFound
"No publications were found in this entry which refer to this Bioseq. If \
a publication descriptor is added to a BioseqSet, it will apply to all \
of the Bioseqs in the set. A publication feature should be used if the \
publication applies only to a subregion of a sequence.",
//  SEQ_DESCR_NoOrgFound
"This entry does not specify the organism that was the source of the \
sequence. Please name the organism.",
//  SEQ_DESCR_MultipleBioSources
"There are multiple BioSource or OrgRef descriptors in the same chain \
with the same taxonomic name. Their information should be combined into \
a single BioSource descriptor.",
//  SEQ_DESCR_NoMolInfoFound
"This sequence does not have a Mol-info descriptor applying to it. This \
indicates genomic vs. message, sequencing technique, and whether the \
sequence is incomplete.",
//  SEQ_DESCR_BadCountryCode
"The country code (up to the first colon) is not on the approved list of \
countries.",
//  SEQ_DESCR_NoTaxonID
"The BioSource is missing a taxonID database identifier. This will be \
inserted by the automated taxonomy lookup called by Clean Up Record.",
//  SEQ_DESCR_InconsistentBioSources
"This population study has BioSource descriptors with different \
taxonomic names. All members of a population study should be from the \
same organism.",
//  SEQ_DESCR_MissingLineage
"A BioSource should have a taxonomic lineage, which can be obtained from \
the taxonomy network server.",
//  SEQ_DESCR_SerialInComment
"Comments that refer to the conclusions of a specific reference should \
not be cited by a serial number inside brackets (e.g., [3]), but should \
instead be attached as a REMARK on the reference itself.",
//  SEQ_DESCR_BioSourceNeedsFocus
"Focus must be set on a BioSource descriptor in records where there is a \
BioSource feature with a different organism name.",
//  SEQ_DESCR_BadOrganelle
"Note that only Kinetoplastida have kinetoplasts, and that only \
Chlorarchniophyta and Cryptophyta have nucleomorphs.",
//  SEQ_DESCR_MultipleChromosomes
"There are multiple chromosome qualifiers on this Bioseq. With the \
exception of some pseudoautosomal genes, this is likely to be a \
biological annotation error.",
//  SEQ_DESCR_BadSubSource
"Unassigned SubSource subtype.",
//  SEQ_DESCR_BadOrgMod
"Unassigned OrgMod subtype.",
//  SEQ_DESCR_InconsistentProteinTitle
"An instantiated protein title descriptor should normally be the same as \
the automatically generated title. This may be a curated exception, or \
it may be out of synch with the current annotation.",
//  SEQ_DESCR_Inconsistent
"There are two descriptors of the same type which are inconsistent with \
each other. Please make them consistent.",
//  SEQ_DESCR_ObsoleteSourceLocation
"There is a source location that is no longer legal for use in GenBank \
records.",
//  SEQ_DESCR_ObsoleteSourceQual
"There is a source qualifier that is no longer legal for use in GenBank \
records.",
//  SEQ_DESCR_StructuredSourceNote
"The name of a structured source field is present as text in a note. \
The data should probably be put into the appropriate field instead.",
//  SEQ_DESCR_MultipleTitles
"There are multiple title descriptors in the same chain.",
//  SEQ_DESCR_Obsolete
"Obsolete descriptor type.",
//  SEQ_DESCR_UnnecessaryBioSourceFocus
"Focus should not be set on a BioSource descriptor in records where there is no \
BioSource feature.",
//  SEQ_DESCR_RefGeneTrackingWithoutStatus
"The RefGeneTracking user object does not have the required Status field set.",
//  SEQ_DESCR_UnwantedCompleteFlag
"The Mol-info.completeness flag should not be set on a genomic sequence unless \
the title also says it is a complete sequence or complete genome.",
//  SEQ_DESCR_CollidingPublications
"Multiple publication descriptors with the same PMID or MUID apply to a Bioseq. \
The lower-level ones are redundant, and should be removed.",
//  SEQ_DESCR_TransgenicProblem
"A BioSource descriptor with /transgenic set must be accompanied by a BioSource \
feature on the nucleotide record.",

END(SEQ_DESCR),

/* SEQ_GENERIC */
BEGIN(GENERIC),

//  GENERIC_NonAsciiAsn
"There is a non-ASCII type character in this entry.",
//  GENERIC_Spell
"There is a potentially misspelled word in this entry.",
//  GENERIC_AuthorListHasEtAl
"The author list contains et al, which should be replaced with the \
remaining author names.",
//  GENERIC_MissingPubInfo
"The publication is missing essential information, such as title or \
authors.",
//  GENERIC_UnnecessaryPubEquiv
"A nested Pub-equiv is not normally expected in a publication. This may \
prevent proper display of all publication information.",
//  GENERIC_BadPageNumbering
"The publication page numbering is suspect.",
//  GENERIC_MedlineEntryPub
"Publications should not be of type medline-entry.  This has abstract and MeSH \
term information that does not appear in the GenBank flatfile.  Type cit-art \
should be used instead.",

END(GENERIC),

/* SEQ_PKG */
BEGIN(SEQ_PKG),

//  SEQ_PKG_NoCdRegionPtr
"A protein is found in this entry, but the coding region has not been \
described. Please add a CdRegion feature to the nucleotide Bioseq.",
//  SEQ_PKG_NucProtProblem
"Both DNA and protein sequences were expected, but one of the two seems \
to be missing. Perhaps this is the wrong package to use.",
//  SEQ_PKG_SegSetProblem
"A segmented sequence was expected, but it was not found. Perhaps this \
is the wrong package to use.",
//  SEQ_PKG_EmptySet
"No Bioseqs were found in this BioseqSet. Is that what was intended?",
//  SEQ_PKG_NucProtNotSegSet
"A nuc-prot set should not contain any other BioseqSet except segset.",
//  SEQ_PKG_SegSetNotParts
"A segset should not contain any other BioseqSet except parts.",
//  SEQ_PKG_SegSetMixedBioseqs
"A segset should not contain both nucleotide and protein Bioseqs.",
//  SEQ_PKG_PartsSetMixedBioseqs
"A parts set should not contain both nucleotide and protein Bioseqs.",
//  SEQ_PKG_PartsSetHasSets
"A parts set should not contain BioseqSets.",
//  SEQ_PKG_FeaturePackagingProblem
"A feature should be packaged on its bioseq, or on a set containing the \
Bioseq.",
//  SEQ_PKG_GenomicProductPackagingProblem
"The product of an mRNA feature in a genomic product set should point to \
a cDNA Bioseq packaged in the set, perhaps within a nuc-prot set. \
RefSeq records may however be referenced remotely.",
//  SEQ_PKG_InconsistentMolInfoBiomols
"Mol-info.biomol is inconsistent within a segset or parts set.",
//  SEQ_PKG_GraphPackagingProblem
"A graph should be packaged on its bioseq, or on a set containing the Bioseq.",

END(SEQ_PKG),

/* SEQ_FEAT */
BEGIN(SEQ_FEAT),

//  SEQ_FEAT_InvalidForType
"This feature type is illegal on this type of Bioseq.",
//  SEQ_FEAT_PartialProblem
"There are several places in an entry where a sequence can be described \
as either partial or complete. In this entry, these settings are \
inconsistent. Make sure that the location and product Seq-locs, the \
Bioseqs, and the SeqFeat partial flag all agree in describing this \
SeqFeat as partial or complete.",
// SEQ_FEAT_PartialsInconsistent
"This segmented sequence is described as complete or incomplete in several \
places, but these settings are inconsistent.",
//  SEQ_FEAT_InvalidType
"A feature with an invalid type has been detected. This is most likely a \
software problem.",
//  SEQ_FEAT_Range
"The coordinates describing the location of a feature do not fall within \
the sequence itself. A feature location or a product Seq-loc is out of \
range of the Bioseq it points to.",
//  SEQ_FEAT_MixedStrand
"Mixed strands (plus and minus) have been found in the same location. \
While this is biologically possible, it is very unusual. Please check \
that this is really what you mean.",
//  SEQ_FEAT_SeqLocOrder
"This location has intervals that are out of order. While whis is \
biologically possible, it is very unusual. Please check that this is \
really what you mean.",
//  SEQ_FEAT_CdTransFail
"A fundamental error occurred in software while attempting to translate \
this coding region. It is either a software problem or sever data \
corruption.",
//  SEQ_FEAT_StartCodon
"An illegal start codon was used. Some possible explanations are: (1) \
the wrong genetic code may have been selected; (2) the wrong reading \
frame may be in use; or (3) the coding region may be incomplete at the \
5' end, in which case a partial location should be indicated.",
//  SEQ_FEAT_InternalStop
"Internal stop codons are found in the protein sequence. Some possible \
explanations are: (1) the wrong genetic code may have been selected; (2) \
the wrong reading frame may be in use; (3) the coding region may be \
incomplete at the 5' end, in which case a partial location should be \
indicated; or (4) the CdRegion feature location is incorrect.",
//  SEQ_FEAT_NoProtein
"Normally a protein sequence is supplied. This sequence can then be \
compared with the translation of the coding region. In this entry, no \
protein Bioseq was found, and the comparison could not be made.",
 //  SEQ_FEAT_MisMatchAA
"The protein sequence that was supplied is not identical to the \
translation of the coding region. Mismatching amino acids are found \
between these two sequences.",
//  SEQ_FEAT_TransLen
"The protein sequence that was supplied is not the same length as the \
translation of the coding region. Please determine why they are \
different.",
//  SEQ_FEAT_NoStop
"A coding region that is complete should have a stop codon at the 3'end. \
 A stop codon was not found on this sequence, although one was \
expected.",
//  SEQ_FEAT_TranslExcept
"An unparsed transl_except qualifier was found. This indicates a parser \
problem.",
//  SEQ_FEAT_NoProtRefFound
"The name and description of the protein is missing from this entry. \
Every protein Bioseq must have one full-length Prot-ref feature to \
provide this information.",
//  SEQ_FEAT_NotSpliceConsensus
"Splice junctions typically have GT as the first two bases of the intron \
(splice donor) and AG as the last two bases of the intron (splice \
acceptor). This intron does not conform to that pattern.",
//  SEQ_FEAT_OrfCdsHasProduct
"A coding region flagged as orf has a protein product. There should be \
no protein product bioseq on an orf.",
//  SEQ_FEAT_GeneRefHasNoData
"A gene feature exists with no locus name or other fields filled in.",
//  SEQ_FEAT_ExceptInconsistent
"A coding region has an exception gbqual but the excpt flag is not \
set.",
//  SEQ_FEAT_ProtRefHasNoData
"A protein feature exists with no name or other fields filled in.",
//  SEQ_FEAT_GenCodeMismatch
"The genetic code stored in the BioSource is different than that for \
this CDS.",
//  SEQ_FEAT_RNAtype0
"RNA type 0 (unknown RNA) should be type 255 (other).",
//  SEQ_FEAT_UnknownImpFeatKey
"An import feature has an unrecognized key.",
//  SEQ_FEAT_UnknownImpFeatQual
"An import feature has an unrecognized qualifier.",
//  SEQ_FEAT_WrongQualOnImpFeat
"This qualifier is not legal for this feature.",
//  SEQ_FEAT_MissingQualOnImpFeat
"An essential qualifier for this feature is missing.",
//  SEQ_FEAT_PsuedoCdsHasProduct
"A coding region flagged as pseudo has a protein product. There should \
be no protein product bioseq on a pseudo CDS.",
//  SEQ_FEAT_IllegalDbXref
"The database in a cross-reference is not on the list of officially \
recognized database abbreviations.",
//  SEQ_FEAT_FarLocation
"The location has a reference to a bioseq that is not packaged in this \
record.",
//  SEQ_FEAT_DuplicateFeat
"The intervals on this feature are identical to another feature of the \
same type, but the label or comment are different.",
//  SEQ_FEAT_UnnecessaryGeneXref
"This feature has a gene xref that is identical to the overlapping gene. \
This is redundant, and probably should be removed.",
//  SEQ_FEAT_TranslExceptPhase
"A /transl_except qualifier was not on a codon boundary.",
//  SEQ_FEAT_TrnaCodonWrong
"The tRNA codon recognized does not code for the indicated amino acid \
using the specified genetic code.",
//  SEQ_FEAT_BadTrnaAA
"The tRNA encoded amino acid is an illegal value.",
//  SEQ_FEAT_BothStrands
"Feature location indicates that it is on both strands. This is not \
biologically possible for this kind of feature. Please indicate the \
correct strand (plus or minus) for this feature.",
//  SEQ_FEAT_CDSgeneRange
"A CDS is overlapped by a gene feature, but is not completely contained \
by it. This may be an annotation error.",
//  SEQ_FEAT_CDSmRNArange
"A CDS is overlapped by an mRNA feature, but the mRNA does not cover all \
intervals (i.e., exons) on the CDS. This may be an annotation error.",
//  SEQ_FEAT_OverlappingPeptideFeat
"The intervals on this processed protein feature overlap another protein \
feature. This may be caused by errors in originally annotating these \
features on DNA coordinates, where start or stop positions do not occur \
in between codon boundaries. These then appear as errors when the \
features are converted to protein coordinates by mapping through the \
CDS.",
//  SEQ_FEAT_SerialInComment
"Comments that refer to the conclusions of a specific reference should \
not be cited by a serial number inside brackets (e.g., [3]), but should \
instead be attached as a REMARK on the reference itself.",
//  SEQ_FEAT_MultipleCDSproducts
"More than one CDS feature points to the same protein product. This can \
happen with viral long terminal repeats (LTRs), but GenBank policy is to \
have each equivalent CDS point to a separately accessioned protein \
Bioseq.",
//  SEQ_FEAT_FocusOnBioSourceFeature
"The /focus flag is only appropriate on BioSource descriptors, not \
BioSource features.",
//  SEQ_FEAT_PeptideFeatOutOfFrame
"The start or stop positions of this processed peptide feature do not \
occur in between codon boundaries. This may incorrectly overlap other \
peptides when the features are converted to protein coordinates by \
mapping through the CDS.",
//  SEQ_FEAT_InvalidQualifierValue
"The value of this qualifier is constrained to a particular vocabulary \
of style. This value does not conform to those constraints. Please see \
the feature table documentation for more information.",
//  SEQ_FEAT_MultipleMRNAproducts
"More than one mRNA feature points to the same cDNA product. This is an \
error in the genomic product set. Each mRNA feature should have a \
unique product Bioseq.",
//  SEQ_FEAT_mRNAgeneRange
"An mRNA is overlapped by a gene feature, but is not completely \
contained by it. This may be an annotation error.",
//  SEQ_FEAT_TranscriptLen
"The mRNA sequence that was supplied is not the same length as the \
transcription of the mRNA feature. Please determine why they are \
different.",
//  SEQ_FEAT_TranscriptMismatches
"The mRNA sequence and the transcription of the mRNA feature are \
different. If the number is large, it may indicate incorrect intron/exon \
boundaries.",
//  SEQ_FEAT_CDSproductPackagingProblem
"The nucleotide location and protein product of the CDS are not packaged \
together in the same nuc-prot set. This may be an error in the software \
used to create the record.",
//  SEQ_FEAT_DuplicateInterval
"The location has identical adjacent intervals, e.g., a duplicate exon \
reference.",
//  SEQ_FEAT_PolyAsiteNotPoint
"A polyA_site should be at a single nucleotide position.",
//  SEQ_FEAT_ImpFeatBadLoc
"An import feature loc field does not equal the feature location. This \
should be corrected, and then the loc field should be cleared.",
//  SEQ_FEAT_LocOnSegmentedBioseq
"Feature locations traditionally go on the individual parts of a \
segmented bioseq, not on the segmented sequence itself. These features \
are invisible in asn2ff reports, and are now being flagged for \
correction.",
//  SEQ_FEAT_UnnecessaryCitPubEquiv
"A set of citations on a feature should not normally have a nested \
Pub-equiv construct. This may prevent proper matching to the correct \
publication.",
//  SEQ_FEAT_ImpCDShasTranslation
"A CDS that has known translation errors cannot have a /translation \
qualifier.",
//  SEQ_FEAT_ImpCDSnotPseudo
"A CDS that has known translation errors must be marked as pseudo to \
suppress the translation.",
//  SEQ_FEAT_MissingMRNAproduct
"The mRNA feature points to a cDNA product that is not packaged in the \
record. This is an error in the genomic product set.",
//  SEQ_FEAT_AbuttingIntervals
"The start of one interval is next to the stop of another. A single \
interval may be desirable in this case.",
//  SEQ_FEAT_CollidingGeneNames
"Two gene features should not have the same name.",
//  SEQ_FEAT_CollidingLocusTags
"Two gene features should not have the same locus_tag, which is supposed \
to be a unique identifer.",
//  SEQ_FEAT_MultiIntervalGene
"A gene feature on a single Bioseq should have a single interval \
spanning everything considered to be under that gene.",
//  SEQ_FEAT_FeatContentDup
"The intervals on this feature are identical to another feature of the \
same type, and the label and comment are also identical. This is likely \
to be an error in annotating the record. Note that GenBank format \
suppresses duplicate features, so use of Graphic view is recommended.",
//  SEQ_FEAT_BadProductSeqId
"The feature product refers to a database ID that has a locus name \
but no accession. This is probably an error in parsing of a submission.",
//  SEQ_FEAT_RnaProductMismatch
"The RNA feature product type does not correspond to the RNA feature type. \
These need to be consistent.",
//  SEQ_FEAT_DifferntIdTypesInSeqLoc
"All ids in a single seq-loc which refer to the same bioseq should be of the \
same id type",
//  SEQ_FEAT_MissingCDSproduct
"The CDS should have a product, but does not.  Pseudo or short CDSs (less than 6 \
amino acids), or those marked with a rearrangement required for product exception, \
are exempt from needing a product.",
//  SEQ_FEAT_MissingLocation
"A feature must specify its location.",
//  SEQ_FEAT_OnlyGeneXrefs
"There are gene xrefs but no gene features.  Records should normally have  \
single-interval gene features covering other biological features.  Gene \
xrefs are used only to override the inheritance by overlap.",
//  SEQ_FEAT_UTRdoesNotAbutCDS
"The 5'UTR and 3'UTR features should exactly abut the CDS feature.",
//  SEQ_FEAT_MultipleCdsOnMrna
"Only a single Cdregion feature should be annotated on mRNA bioseq.",
//  SEQ_FEAT_BadConflictFlag
"The coding region conflict flag is set, but the translated product is the \
same as the instantiated product Bioseq.",
//  SEQ_FEAT_ConflictFlagSet
"The coding region conflict flag is appropriately set, but this record should \
be brought to the attention of the source database for possible correction.",
//  SEQ_FEAT_LocusTagProblem
"A gene locus_tag should be a single token, with no spaces.",
//  SEQ_FEAT_AltStartCodon
"An alternative start codon was used. This is rare, and it is expected that \
confirmatory evidence will be cited.",
//  SEQ_FEAT_GenesInconsistent
"The gene on the genomic sequence of a genomic product set should be the \
same as the gene on the cDNA product of the mRNA feature.",
//  SEQ_FEAT_DuplicateTranslExcept
"There are multiple /transl_except qualifiers at the same location on this \
CDS but with different amino acids indicated.",
//  SEQ_FEAT_TranslExceptAndRnaEditing
"A CDS has both /exception=RNA editing and /transl_except qualifiers.  RNA \
editing indicates post-transcriptional changes prior to translation.  Use \
/transl_except for individual codon exceptions such as selenocysteine or \
other nonsense suppressors.",
//  SEQ_FEAT_NoNameForProtein
"A protein feature has a description, but no product name.",
//  TaxonDbxrefOnFeature
"A BioSource feature has a taxonID database identifier in the db_xref area \
common to all features.  This db_xref should only exist within the separate \
BioSource xref list.",

END(SEQ_FEAT),

/* SEQ_ALIGN */
BEGIN(SEQ_ALIGN),

//  SEQ_ALIGN_SeqIdProblem
"The seqence referenced by an alignment SeqID is not packaged in the record.",
//  SEQ_ALIGN_StrandRev
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_DensegLenStart
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_StartMorethanBiolen
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_EndMorethanBiolen
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_LenMorethanBiolen
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_SumLenStart
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_SegsDimMismatch
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_SegsNumsegMismatch
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_SegsStartsMismatch
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_SegsPresentMismatch
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_SegsPresentStartsMismatch
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_SegsPresentStrandsMismatch
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_FastaLike
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_SegmentGap
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_SegsInvalidDim
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_Segtype
"Please contact the sequence database for further help with this error.",
//  SEQ_ALIGN_BlastAligns
"BLAST alignments are not desired in records submitted to the sequence database.",

END(SEQ_ALIGN),

/* SEQ_GRAPH */
BEGIN(SEQ_GRAPH),

//  SEQ_GRAPH_GraphMin
"The graph minimum value is outside of the 0-100 range.",
//  SEQ_GRAPH_GraphMax
"The graph maximum value is outside of the 0-100 range.",
//  SEQ_GRAPH_GraphBelow
"Some quality scores are below the stated graph minimum value.",
//  SEQ_GRAPH_GraphAbove
"Some quality scores are above the stated graph maximum value.",
//  SEQ_GRAPH_GraphByteLen
"The number of bytes in the quality graph does not correspond to the \
stated length of the graph.",
//  SEQ_GRAPH_GraphOutOfOrder
"The quality graphs are not packaged in order - may be due to an old \
fa2htgs bug.",
//  SEQ_GRAPH_GraphBioseqLen
"The length of the quality graph does not correspond to the length of \
the Bioseq.",
//  SEQ_GRAPH_GraphSeqLitLen
"The length of the quality graph does not correspond to the length of \
the delta Bioseq literal component.",
//  SEQ_GRAPH_GraphSeqLocLen
"The length of the quality graph does not correspond to the length of \
the delta Bioseq location component.",
//  SEQ_GRAPH_GraphStartPhase
"The quality graph does not start or stop on a sequence segment \
boundary.",
//  SEQ_GRAPH_GraphStopPhase
"The quality graph does not start or stop on a sequence segment \
boundary.",
//  SEQ_GRAPH_GraphDiffNumber
"The number quality graph does not equal the number of sequence \
segments.",
//  SEQ_GRAPH_GraphACGTScore
"Quality score values for known bases should be above 0.",
//  SEQ_GRAPH_GraphNScore
"Quality score values for unknown bases should not be above 0.",
//  SEQ_GRAPH_GraphGapScore
"Gap positions should not have quality scores above 0.",
//  SEQ_GRAPH_GraphOverlap
"Quality graphs overlap - may be due to an old fa2htgs bug.",

END(SEQ_GRAPH),

BEGIN(INTERNAL),

//  Internal_Exception
"Exception was caught while performing validation. Vaidation terminated.",

END(INTERNAL),

"UNKNOWN"
};

#undef BEGIN
#undef END

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

 
/*
* ===========================================================================
*
* $Log$
* Revision 1.53  2004/08/09 14:54:10  shomrat
* Added eErr_SEQ_INST_CompleteTitleProblem and eErr_SEQ_INST_CompleteCircleProblem
*
* Revision 1.52  2004/08/04 17:47:34  shomrat
* + SEQ_FEAT_TaxonDbxrefOnFeature
*
* Revision 1.51  2004/08/03 13:39:58  shomrat
* + GENERIC_MedlineEntryPub
*
* Revision 1.50  2004/07/29 17:08:20  shomrat
* + SEQ_DESCR_TransgenicProblem
*
* Revision 1.49  2004/07/29 16:07:58  shomrat
* Separated error message from offending object's description; Added error group
*
* Revision 1.48  2004/07/07 13:26:56  shomrat
* + SEQ_FEAT_NoNameForProtein
*
* Revision 1.47  2004/06/25 15:59:18  shomrat
* + eErr_SEQ_INST_MissingGaps
*
* Revision 1.46  2004/06/25 14:55:59  shomrat
* changes to CValidError and CValidErrorItem; +SEQ_FEAT_DuplicateTranslExcept,SEQ_FEAT_TranslExceptAndRnaEditing
*
* Revision 1.45  2004/06/17 17:03:53  shomrat
* Added CollidingPublications check
*
* Revision 1.44  2004/05/21 21:42:56  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.43  2004/03/25 18:31:41  shomrat
* + SEQ_FEAT_GenesInconsistent
*
* Revision 1.42  2004/03/19 14:47:56  shomrat
* + SEQ_FEAT_PartialsInconsistent
*
* Revision 1.41  2004/03/10 21:22:35  shomrat
* + SEQ_DESCR_UnwantedCompleteFlag
*
* Revision 1.40  2004/03/01 18:38:58  shomrat
* Added alternative start codon error
*
* Revision 1.39  2004/02/25 15:52:15  shomrat
* Added CollidingLocusTags error
*
* Revision 1.38  2004/01/16 20:08:35  shomrat
* Added LocusTagProblem error
*
* Revision 1.37  2003/12/17 19:13:58  shomrat
* Added SEQ_PKG_GraphPackagingProblem
*
* Revision 1.36  2003/12/16 17:34:23  shomrat
* Added SEQ_INST_SeqLocLength
*
* Revision 1.35  2003/12/16 16:17:43  shomrat
* Added SEQ_FEAT_BadConflictFlag and SEQ_FEAT_ConflictFlagSet
*
* Revision 1.34  2003/11/14 15:55:09  shomrat
* added SEQ_INST_TpaAssemblyProblem
*
* Revision 1.33  2003/11/12 20:30:24  shomrat
* added SEQ_FEAT_MultipleCdsOnMrna
*
* Revision 1.32  2003/10/27 14:54:11  shomrat
* added SEQ_FEAT_UTRdoesNotAbutCDS
*
* Revision 1.31  2003/10/27 14:14:41  shomrat
* added SEQ_INST_SeqLitGapLength0
*
* Revision 1.30  2003/10/20 16:07:07  shomrat
* Added SEQ_FEAT_OnlyGeneXrefs
*
* Revision 1.29  2003/10/13 18:45:23  shomrat
* Added SEQ_FEAT_BadTrnaAA
*
* Revision 1.28  2003/09/03 18:24:46  shomrat
* added SEQ_DESCR_RefGeneTrackingWithoutStatus
*
* Revision 1.27  2003/08/06 15:04:30  shomrat
* Added SEQ_INST_InternalNsInSeqLit
*
* Revision 1.26  2003/07/21 21:17:45  shomrat
* Added SEQ_FEAT_MissingLocation
*
* Revision 1.25  2003/06/02 16:06:43  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.24  2003/05/28 16:22:13  shomrat
* Added MissingCDSProduct error.
*
* Revision 1.23  2003/05/14 20:33:31  shomrat
* Using CRef instead of auto_ptr
*
* Revision 1.22  2003/05/05 15:32:16  shomrat
* Added SEQ_FEAT_MissingCDSproduct
*
* Revision 1.21  2003/04/29 14:56:08  shomrat
* Changes to SeqAlign related validation
*
* Revision 1.20  2003/04/15 14:55:37  shomrat
* Implemented a progress callback mechanism
*
* Revision 1.19  2003/04/07 14:55:17  shomrat
* Added SEQ_FEAT_DifferntIdTypesInSeqLoc
*
* Revision 1.18  2003/04/04 18:36:55  shomrat
* Added Internal_Exception error; Changed limits of min/max severity
*
* Revision 1.17  2003/03/31 14:41:07  shomrat
* $id: -> $id$
*
* Revision 1.16  2003/03/28 16:27:40  shomrat
* Added comments
*
* Revision 1.15  2003/03/21 21:10:26  shomrat
* Added SEQ_DESCR_UnnecessaryBioSourceFocus
*
* Revision 1.14  2003/03/21 16:20:33  shomrat
* Added error SEQ_INST_UnexpectedIdentifierChange
*
* Revision 1.13  2003/03/20 18:54:00  shomrat
* Added CValidator implementation
*
* Revision 1.12  2003/03/10 18:12:50  shomrat
* Record statistics information for each item
*
* Revision 1.11  2003/03/06 19:33:02  shomrat
* Bug fix and code cleanup in CVAlidError_CI
*
* Revision 1.10  2003/02/24 20:20:18  shomrat
* Pass the CValidError object to the implementation class instead of the internal TErrs vector
*
* Revision 1.9  2003/02/12 17:38:58  shomrat
* Added eErr_SEQ_DESCR_Obsolete
*
* Revision 1.8  2003/02/07 21:10:55  shomrat
* Added eErr_SEQ_INST_TerminalNs
*
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

