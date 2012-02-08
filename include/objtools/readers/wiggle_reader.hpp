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
 *   WIGGLE file reader
 *
 */

#ifndef OBJTOOLS_READERS___WIGGLEREADER__HPP
#define OBJTOOLS_READERS___WIGGLEREADER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CWiggleRecord;
class CWiggleTrack;
class CTrackData;

//  ----------------------------------------------------------------------------
enum CWiggleLineType
//  ----------------------------------------------------------------------------
{
    TYPE_NONE,
    TYPE_COMMENT,
    TYPE_BROWSER,
    TYPE_TRACK,
    TYPE_DECLARATION_VARSTEP,
    TYPE_DECLARATION_FIXEDSTEP,
    TYPE_DATA_BED,
    TYPE_DATA_VARSTEP,
    TYPE_DATA_FIXEDSTEP
};

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CWiggleReader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
public:
    //
    //  object management:
    //
public:
    CWiggleReader( 
        int =fDefaults );
        
    virtual ~CWiggleReader();
    
    //
    //  object interface:
    //
public:
    enum EWiggleFlags {
        fDefaults = 0,
        fJoinSame = 1<<0,
        fAsByte = 1<<1,
        fAsGraph = 1<<2,
        fDumpStats = 1<<3
    };
    typedef int TFlags;

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
                
    virtual CRef< CSerialObject >
    ReadObject(
        ILineReader&,
        IErrorContainer* =0 );
                
    //
    //  helpers:
    //
protected:
    bool x_ReadLineData( 
        ILineReader&,
        vector<string>& );

    bool x_ProcessLineData(
        const vector<string>&,
        CWiggleTrack*& );

    bool x_ParseSequence(
        ILineReader&,
        CWiggleTrack*&,
        IErrorContainer* );

    void x_ParseDataRecord(
        const vector<string>& parts );

    virtual void x_AssignBrowserData(
        CRef<CSeq_annot>& );
                
    unsigned int x_GetLineType(
        const vector<string>& parts);

    virtual void x_DumpStats(
        CNcbiOstream&,
        CWiggleTrack* );    
    //
    //  data:
    //
protected:
    static const string s_WiggleDelim;
//    TFlags m_Flags;
    unsigned int m_uCurrentRecordType;
    CWiggleRecord* m_pControlData;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___WIGGLEREADER__HPP
