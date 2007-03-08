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
 *   Repeat Masker file reader
 *
 */

#ifndef OBJTOOLS_READERS___RMREADER__HPP
#define OBJTOOLS_READERS___RMREADER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//-----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CRmReader
//-----------------------------------------------------------------------------
{
    //
    //  object management:
    //
protected:
    CRmReader( CNcbiIstream& );
public:
    virtual ~CRmReader();
    
    //
    //  interface:
    //
public:
    static CRmReader* OpenReader( CNcbiIstream& );
    static void CloseReader( CRmReader* );

    enum ERmFlags {
        fIncludeStatistics   = 0x01,
        fIncludeRepeatName   = 0x02,
        fIncludeRepeatClass  = 0x04,

        fDefaults = fIncludeRepeatName
    };
    typedef int TFlags;
    virtual void Read( CRef<CSeq_annot>, TFlags flags = fDefaults) =0;

    //
    //  data:
    //
protected:
    CNcbiIstream& m_InStream;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___RMREADER__HPP
