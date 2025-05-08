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
 * Authors:  Frank Ludwig
 *
 * File Description:  Formatter, Genbank to BED.
 *
 */

#ifndef OBJTOOLS_WRITERS___BED_WRITER__HPP
#define OBJTOOLS_WRITERS___BED_WRITER__HPP

#include <objtools/writers/writer.hpp>
#include <objtools/writers/bed_track_record.hpp>
#include <objtools/writers/bed_feature_record.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


//  ----------------------------------------------------------------------------
class CThreeFeatRecord
    //  ----------------------------------------------------------------------------
{
    friend class CThreeFeatManager;

public:
    CThreeFeatRecord() {};
    ~CThreeFeatRecord() {};

    bool AddFeature(
        const CSeq_feat&);

    bool IsRecordComplete() const;

    bool GetBedFeature(
        CBedFeatureRecord&) const;

private:
    bool xAddFound(
        int);
    bool xAddAll(
        int);

    CRef<CSeq_feat> mpChrom;
    CRef<CSeq_feat> mpThick;
    CRef<CSeq_feat> mpBlocks;
    vector<int> mFeatsAll;
    vector<int> mFeatsFound;
};

//  ----------------------------------------------------------------------------
class CThreeFeatManager
    //  ----------------------------------------------------------------------------
{
public:
    using RECORDS = vector<CThreeFeatRecord>;
    using RECORD_IT = RECORDS::iterator;

    CThreeFeatManager(){};
    ~CThreeFeatManager() {};

    bool AddFeature(
        const CSeq_feat&);

    bool IsRecordComplete(
        const CSeq_feat&);

    bool ProcessRecord(
        const CSeq_feat&,
        CBedFeatureRecord&);

    bool GetAnyRecord(
        CBedFeatureRecord&);

private:
    RECORD_IT xFindExistingRecord(
        const CSeq_feat&);

    RECORD_IT xAddRecord(
        const CSeq_feat&);

    RECORDS mRecords;
};

//  ============================================================================
/// CWriterBase implementation that will render given Genbank objects in the
/// BED file format (http://http://genome.ucsc.edu/FAQ/FAQformat#format1).
///
/// When assigned from typical Genbank annotations, only locations will be
/// generated. Multi-interval Genbank features will be broken up into multiple
/// single interval BED feature records. Block parameters could be used to
/// encode multi-interval BED records but this is currently not supported.
/// [[I will ad support though upon request]]
///
class NCBI_XOBJWRITE_EXPORT CBedWriter:
    public CWriterBase, public CFeatWriter
//  ============================================================================
{
public:
    /// Constructor.
    /// @param scope
    ///   scope to be used for ID reference resolution (it's OK to create one
    ///   on the fly).
    /// @param ostr
    ///   stream objects should be written to.
    /// @param colCount
    ///   number of columns per output record. Each record in a BED file must
    ///   have the same number of columns. Hence the writer will truncate or
    ///   extend all records to colCount output columns.
    /// @param flags
    ///   any output customization flags.
    ///
    CBedWriter(
        CScope& scope,
        CNcbiOstream& ostr,
        unsigned int colCount=12,
        unsigned int flags=fNormal );

    virtual ~CBedWriter();

    /// Write a raw Seq-annot to the internal output stream.
    /// The Seq-annot is expected to contain a feature table. If so, each
    /// feature will to formatted as a single BED record.
    /// @param annot
    ///   the Seq-annot object to be written.
    /// @param name
    ///   parameter describing the object. Handling will be format specific
    /// @param descr
    ///   parameter describing the object. Handling will be format specific
    /// @return
    ///   true if the Seq-annot was processed.
    ///   false if the Seq-annot did not contain a feature table.
    ///
    bool WriteAnnot(
        const CSeq_annot&,
        const string& = "",
        const string& = "") override;

    bool WriteSeqEntryHandle(
        CSeq_entry_Handle seh,
        const string& asmblyName="",
        const string& asmblyAccession="" ) override;

protected:
    bool xWriteTrackedAnnot(
        const CBedTrackRecord&,
        const CSeq_annot_Handle&);

    bool xWriteAnnotThreeFeatData(
        const CBedTrackRecord&,
        const CSeq_annot_Handle&);

    bool xWriteFeaturesThreeFeatData(
        CThreeFeatManager&,
        feature::CFeatTree&,
        const CMappedFeat&);

    bool xWriteChildrenThreeFeatData(
        CThreeFeatManager&,
        feature::CFeatTree&,
        const CMappedFeat&);

    bool xWriteFeaturesTracked(
        const CBedTrackRecord&,
        feature::CFeatTree&,
        const CMappedFeat&);

    bool xWriteChildrenTracked(
        const CBedTrackRecord&,
        feature::CFeatTree&,
        const CMappedFeat&);

    CScope& m_Scope;
    unsigned int m_colCount;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___BED_WRITER__HPP
