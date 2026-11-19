// These are basically copies of flat2asn.msg, indx_err.msg,
// validatr.msg and medarch.msg files from /am/ncbiapdata/errmsg
// directory
//

#if 0
// satisfy commit hooks
#include <ncbi_pch.hpp>
#endif

const string indx_err_msg = R"(MODULE indx_err
$$ FORMAT, 1
$^   NonAsciiChar, 1
# This is a comment REJECT(LANL,EMBL,DDBJ,PIR,SP)
No column of any block of the flat file in any of the formats
is allowed to have a nonASCII character.  NonASCII is defined
as anything greater than  decimal value 126 (`~`) or less than
decimal 32 (space) except decimal 10, which is a newline.
$^   MissingEnd, 2, SEV_ERROR
The // line was not found after sequence was found before a line
with an letter was found.  Likely to be a truncated entry.
$^   MissingField, 3, SEV_ERROR
 Required field was not found in the flat file
$^   LocusLinePosition, 4, SEV_ERROR
The GenBank flat file format requires fixed column positions
for a variety of fields.
$^   DirSubMode, 5, SEV_WARNING
Not standard EMBL format, used in DirSubMode only
$^   LineTypeOrder, 6, SEV_ERROR
$^   Multiple_NI, 8, SEV_ERROR
$^   ContigInSegset, 9, SEV_ERROR
$^   Multiple_SV, 10, SEV_FATAL
$^   MissingNIDLine, 11, SEV_FATAL
$^   IncorrectNIDLine, 12, SEV_FATAL
$^   UnexpectedEnd, 13, SEV_ERROR
$^   XMLMissingStartTag, 14, SEV_ERROR
$^   XMLMissingEndTag, 15, SEV_ERROR
$^   XMLFormatError, 16, SEV_ERROR
$^   XMLInvalidINSDInterval, 17, SEV_ERROR
$^   IncorrectMGALine, 18, SEV_REJECT
$^   IllegalCAGEMoltype, 19, SEV_REJECT
$^   BadlyFormattedIDLine, 20, SEV_REJECT
$^   InvalidIDlineMolType, 21, SEV_REJECT
$$ ENTRY, 2
$^   Skipped, 1, SEV_ERROR
$^   ParsingSetup, 2, SEV_INFO
$^   Begin, 3, SEV_ERROR
Looking for valid begining according to -f arguement in command line:
LOCUS for GenBank; ID for EMBL; ENTRY for PIR;
$^   InvalidLineType, 6
$^   Parsed, 7, SEV_INFO
$^   ParsingComplete, 8, SEV_INFO
$$ ACCESSION, 3
$^   BadAccessNum, 2, SEV_ERROR
Accession must be upper case letter followed by 5 digits.
$^   NoAccessNum, 3, SEV_ERROR
No accession number could be found for this entry. The line number
given is only approximate.
$^   MoreAccessLine, 4, SEV_INFO
More than one accession block was found.  Continuation lines with
the wrong format can cause this in GenBank format.
$^   ForeignAccessNum, 5, SEV_WARNING
$^   DirSubTakeOver, 6, SEV_WARNING
$^   WGSProjectAccIsPri, 7, SEV_REJECT
$^   UnusualWGS_Secondary, 8
$^   ScfldHasWGSContigSec, 9, SEV_REJECT
$^   WGSWithNonWGS_Sec, 10, SEV_REJECT
$^   2ndAccPrefixMismatch, 11, SEV_REJECT
$^   Invalid2ndAccRange, 12, SEV_REJECT
$^   WGSMasterAsSecondary, 13, SEV_REJECT
$$ LOCUS, 4
$^   WrongTopology, 2, SEV_WARNING
This message occurs when looking for either Circular or 'RNA' or 'DNA' in
embl mode.  Anything other than this will cause this warning.
$^   NoGIBBModMolType, 3, SEV_WARNING
 In the flat files, the only legal values are blank, RNA, pre-mRNA, mRNA,
 rRNA, tRNA, uRNA, ss-RNA, ds-RNA, ms-RNA, scRNA, DNA, ds-DNA, and ss-DNA
$^   MayBeNewSpeciesCode, 4, SEV_WARNING
Swiss-Prot specific error.  The species is not in the list of
LOCUS name prefix-species pairs.
$^   NoSpeciesCode, 5, SEV_WARNING
Swiss-Prot error when no species can be found.
$^   NoMolType, 6, SEV_WARNING
Can't find Molecule type
$^   BadLocusName, 7, SEV_ERROR
There are multiple possible ways to get this error message.
In all formats, this identifier must have either digits or
uppercase letters.
For Swiss-Prot, the rules are more complicated:
Locus name consists of up to 10 uppercase alphanumeric characters
      rule: X_Y format
         X is a mnemonic code, up to 4 alphanumeric characters to represent
             the protein name.
         Y is a mnemonic species identification code of at most 5
             alphanumeric characters to representing the biological source of
             the protein
$^   NoLocusName, 8, SEV_ERROR
No token after 'LOCUS' found in GenBank format.
$$ SEGMENT, 5
$^   MissSegEntry, 1, SEV_WARNING
The Segmented set (GenBank) that is declared has some
missing members.  That is, if there 6 members declared,
The set might only have:
 1 of 6,
 2 of 6,
 4 of 6,
 5 of 6 and
 6 of 6, and thus be missing 3 of 6.
   Another possible problem is there could be a line:
      "3 of 5", instad of "3 of 6"
$^   DiffMolType, 2, SEV_WARNING
A segmented set is supposed to be from the same molecule,
but with some unknown regions.  It should therefore all be
of the same type of molecule.  For this error to occur, there
have to be different molecule types for different segments
within the set.
$^   BadLocusName, 3
For GenBank in a segmented set, it is an error if the segment number
can not be found at the end of the LOCUS name.  So a LOCUS name
in SEGMENT 2 of 10 must end in 02, as ABCD02.
$^   IncompSeg, 4, SEV_ERROR
There were not three blank-separated token on the SEGMENT line, in
GenBank Flat File format, the segment line has to look like:
SEGMENT     4 of 14
 for example.
