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
 * Author: Frank Ludwig, Wratko Hlavina
 *
 * File Description:
 *   Repeat Masker file reader
 *
 */

#ifndef OBJTOOLS_READERS___RMREADER__HPP
#define OBJTOOLS_READERS___RMREADER__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbicntr.hpp>

#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/reader_idgen.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


/// Interface for looking up taxonomy IDs given taxonomic names.
///
class ITaxonomyResolver : public CObject
{
public:
    typedef ITaxonomyResolver TThisType;
    typedef unsigned int TTaxId;

    /// Enforce virtual destructor.
    virtual ~ITaxonomyResolver() { }

    /// Returns a normalized representation of a sequence
    /// identifier, as Seq-id handle.
    ///
    virtual TTaxId GetTaxId(const string& name) const = 0;

    /// Returns the name (e.g. scientific name) for a
    /// given taxonomy ID.
    ///
    virtual string GetName(TTaxId taxid) const = 0;
};


/// Minimalist pure virtual (except destructor) interface defining
/// a read-only RepeatMasker library repeat.
///
/// This interface is equvalent to the semantics of
/// a tabular RepeatMasker output file.
///
/// @note Clients are persuaded to use IRepeatRegion, the subclass of
///     this minimalist interface. That interface makes some
///     accomodations for conventions of the NCBI ASN.1 data model.
///
class IRepeat
{
public:
    typedef ITaxonomyResolver::TTaxId TTaxId;
    static const TTaxId kInvalidTaxId = 0;

    /// Enforce virtual destructor.
    virtual ~IRepeat(void) { }

    /// Gets repeat name.
    virtual string GetRptName() const = 0;

    /// Gets repeat family, or empty string if not known.
    virtual string GetRptFamily() const = 0;

    /// Gets repeat class, or empty string if not known.
    virtual string GetRptClass() const = 0;

    /// Gets repeat length, or kInvalidSeqPos if not known.
    virtual TSeqPos GetRptLength() const = 0;

    /// Gets specificity as a taxonomy ID, or 0 if not known.
    virtual TTaxId GetRptSpecificity() const = 0;

    /// Gets specificity as a name, or empty string if not known.
    virtual string GetRptSpecificityName() const = 0;

    /// Gets the RepbaseID, or empty string if not known.
    virtual string GetRptRepbaseId() const = 0;
};


/// Implementation of IRepeat backed by a simple structure.
///
class SRepeat : public IRepeat
{
public:
    SRepeat() : m_RptLength(kInvalidSeqPos), m_RptSpecificity(0) { }

    string GetRptName() const { return m_RptName; }
    string GetRptFamily() const { return m_RptFamily; }
    string GetRptClass() const { return m_RptClass; }
    TSeqPos GetRptLength() const { return m_RptLength; }
    TTaxId GetRptSpecificity() const { return m_RptSpecificity; }
    string GetRptSpecificityName() const { return m_RptSpecificityName; }
    string GetRptRepbaseId() const { return m_RptRepbaseId; }

    // Structural members. Public, for maximum freedom to modify.

    string m_RptName;
    string m_RptFamily;
    string m_RptClass;
    TSeqPos m_RptLength;
    TTaxId m_RptSpecificity;
    string m_RptSpecificityName;
    string m_RptRepbaseId;
};


/// Minimalist pure virtual (except destructor) interface defining
/// a read-only RepeatMasker repeat feature.
///
/// This interface is equvalent to the semantics of
/// a tabular RepeatMasker output file.
///
/// @note Clients are persuaded to use IRepeatRegion, the subclass of
///     this minimalist interface. That interface makes some
///     accomodations for conventions of the NCBI ASN.1 data model.
///
/// @warning This interface subclasses IRepeat. It is an abuse
///     of the inheritance relationship, better modeled via a
///     containment relationship, e.g. HAS-A, by adding a GetRepeat()
///     member to get an object exposing IRepeat. However:
///     (i)  RepeatMasker *.out format is denormalized (in the database
///          sense that the same repeat, placed multiply, will have
///          its attributes copied redundantly to multiple lines
///          of output), and further,
///     (ii) attributes which should be constant for a given repeat
///          despite placement, are in fact modifed per placement
///          (e.g. a given specific repeat has a "length", but it
///          can vary, e.g. depending on copy number).
///     Due to the above, it was considered reasonable to
///     pretend that a repeat region IS-A repeat, rather than
///     point to one.
///
class IRawRepeatRegion : public IRepeat
{
public:
    typedef ENa_strand TStrand;
    typedef unsigned int TRptId;
    typedef unsigned long TScore;
    typedef double TPercent;

