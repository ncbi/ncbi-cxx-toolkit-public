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
 * File Description:  Write bedgraph file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqres/Int_graph.hpp>
#include <objects/seqres/Real_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <objects/seqtable/Seq_table.hpp>
#include <objects/seqtable/SeqTable_column.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/bed_track_record.hpp>
#include <objtools/writers/bed_graph_record.hpp>
#include <objtools/writers/bedgraph_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CBedGraphWriter::CBedGraphWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int colCount,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CWriterBase(ostr, uFlags),
    m_Scope(scope),
    m_colCount(colCount)
{
    // the first three columns are mandatory
    if (m_colCount < 3) {
        m_colCount = 3;
    }
};

//  ----------------------------------------------------------------------------
CBedGraphWriter::~CBedGraphWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CBedGraphWriter::WriteAnnot( 
    const CSeq_annot& annot,
    const string&,
    const string& )
//  ----------------------------------------------------------------------------
{
    m_colCount = 4;
    
    CBedTrackRecord track;
    if (!track.Assign(annot)) {
        return false;
    }
    track.Write(m_Os);
    if (xWriteAnnotGraphs(track, annot)) {
        return true;
    }
    if (xWriteAnnotFeatureTable(track, annot)) {
        return true;
    }
    if (xWriteAnnotSeqTable(track, annot)) {
        return true;
    }
    NCBI_THROW(
        CObjWriterException,
        eBadInput,
        "BedGraph writer does not support this annotation type.");
    return false;
}