$^   PubMatch, 5, SEV_WARNING
There were matching reference with different serial numbers in
segments.
$$ VERSION, 6
$^   MissingVerNum, 1, SEV_FATAL
$^   NonDigitVerNum, 2, SEV_FATAL
$^   AccessionsDontMatch, 3, SEV_FATAL
$^   BadVersionLine, 4
$^   IncorrectGIInVersion, 5, SEV_FATAL
$^   NonDigitGI, 6, SEV_FATAL
$^   GIsDontMatch, 7, SEV_FATAL
$^   InvalidVersion, 8, SEV_ERROR
$$ REFERENCE, 8
$^   IllegalDate, 1
$$ FEATURE, 9
$^   NoFeatData, 1, SEV_WARNING
$$ SEQID, 10
$^   NoSeqId, 1, SEV_ERROR
$$ ORGANISM, 11
$^   Multiple, 1, SEV_ERROR
$$ INPUT, 12
$^   CannotReadEntry, 1, SEV_FATAL
$$ TPA, 13
$^   TpaSpansMissing, 1, SEV_REJECT
$$ DATE, 14
$^   IllegalDate, 1, SEV_WARNING
$$ KEYWORD, 15
$^   InvalidTPATier, 1, SEV_REJECT
$^   UnexpectedTPA, 2, SEV_WARNING
$^   MissingTPAKeywords, 3, SEV_REJECT
$^   MissingTPATier, 4, SEV_ERROR
$^   ConflictingTPATiers, 5, SEV_REJECT
$^   MissingTSAKeywords, 6, SEV_REJECT
$^   MissingMGAKeywords, 7, SEV_REJECT
$^   ConflictingMGAKeywords, 8, SEV_REJECT
$^   MissingTLSKeywords, 9, SEV_REJECT
$$ TSA, 16
$^   TsaSpansMissing, 1, SEV_WARNING
$$ QSCORE, 17
$^   RedundantScores, 1, SEV_FATAL
$^   NoSequenceRecord, 2, SEV_FATAL
$^   NoScoreDataFound, 3, SEV_FATAL)";

const string medarch_msg = R"(MODULE medarch
$$ REFERENCE, 1
$^   MuidNotFound, 1, SEV_WARNING
$^   SuccessfulMuidLookup, 2, SEV_INFO
$^   OldInPress, 3, SEV_WARNING
$^   No_reference, 4, SEV_WARNING
$^   Multiple_ref, 5, SEV_WARNING
$^   Multiple_muid, 6, SEV_WARNING
$^   MedlineMatchIgnored, 7, SEV_ERROR
$^   MuidMissmatch, 8, SEV_WARNING
$^   NoConsortAuthors, 9, SEV_WARNING
$^   DiffConsortAuthors, 10, SEV_WARNING
$^   PmidMissmatch, 11, SEV_WARNING
$^   Multiple_pmid, 12, SEV_WARNING
$^   FailedToGetPub, 13, SEV_ERROR
$^   MedArchMatchIgnored, 14, SEV_ERROR
$^   SuccessfulPmidLookup, 15, SEV_INFO
$^   PmidNotFound, 16, SEV_WARNING
$^   NoPmidJournalNotInPubMed, 17, SEV_INFO
$^   PmidNotFoundInPress, 18, SEV_WARNING
$^   NoPmidJournalNotInPubMedInPress, 19, SEV_INFO
$$ PRINT, 2
$^   Failed, 1, SEV_WARNING)";

const string validatr_msg = R"(MODULE validatr
$$ FEATURE, 1
$^   UnknownFeatureKey, 1, SEV_WARNING
$^   MissManQual, 2, SEV_WARNING
Mandatory qualifier missing.
$^   QualWrongThisFeat, 3, SEV_WARNING
$^   FeatureKeyReplaced, 4, SEV_WARNING
$^   LocationParsing, 5, SEV_ERROR
$^   IllegalFormat, 6, SEV_ERROR
$$ QUALIFIER, 2
$^   InvalidDataFormat, 1, SEV_WARNING
$^   Too_many_tokens, 2, SEV_WARNING
$^   MultiValue, 3, SEV_WARNING
$^   UnknownSpelling, 4, SEV_WARNING
$^   Xtratext, 5, SEV_WARNING
$^   SeqPosComma, 6, SEV_ERROR
$^   Pos, 7, SEV_WARNING
$^   EmptyNote, 8, SEV_WARNING
$^   NoteEmbeddedQual, 9, SEV_ERROR
$^   EmbeddedQual, 10, SEV_INFO
$^   AA, 11, SEV_WARNING
$^   Seq, 12, SEV_WARNING
$^   BadECnum, 13, SEV_WARNING
$^   Cons_splice, 14, SEV_WARNING)";

const string flat2asn_msg = R"flat2asn(MODULE flat2asn
$$ FORMAT, 1
$^   NonAsciiChar, 1, SEV_ERROR
# This is a comment REJECT(LANL,EMBL,DDBJ,PIR,SP)
No column of any block of the flat file in any of the formats
is allowed to have a nonASCII character.  NonASCII is defined
as anything greater than  decimal value 126 (`~`) or less than
decimal 32 (space) except decimal 10, which is a newline.
$^   MissingEnd, 2, SEV_ERROR
The // line was not found after sequence was found before a line
with an letter was found.  Likely to be a truncated entry.
$^   MissingField, 3, SEV_ERROR
 Required field was not found in the flat file
$^   LocusLinePosition, 4, SEV_ERROR
The GenBank flat file format requires fixed column positions
for a variety of fields.  The following are the fields and
the columns that are expected:
      bp               31-32  literally, "bp"
      Strand           34-36
          This must be blank or ss-, ds-, ms-};
      Molecule type    37-40
          This must be blank or DNA , RNA , pre-mRNA, mRNA,
             rRNA, tRNA, uRNA, or scRNA.
      Topology          43-52
             Circular, may be specified or it may be blank
             ("Tandem???")
      Division          53-55
          Legal division codes are: PRI, ROD, MAM, VRT, INV, PLN,
          BCT, RNA, VRL, PHG, SYN, UNA, or EST.
        (blank???)
         New or mispelled Embl divisions will be reported with a
         DIVISION_NewDivCode error code.  Note that EMBL uses FUN.
      Date               63-73, and be in dd-mmm-yyyy format.
