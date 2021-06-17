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
 * Author: Frank Ludwig
 *
 * File Description:
 *   BED file reader
 *
 */

#ifndef OBJTOOLS_READERS___MICROARRAYREADER__HPP
#define OBJTOOLS_READERS___MICROARRAYREADER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objtools/readers/reader_base.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CReaderListener;

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CMicroArrayReader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
public:
    enum {
        fDefaults = 0,
        fReadAsBed = (1 << 0),          // discard MicroArray specific columns
                                        //  and produce regular BED seq-annot
    };

public:
    CMicroArrayReader(
        int =fDefaults,
        CReaderListener* = nullptr );

    virtual ~CMicroArrayReader();

    CRef<CSeq_annot>
    ReadSeqAnnot(
        ILineReader&,
        ILineErrorListener* = nullptr ) override;

protected:
    CRef<CSeq_annot> xCreateSeqAnnot() override;

    void xGetData(
        ILineReader&,
        TReaderData&) override;

    void xProcessData(
        const TReaderData&,
        CSeq_annot&) override;

    virtual bool xProcessTrackLine(
        const string&);

    bool xProcessFeature(
        const string&,
        CSeq_annot&);

    void xSetFeatureLocation(
        CRef<CSeq_feat>&,
        const vector<string>& );

    void xSetFeatureDisplayData(
        CRef<CSeq_feat>&,
        const vector<string>& );

    static void xCleanColumnValues(
        vector<string>&);

protected:
    string m_currentId;
    vector<string>::size_type m_columncount;
    bool m_usescore;
    string m_strExpNames;
    int m_iExpScale;
    int m_iExpStep;
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___MICROARRAYREADER__HPP