    static const unsigned int kInvalidRptId = kMax_UInt;

    /// Enforce virtual destructor.
    virtual ~IRawRepeatRegion(void) { }

    virtual TRptId GetRptId() const = 0;
    virtual TScore GetSwScore() const = 0;
    virtual TPercent GetPercDiv() const = 0;
    virtual TPercent GetPercDel() const = 0;
    virtual TPercent GetPercIns() const = 0;
    virtual TSeqPos GetRptPosBegin() const = 0;
    virtual TSeqPos GetRptPosEnd() const = 0;
    virtual TSeqPos GetRptLeft() const = 0;
    virtual TSeqPos GetSeqLeft() const = 0;

    /// Flag that there is a higher-scoring match whose
    /// domain partly (<80%) includes the domain of this match.
    ///
    virtual bool IsOverlapped() const = 0;

    // API for native representation of location.
    //
    // The native representation may diverge from the
    // normalized representation of IRepeatRegion.

    virtual string GetSeqIdString() const = 0;
    virtual TSeqPos GetSeqPosBegin() const = 0;
    virtual TSeqPos GetSeqPosEnd() const = 0;
    virtual bool IsReverseStrand() const = 0;

    /// Covenience function to get the class and family as one value,
    /// the way that RepeatMasker emits them.
    ///
    /// The two are separated by a slash '/' character.
    ///
    string GetRptClassFamily() const;
};


/// Interface defining a read-only RepeatMasker repeat feature.
///
/// This interface is almost equvalent to the semantics of
/// a tabular RepeatMasker output file, but locations are
/// represented using the NCBI ASN.1 data model.
///
/// @note Please use this interface in favor of IRawRepeatRegion.
///
/// @warning See GetLocation() regarding multi-interval features.
///
class IRepeatRegion : public IRawRepeatRegion
{
public:
    /// Gets the location of this repeat.
    ///
    /// @warning Repeat features may be multi-interval.
    ///     RepeatMasker can identify repeats that have been split
    ///     by other intervening repeats, and while RepeatMasker
    ///     output will emit these as multiple lines, tied together
    ///     by repeat ID, in the NCBI ASN.1 data model this may
    ///     be normalized into a single multi-interval feature.
    ///
    /// @todo For multi-interval features, should they be implemented,
    ///     will pose an issue for statistics which apply per segment.
    ///     This API will need significant revision to accommodate
    ///     that change.
    ///
    virtual CConstRef<CSeq_loc> GetLocation(void) const = 0;

    /// Gets the more general feature ID for this repeat, which identifies
    /// a single repeat, which may be multi-segement, and allows linking
    /// the segments together.
    ///
    /// The feature ID can be either a CFeat_id as in ASN.1,
    /// or a RepeatMasker feature ID encoded as such a CFeat_id using
    /// a local integer ID.
    ///
    virtual CConstRef<CFeat_id> GetId() const = 0;

    // API with performance optimizations.

    /// Gets a copy of the location into the Seq-loc instance provided by
    /// the caller.
    ///
    /// This function is provided with a default implementation,
    /// using GetLocation(). The latter may be able to take advantage of
    /// a copy-less Seq-loc that is already a member of the object, if
    /// that is the native representation. 
    ///
    /// The default implementation copies the result of GetLocation(void)
    /// into the result. Subclasses may override with a more direct
    /// translation from internal state.
    ///
    virtual void GetLocation(CSeq_loc& result) const;