$^   DirSubMode, 5, SEV_WARNING
Not standard EMBL format, used in DirSubMode only
$^   LineTypeOrder, 6, SEV_WARNING
$^   MissingSequenceData, 7, SEV_ERROR
$^   ContigWithSequenceData, 8
$^   MissingContigFeature, 9, SEV_ERROR
$^   MissingSourceFeature, 10, SEV_ERROR
$^   MultipleCopyright, 11, SEV_WARNING
$^   MissingCopyright, 12, SEV_WARNING
$^   MultiplePatRefs, 13, SEV_ERROR
$^   DuplicateCrossRef, 14, SEV_WARNING
$^   InvalidMolType, 15, SEV_FATAL
$^   Unknown, 16, SEV_WARNING
$^   UnexpectedData, 17, SEV_ERROR
$^   InvalidECNumber, 18, SEV_ERROR
$^   LongECNumber, 19, SEV_WARNING
$^   UnusualECNumber, 20, SEV_WARNING
$^   UnknownDetermineField, 21, SEV_ERROR
$^   UnknownGeneField, 22, SEV_REJECT
$^   ExcessGeneFields, 23, SEV_REJECT
$^   MissingGeneName, 24, SEV_ERROR
$^   InvalidPDBCrossRef, 25, SEV_ERROR
$^   MixedPDBXrefs, 26, SEV_REJECT
$^   IncorrectPROJECT, 27
$^   Date, 28, SEV_REJECT
$^   ECNumberNotPresent, 29, SEV_WARNING
$^   NoProteinNameCategory, 30, SEV_REJECT
$^   MultipleRecName, 31, SEV_REJECT
$^   MissingRecName, 32, SEV_REJECT
$^   SwissProtHasSubName, 33, SEV_REJECT
$^   MissingFullRecName, 34, SEV_REJECT
$^   IncorrectDBLINK, 35
$^   SequenceDataWithAssemblyGap, 36, SEV_REJECT
$^   AssemblyGapWithoutContig, 37, SEV_REJECT
$^   ContigVersusAssemblyGapMissmatch, 38, SEV_REJECT
$^   WrongBioProjectPrefix, 39
$^   InvalidBioProjectAcc, 40, SEV_REJECT
$$ DATACLASS, 2
$^   UnKnownClass, 1, SEV_WARNING
Swissprot error, only standard and preliminary are allowed.
$$ ENTRY, 3
$^   ParsingComplete, 2, SEV_INFO
$^   Begin, 3, SEV_ERROR
Looking for valid begining according to -f arguement in command line:
LOCUS for GenBank; ID for EMBL; ENTRY for PIR;
$^   Skipped, 6, SEV_ERROR
$^   Repeated, 7, SEV_WARNING
$^   LongSequence, 8, SEV_REJECT
$^   THC_Sequence, 9, SEV_WARNING
$^   Parsed, 10, SEV_INFO
$^   ParsingSetup, 11, SEV_INFO
$^   GBBlock_not_Empty, 12, SEV_WARNING
$^   LongHTGSSequence, 13, SEV_WARNING
$^   NumKeywordBlk, 14, SEV_ERROR
No or more than one ENTRY keyword found, making this entry suspect,
perhaps because of data corruption resulting in two records
begin combined in the middle.
$^   Dropped, 15, SEV_ERROR
$^   TSALacksStructuredComment, 16, SEV_WARNING
$^   TSALacksBioProjectLink, 17, SEV_WARNING
$^   TLSLacksStructuredComment, 18, SEV_WARNING
$^   TLSLacksBioProjectLink, 19, SEV_WARNING
$$ COMMENT, 4
$^   NCBI_gi_in, 1, SEV_WARNING
$^   InvalidStructuredComment, 2, SEV_REJECT
$^   SameStructuredCommentTags, 3, SEV_ERROR
$^   StructuredCommentLacksDelim, 4, SEV_ERROR
$$ DATE, 5
$^   NumKeywordBlk, 1, SEV_ERROR
No or more than one DATE keyword found, making this entry suspect,
perhaps because of data corruption resulting in two records
begin combined in the middle.
$^   IllegalDate, 2
Date not in dd-mmm-yyy format
$$ QUALIFIER, 6
$^   MissingTerminalDoubleQuote, 1, SEV_WARNING
$^   UnbalancedQuotes, 2, SEV_ERROR
$^   EmbeddedQual, 3
$^   EmptyQual, 4, SEV_WARNING
Will be reported for all text quals lacking a data value.
All empty qualifiers are ignored.
$^   ShouldNotHaveValue, 5, SEV_WARNING
$^   DbxrefIncorrect, 6, SEV_ERROR
$^   DbxrefShouldBeNumeric, 7, SEV_ERROR
$^   DbxrefUnknownDBName, 8, SEV_ERROR
$^   DbxrefWrongType, 9
$^   DuplicateRemoved, 10, SEV_ERROR
$^   MultRptUnitComma, 11, SEV_WARNING
$^   IllegalCompareQualifier, 12, SEV_ERROR
$^   InvalidEvidence, 13, SEV_ERROR
$^   InvalidException, 14
$^   ObsoleteRptUnit, 15, SEV_ERROR
$^   InvalidRptUnitRange, 16, SEV_ERROR
$^   InvalidPCRprimer, 17, SEV_REJECT
$^   MissingPCRprimerSeq, 18, SEV_REJECT
$^   PCRprimerEmbeddedComma, 19, SEV_REJECT
$^   Conflict, 20, SEV_REJECT
$^   InvalidArtificialLoc, 21, SEV_ERROR
$^   MissingGapType, 22, SEV_REJECT
$^   MissingLinkageEvidence, 23, SEV_REJECT
$^   InvalidGapTypeForLinkageEvidence, 24, SEV_REJECT
$^   InvalidGapType, 25, SEV_REJECT
$^   InvalidLinkageEvidence, 26, SEV_REJECT
$^   MultiplePseudoGeneQuals, 27, SEV_ERROR
$^   InvalidPseudoGeneValue, 28, SEV_ERROR
$^   OldPseudoWithPseudoGene, 29, SEV_ERROR
$^   AntiCodonLacksSequence, 30, SEV_ERROR
$^   UnexpectedGapTypeForHTG, 31, SEV_ERROR
$^   LinkageShouldBeUnspecified, 32
$^   LinkageShouldNotBeUnspecified, 33, SEV_ERROR
$^   InvalidRegulatoryClass, 34, SEV_REJECT
$^   MissingRegulatoryClass, 35, SEV_REJECT
$^   MultipleRegulatoryClass, 36, SEV_REJECT
$^   NoNoteForOtherRegulatory, 37, SEV_REJECT
$^   NoRefForCiteQual, 38, SEV_WARNING
$^   NoTextAfterEqualSign, 39, SEV_INFO
$^   UnknownQualifier, 40, SEV_ERROR
$$ SEQUENCE, 7
$^   UnknownBaseHTG3, 1, SEV_WARNING
$^   SeqLenNotEq, 2, SEV_WARNING
The declared length of the sequence in the record was not
equal to the acutal number of residues found.
$^   BadResidue, 3, SEV_ERROR
Depending upon whether nucleic acid or protein, there are
different single letter legal codes.
$^   BadData, 4, SEV_REJECT
Can't parse the entry because of bad sequence data.
$^   HTGWithoutGaps, 5, SEV_WARNING
HTG raw sequence does not have gaps with size >= 100.
$^   HTGPossibleShortGap, 6, SEV_WARNING
HTG raw sequence has short gaps ( >= 20 but < 100 ) along with
the regular ones ( >= 100 ).
$^   NumKeywordBlk, 7, SEV_ERROR
No or more than one SEQUENCE keyword found, making this entry suspect,
perhaps because of data corruption resulting in two records
begin combined in the middle.
$^   HTGPhaseZeroHasGap, 8, SEV_WARNING
$^   TooShort, 9
$^   AllNs, 10, SEV_REJECT
$^   TooShortIsPatent, 11
$^   HasManyComponents, 12, SEV_INFO
$^   MultipleWGSProjects, 13, SEV_WARNING
$$ SEGMENT, 8
$^   MissSegEntry, 1, SEV_ERROR
The Segmented set (GenBank) that is declared has some
missing members.  That is, if there 6 members declared,
The set might only have:
 1 of 6,
 2 of 6,
 4 of 6,
 5 of 6 and
 6 of 6, and thus be missing 3 of 6.
   Another possible problem is there could be a line:
      "3 of 5", instad of "3 of 6"
