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
#include <objects/seq/Bioseq.hpp>
// #include <objects/seqset/Seq_entry.hpp>
#include <stack>

/** @addtogroup Miscellaneous
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_data;
class CSeq_entry;
class CSeq_id;
class CSeq_loc;

class CSeqIdGenerator;

/// Base class for reading FASTA sequences.
///
/// This class should suffice as for typical usage, but its
/// implementation has been broken up into virtual functions, allowing
/// for a wide range of specialized subclasses.
///
/// @sa CFastaOstream
class NCBI_XOBJREAD_EXPORT CFastaReader {
public:
    /// Note on fAllSeqIds: some databases (notably nr) have merged
    /// identical sequences, joining their deflines together with
    /// control-As.  Normally, the reader stops at the first
    /// control-A; however, this flag makes it parse all the IDs.
    enum EFlags {
        fAssumeNuc  = 0x1,   ///< Assume nucs unless accns indicate otherwise
        fAssumeProt = 0x2,   ///< Assume prots unless accns indicate otherwise
        fForceType  = 0x4,   ///< Force specified type regardless of accession
        fNoParseID  = 0x8,   ///< Generate an ID (whole defline -> title)
        fParseGaps  = 0x10,  ///< Make a delta sequence if gaps found
        fOneSeq     = 0x20,  ///< Just read the first sequence found
        fAllSeqIds  = 0x40,  ///< Read Seq-ids past the first ^A (see note)
        fNoSeqData  = 0x80,  ///< Parse the deflines but skip the data
        fRequireID  = 0x100, ///< Reject deflines that lack IDs
        fDLOptional = 0x200  ///< Don't require a leading defline
    };
    typedef int TFlags; ///< binary OR of EFlags

    CFastaReader(ILineReader& reader, TFlags flags = 0);
    virtual ~CFastaReader(void);

    /// Read a single effective sequence, which may turn out to be a
    /// segmented set.
    virtual CRef<CSeq_entry> ReadOneSeq(void);

    /// Read multiple sequences (by default, as many as are available.)
    CRef<CSeq_entry> ReadSet(int max_seqs = kMax_Int);

    /// Read as many sequences as are available, and interpret them as
    /// an alignment, with hyphens marking relative deletions.
    /// @param reference_row
    ///   0-based; the special value -1 yields a full (pseudo-?)N-way alignment.
    CRef<CSeq_entry> ReadAlignedSet(int reference_row);

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

protected:
    enum EInternalFlags {
        fAligning = 0x40000000,
        fInSegSet = 0x20000000
    };

    typedef CTempString TStr;

    virtual CRef<CSeq_entry> x_ReadSegSet(void);

    virtual void   ParseDefLine  (const TStr& s);
    virtual bool   ParseIDs      (const TStr& s);
    virtual size_t ParseRange    (const TStr& s, TSeqPos& start, TSeqPos& end);
    virtual void   ParseTitle    (const TStr& s);
    virtual bool   IsValidLocalID(const string& s);
    virtual void   GenerateID    (void);
    virtual void   ParseDataLine (const TStr& s);
    virtual void   x_CloseGap    (TSeqPos len);
    virtual void   x_OpenMask    (void);
    virtual void   x_CloseMask   (void);
    virtual void   AssembleSeq   (void);
    virtual void   AssignMolType (void);
    virtual void   SaveSeqData   (CSeq_data& seq_data, const TStr& raw_string);

    typedef int                         TRowNum;
    typedef map<TRowNum, TSignedSeqPos> TSubMap;
    // align coord -> row -> seq coord
    typedef map<TSeqPos, TSubMap>       TStartsMap;
    typedef vector<CRef<CSeq_id> >      TIds;

    CRef<CSeq_entry> x_ReadSeqsToAlign(TIds& ids);
    void             x_AddPairwiseAlignments(CSeq_annot& annot, const TIds& ids,
                                             TRowNum reference_row);
    void             x_AddMultiwayAlignment(CSeq_annot& annot, const TIds& ids);

    // inline utilities 
    void CloseGap(void);
    void OpenMask(void);
    void CloseMask(void)
        { if (m_MaskRangeStart != kInvalidSeqPos) { x_CloseMask(); } }
    Int8 StreamPosition(void) const
        { return NcbiStreamposToInt8(m_LineReader->GetPosition()); }

    ILineReader&  GetLineReader(void)         { return *m_LineReader; }
    bool          TestFlag(EFlags flag) const
        { return (GetFlags() & flag) != 0; }
    bool          TestFlag(EInternalFlags flag) const
        { return (GetFlags() & flag) != 0; }
    CBioseq::TId& SetIDs(void)                { return m_CurrentSeq->SetId(); }

    enum EPosType {
        eRawPos,
        ePosWithGaps,
        ePosWithGapsAndSegs
    };
    TSeqPos GetCurrentPos(EPosType pos_type);

private:
    struct SGap {
        TSeqPos pos; // 0-based, and NOT counting previous gaps
        TSeqPos len; // may be zero for gaps of unknown length
    };
    typedef vector<SGap> TGaps;

    CRef<ILineReader>     m_LineReader;
    stack<TFlags>         m_Flags;
    CRef<CBioseq>         m_CurrentSeq;
    TMask                 m_CurrentMask;
    TMask                 m_NextMask;
    TMasks *              m_MaskVec;
    CRef<CSeqIdGenerator> m_IDGenerator;
    string                m_SeqData;
    TGaps                 m_Gaps;
    TSeqPos               m_CurrentPos; // does not count gaps
    TSeqPos               m_ExpectedEnd;
    TSeqPos               m_MaskRangeStart;
    TSeqPos               m_SegmentBase;
    TSeqPos               m_CurrentGapLength;
    TSeqPos               m_TotalGapLength;
    CRef<CSeq_id>         m_BestID;
    TStartsMap            m_Starts;
    TRowNum               m_Row;
    TSeqPos               m_Offset;
};


class NCBI_XOBJREAD_EXPORT CSeqIdGenerator : public CObject
{
public:
    typedef CAtomicCounter::TValue TInt;
    CSeqIdGenerator(TInt counter = 1, const string& prefix = kEmptyStr,
                    const string& suffix = kEmptyStr)
        : m_Prefix(prefix), m_Suffix(suffix)
        { m_Counter.Set(counter); }

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
    CAtomicCounter m_Counter;
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


/// Traditional interface for reading FASTA files.  The
/// USE_NEW_IMPLEMENTATION parameter governs whether to use
/// CFastaReader or the traditional implementation.
NCBI_XOBJREAD_EXPORT
CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, TReadFastaFlags flags = 0,
                           int* counter = 0,
                           vector<CConstRef<CSeq_loc> >* lcv = 0);

NCBI_PARAM_DECL(bool, READ_FASTA, USE_NEW_IMPLEMENTATION);


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
void CFastaReader::CloseGap(void)
{
    if (m_CurrentGapLength > 0) {
        x_CloseGap(m_CurrentGapLength);
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

/*
* ===========================================================================
*
* $Log$
* Revision 1.16  2006/09/20 19:26:31  ucko
* When a defline specifies a range of locations, give the excerpt a
* local ID and an appropriate hist.assembly record to avoid
* misrepresenting the parent ID's contents in any fashion.
*
* Revision 1.15  2006/09/19 19:19:20  ucko
* CFastaReader::StreamPosition: change return type to Int8, even if some
* callers may have to truncate the result.
* CFastaReader::TestFlag: add redundant syntax to avoid MSVC warnings.
*
* Revision 1.14  2006/09/18 20:31:45  ucko
* CFastaReader: add an fIgnoreRange flag to disregard range information
* in deflines, as the resulting leading gap confuses some callers.
*
* Revision 1.13  2006/06/27 19:03:55  ucko
* #include <corelib/ncbi_param.hpp> here, not in fasta.cpp!
* DOXYGENize existing comments, and add some new ones.
*
* Revision 1.12  2006/06/27 18:36:08  ucko
* Optionally implement ReadFasta as a wrapper around CFastaReader, as
* controlled by a NCBI_PARAM (READ_FASTA, USE_NEW_IMPLEMENTATION).
*
* Revision 1.11  2006/04/13 14:44:00  ucko
* Add a new class-based FASTA reader, but leave the existing reader
* alone for now.
*
* Revision 1.10  2005/10/17 12:19:35  rsmith
* initialize streampos members.
*
* Revision 1.9  2005/09/29 19:35:51  kuznets
* Added callback based fasta reader
*
* Revision 1.8  2005/09/26 15:14:54  kuznets
* Added list of ids to fasta map
*
* Revision 1.7  2005/09/23 12:41:56  kuznets
* Minor comment change
*
* Revision 1.6  2004/11/24 19:27:42  ucko
* +fReadFasta_RequireID
*
* Revision 1.5  2004/01/20 16:27:53  ucko
* Fix a stray reference to sequence.hpp's old location.
*
* Revision 1.4  2003/08/08 21:31:37  dondosha
* Changed type of lcase_mask in ReadFasta to vector of CConstRefs
*
* Revision 1.3  2003/08/07 21:12:56  ucko
* Support a counter for assigning local IDs to sequences with no ID given.
*
* Revision 1.2  2003/08/06 19:08:28  ucko
* Slight interface tweak to ReadFasta: report lowercase locations in a
* vector with one entry per Bioseq rather than a consolidated Seq_loc_mix.
*
* Revision 1.1  2003/06/04 17:26:08  ucko
* Split out from Seq_entry.hpp.
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_READERS___FASTA__HPP */