    // API for native representation of location.
    //
    // The native representation may diverge from the
    // normalized representation.

    /// Gets the sequence from the location of the repeat,
    /// without dealing with a Seq-loc. This is more than merely
    /// a conevience function.
    ///
    /// This function is provided with a default implementation,
    /// using GetLocation(). The latter is richer, dealing with Seq-ids
    /// which can be more than just strings, so that's the one which should
    /// be provided by the concrete implementation.
    ///
    /// The default implementation returns the Seq-id in FASTA format.
    ///
    virtual string GetSeqIdString() const;

    /// Convenience function that gets the position start on the sequence,
    /// without dealing with a Seq-loc.
    ///
    /// This function is provided with a default implementation,
    /// using GetLocation(). The latter is richer, dealing with Seq-ids
    /// which can be more than just strings, so that's the one which should
    /// be provided by the concrete implementation.
    ///
    virtual TSeqPos GetSeqPosBegin() const;

    /// Convenience functions that gets the position end on the sequence,
    /// without dealing with a Seq-loc.
    ///
    /// This function is provided with a default implementation,
    /// using GetLocation(). The latter is richer, dealing with Seq-ids
    /// which can be more than just strings, so that's the one which should
    /// be provided by the concrete implementation.
    ///
    virtual TSeqPos GetSeqPosEnd() const;

    /// Convenience functions that gets the strand on the sequence,
    /// without dealing with a Seq-loc.
    ///
    /// This function is provided with a default implementation,
    /// using GetLocation(). The latter is richer, dealing with Seq-ids
    /// which can be more than just strings, so that's the one which should
    /// be provided by the concrete implementation.
    ///
    virtual bool IsReverseStrand() const;
};


/// Structure implementing the IRepeatRegion API as a simple store of
/// data memebers.
///
/// The requirements for this class include that it be
/// lossless with regard to the content from tabular RepeatMasker
/// output. If this class doesn't represent some attribute, then
/// as far as all consumers of RepeatMasker data are concerned,
/// that attribute doesn't exist. As an example of data loss,
/// even standardization of the sequence identifier, from string
/// name to Seq-id, is potentially lossy. Thus, this class overrides
/// the default implementation of GetSeqIdString().
///
/// There are several clients in need of a lightweight and minimalist
/// representation of a repeat match. This structure provides that
/// storage. This representation is very raw, and closely mirrors
/// one line of tabular RepeatMasker output.
///
/// By contrast, the NCBI data model provides a Seq-feat representation
/// of repeats, but that representation is burdened with INSDC and NCBI
/// standards.
///
/// @note This class only implements the columns present in
///     the RepeatMasker *.out file format. Several attributes
///     of IRepeat are not present in this format, and are thus
///     not available.
///
class NCBI_XOBJREAD_EXPORT SRepeatRegion : public IRepeatRegion
{
public:
    typedef SRepeatRegion TThisType;
    typedef IRepeatRegion TParent;

    // Implement the IRepeatRegion API.

    CConstRef<CSeq_loc> GetLocation(void) const;
    CConstRef<CFeat_id> GetId() const;

    string GetRptName() const;
    string GetRptFamily() const;
    string GetRptClass() const;
    TSeqPos GetRptLength() const;

    /// Returns 0, not known.
    ///
    /// The specificity is not present in RepeatMasker output.
    ///
    TTaxId GetRptSpecificity() const;

    /// Returns an empty string, not known.
    ///
    /// The specificity is not present in RepeatMasker output.
    ///
    string GetRptSpecificityName() const;

    /// Returns an empty string, not known.
    ///
    /// The Repbase ID is not present in RepeatMasker output.
    ///
    string GetRptRepbaseId() const;

    TRptId GetRptId() const;
    TScore GetSwScore() const ;
    TPercent GetPercDiv() const;
    TPercent GetPercDel() const;
    TPercent GetPercIns() const;
    TSeqPos GetRptPosBegin() const;
    TSeqPos GetRptPosEnd() const;
    TSeqPos GetRptLeft() const;
    TSeqPos GetSeqLeft() const;
    bool IsOverlapped() const;