//  ----------------------------------------------------------------------------
bool CBedGraphWriter::xWriteAnnotSeqTable(
    const CBedTrackRecord& trackdata,
    const CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if (!annot.IsSeq_table()) {
        return false;
    }
    CBedGraphRecord bedRecord;

    const CSeq_table& table = annot.GetData().GetSeq_table();
    int numRows = table.GetNum_rows();
    for (int row=0; row < numRows; ++row) {
        string chromId;
        {{
            const vector<CRef<CSeqTable_column> > columns = table.GetColumns();
            for (size_t col = 0; col < columns.size(); ++col) {
                const CSeqTable_column_info& header = columns[col]->GetHeader();
                if (header.IsSetField_name()) {
                    string fieldName = header.GetField_name();
                    if (fieldName == "Seq-table location") {
                        CConstRef< CSeq_loc > pLoc = columns[col]->GetSeq_loc(row);
                        pLoc->GetId()->GetLabel(&chromId, CSeq_id::eContent);
                        break;
                    }
                }
                if (header.IsSetField_id()) {
                    int fieldId = header.GetField_id();
                    if (fieldId == CSeqTable_column_info::eField_id_location_id) {
                        CConstRef< CSeq_id > pId = columns[col]->GetSeq_id(row);
                        pId->GetLabel(&chromId, CSeq_id::eContent);
                        break;
                    }
                }
            }
        }}
        if (chromId.empty()) {
            chromId = "unknown";
        }
        bedRecord.SetChromId(chromId);

        int chromStart(0);
        {{
            const vector<CRef<CSeqTable_column> > columns = table.GetColumns();
            for (size_t col = 0; col < columns.size(); ++col) {
                const CSeqTable_column_info& header = columns[col]->GetHeader();
                if (header.IsSetField_id()) {
                    int fieldId = header.GetField_id();
                    if (fieldId == CSeqTable_column_info::eField_id_location_from) {
                        if (columns[col]->TryGetInt(row, chromStart)) {
                            break;
                        }
                    }
                }
            }
        }}
        bedRecord.SetChromStart(chromStart);
        
        int chromEnd(0);
        {{
            const vector<CRef<CSeqTable_column> > columns = table.GetColumns();
            for (size_t col = 0; col < columns.size(); ++col) {
                const CSeqTable_column_info& header = columns[col]->GetHeader();
                if (header.IsSetField_name()) {
                    string fieldName = header.GetField_name();
                    if (fieldName == "span") {
                        if (columns[col]->TryGetInt(row, chromEnd)) {
                            chromEnd += chromStart;
                            break;
                        }
                    }
                }
                if (header.IsSetField_id()) {
                    int fieldId = header.GetField_id();
                    if (fieldId == CSeqTable_column_info::eField_id_location_to) {
                        if (columns[col]->TryGetInt(row, chromEnd)) {
                            break;
                        }
                    }
                }
            }
        }}
        bedRecord.SetChromEnd(chromEnd);

        double chromValue(0);
        {{
            const vector<CRef<CSeqTable_column> > columns = table.GetColumns();
            for (size_t col = 0; col < columns.size(); ++col) {
                const CSeqTable_column_info& header = columns[col]->GetHeader();
                if (header.IsSetField_name()) {
                    string fieldName = header.GetField_name();
                    if (fieldName == "values") {
                        if (columns[col]->TryGetReal(row, chromValue)) {
                            break;
                        }
                        int intValue(0);
                        if (columns[col]->TryGetInt(row, intValue)) {
                            chromValue = intValue;
                            break;
                        } 
                    }
                }
            }
        }}
        bedRecord.SetChromValue(chromValue);
        
        bedRecord.Write(m_Os);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedGraphWriter::xWriteAnnotGraphs(
    const CBedTrackRecord& trackdata,
    const CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if (!annot.IsGraph()) {
        return false;
    }
    const list<CRef<CSeq_graph> >& graphs = annot.GetData().GetGraph();
    for (list<CRef<CSeq_graph> >::const_iterator cit = graphs.begin();
            cit != graphs.end();
            ++cit) {
        try {
            xWriteSingleGraph(trackdata, **cit);
        }
        catch(const CObjWriterException& except) {
            cerr << except.GetMsg() << endl;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CBedGraphWriter::xWriteAnnotFeatureTable(
    const CBedTrackRecord& trackdata,
    const CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if (!annot.IsFtable()) {
        return false;
    }
    const list<CRef<CSeq_feat> >& features = annot.GetData().GetFtable();
    for (list<CRef<CSeq_feat> >::const_iterator cit = features.begin();
            cit != features.end();
            ++cit) {
        try {
            xWriteSingleFeature(trackdata, **cit);
        }
        catch(CObjWriterException& except) {
            cerr << except.GetMsg() << endl;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CBedGraphWriter::xWriteSingleGraph(
    const CBedTrackRecord& trackdata,
    const CSeq_graph& graph)
//  ----------------------------------------------------------------------------
{
    if (graph.GetGraph().IsInt()) {
        return xWriteSingleGraphInt(trackdata, graph);
    }
    if (graph.GetGraph().IsByte()) {
        return xWriteSingleGraphByte(trackdata, graph);
    }
    if (graph.GetGraph().IsReal()) {
        return xWriteSingleGraphReal(trackdata, graph);
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CBedGraphWriter::xWriteSingleFeature(
    const CBedTrackRecord& trackdata,
    const CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    CBedGraphRecord bedRecord;

    const CSeq_loc& location = feature.GetLocation();
    if (!location.IsInt()) {
        NCBI_THROW(
            CObjWriterException,
            eInterrupted,
            "BedGraph writer does not support feature locations that are not intervals.");
    }
    const CSeq_interval& interval = location.GetInt();

    const string& scoreStr = feature.GetNamedQual("score");
    if (scoreStr.empty()) {
        NCBI_THROW(
            CObjWriterException,
            eInterrupted,
            "BedGraph writer only supports features with a \"score\" qualifier.");
    }
    double score = 0;
    try {
        score = NStr::StringToDouble(scoreStr);
    }
    catch(CException&) {
        NCBI_THROW(
            CObjWriterException,
            eInterrupted,
            "BedGraph writer encountered feature with bad \"score\" qualifier.");
    }

    const CSeq_id& id = interval.GetId();
    string recordId;
    id.GetLabel(&recordId);
    bedRecord.SetChromId(recordId);

    bedRecord.SetChromStart(interval.GetFrom());
    bedRecord.SetChromEnd(interval.GetTo()-1);
    bedRecord.SetChromValue(score);
    bedRecord.Write(m_Os);
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedGraphWriter::xWriteSingleGraphInt(
    const CBedTrackRecord& trackdata,
    const CSeq_graph& graph)
//  ----------------------------------------------------------------------------
{
    const CSeq_loc& location = graph.GetLoc();
    string chromId;
    location.GetId()->GetLabel(&chromId);
    size_t chromStart = 0;
    size_t chromStep = graph.GetComp();
    double scale = graph.GetA();
    double offset = graph.GetB();

    CBedGraphRecord bedRecord;
    size_t numValues = graph.GetNumval();
    const vector<int> values = graph.GetGraph().GetInt().GetValues();
    for (size_t valIndex = 0; valIndex < numValues; ++valIndex) {
        bedRecord.SetChromId(chromId);

        size_t recordStart = valIndex * chromStep;
        bedRecord.SetChromStart(recordStart);

        size_t recordEnd = recordStart + chromStep - 1;
        bedRecord.SetChromEnd(recordEnd);

        double value = offset + scale*values[valIndex];
        bedRecord.SetChromValue(value);
        bedRecord.Write(m_Os);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedGraphWriter::xWriteSingleGraphByte(
    const CBedTrackRecord& trackdata,
    const CSeq_graph& graph)
//  ----------------------------------------------------------------------------
{
    const CSeq_loc& location = graph.GetLoc();
    string chromId;
    location.GetId()->GetLabel(&chromId);
    size_t chromStart = 0;
    size_t chromStep = graph.GetComp();
    double scale = graph.GetA();
    double offset = graph.GetB();

    CBedGraphRecord bedRecord;
    size_t numValues = graph.GetNumval();
    const vector<char> values = graph.GetGraph().GetByte().GetValues();
    for (size_t valIndex = 0; valIndex < numValues; ++valIndex) {
        bedRecord.SetChromId(chromId);

        size_t recordStart = valIndex * chromStep;
        bedRecord.SetChromStart(recordStart);

        size_t recordEnd = recordStart + chromStep - 1;
        bedRecord.SetChromEnd(recordEnd);

        double value = offset + scale*values[valIndex];
        bedRecord.SetChromValue(value);
        bedRecord.Write(m_Os);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CBedGraphWriter::xWriteSingleGraphReal(
    const CBedTrackRecord& trackdata,
    const CSeq_graph& graph)
//  ----------------------------------------------------------------------------
{
    const CSeq_loc& location = graph.GetLoc();
    string chromId;
    location.GetId()->GetLabel(&chromId);
    size_t chromStart = 0;
    size_t chromStep = graph.GetComp();
    double scale = graph.GetA();
    double offset = graph.GetB();

    CBedGraphRecord bedRecord;
    size_t numValues = graph.GetNumval();
    const vector<double> values = graph.GetGraph().GetReal().GetValues();
    for (size_t valIndex = 0; valIndex < numValues; ++valIndex) {
        bedRecord.SetChromId(chromId);

        size_t recordStart = valIndex * chromStep;
        bedRecord.SetChromStart(recordStart);

        size_t recordEnd = recordStart + chromStep - 1;
        bedRecord.SetChromEnd(recordEnd);

        double value = offset + scale*values[valIndex];
        bedRecord.SetChromValue(value);
        bedRecord.Write(m_Os);
    }
    return true;
}

END_NCBI_SCOPE
