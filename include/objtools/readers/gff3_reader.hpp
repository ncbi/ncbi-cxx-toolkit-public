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

#ifndef OBJTOOLS_READERS___GFF3_READER__HPP
#define OBJTOOLS_READERS___GFF3_READER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/gff_reader.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

class CGFFReader;

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CGff3Reader
//  ----------------------------------------------------------------------------
    : protected CGFFReader, public CReaderBase
{
    //
    //  object management:
    //
public:
    CGff3Reader( 
        int =0 );

    virtual ~CGff3Reader();
    
    //
    //  object interface:
    //
public:
    unsigned int 
    ObjectType() const { return OT_SEQENTRY; };
    
    CRef< CSeq_entry >
    ReadSeqEntry(
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
    virtual bool x_ParseBrowserLineToSeqEntry(
        const string&,
        CRef<CSeq_entry>& );
        
    virtual bool x_ParseTrackLineToSeqEntry(
        const string&,
        CRef<CSeq_entry>& );
        
    virtual void x_SetTrackDataToSeqEntry(
        CRef<CSeq_entry>&,
        CRef<CUser_object>&,
        const string&,
        const string& );
                
    virtual void x_Info(
        const string& message,
        unsigned int line = 0);

    virtual void x_Warn(
        const string& message,
        unsigned int line = 0);

    virtual void x_Error(
        const string& message,
        unsigned int line = 0);

    bool x_IsCommentLine(
        const string& );

    bool x_ReadLine(
        ILineReader&,
        string& );

    //
    //  data:
    //
protected:
    int m_iReaderFlags;
    IErrorContainer* m_pErrors;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___GFF3_READER__HPP