    string GetSeqIdString() const;

    // Structural members. Public, for maximum freedom to modify.

    CRef<CSeq_loc> query_location;
    TScore sw_score;
    TSeqPos query_left;
    TPercent perc_div;
    TPercent perc_del;
    TPercent perc_ins;
    string query_sequence;
    string strand;
    string matching_repeat;
    string rpt_class;
    string rpt_family;
    TSeqPos rpt_pos_begin;
    TSeqPos rpt_pos_end;
    TSeqPos rpt_left;
    TRptId rpt_id;
    bool overlapped;
};


/// Class acting as an interface to a RepeatMasker library.
///
class NCBI_XOBJREAD_EXPORT CRepeatLibrary : public CObject
{
public:
    typedef SRepeat TRepeat;

    CRepeatLibrary(const ITaxonomyResolver& taxonomy) :
            m_Taxonomy(&taxonomy) { }

    /// Gets information about a given repeat, specified by name.
    ///
    bool Get(const string& name, TRepeat& dest) const;

    // const string& GetRelease() const;

    /// Reads a library from the RepeatMaskerLib.embl-style input.
    ///
    void Read(CNcbiIstream& stream);

    /// Check if a given taxid's scientific name
    /// matches the original specificity string.
    bool TestSpecificityMatchesName(TRepeat::TTaxId taxid,
                                    const string& name) const;

private:
    typedef map<string, TRepeat> TMap;
    typedef map<string, TRepeat::TTaxId> TSpecificity2Taxid;

    CConstIRef<ITaxonomyResolver> m_Taxonomy;
    TMap m_Map;
    TSpecificity2Taxid m_Specificity2TaxId;
    string m_Release;
};


/// Pure interface to define flags controlling how repeats
/// are read from RepeatMasker output into a Seq-feat.
///
/// These flags belong most logically in CRepeatToFeat,
/// which was factored out of CRepeatMaskerReader. However,
/// CRepeatMaskerReader also needs those flags, to preserve backward
/// compatibility with its prior interface. These flags should be deprecated
/// within CRepeatMaskerReader, at some point.
///
class IRmReaderFlags
{
public:
    enum EFlags {

        /// Translate RepeatMasker output to INSDC standard
        /// nomenclature for repeats. This includes remapping repeat
        /// family to satellite and mobile element qualifiers, as
        /// appropriate.
        ///
        /// Recommended.
        ///
        fStandardizeNomenclature      = 1 << 0,

        /// Removes redundant fields. For example, rpt_left
        /// can often be computed from repeat length.
        ///
        /// Recommended.
        ///
        fRemoveRedundancy             = 1 << 1,

        /// Avoid user objects and instead, put selected information in
        /// non-standard and invalid GenBank qualifiers. When
        /// this option is used and content is rendered in
        /// non-strict GenBank Flat-File format, every attribute is
        /// visible in the output, even if it is not conforming
        /// to INSDC standards. When this option is not used,
        /// the information beyond what is accepted by the
        /// INSDC standard nomenclature will not be exposed but
        /// will be available in user objects, under
        /// seq-feat.ext."RepeatMasker".
        ///
        /// Not recommended. 
        ///
        fAllowNonstandardQualifiers   = 1 << 2,
 
        /// Selected attributes beyond what is stored in GenBank
        /// standard qualifiers will be included as comments.
        /// Such comments are rendered via a /note
        /// in GenBank Flat File format. As a result, the
        /// selected attributes will be visible in Flat-File output.
        ///
        fSetComment                   = 1 << 3,

        /// Store core statistics, which include the scores
        /// of sw_score, perc_div, perc_del, perc_ins, and
        /// the length of the repeat (or rpt_left, equivalently).
        ///
        fIncludeCoreStatistics        = 1 << 4,