$^   DiffMolType, 2, SEV_WARNING
A segmented set is supposed to be from the same molecule,
but with some unknown regions.  It should therefore all be
of the same type of molecule.  For this error to occur, there
have to be different molecule types for different segments
within the set.
$^   BadLocusName, 3, SEV_ERROR
For GenBank in a segmented set, it is an error if the segment number
can not be found at the end of the LOCUS name.  So a LOCUS name
in SEGMENT 2 of 10 must end in 02, as ABCD02.
$^   IncompSeg, 4, SEV_ERROR
There were not three blank-separated token on the SEGMENT line, in
GenBank Flat File format, the segment line has to look like:
SEGMENT     4 of 14
 for example.
$^   PubMatch, 5, SEV_WARNING
There were matching reference with different serial numbers in
segments.
$^   OnlyOneMember, 6, SEV_WARNING
$^   Rejected, 7, SEV_WARNING
$^   GPIDMissingOrNonUnique, 8, SEV_REJECT
$^   DBLinkMissingOrNonUnique, 9, SEV_REJECT
$$ ACCESSION, 9
$^   CannotGetDivForSecondary, 1, SEV_ERROR
$^   BadAccessNum, 2, SEV_ERROR
Accession must be upper case letter followed by 5 digits.
$^   NoAccessNum, 3, SEV_ERROR
No accession number could be found for this entry. The line number
given is only approximate.
$^   MoreAccessLine, 4, SEV_INFO
More than one accession block was found.  Continuation lines with
the wrong format can cause this in GenBank format.
$^   InvalidAccessNum, 5, SEV_WARNING
$^   WGSWithNonWGS_Sec, 6
$^   WGSMasterAsSecondary, 7
$^   UnusualWGS_Secondary, 8
$^   ScfldHasWGSContigSec, 9, SEV_REJECT
$^   WGSPrefixMismatch, 10, SEV_WARNING
$$ LOCUS, 10
$^   WrongTopology, 2, SEV_WARNING
This message occurs when looking for either Circular or 'RNA' or 'DNA' in
embl mode.  Anything other than this will cause this warning.
$^   NoGIBBModMolType, 3, SEV_WARNING
 In the flat files, the only legal values are blank, RNA, pre-mRNA, mRNA,
 rRNA, tRNA, uRNA, ss-RNA, ds-RNA, ms-RNA, scRNA, DNA, ds-DNA, and ss-DNA
$^   MayBeNewSpeciesCode, 4, SEV_WARNING
Swiss-Prot specific error.  The species is not in the list of
LOCUS name prefix-species pairs.
$^   NoSpeciesCode, 5, SEV_WARNING
Swiss-Prot error when no species can be found.
$^   NoMolType, 6, SEV_WARNING
Can't find Molecule type
$^   BadLocusName, 7, SEV_ERROR
There are multiple possible ways to get this error message.
In all formats, this identifier must have either digits or
uppercase letters.
For Swiss-Prot, the rules are more complicated:
Locus name consists of up to 10 uppercase alphanumeric characters
      rule: X_Y format
         X is a mnemonic code, up to 4 alphanumeric characters to represent
             the protein name.
         Y is a mnemonic species identification code of at most 5
             alphanumeric characters to representing the biological source of
             the protein
