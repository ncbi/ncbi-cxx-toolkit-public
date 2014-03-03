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
// #include <objects/seqset/Seq_entry.hpp>
#include <stack>
#include <sstream>

/** @addtogroup Miscellaneous
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_data;
class CSeq_entry;
class CSeq_loc;

class CSeqIdGenerator;

class IMessageListener;

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
        fAllSeqIds            = 1<< 6, ///< Read Seq-ids past the first ^A (see note)
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
        fBadModThrow          = 1<<20, ///< Throw an exception if there's a bad modifier value (e.g. "[topology=nonsense]")
        fUnknModThrow         = 1<<21, ///< Throw if there are any unknown (unused) mods left over
        fLeaveAsText          = 1<<22, ///< Don't reencode at all, just parse
        fQuickIDCheck         = 1<<23, ///< Just check local IDs' first characters
        fUseIupacaa           = 1<<24, ///< If Prot, use iupacaa instead of the default ncbieaa.
        fHyphensIgnoreAndWarn = 1<<25, ///< When a hyphen is encountered in seq data, ignore it but warn.
    };
    typedef int TFlags; ///< binary OR of EFlags

    CFastaReader(ILineReader& reader, TFlags flags = 0);
    CFastaReader(CNcbiIstream& in,    TFlags flags = 0);
    CFastaReader(const string& path,  TFlags flags = 0);
    CFastaReader(CReaderBase::TReaderFlags fBaseFlags,  TFlags flags = 0);
    virtual ~CFastaReader(void);

    /// CReaderBase overrides
    virtual CRef<CSerialObject> ReadObject   (ILineReader &lr, IMessageListener *pErrors);
    virtual CRef<CSeq_entry>    ReadSeqEntry (ILineReader &lr, IMessageListener *pErrors);

    /// Indicates (negatively) whether there is any more input.
    bool AtEOF(void) const { return m_LineReader->AtEOF(); }

    /// Read a single effective sequence, which may turn out to be a
    /// segmented set.
    virtual CRef<CSeq_entry> ReadOneSeq(IMessageListener * pMessageListener = 0);

    /// Read multiple sequences (by default, as many as are available.)
    CRef<CSeq_entry> ReadSet(int max_seqs = kMax_Int, IMessageListener * pMessageListener = 0);

    /// Read as many sequences as are available, and interpret them as
    /// an alignment, with hyphens marking relative deletions.
    /// @param reference_row
    ///   0-based; the special value -1 yields a full (pseudo-?)N-way alignment.
    CRef<CSeq_entry> ReadAlignedSet(int reference_row, IMessageListener * pMessageListener = 0);

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

    const CSeqIdGenerator& GetIDGenerator(void) const { return *m_IDGenerator; }
    CSeqIdGenerator&       SetIDGenerator(void)       { return *m_IDGenerator; }
    void                   SetIDGenerator(CSeqIdGenerator& gen);

    /// Re-allow previously seen IDs even if fUniqueIds is on.
    void ResetIDTracker(void) { m_IDTracker.clear(); }
    
    const CSourceModParser::TMods & GetUnusedMods(void) const { return m_UnusedMods; }
    const CSourceModParser::TMods & GetBadMods(void) const { return m_BadMods; }
    void  ClearBadMods(void) { m_BadMods.clear(); }

    void SetModFilter( CRef<CSourceModParser::CModFilter> pModFilter ) {
        m_pModFilter = pModFilter;
    }

    /// If this is set, an exception will be thrown if a Sequence ID exceeds the
    /// given length.
    /// @param max_len
    ///   The new maximum to set.  Of course, you can set it to kMax_UI4
    ///   to effectively have no limit.
    void SetMaxIDLength(Uint4 max_len) { m_MaxIDLength = max_len; }

    /// Get the maximum ID allowed, which will be kMax_UI4
    /// unless someone has manually lowered it.
    ///
    /// @returns currently set maximum ID length
    Uint4 GetMaxIDLength(void) const { return m_MaxIDLength; }

    /// indicates which linkage-evidences a given gap-type
    /// can accept, if any
    enum ELinkEvid {
        /// only the "unspecified" linkage-evidence is allowed
        eLinkEvid_UnspecifiedOnly,
        /// no linkage-evidence is allowed
        eLinkEvid_Forbidden,
        /// any linkage-evidence is allowed, and at least one is required
        eLinkEvid_Required, 
    };
    /// Holds information about a given gap-type string
    struct SGapTypeInfo {
        /// The underlying type that the string corresponds to
        CSeq_gap::EType m_eType;
        /// Indicates what linkage-evidences are compatible with this
        ELinkEvid       m_eLinkEvid;
    };
    /// Map a gap-type string to its information
    /// Note that PCase_CStr, which means direct lookup is 
    /// NOT insensitive to case, etc.
    typedef CStaticArrayMap<const char *, SGapTypeInfo, PCase_CStr> TGapTypeMap;

    /// From a gap-type string, get the SGapTypeInfo, 
    /// insensitive to case, etc.  This is NOT quite the same as calling
    /// find on the TGapTypeMap.
    ///
    /// @returns NULL if not found
    static const SGapTypeInfo * NameToGapTypeInfo(const CTempString & sName);
    /// This is for if the user needs to get the gap-type string
    /// to SGapTypeInfo info directly (For example, to iterate through
    /// all possible types).
    static const TGapTypeMap & GetNameToGapTypeInfoMap(void);

protected:
    enum EInternalFlags {
        fAligning = 0x40000000,
        fInSegSet = 0x20000000
    };

    typedef CTempString TStr;
    struct SLineTextAndLoc {
        SLineTextAndLoc(
            const string & sLineText,
            TSeqPos iLineNum ) 
            : m_sLineText(sLineText),
              m_iLineNum(iLineNum) { }

        string  m_sLineText;
        TSeqPos m_iLineNum;
    };

    virtual CRef<CSeq_entry> x_ReadSegSet(IMessageListener * pMessageListener);

    virtual void   ParseDefLine  (const TStr& s, IMessageListener * pMessageListener);
    virtual bool   ParseIDs      (const TStr& s, IMessageListener * pMessageListener);
    virtual size_t ParseRange    (const TStr& s, TSeqPos& start, TSeqPos& end, IMessageListener * pMessageListener);
    virtual void   ParseTitle    (const SLineTextAndLoc & lineInfo, IMessageListener * pMessageListener);
    virtual bool   IsValidLocalID(const TStr& s);
    virtual void   GenerateID    (void);
    virtual void   ParseDataLine (const TStr& s, IMessageListener * pMessageListener);
    virtual void   CheckDataLine (const TStr& s, IMessageListener * pMessageListener);
    virtual void   x_CloseGap    (TSeqPos len, bool atStartOfLine, IMessageListener * pMessageListener);
    virtual void   x_OpenMask    (void);
    virtual void   x_CloseMask   (void);
    virtual bool   ParseGapLine  (const TStr& s, IMessageListener * pMessageListener);
    virtual void   AssembleSeq   (IMessageListener * pMessageListener);
    virtual void   AssignMolType (IMessageListener * pMessageListener);
    virtual bool   CreateWarningsForSeqDataInTitle(
        const TStr& sLineText, 
        TSeqPos iLineNum,
        IMessageListener * pMessageListener);

    typedef int                         TRowNum;
    typedef map<TRowNum, TSignedSeqPos> TSubMap;
    // align coord -> row -> seq coord
    typedef map<TSeqPos, TSubMap>       TStartsMap;
    typedef vector<CRef<CSeq_id> >      TIds;

    CRef<CSeq_entry> x_ReadSeqsToAlign(TIds& ids, IMessageListener * pMessageListener);
    void             x_AddPairwiseAlignments(CSeq_annot& annot, const TIds& ids,
                                             TRowNum reference_row);
    void             x_AddMultiwayAlignment(CSeq_annot& annot, const TIds& ids);

    // inline utilities 
    void CloseGap(bool atStartOfLine=true, IMessageListener * pMessageListener = 0);
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

    void x_ApplyAllMods( CBioseq & bioseq, TSeqPos iLineNum, 
        IMessageListener * pMessageListener );

    std::string x_NucOrProt(void) const;
    
    // Make case-sensitive and other kinds of insensitivity, too
    // (such as "spaces" and "underscores" becoming "hyphens"
    static string CanonicalizeString(const TStr & sValue);

private:
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
            TNullableGapType pGapType =
                TNullableGapType(),
            const set<CLinkage_evidence::EType> & setOfLinkageEvidence =
                set<CLinkage_evidence::EType>() );

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
    typedef CRef<SGap> TGapRef;
    typedef vector<TGapRef>     TGaps;
    typedef set<CSeq_id_Handle> TIDTracker;

    CRef<ILineReader>       m_LineReader;
    stack<TFlags>           m_Flags;
    CRef<CBioseq>           m_CurrentSeq;
    TMask                   m_CurrentMask;
    TMask                   m_NextMask;
    TMasks *                m_MaskVec;
    CRef<CSeqIdGenerator>   m_IDGenerator;
    string                  m_SeqData;
    TGaps                   m_Gaps;
    TSeqPos                 m_CurrentPos; // does not count gaps
    TSeqPos                 m_ExpectedEnd;
    TSeqPos                 m_MaskRangeStart;
    TSeqPos                 m_SegmentBase;
    TSeqPos                 m_CurrentGapLength;
    TSeqPos                 m_TotalGapLength;
    char                    m_CurrentGapChar;
    CRef<CSeq_id>           m_BestID;
    TStartsMap              m_Starts;
    TRowNum                 m_Row;
    TSeqPos                 m_Offset;
    TIDTracker              m_IDTracker;
    CSourceModParser::TMods m_BadMods;
    CSourceModParser::TMods m_UnusedMods;
    Uint4                   m_MaxIDLength;
    vector<SLineTextAndLoc> m_CurrentSeqTitles;
    CRef<CSourceModParser::CModFilter> m_pModFilter;
};


class NCBI_XOBJREAD_EXPORT CSeqIdGenerator : public CObject
{
public:
    typedef CAtomicCounter::TValue TInt;
    CSeqIdGenerator(TInt counter = 1, const string& prefix = kEmptyStr,
                    const string& suffix = kEmptyStr)
        : m_Prefix(prefix),
          m_Suffix(suffix),
          m_Counter(counter)
        { }

    CRef<CSeq_id> GenerateID(bool advance);
    /// Equivalent to GenerateID(false)
    CRef<CSeq_id> GenerateID(void) const;
    
    const string& GetPrefix (void)  { return m_Prefix;        }
    TInt          GetCounter(void)  { return m_Counter.Get(); }
    const string& GetSuffix (void)  { return m_Suffix;        }

    void SetPrefix (const string& s)  { m_Prefix  = s;    }
    void SetCounter(TInt n)           { m_Counter.Set(n); }
    void SetSuffix (const string& s)  { m_Suffix  = s;    }

private:
    string         m_Prefix, m_Suffix;
    CAtomicCounter_WithAutoInit m_Counter;
};


enum EReadFastaFlags {
    fReadFasta_AssumeNuc  = CFastaReader::fAssumeNuc,
    fReadFasta_AssumeProt = CFastaReader::fAssumeProt,
    fReadFasta_ForceType  = CFastaReader::fForceType,
    fReadFasta_NoParseID  = CFastaReader::fNoParseID,
    fReadFasta_ParseGaps  = CFastaReader::fParseGaps,
    fReadFasta_OneSeq     = CFastaReader::fOneSeq,
    fReadFasta_AllSeqIds  = CFastaReader::fAllSeqIds,
    fReadFasta_NoSeqData  = CFastaReader::fNoSeqData,
    fReadFasta_RequireID  = CFastaReader::fRequireID
};
typedef CFastaReader::TFlags TReadFastaFlags;


/// Traditional interface for reading FASTA files.
/// @deprecated
/// @sa CFastaReader
NCBI_DEPRECATED
NCBI_XOBJREAD_EXPORT
CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, TReadFastaFlags flags = 0,
                           int* counter = 0,
                           vector<CConstRef<CSeq_loc> >* lcv = 0,
                           IMessageListener * pMessageListener = 0);

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
                                        TReadFastaFlags  fread_flags);


/////////////////// CFastaReader inline methods


inline
void CFastaReader::CloseGap(bool atStartOfLine, IMessageListener * pMessageListener)
{
    if (m_CurrentGapLength > 0) {
        x_CloseGap(m_CurrentGapLength, atStartOfLine, pMessageListener);
    }
    m_CurrentGapLength = 0;
}

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
        pos += m_SegmentBase; // fall through
    case ePosWithGaps:
        pos += m_TotalGapLength; // fall through
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