        /// Store extra statistics, which includes the
        /// length of the query (or query_left, equivalently),
        /// and the flag has_higher_score_overlapping_match.
        ///
        fIncludeExtraStatistics       = 1 << 5,

        /// Store the repeat name.
        /// @example FLAM_A, Bov-tA2, BTSAT4.
        ///
        fIncludeRepeatName            = 1 << 6,

        /// Store the repeat family (in Genbank terminology).
        /// In RepeatMasker terminology, this includes the class and family.
        /// @example SINE/Alu, SINE/BovA, Satellite/centr.
        ///
        fIncludeRepeatFamily          = 1 << 7,

        /// Same as fIncludeRepeatFamily.
        /// @see fIncludeRepeatFamily
        /// @deprecated  This flag uses RepeatMasker terminology (class/family),
        ///     which diverges from Genbank terminology (family). We prefer
        ///     to adopt the Genbank terminolgy in the reader API.
        ///
        fIncludeRepeatClass = fIncludeRepeatFamily,

        /// Store the repeat position, that is, the interval
        /// on the repeat sequence.
        ///
        fIncludeRepeatPos             = 1 << 8,

        /// Store original RepeatMasker repeat_id.
        ///
        fIncludeRepeatId              = 1 << 9,

        /// Store the specificity from the RepeatMasker library,
        /// if provided.
        ///
        fIncludeRepeatSpecificity     = 1 << 10,

        /// Store the repeat length as reported in the library.
        ///
        fIncludeRepeatLength          = 1 << 11,

        /// Store the RepbaseID from the RepeatMasker library,
        /// if provided.
        ///
        fIncludeRepeatRepbaseId       = 1 << 12,

        // The remaining flags are combinations of the above.

        /// Store the repeat position and RepeatMasker repeat_id.
        /// @deprecated
        ///
        fIncludeRepeatPosId         = fIncludeRepeatPos | \
                                      fIncludeRepeatId,

        /// Perform all standardization, including adjustment
        /// of nomenclature and redundancy removal.
        fStandardize                = fStandardizeNomenclature | \
                                      fRemoveRedundancy,
        
        /// Store all statistics.
        ///
        fIncludeStatistics          = fIncludeCoreStatistics |
                                      fIncludeExtraStatistics,

        /// Store repeat library information.
        ///
        fIncludeLibraryAttributes   = fIncludeRepeatName | \
                                      fIncludeRepeatFamily | \
                                      fIncludeRepeatSpecificity | \
                                      fIncludeRepeatLength | \
                                      fIncludeRepeatRepbaseId,

        /// Store all attributes from RepeatMasker.
        ///
        fIncludeAll                 = fIncludeStatistics | \
                                      fIncludeLibraryAttributes | \
                                      fIncludeRepeatPos | \
                                      fIncludeRepeatId,

        /// Nonredundantly, ensures that a full fidelity representation
        /// of the repeat data is available, sufficient to round-trip
        /// a RepeatMasker file through ASN.1 and back to
        /// the RepeatMasker format, without data loss as far
        /// as essential attributes of the repeat are concerned.
        ///
        /// Essential attributes do NOT include
        /// rpt_id or attributes from any RepeatMasker library file.
        /// 
        /// This may be less information than fIncludeAll.
        /// For example, repeat IDs are not significant, beyond
        /// their capacity to link parts of a split repeat together.
        ///
        fPreserveContent            = fRemoveRedundancy | \
                                      fIncludeStatistics | \
                                      fIncludeRepeatName | \
                                      fIncludeRepeatFamily | \
                                      fIncludeRepeatPos,

        fDefaults                   = fStandardize | \
                                      fIncludeAll
    };
    typedef int TFlags;
};


/// Class which, given an input IRepeatRegion, can generate an appropriate
/// and normalized NCBI ASN.1 representation as a sequence feature.
///
/// Such conversions are needed outside the scope of reading repeats
/// from tabular RepeatMasker output files, so this logic has been
/// pulled off into its own independent class.
///
class NCBI_XOBJREAD_EXPORT CRepeatToFeat : public IRmReaderFlags
{
public:
    typedef CRepeatLibrary TRepeatLibrary;
    typedef IIdGenerator< CRef<CFeat_id> > TIdGenerator;

