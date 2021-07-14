#ifndef OBJTOOLS_READERS___FASTA__HPP
#define OBJTOOLS_READERS___FASTA__HPP

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
* Authors:  Aaron Ucko, NCBI
*
*/

/// @file fasta.hpp
/// Interfaces for reading and scanning FASTA-format sequences.

#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbi_param.hpp>
#include <util/line_reader.hpp>
#include <util/static_map.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Linkage_evidence.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/source_mod_parser.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objtools/readers/fasta_reader_utils.hpp>
#include <objtools/readers/mod_reader.hpp>

#include <stack>
#include <sstream>

/** @addtogroup Miscellaneous
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_loc;

//class CSeqIdGenerator;

class ILineErrorListener;
class CSourceModParser;

/// Base class for reading FASTA sequences.
///
/// This class should suffice as for typical usage, but its
/// implementation has been broken up into virtual functions, allowing
/// for a wide range of specialized subclasses.
///
/// @sa CFastaOstream
class NCBI_XOBJREAD_EXPORT CFastaReader : public CReaderBase {
public:
    /// Note on fAllSeqIds: some databases (notably nr) have merged
    /// identical sequences, joining their deflines together with
    /// control-As.  Normally, the reader stops at the first
    /// control-A; however, this flag makes it parse all the IDs.
    enum EFlags {
        fAssumeNuc            = 1<< 0, ///< Assume nucs unless accns indicate otherwise
        fAssumeProt           = 1<< 1, ///< Assume prots unless accns indicate otherwise
        fForceType            = 1<< 2, ///< Force specified type regardless of accession
        fNoParseID            = 1<< 3, ///< Generate an ID (whole defline -&gt; title)
        fParseGaps            = 1<< 4, ///< Make a delta sequence if gaps found
        fOneSeq               = 1<< 5, ///< Just read the first sequence found
        fAllSeqIds NCBI_STD_DEPRECATED("This flag is no longer used in CFastaReader.")   = 1<< 6, ///< Read Seq-ids past the first ^A (see note)
        fNoSeqData            = 1<< 7, ///< Parse the deflines but skip the data
        fRequireID            = 1<< 8, ///< Reject deflines that lack IDs
        fDLOptional           = 1<< 9, ///< Don't require a leading defline
        fParseRawID           = 1<<10, ///< Try to identify raw accessions
        fSkipCheck            = 1<<11, ///< Skip (rudimentary) body content check
        fNoSplit              = 1<<12, ///< Don't split out ambiguous sequence regions
        fValidate             = 1<<13, ///< Check (alphabetic) residue validity
        fUniqueIDs            = 1<<14, ///< Forbid duplicate IDs
        fStrictGuess          = 1<<15, ///< Assume no typos when guessing sequence type
        fLaxGuess             = 1<<16, ///< Use legacy heuristic for guessing seq. type
        fAddMods              = 1<<17, ///< Parse defline mods and add to SeqEntry
        fLetterGaps           = 1<<18, ///< Parse runs of Ns when splitting data
        fNoUserObjs           = 1<<19, ///< Don't save raw deflines in User-objects
        fBadModThrow  NCBI_STD_DEPRECATED("This flag is redundant as CFastaReader no longer utilizes CSourceModParser.")    = 1<<20,
        fUnknModThrow NCBI_STD_DEPRECATED("This flag is redundant as CFastaReader no longer utilizes CSourceModParser.")    = 1<<21,
        fLeaveAsText          = 1<<22, ///< Don't reencode at all, just parse
        fQuickIDCheck         = 1<<23, ///< Just check local IDs' first characters
        fUseIupacaa           = 1<<24, ///< If Prot, use iupacaa instead of the default ncbieaa.
        fHyphensIgnoreAndWarn = 1<<25, ///< When a hyphen is encountered in seq data, ignore it but warn.
        fDisableNoResidues    = 1<<26, ///< If no residues found do not raise an error
        fDisableParseRange    = 1<<27, ///< No ranges in seq-ids.  Ranges part of seq-id instead.
        fIgnoreMods           = 1<<28  ///< Ignore mods entirely. Incompatible with fAddMods.
    };
    using TFlags = long; ///< binary OR of EFlags
    static void AddStringFlags(
        const list<string>& stringFlags,
        TFlags& baseFlags);

    using SDefLineParseInfo = CFastaDeflineReader::SDeflineParseInfo;
    using FIdCheck = CFastaDeflineReader::FIdCheck;
    using FModFilter = function<bool(const CTempString& mod_name)>;


    CFastaReader(ILineReader& reader, TFlags flags = 0, FIdCheck f_idcheck = CSeqIdCheck());
    CFastaReader(CNcbiIstream& in,    TFlags flags = 0, FIdCheck f_idcheck = CSeqIdCheck());
    CFastaReader(const string& path,  TFlags flags = 0, FIdCheck f_idcheck = CSeqIdCheck());
    CFastaReader(CReaderBase::TReaderFlags fBaseFlags,
                 TFlags flags = 0,
                 FIdCheck f_idcheck = CSeqIdCheck()
                 );
    virtual ~CFastaReader(void);

    /// CReaderBase overrides
    virtual CRef<CSerialObject> ReadObject   (ILineReader &lr, ILineErrorListener *pErrors);
    virtual CRef<CSeq_entry>    ReadSeqEntry (ILineReader &lr, ILineErrorListener *pErrors);

    /// Indicates (negatively) whether there is any more input.
    bool AtEOF(void) const { return m_LineReader->AtEOF(); }

    /// Read a single effective sequence, which may turn out to be a
    /// segmented set.
    virtual CRef<CSeq_entry> ReadOneSeq(ILineErrorListener * pMessageListener = 0);

    /// Read multiple sequences (by default, as many as are available.)
    CRef<CSeq_entry> ReadSet(int max_seqs = kMax_Int, ILineErrorListener * pMessageListener = 0);

    /// Read as many sequences as are available, and interpret them as
    /// an alignment, with hyphens marking relative deletions.
    /// @param reference_row
    ///   0-based; the special value -1 yields a full (pseudo-?)N-way alignment.
    CRef<CSeq_entry> ReadAlignedSet(int reference_row, ILineErrorListener * pMessageListener = 0);

    // also allow changing?
    TFlags GetFlags(void) const { return m_Flags.top(); }

    typedef CRef<CSeq_loc> TMask;
    typedef vector<TMask>  TMasks;

    /// Directs the *following* call to ReadOneSeq to note the locations
    /// of lowercase letters.
    /// @return
    ///     A smart pointer to the Seq-loc that will be populated.
    CRef<CSeq_loc> SaveMask(void);

    void SaveMasks(TMasks* masks) { m_MaskVec = masks; }

    const CBioseq::TId& GetIDs(void) const    { return m_CurrentSeq->GetId(); }
    const CSeq_id&      GetBestID(void) const { return *m_BestID; }

    const CSeqIdGenerator& GetIDGenerator(void) const { return  m_IDHandler->GetGenerator(); }
    CSeqIdGenerator&       SetIDGenerator(void)       { return  m_IDHandler->SetGenerator(); }
    void                   SetIDGenerator(CSeqIdGenerator& gen);

    /// Re-allow previously seen IDs even if fUniqueIds is on.
    //void ResetIDTracker(void) { m_IDTracker.clear(); }
    void ResetIDTracker(void) { m_IDHandler->ClearIdCache(); }

    void SetExcludedMods(const vector<string>& excluded_mods);

    /// If this is set, an exception will be thrown if a Sequence ID exceeds the
    /// given length. Overrides the id lengths specified in class CSeq_id.
    /// @param max_len
    ///   The new maximum to set.  Of course, you can set it to kMax_UI4
    ///   to effectively have no limit.
    void SetMaxIDLength(Uint4 max_len);

    /// Get the maximum ID allowed, which will be kMax_UI4
    /// unless someone has manually lowered it.
    ///
    /// @returns currently set maximum ID length
    Uint4 GetMaxIDLength(void) const { return m_MaxIDLength; }

    // Make case-sensitive and other kinds of insensitivity, too
    // (such as "spaces" and "underscores" becoming "hyphens"
    static string CanonicalizeString(const CTempString & sValue);

    void SetMinGaps(TSeqPos gapNmin, TSeqPos gap_Unknown_length);

    void SetGapLinkageEvidence(
            CSeq_gap::EType type,
            const set<int>& defaultEvidence,
            const map<TSeqPos, set<int>>& countToEvidenceMap);

    void SetGapLinkageEvidences(
        CSeq_gap::EType type,
        const set<int>& evidences);

    void IgnoreProblem(ILineError::EProblem problem);

    using SLineTextAndLoc = CFastaDeflineReader::SLineTextAndLoc;

    using TIgnoredProblems = vector<ILineError::EProblem>;
    using TSeqTitles = vector<SLineTextAndLoc>;
    typedef CTempString TStr;

    static void ParseDefLine(const TStr& defLine,
        const SDefLineParseInfo& info,
        const TIgnoredProblems& ignoredErrors,
        list<CRef<CSeq_id>>& ids,
        bool& hasRange,
        TSeqPos& rangeStart,
        TSeqPos& rangeEnd,
        TSeqTitles& seqTitles,
        ILineErrorListener* pMessageListener);

protected:
    enum EInternalFlags {
        fAligning = 0x40000000,
        fInSegSet = 0x20000000
    };


    virtual void ParseDefLine (const TStr& s, ILineErrorListener * pMessageListener);

    virtual void PostProcessIDs(const CBioseq::TId& defline_ids,
        const string& defline,
        bool has_range=false,
        TSeqPos range_start=kInvalidSeqPos,
        TSeqPos range_end=kInvalidSeqPos);

    static size_t ParseRange    (const TStr& s, TSeqPos& start, TSeqPos& end, ILineErrorListener * pMessageListener);
    virtual void   ParseTitle    (const SLineTextAndLoc & lineInfo, ILineErrorListener * pMessageListener);
    virtual bool   IsValidLocalID(const TStr& s) const;
    static bool IsValidLocalID(const TStr& idString, TFlags fFastaFlags);
    virtual void   GenerateID    (void);
    virtual void   ParseDataLine (const TStr& s, ILineErrorListener * pMessageListener);
    virtual void   CheckDataLine (const TStr& s, ILineErrorListener * pMessageListener);
    virtual void   x_CloseGap    (TSeqPos len, bool atStartOfLine, ILineErrorListener * pMessageListener);
    virtual void   x_OpenMask    (void);
    virtual void   x_CloseMask   (void);
    virtual bool   ParseGapLine  (const TStr& s, ILineErrorListener * pMessageListener);
    virtual void   AssembleSeq   (ILineErrorListener * pMessageListener);
    virtual void   AssignMolType (ILineErrorListener * pMessageListener);
    virtual bool   CreateWarningsForSeqDataInTitle(
        const TStr& sLineText,
        TSeqPos iLineNum,
        ILineErrorListener * pMessageListener) const;
    virtual void   PostWarning(ILineErrorListener * pMessageListener,
            EDiagSev _eSeverity, size_t _uLineNum, CTempString _MessageStrmOps,
            CObjReaderParseException::EErrCode _eErrCode,
            ILineError::EProblem _eProblem,
            CTempString _sFeature, CTempString _sQualName, CTempString _sQualValue) const;

    typedef int                         TRowNum;
    typedef map<TRowNum, TSignedSeqPos> TSubMap;
    // align coord -> row -> seq coord
    typedef map<TSeqPos, TSubMap>       TStartsMap;
    typedef vector<CRef<CSeq_id> >      TIds;

    CRef<CSeq_entry> x_ReadSeqsToAlign(TIds& ids, ILineErrorListener * pMessageListener);
    void             x_AddPairwiseAlignments(CSeq_annot& annot, const TIds& ids,
                                             TRowNum reference_row);
    void             x_AddMultiwayAlignment(CSeq_annot& annot, const TIds& ids);

    // inline utilities
    void CloseGap(bool atStartOfLine=true, ILineErrorListener * pMessageListener = 0) {
        if (m_CurrentGapLength > 0) {
            x_CloseGap(m_CurrentGapLength, atStartOfLine, pMessageListener);
            m_CurrentGapLength = 0;
        }
    }
    void OpenMask(void);
    void CloseMask(void)
        { if (m_MaskRangeStart != kInvalidSeqPos) { x_CloseMask(); } }
    Int8 StreamPosition(void) const
        { return NcbiStreamposToInt8(m_LineReader->GetPosition()); }
    unsigned int LineNumber(void) const
        { return m_LineReader->GetLineNumber(); }

    ILineReader&  GetLineReader(void)         { return *m_LineReader; }
    bool          TestFlag(EFlags flag) const
        { return (GetFlags() & flag) != 0; }
    bool          TestFlag(EInternalFlags flag) const
        { return (GetFlags() & flag) != 0; }
    CBioseq::TId& SetIDs(void)                { return m_CurrentSeq->SetId(); }

    // NB: Not necessarily fully-formed!
    CBioseq& SetCurrentSeq(void)              { return *m_CurrentSeq; }

    enum EPosType {
        eRawPos,
        ePosWithGaps,
        ePosWithGapsAndSegs
    };
    TSeqPos GetCurrentPos(EPosType pos_type);

    std::string x_NucOrProt(void) const;

private:
    CModHandler m_ModHandler;

    void x_ApplyMods(const string& title,
                     TSeqPos line_number,
                     CBioseq& bioseq,
                     ILineErrorListener* pMessageListener);

    void x_SetDeflineParseInfo(SDefLineParseInfo& info);

    bool m_bModifiedMaxIdLength=false;

protected:
    struct SGap : public CObject {
        enum EKnownSize {
            eKnownSize_No,
            eKnownSize_Yes
        };

        // This lets us represent a "not-set" state for CSeq_gap::EType
        // as an unset pointer.  This is needed because CSeq_gap::EType doesn't
        // have a "not_set" enum value at this time.
        typedef CObjectFor<CSeq_gap::EType> TGapTypeObj;
        typedef CConstRef<TGapTypeObj> TNullableGapType;

        SGap(
            TSeqPos pos,
            TSignedSeqPos len, // signed so we can catch negative numbers and throw an exception
            EKnownSize eKnownSize,
            TSeqPos uLineNumber,
            TNullableGapType pGapType = TNullableGapType(),
            const set<CLinkage_evidence::EType>& setOfLinkageEvidence =
                set<CLinkage_evidence::EType>());
        // immutable once created

        // 0-based, and NOT counting previous gaps
        const TSeqPos m_uPos;
        // 0: unknown, negative: negated nominal length of unknown gap size.
        // positive: known gap size
        const TSeqPos m_uLen;
        const EKnownSize m_eKnownSize;
        const TSeqPos m_uLineNumber;
        // NULL means "no gap type specified"
        const TNullableGapType m_pGapType;
        typedef set<CLinkage_evidence::EType> TLinkEvidSet;
        const TLinkEvidSet m_setOfLinkageEvidence;
    private:
        // forbid assignment and copy-construction
        SGap & operator = (const SGap & );
        SGap(const SGap & rhs);
    };

    CRef<CSeq_align> xCreateAlignment(CRef<CSeq_id> old_id,
        CRef<CSeq_id> new_id,
        TSeqPos range_start,
        TSeqPos range_end);

    bool xSetSeqMol(const list<CRef<CSeq_id>>& ids, CSeq_inst_Base::EMol& mol);

    typedef CRef<SGap> TGapRef;
    typedef vector<TGapRef>     TGaps;
    typedef set<CSeq_id_Handle> TIDTracker;

    CRef<ILineReader>       m_LineReader;
    stack<TFlags>           m_Flags;
    CRef<CBioseq>           m_CurrentSeq;
    TMask                   m_CurrentMask;
    TMask                   m_NextMask;
    TMasks *                m_MaskVec;
    CRef<CFastaIdHandler>   m_IDHandler;
    string                  m_SeqData;
    TGaps                   m_Gaps;
    TSeqPos                 m_CurrentPos; // does not count gaps
    TSeqPos                 m_MaskRangeStart;
    TSeqPos                 m_SegmentBase;
    TSeqPos                 m_CurrentGapLength;
    TSeqPos                 m_TotalGapLength;
    TSeqPos                 m_gapNmin;
    TSeqPos                 m_gap_Unknown_length;
    char                    m_CurrentGapChar;
    CRef<CSeq_id>           m_BestID;
    TStartsMap              m_Starts;
    TRowNum                 m_Row;
    TSeqPos                 m_Offset;
    TIDTracker              m_IDTracker;
    CSourceModParser::TMods m_BadMods;
    CSourceModParser::TMods m_UnusedMods;
    Uint4                   m_MaxIDLength;

    using TCountToLinkEvidMap = map<TSeqPos, SGap::TLinkEvidSet>;
    TCountToLinkEvidMap     m_GapsizeToLinkageEvidence;
private:
    SGap::TLinkEvidSet      m_DefaultLinkageEvidence;
protected:
    SGap::TNullableGapType  m_gap_type;

    TSeqTitles m_CurrentSeqTitles;
    std::vector<ILineError::EProblem> m_ignorable;
    FIdCheck m_fIdCheck;
    FModFilter m_fModFilter = nullptr;
};


typedef CFastaReader::TFlags TReadFastaFlags;


/// A const-correct replacement for the deprecated ReadFasta function
/// @sa CFastaReader
NCBI_XOBJREAD_EXPORT
CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, CFastaReader::TFlags flags=0,
                           int* counter = nullptr,
                           CFastaReader::TMasks* lcv=nullptr,
                           ILineErrorListener* pMessageListener=nullptr);

//////////////////////////////////////////////////////////////////
//
// Class - description of multi-entry FASTA file,
// to keep list of offsets on all molecules in the file.
//
struct SFastaFileMap
{
    struct SFastaEntry
    {
        SFastaEntry() : stream_offset(0) {}
        /// List of qll sequence ids
        typedef list<string>  TFastaSeqIds;

        string         seq_id;        ///< Primary sequence Id
        string         description;   ///< Molecule description
        CNcbiStreampos stream_offset; ///< Molecule offset in file
        TFastaSeqIds   all_seq_ids;   ///< List of all seq.ids
    };

    typedef vector<SFastaEntry>  TMapVector;

    TMapVector   file_map; // vector keeps list of all molecule entries
};

/// Callback interface to scan fasta file for entries
class NCBI_XOBJREAD_EXPORT IFastaEntryScan
{
public:
    virtual ~IFastaEntryScan();

    /// Callback function, called after reading the fasta entry
    virtual void EntryFound(CRef<CSeq_entry> se,
                            CNcbiStreampos stream_position) = 0;
};


/// Function reads input stream (assumed that it is FASTA format) one
/// molecule entry after another filling the map structure describing and
/// pointing on molecule entries. Fasta map can be used later for quick
/// CSeq_entry retrival
void NCBI_XOBJREAD_EXPORT ReadFastaFileMap(SFastaFileMap* fasta_map,
                                           CNcbiIfstream& input);

/// Scan FASTA files, call IFastaEntryScan::EntryFound (payload function)
void NCBI_XOBJREAD_EXPORT ScanFastaFile(IFastaEntryScan* scanner,
                                        CNcbiIfstream&   input,
                                        CFastaReader::TFlags fread_flags);


/////////////////// CFastaReader inline methods


inline
void CFastaReader::OpenMask()
{
    if (m_MaskRangeStart == kInvalidSeqPos  &&  m_CurrentMask.NotEmpty()) {
        x_OpenMask();
    }
}

inline
TSeqPos CFastaReader::GetCurrentPos(EPosType pos_type)
{
    TSeqPos pos = m_CurrentPos;
    switch (pos_type) {
    case ePosWithGapsAndSegs:
        pos += m_SegmentBase;
        // FALL THROUGH!!
    case ePosWithGaps:
        pos += m_TotalGapLength;
        // FALL THROUGH!!
    case eRawPos:
        return pos;
    default:
        return kInvalidSeqPos;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE

/* @} */

#endif  /* OBJTOOLS_READERS___FASTA__HPP */
