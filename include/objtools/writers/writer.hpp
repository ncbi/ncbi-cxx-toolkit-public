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
 * File Description:  Stubs for various writer mdules
 *
 */

#ifndef OBJTOOLS_WRITERS___WRITER__HPP
#define OBJTOOLS_WRITERS___WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CWriterBase:
    public CObject
//  ============================================================================
{
public:
    CWriterBase(
        CNcbiOstream& ostr,
        unsigned int uFlags=0 ) :
        m_Os( ostr ),
        m_uFlags( uFlags )
    {};
    virtual ~CWriterBase()
    {};

    //  ------------------------------------------------------------------------
    //  Supported object types:
    //  ------------------------------------------------------------------------
    virtual bool WriteAnnot( 
        const CSeq_annot&,
        const string& = "",
        const string& = "" )
    {
        cerr << "Object type not supported!" << endl;
        return false;
    };
    virtual bool WriteAlign( 
        const CSeq_align&,
        const string& = "",
        const string& = "" )
    {
        cerr << "Object type not supported!" << endl;
        return false;
    };

    virtual bool WriteHeader() { return true; };
    virtual bool WriteFooter() { return true; };

    //  ------------------------------------------------------------------------
    //  Supported handle types:
    //  ------------------------------------------------------------------------
    virtual bool WriteSeqEntryHandle(
        CSeq_entry_Handle,
        const string& = "",
        const string& = "" )
    {
        cerr << "Object type not supported!" << endl;
        return false;
    };
    virtual bool WriteBioseqHandle(
        CBioseq_Handle,
        const string& = "",
        const string& = "" )
    {
        cerr << "Object type not supported!" << endl;
        return false;
    };
    virtual bool WriteSeqAnnotHandle(
        CSeq_annot_Handle,
        const string& = "",
        const string& = "" )
    {
        cerr << "Object type not supported!" << endl;
        return false;
    };

protected:
    CNcbiOstream& m_Os;
    unsigned int m_uFlags;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___WRITER__HPP