    CRepeatToFeat(TFlags flags = fDefaults,
            CConstRef<TRepeatLibrary> lib = null,
            TIdGenerator& ids =
                    *(CIRef<TIdGenerator>(new COrdinalFeatIdGenerator)));

    /// Clear out any repeat library which may be used to add
    /// additional attributes to repeats.
    ///
    void ResetRepeatLibrary();

    /// Set a repeat library which may be used to add
    /// additional attributes to repeats.
    ///
    void SetRepeatLibrary(const TRepeatLibrary& lib);

    /// Reset the Feature-id generator, do use a default implementation
    /// which will generate unique integer local IDs.
    ///
    void ResetIdGenerator();

    /// Set the Feature-id generator which will be used to assign
    /// unique feature IDs.
    ///
    void SetIdGenerator(TIdGenerator& generator);

    /// Asserts that all forward/backward references between
    /// any objects visited have now been resolved.
    ///
    /// This means that, if a multi-segment repeat spans multiple
    /// repeat regions, then all relevant repeat regions have been seen
    /// and no repeat region shall reference a repeat (via repeat_id)
    /// which has been visited prior to the call to this assertion.
    ///
    /// For example, if all repeats on a given sequence have been
    /// processed, it is recommended to call AssertReferencesResolved().
    ///
    void AssertReferencesResolved();

    /// Transforms the input repeat into a repeat feature.
    ///
    CRef<CSeq_feat> operator()(const IRepeatRegion& repeat);

protected:
    typedef map< IRepeatRegion::TRptId, CConstRef< CFeat_id > > TIdMap;

private:
    TFlags m_Flags;
    CConstRef<TRepeatLibrary> m_Library;
    CIRef<TIdGenerator> m_Ids;
    TIdMap m_IdMap;
};


/// Implements a concrete class for reading RepeatMasker output
/// from tabular form and rendering it as ASN.1 using the NCBI data model.
///
class NCBI_XOBJREAD_EXPORT CRepeatMaskerReader : public CReaderBase, public IRmReaderFlags
{
public:
    typedef CRepeatToFeat TConverter;
    typedef TConverter::TRepeatLibrary TRepeatLibrary;
    typedef TConverter::TIdGenerator TIdGenerator;

    /// Implement CReaderBase.

    CRepeatMaskerReader(TFlags flags = fDefaults,
            CConstRef<TRepeatLibrary> lib = null,
            const ISeqIdResolver& seqid_resolver =
                    *(CConstIRef<ISeqIdResolver>(new CFastaIdsResolver)),
            TIdGenerator& ids =
                    *(CIRef<TIdGenerator>(new COrdinalFeatIdGenerator)));

    virtual ~CRepeatMaskerReader(void);

    using CReaderBase::ReadObject;

    CRef<CSerialObject>
    ReadObject(ILineReader& lr,
               ILineErrorListener* pMessageListener = 0);

    using CReaderBase::ReadSeqAnnot;

    CRef<CSeq_annot>
    ReadSeqAnnot(ILineReader& lr,
                 ILineErrorListener* pMessageListener = 0);

    /// Use default Seq-id resolution.
    ///
    void ResetSeqIdResolver();

    /// Use specified delegate for Seq-id resolution.
    ///
    void SetSeqIdResolver(ISeqIdResolver& seqid_resolver);

    /// Delegate for conversion from IRepeatRegion to ASN.1.
    ///
    TConverter& SetConverter();

protected:
    virtual bool IsHeaderLine(const string& line);
    virtual bool IsIgnoredLine(const string& line);
    
    virtual bool ParseRecord(const string& record, SRepeatRegion& mask_data);
    virtual bool VerifyData(const SRepeatRegion& mask_data);

private:
    CConstIRef<ISeqIdResolver> m_SeqIdResolver;
    TConverter m_ToFeat;

};