$^   NoLocusName, 8, SEV_ERROR
No token after 'LOCUS' found in GenBank format.
$^   NonViralRNAMoltype, 9, SEV_ERROR
$$ ORGANISM, 11
$^   NoOrganism, 1, SEV_WARNING
EMBL: no OS line found
GenBank: No ORGANISM line in the SOURCE block.
This message might repeat which trying to guess genetic code.
$^   HybridOrganism, 2, SEV_WARNING
In EMBL format only, can have multiple organisms (OS blocks).
If the taxonomy changes (OC block), this warning is produced.
$^   Unclassified, 3, SEV_WARNING
$^   MissParen, 4, SEV_WARNING
In EMBL format missing parenthesis after common name
$^   UnknownReplace, 5, SEV_INFO
$^   NoSourceFeatMatch, 6, SEV_ERROR
$^   UnclassifiedLineage, 7
$^   TaxIdNotUnique, 10, SEV_ERROR
$^   TaxNameNotFound, 11, SEV_ERROR
$^   TaxIdNotSpecLevel, 12, SEV_WARNING
$^   NewSynonym, 13, SEV_INFO
$^   NoFormalName, 14, SEV_WARNING
$^   Empty, 15, SEV_WARNING
$^   Diff, 16, SEV_ERROR
$^   SynOrgNameNotSYNdivision, 17
$^   LineageLacksMetagenome, 18, SEV_ERROR
$^   OrgNameLacksMetagenome, 19, SEV_ERROR
$$ KEYWORD, 12
$^   MultipleHTGPhases, 1, SEV_ERROR
$^   ESTSubstring, 2, SEV_WARNING
$^   STSSubstring, 3, SEV_WARNING
$^   GSSSubstring, 4, SEV_WARNING
$^   ConflictingKeywords, 5, SEV_REJECT
The mixture of HTG, EST, STS, GSS, WGS, FLI_CDNA, HTC or TPA keywords.
$^   ShouldNotBeTPA, 6, SEV_REJECT
$^   MissingTPA, 7, SEV_REJECT
$^   IllegalForCON, 8, SEV_REJECT
$^   ShouldNotBeCAGE, 9, SEV_REJECT
$^   MissingCAGE, 10, SEV_REJECT
$^   NoGeneExpressionKeywords, 11, SEV_ERROR
$^   ENV_NoMatchingQualifier, 12, SEV_REJECT
$^   ShouldNotBeTSA, 13, SEV_REJECT
$^   MissingTSA, 14, SEV_REJECT
$^   HTGPlusENV, 15, SEV_WARNING
$^   ShouldNotBeTLS, 16, SEV_REJECT
$^   MissingTLS, 17, SEV_REJECT
$$ DIVISION, 13
$^   UnknownDivCode, 1, SEV_REJECT
EMBL only (GenBank format related error is a FORMAT.LocusLinePosition
error), and the legal codes are:
    FUN, INV, MAM, ORG, PHG, PLN, PRI, PRO, ROD, SYN,
    UNA, VRL, VRT, and UNC  (UNA == UNC)
$^   MappedtoEST, 2, SEV_INFO
KW line maps one of these words:
     "EST", "EST PROTO((expressed sequence tag)", "expressed sequence tag",
     "partial cDNA sequence", "transcribed sequence fragment", "TSR",
     "putatively transcribed partial sequence", "UK putts" };
GB-block.div becomes "EST"
$^   MappedtoPAT, 3, SEV_INFO
$^   MappedtoSTS, 4, SEV_WARNING
$^   Mismatch, 5, SEV_WARNING
$^   MissingESTKeywords, 6, SEV_WARNING
$^   MissingSTSKeywords, 7, SEV_WARNING
$^   MissingPatentRef, 8, SEV_REJECT
$^   PATHasESTKeywords, 9, SEV_WARNING
$^   PATHasSTSKeywords, 10, SEV_WARNING
$^   PATHasCDSFeature, 11, SEV_INFO
$^   STSHasCDSFeature, 12, SEV_WARNING
$^   NotMappedtoSTS, 13, SEV_WARNING
$^   ESTHasSTSKeywords, 14, SEV_INFO
$^   ESTHasCDSFeature, 15, SEV_WARNING
$^   NotMappedtoEST, 16, SEV_WARNING
$^   ShouldBeHTG, 17, SEV_ERROR
$^   MissingGSSKeywords, 18, SEV_INFO
$^   GSSHasCDSFeature, 19, SEV_WARNING
$^   NotMappedtoGSS, 20, SEV_WARNING
$^   MappedtoGSS, 21, SEV_WARNING
$^   PATHasGSSKeywords, 22, SEV_WARNING
$^   LongESTSequence, 23, SEV_WARNING
$^   LongSTSSequence, 24, SEV_WARNING
$^   LongGSSSequence, 25, SEV_WARNING
$^   GBBlockDivision, 26, SEV_WARNING
$^   MappedtoCON, 27, SEV_WARNING
$^   MissingHTGKeywords, 28, SEV_ERROR
$^   ShouldNotBeHTG, 29, SEV_WARNING
$^   ConDivInSegset, 30, SEV_ERROR
$^   ConDivLacksContig, 31, SEV_WARNING
$^   MissingHTCKeyword, 32, SEV_ERROR
$^   InvalidHTCKeyword, 33, SEV_ERROR
$^   HTCWrongMolType, 34, SEV_ERROR
$^   ShouldBePAT, 35, SEV_WARNING
$^   BadTPADivcode, 36, SEV_REJECT
$^   ShouldBeENV, 37
$^   TGNnotTransgenic, 38, SEV_ERROR
$^   TransgenicNotSYN_TGN, 39, SEV_REJECT
$^   BadTSADivcode, 40, SEV_REJECT
$^   NotPatentedSeqId, 41, SEV_REJECT
$$ DEFINITION, 15
$^   HTGNotInProgress, 1, SEV_WARNING
$^   DifferingRnaTokens, 2, SEV_WARNING
$^   HTGShouldBeComplete, 3, SEV_ERROR
$^   ShouldNotBeTPA, 4, SEV_REJECT
$^   MissingTPA, 5, SEV_REJECT
$^   ShouldNotBeTSA, 6, SEV_REJECT
$^   MissingTSA, 7, SEV_REJECT
$^   ShouldNotBeTLS, 8, SEV_REJECT
$^   MissingTLS, 9, SEV_REJECT
)flat2asn"
// need to split long string here to avoid 16K limit on Windows
R"flat2asn($$ REFERENCE, 16
$^   IllegPageRange, 3, SEV_WARNING
There are many classes of problems that can give this error message, some
of which are really warnings to have a human take a closer look.
$^   UnkRefRcToken, 4, SEV_WARNING
There are a limited number of valid Swiss-Prot Reference Comment
   (RC line) tokens.  This one is not one.
$^   UnkRefSubType, 5, SEV_WARNING
Illegal Swiss-prot Reference SubType.
$^   IllegalFormat, 6, SEV_WARNING
$^   IllegalAuthorName, 7, SEV_WARNING
$^   YearEquZero, 8, SEV_WARNING
$^   IllegalDate, 9
$^   Patent, 10, SEV_WARNING
$^   Thesis, 12, SEV_WARNING
$^   Book, 14, SEV_WARNING
$^   NoContactInfo, 15
$^   Illegalreference, 16, SEV_ERROR
$^   Fail_to_parse, 17
$^   No_references, 18, SEV_ERROR
$^   Xtratext, 19, SEV_WARNING
$^   InvalidInPress, 22, SEV_WARNING
$^   EtAlInAuthors, 24, SEV_WARNING
$^   UnusualPageNumber, 25, SEV_WARNING
$^   LargePageRange, 26, SEV_WARNING
  Total pages more than ....   means that the total number in the
                               article is greater than normal.  This
                               is usually caused by a typographical error.
