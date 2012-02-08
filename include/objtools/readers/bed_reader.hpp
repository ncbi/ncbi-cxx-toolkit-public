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

#ifndef OBJTOOLS_READERS___BEDREADER__HPP
#define OBJTOOLS_READERS___BEDREADER__HPP

#include <corelib/ncbistd.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/error_container.hpp>
#include <objects/seq/Seq_annot.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CBedReader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
    //
    //  object management:
    //
public:
    CBedReader( 
        int =fDefaults );
    virtual ~CBedReader();
    
    //
    //  object interface:
    //
public:
    enum EBedFlags {
        fDefaults = 0,
        fAllIdsAsLocal = 1<<0,
        fNumericIdsAsLocal = 1<<1,
    };
    typedef int TFlags;

    virtual CRef< CSerialObject >
    ReadObject(
        ILineReader&,
        IErrorContainer* =0 );
                
    virtual CRef< CSeq_annot >
    ReadSeqAnnot(
        ILineReader&,
        IErrorContainer* =0 );

    virtual void
    ReadSeqAnnots(
        vector< CRef<CSeq_annot> >&,
        CNcbiIstream&,
        IErrorContainer* =0 );
                        
    virtual void
    ReadSeqAnnots(
        vector< CRef<CSeq_annot> >&,
        ILineReader&,
        IErrorContainer* =0 );
                        
    //
    //  helpers:
    //
protected:
    virtual bool x_ParseTrackLine(
        const string&,
        vector< CRef< CSeq_annot > >&,
        CRef< CSeq_annot >& );
        
    bool x_ParseFeature(
        const string&,
        CRef<CSeq_annot>& );
    /* throws CObjReaderLineException */

    void x_SetFeatureLocation(
        CRef<CSeq_feat>&,
        const vector<string>& );
        
    void x_SetFeatureDisplayData(
        CRef<CSeq_feat>&,
        const vector<string>& );

    virtual void x_SetTrackData(
        CRef<CSeq_annot>&,
        CRef<CUser_object>&,
        const string&,
        const string& );

    CRef< CSeq_annot > x_AppendAnnot(
        vector< CRef< CSeq_annot > >& );
                    
    void
    x_ProcessError(
        CObjReaderLineException&,
        IErrorContainer* );

    //
    //  data:
    //
protected:
    CErrorContainerLenient m_ErrorsPrivate;
    
    vector<string>::size_type m_columncount;
    bool m_usescore;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___BEDREADER__HPP