/// Deprecated, old API for loading RepeatMasker output.
///
class NCBI_XOBJREAD_EXPORT CRmReader : public IRmReaderFlags
{
public:
    NCBI_DEPRECATED static CRmReader* OpenReader(CNcbiIstream& istr);
    NCBI_DEPRECATED static void CloseReader(CRmReader* reader);
    NCBI_DEPRECATED void Read(CRef<CSeq_annot> annot,
                              TFlags flags = fDefaults,
                              size_t errors = kMax_UInt);
private:
    CRmReader(CNcbiIstream& istr);
    CNcbiIstream& m_Istr;
};

END_objects_SCOPE
END_NCBI_SCOPE

/**
    @page objtools_readers_rm_reader RepeatMasker output

    @section Overview
    This library provides an API for representing the information
    from RepeatMasker output, as well as a reader to transform
    such files into the NCBI ASN.1 data model.

    The implementation can also standardize the representation
    to conform with INSDC standards for repeat annotation.

    @section Design
    The initial implementations were excessively complex,
    with copious transformations. How does one RepeatMasker output
    file format diverge as it spreads across 3 different C++ structs,
    a database table, an ASN.1 structure that has evolved
    through 2 major revisions, and a pair of MapViewer files?
    Why does each on of these 7 representations, if we include
    the original from RepeatMasker and count the ASN.1 twice,
    have to invent its own column names, just slightly different?

    From those observations, an Object Oriented API, with
    abstract interface and pure virtual methods was decided.
    Two backend storage options are implemented: as a simple struct,
    and as a Seq-feat. The former is light-weight and a very raw
    interface, close to Repetmasker native output, whereas the latter
    permits both the simpler storage and richer, standardized
    representation.

    The use of C++ generics was considered for the interfaces,
    but there was no compelling immediacy to using them. There
    would be performance compromises with type erasure, and
    without C++ concepts, enforcing contracts of the API
    wouldn't be as clear. While there may be performance improvements
    from defining templated implementations to avoid virtual
    method lookups, such gains are likely insufficient to warrent
    the complexity icrease over plain OO-style abstract interfaces.

    @section impl_notes Implementation Notes
    A Seq-feat is a reasonable container in most cases, obviating
    the need for copying attributes into structs. Layering an
    abstract API on top of a Seq-feat permits ease of use,
    without need to know the strucutural details of the ASN.1
    intimately.

    @section Specifications
    The requirements for fidelity in representing the content
    of RepeatMasker output are summarized by stating, "if it's
    indistinguishable from a native RepeatMasker file by BioPerl,
    it's good enough."
    
    The order of features does not need to be preserved.
    The attribute of being overlapped by a higher-scoring hit,
    as represented by the asterisk in the last column of RepeatMasker
    output, does not need to be preserved. The repeat ID from
    RepeatMasker output does not need to be preserved, but the
    relationship that several matches are part of the same repeat
    does need to be maintained.

    @see https://sp.ncbi.nlm.nih.gov/IEB/mss/Gpipe/RepeatDb/RepeatMasker%20and%20GPipe.docx
            for official requirements specifications
    @see jira.be-md.ncbi.nlm.nih.gov/browse/GP-1000
            for developer discussion.
    @see www.repeatmasker.org/ for RepeatMasker.
    @see www.repeatmasker.org/webrepeatmaskerhelp.html#reading
            for RepeatMasker file format specifications.
    @see www.girinst.org/repbase/update/index.html
            for examples of EMBL format for RepBase repeat libraries.
    @see www.ebi.ac.uk/embl/Documentation/User_manual/usrman.html#3_4
            for EMBL file format specifications, as used by
            the RepeatMaskerLib.embl repeat library.
    @see www.ebi.ac.uk/ena/WebFeat/repeat_region_s.html
            for INSDC standards related to annotating repeat features.
    @see www.ebi.ac.uk/embl/Standards/web/repetitive.html
            for examples of EMBL standards for annotating repeat features.
*/

#endif // OBJTOOLS_READERS___RMREADER__HPP