$^   InvertPageRange, 27, SEV_WARNING
  Page number may invert . . .  it looks like the first page is greater
                               that the last page.
$^   SingleTokenPageRange, 28
$^   MissingBookPages, 29, SEV_WARNING
$^   MissingBookAuthors, 30, SEV_WARNING
$^   DateCheck, 31, SEV_WARNING
$^   GsdbRefDropped, 32, SEV_WARNING
$^   UnusualBookFormat, 33, SEV_ERROR
$^   ImpendingYear, 34, SEV_WARNING
$^   YearPrecedes1950, 35, SEV_WARNING
$^   YearPrecedes1900, 36, SEV_ERROR
$^   NumKeywordBlk, 37, SEV_ERROR
No REFERENCE block found.
$^   UnparsableLocation, 38, SEV_REJECT
$^   LongAuthorName, 39
$^   MissingAuthors, 40
$^   InvalidPmid, 41, SEV_REJECT
$^   InvalidMuid, 42, SEV_REJECT
$^   CitArtLacksPmid, 43, SEV_REJECT
$^   DifferentPmids, 44, SEV_REJECT
$^   MuidPmidMissMatch, 45, SEV_ERROR
$^   MultipleIdentifiers, 46, SEV_ERROR
$^   MuidIgnored, 47, SEV_WARNING
$^   PmidIgnored, 48, SEV_WARNING
$^   ArticleIdDiscarded, 49, SEV_ERROR
$^   UnusualPubStatus, 50, SEV_WARNING
$^   AuthorNameCrossCheckProblem, 51, SEV_WARNING
$$ FEATURE, 17
$^   MultFocusedFeats, 1, SEV_ERROR
$^   ExpectEmptyComment, 6, SEV_WARNING
specific Swiss-Prot message for INIT_MET feature
$^   DiscardData, 7, SEV_WARNING
includes unbalance double quote
$^   InValidEndPoint, 9, SEV_WARNING
$^   MissManQual, 10
$^   NoFeatData, 11, SEV_WARNING
$^   NoFragment, 12, SEV_WARNING
$^   NotSeqEndPoint, 13, SEV_WARNING
$^   OldNonExp, 15, SEV_WARNING
$^   PartialNoNonTer, 16, SEV_WARNING
$^   Pos, 17, SEV_WARNING
$^   TooManyInitMet, 20, SEV_WARNING
Specific Swiss-Prot error message.
$^   UnEqualEndPoint, 22, SEV_WARNING
$^   UnknownFeatKey, 23, SEV_WARNING
$^   UnknownQualSpelling, 24, SEV_WARNING
$^   LocationParsing, 30
$^   FeatureKeyReplaced, 32, SEV_WARNING
$^   Dropped, 33
$^   UnknownDBName, 36, SEV_WARNING
$^   Duplicated, 37, SEV_WARNING
$^   NoSource, 38, SEV_WARNING
$^   MultipleSource, 39, SEV_WARNING
$^   ObsoleteFeature, 40, SEV_ERROR
$^   UnparsableLocation, 41, SEV_ERROR
$^   BadAnticodonLoc, 42, SEV_ERROR
$^   CDSNotFound, 43, SEV_WARNING
$^   CannotMapDnaLocToAALoc, 44, SEV_ERROR
$^   BadLocation, 45
$^   BadOrgRefFeatOnBackbone, 46, SEV_WARNING
$^   DuplicateRemoved, 47, SEV_WARNING
$^   FourBaseAntiCodon, 48, SEV_WARNING
$^   StrangeAntiCodonSize, 49, SEV_ERROR
$^   MultipleLocusTags, 50, SEV_REJECT
$^   InconsistentLocusTagAndGene, 51
$^   MultipleOperonQuals, 52, SEV_REJECT
$^   MissingOperonQual, 53, SEV_REJECT
$^   OperonQualsNotUnique, 54
$^   InvalidOperonQual, 55, SEV_REJECT
$^   OperonLocationMisMatch, 56
$^   ObsoleteDbXref, 57, SEV_WARNING
$^   EmptyOldLocusTag, 58, SEV_ERROR
$^   RedundantOldLocusTag, 59, SEV_ERROR
$^   OldLocusTagWithoutNew, 60, SEV_REJECT
$^   MatchingOldNewLocusTag, 61, SEV_REJECT
$^   InvalidGapLocation, 62
$^   OverlappingGaps, 63, SEV_REJECT
$^   ContiguousGaps, 64
$^   NsAbutGap, 65, SEV_WARNING
$^   AllNsBetweenGaps, 66, SEV_ERROR
$^   InvalidGapSequence, 67, SEV_REJECT
$^   RequiredQualifierMissing, 68
$^   IllegalEstimatedLength, 69, SEV_REJECT
$^   GapSizeEstLengthMissMatch, 70
$^   UnknownGapNot100, 71, SEV_ERROR
$^   MoreThanOneCAGEFeat, 72, SEV_REJECT
$^   Invalid_INIT_MET, 73, SEV_ERROR
$^   INIT_MET_insert, 74, SEV_WARNING
$^   MissingInitMet, 75, SEV_ERROR
$^   ncRNA_class, 76, SEV_REJECT
$^   InvalidSatelliteType, 77, SEV_REJECT
$^   NoSatelliteClassOrIdentifier, 78, SEV_REJECT
$^   PartialNoNonTerNonCons, 79, SEV_WARNING
$^   AssemblyGapAndLegacyGap, 80, SEV_REJECT
$^   InvalidAssemblyGapLocation, 81, SEV_REJECT
$^   InvalidQualifier, 82
$^   MultipleGenesDifferentLocusTags, 83
$^   InvalidAnticodonPos, 84, SEV_ERROR
$^   InconsistentPseudogene, 85, SEV_ERROR
$^   MultipleWBGeneXrefs, 86, SEV_WARNING
$^   FinishedHTGHasAssemblyGap, 87, SEV_ERROR
$^   MultipleOldLocusTags, 88, SEV_WARNING
$^   InvalidQualifierValue, 89
$$ LOCATION, 18
$^   FailedCheck, 1, SEV_WARNING
$^   MixedStrand, 2, SEV_WARNING
$^   PeptideFeatOutOfFrame, 3, SEV_ERROR
$^   AccessionNotTPA, 4, SEV_REJECT
$^   ContigHasNull, 5, SEV_REJECT
$^   TransSpliceMixedStrand, 6, SEV_INFO
$^   AccessionNotTSA, 7, SEV_REJECT
$^   SeqIdProblem, 8, SEV_REJECT
$^   TpaAndNonTpa, 9, SEV_REJECT
$^   CrossDatabaseFeatLoc, 10
$^   RefersToExternalRecord, 11, SEV_WARNING
$^   NCBIRefersToExternalRecord, 12, SEV_WARNING
$^   ContigAndScaffold, 13, SEV_ERROR
$^   AccessionNotTLS, 14, SEV_REJECT
$$ GENENAME, 19
$^   IllegalGeneName, 1, SEV_WARNING
$^   DELineGeneName, 2, SEV_WARNING
$$ BIOSEQSETCLASS, 20
$^   NewClass, 1, SEV_INFO
$$ CDREGION, 21
$^   MissingStopCodon, 1, SEV_ERROR
$^   InternalStopCodonFound, 2, SEV_ERROR
$^   NoProteinSeq, 6, SEV_WARNING
$^   TerminalStopCodonMissing, 7
$^   TranslationDiff, 8
$^   TranslationsAgree, 9, SEV_INFO
$^   IllegalStart, 10
This error is caused by the start of translation being an unrecognized
initiation codon when the CDS is not recognized as partial.  Usually,
one adds '<' or '>' on the locations to accurately reflect the biology
and remove this error.  On rare occasions, a /partial should be added.
$^   GeneticCodeDiff, 11
Genetic code returned by Taxonomy server is different from /transl_table
$^   UnevenLocation, 12, SEV_WARNING
$^   ShortProtein, 13, SEV_WARNING
$^   GeneticCodeAssumed, 14
$^   NoTranslationCompare, 15, SEV_WARNING
$^   TranslationAdded, 16, SEV_INFO
$^   InvalidGcodeTable, 17, SEV_WARNING
$^   ConvertToImpFeat, 18, SEV_ERROR
$^   BadLocForTranslation, 19, SEV_REJECT
$^   LocationLength, 20, SEV_WARNING
$^   TranslationOverride, 21, SEV_WARNING
$^   InvalidDb_xref, 24, SEV_ERROR
$^   Multiple_PID, 28, SEV_WARNING
$^   TooBad, 29, SEV_ERROR
$^   MissingProteinId, 30, SEV_FATAL
$^   MissingProteinVersion, 31, SEV_FATAL
$^   IncorrectProteinVersion, 32, SEV_FATAL
$^   IncorrectProteinAccession, 33
$^   MissingCodonStart, 34
$^   MissingTranslation, 35
$^   PseudoWithTranslation, 36, SEV_ERROR
$^   UnexpectedProteinId, 37, SEV_ERROR
$^   NCBI_gi_in, 38, SEV_WARNING
$^   StopCodonOnly, 39
$^   StopCodonBadInterval, 40
$^   ProteinLenDiff, 41, SEV_ERROR
$^   SuppliedProteinUsed, 42, SEV_WARNING
$^   IllegalException, 43, SEV_ERROR
$^   BadTermStopException, 44, SEV_ERROR
$^   BadCodonQualFormat, 45, SEV_REJECT
$^   InvalidCodonQual, 46, SEV_REJECT
$^   CodonQualifierUsed, 47, SEV_ERROR
$^   UnneededCodonQual, 48, SEV_ERROR
$$ GENEREF, 22
$^   GeneIntervalOverlap, 1, SEV_WARNING
$^   NoUniqMaploc, 2, SEV_WARNING
$^   BothStrands, 3, SEV_WARNING
$^   CircularHeuristicFit, 4, SEV_INFO
$^   CircularHeuristicDoesNotFit, 5, SEV_INFO
$$ PROTREF, 23
$^   NoNameForProtein, 1, SEV_WARNING
$$ SEQID, 24
$^   NoSeqId, 1, SEV_ERROR
$$ SERVER, 26
$^   NotUsed, 1, SEV_WARNING
No protein translation sequence or organism name has been checked;
Could not guess the genetic code, standard code used.
$^   Failed, 2
Call for Taxonomy or/and Medline service failed.
$^   NoLineageFromTaxon, 3
$^   GcFromSuppliedLineage, 6, SEV_WARNING
$^   TaxNameWasFound, 7, SEV_INFO
$^   TaxServerDown, 8, SEV_FATAL
$^   NoTaxLookup, 9, SEV_WARNING
$^   NoPubMedLookup, 10, SEV_WARNING
$$ NCBI_GI, 27
$^   BadDataFormat, 1, SEV_ERROR
$$ SPROT, 28
$^   DRLine, 1
$^   PELine, 2, SEV_ERROR
$^   DRLineCrossDBProtein, 3, SEV_WARNING
$$ SOURCE, 29
$^   InvalidCountry, 1, SEV_ERROR
$^   OrganelleQualMultToks, 2, SEV_ERROR
$^   OrganelleIllegalClass, 3, SEV_ERROR
$^   GenomicViralRnaAssumed, 4, SEV_WARNING
$^   UnclassifiedViralRna, 5, SEV_ERROR
$^   LineageImpliesGenomicViralRna, 6, SEV_WARNING
$^   InvalidDbXref, 7, SEV_ERROR
$^   FeatureMissing, 8, SEV_REJECT
$^   InvalidLocation, 9, SEV_REJECT
$^   BadLocation, 10, SEV_REJECT
$^   NoOrganismQual, 11, SEV_REJECT
$^   IncompleteCoverage, 12, SEV_REJECT
$^   ExcessSpanning, 13, SEV_REJECT
$^   FocusQualNotNeeded, 14, SEV_REJECT
$^   MultipleOrganismWithFocus, 15, SEV_REJECT
$^   FocusQualMissing, 16
$^   MultiOrgOverlap, 17, SEV_REJECT
$^   UnusualLocation, 18, SEV_ERROR
$^   OrganismIncomplete, 19, SEV_REJECT
$^   UnwantedQualifiers, 20, SEV_WARNING
$^   ManySourceFeats, 21, SEV_WARNING
$^   MissingSourceFeatureForDescr, 22, SEV_ERROR
$^   FocusAndTransposonNotAllowed, 23, SEV_REJECT
$^   FocusQualNotFullLength, 24, SEV_REJECT
$^   UnusualOrgName, 25, SEV_WARNING
$^   QualUnknown, 26, SEV_INFO
$^   QualDiffValues, 27, SEV_WARNING
$^   IllegalQual, 28, SEV_WARNING
$^   NotFound, 29, SEV_WARNING
$^   GeneticCode, 30, SEV_WARNING
$^   TransgenicTooShort, 31, SEV_REJECT
$^   FocusAndTransgenicQuals, 32, SEV_REJECT
$^   MultipleTransgenicQuals, 33, SEV_REJECT
$^   ExcessCoverage, 34, SEV_REJECT
$^   TransSingleOrgName, 35
$^   TransOrgnameNotUnique, 36, SEV_REJECT
$^   PartialLocation, 37, SEV_ERROR
$^   PartialQualifier, 38, SEV_ERROR
$^   SingleSourceTooShort, 39, SEV_WARNING
$^   InconsistentMolType, 40, SEV_REJECT
$^   MultipleMolTypes, 41, SEV_REJECT
$^   InvalidMolType, 42, SEV_REJECT
$^   MolTypesDisagree, 43
$^   MolTypeSeqTypeConflict, 44, SEV_ERROR
$^   MissingMolType, 45, SEV_ERROR
$^   UnknownOXType, 46, SEV_ERROR
$^   InvalidNcbiTaxID, 47, SEV_ERROR
$^   NoNcbiTaxIDLookup, 48, SEV_WARNING
$^   NcbiTaxIDLookupFailure, 49, SEV_ERROR
$^   ConflictingGenomes, 50, SEV_ERROR
$^   MultipleOXTaxID, 51, SEV_WARNING
$^   OrgNameVsTaxIDMissMatch, 52, SEV_ERROR
$^   InconsistentEnvSampQual, 53, SEV_REJECT
$^   MissingEnvSampQual, 54
$^   MissingPlasmidName, 55, SEV_ERROR
$^   UnknownOHType, 56, SEV_ERROR
$^   IncorrectOHLine, 57, SEV_ERROR
$^   HostNameVsTaxIDMissMatch, 58, SEV_WARNING
$^   ObsoleteDbXref, 59, SEV_WARNING
$^   InvalidCollectionDate, 60, SEV_ERROR
$^   FormerCountry, 61, SEV_WARNING
$^   MultipleSubmitterSeqids, 62, SEV_REJECT
$^   DifferentSubmitterSeqids, 63, SEV_REJECT
$^   LackingSubmitterSeqids, 64, SEV_REJECT
$^   SubmitterSeqidNotAllowed, 65, SEV_REJECT
$^   SubmitterSeqidDropped, 66, SEV_ERROR
$^   SubmitterSeqidIgnored, 67, SEV_ERROR
$^   CountryWithGeoLoc, 68, SEV_WARNING
$^   InvalidGeoLocName, 69, SEV_ERROR
$^   FormerGeoLocName, 70, SEV_WARNING
$^   DescriptorDropped, 71, SEV_WARNING
$$ QSCORE, 30
$^   MissingByteStore, 1, SEV_ERROR
$^   NonLiteralDelta, 2, SEV_ERROR
$^   UnknownDelta, 3, SEV_ERROR
$^   EmptyLiteral, 4, SEV_ERROR
$^   ZeroLengthLiteral, 5, SEV_ERROR
$^   MemAlloc, 6, SEV_ERROR
$^   NonZeroInGap, 7, SEV_ERROR
$^   InvalidArgs, 8, SEV_ERROR
$^   BadBioseqLen, 9, SEV_ERROR
$^   BadBioseqId, 10, SEV_ERROR
$^   BadQscoreRead, 11, SEV_ERROR
$^   BadDefline, 12, SEV_ERROR
$^   NoAccession, 13, SEV_ERROR
$^   NoSeqVer, 14, SEV_ERROR
$^   NoTitle, 15, SEV_ERROR
$^   BadLength, 16, SEV_ERROR
$^   BadMinMax, 17, SEV_ERROR
$^   BadScoreLine, 18, SEV_ERROR
$^   ScoresVsLen, 19, SEV_ERROR
$^   ScoresVsBspLen, 20, SEV_ERROR
$^   BadMax, 21, SEV_ERROR
$^   BadMin, 22, SEV_ERROR
$^   BadTitle, 23, SEV_ERROR
$^   OutOfScores, 24, SEV_ERROR
$^   NonByteGraph, 25, SEV_ERROR
$^   FailedToParse, 26, SEV_ERROR
$^   DoubleSlash, 27, SEV_WARNING
$$ TITLE, 31
$^   NumKeywordBlk, 1, SEV_ERROR
No or more than one TITLE keyword found, making this entry suspect,
perhaps because of data corruption resulting in two records
begin combined in the middle.
$$ SUMMARY, 32
$^   NumKeywordBlk, 1, SEV_ERROR
No or more than one SUMMARY keyword found, making this entry suspect,
perhaps because of data corruption resulting in two records
begin combined in the middle.
$$ TPA, 33
$^   InvalidPrimarySpan, 1, SEV_REJECT
$^   InvalidPrimarySeqId, 2, SEV_REJECT
$^   InvalidPrimaryBlock, 3, SEV_REJECT
$^   IncompleteCoverage, 4, SEV_ERROR
$^   SpanLengthDiff, 5, SEV_ERROR
$^   SpanDiffOver300bp, 6, SEV_ERROR
$^   TpaSpansMissing, 7, SEV_REJECT
$^   TpaCommentMissing, 8, SEV_REJECT
$$ DRXREF, 34
$^   UnknownDBname, 1, SEV_WARNING
$^   InvalidBioSample, 2, SEV_REJECT
$^   DuplicatedBioSamples, 3, SEV_WARNING
$^   InvalidSRA, 4, SEV_REJECT
$^   DuplicatedSRA, 5, SEV_WARNING
$^   PMIDNotFoundInPubMed, 6, SEV_ERROR
$^   PMIDsNotProcessed, 7, SEV_WARNING
$$ TSA, 35
$^   InvalidPrimarySpan, 1, SEV_REJECT
$^   InvalidPrimarySeqId, 2, SEV_REJECT
$^   InvalidPrimaryBlock, 3, SEV_REJECT
$^   IncompleteCoverage, 4, SEV_ERROR
$^   SpanLengthDiff, 5, SEV_ERROR
$^   SpanDiffOver300bp, 6, SEV_ERROR
$^   UnexpectedPrimaryAccession, 7, SEV_WARNING
$$ DBLINK, 36
$^   InvalidIdentifier, 1, SEV_WARNING
$^   DuplicateIdentifierRemoved, 2, SEV_WARNING)flat2asn";
