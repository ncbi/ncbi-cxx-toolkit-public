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
 * File Description:  Stubs for various writer modules
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
/// Defines and provides stubs for a general interface to a variety of file
/// formatters. These writers take Genbank object in raw or handle form, and 
/// render them to an output stream in their respective formats.
///
class NCBI_XOBJWRITE_EXPORT CWriterBase:
    public CObject
//  ============================================================================
{
public:
    /// Customization flags that are relevant to all CWriterBase derived writers.
    ///
    typedef enum {
        fNormal = 0,
    } TFlags;
    
protected:
    CWriterBase(
        CNcbiOstream& ostr,
        unsigned int uFlags=0 ) :
        m_Os( ostr ),
        m_uFlags( uFlags )
    {};
public:
    virtual ~CWriterBase()
    {};

    /// Write a raw Seq-annot to the internal output stream.
    /// This implementation will just generate an error and then exit. It should
    /// be re-implemented in format specific subclasses.
    /// @param annot
    ///   the Seq-annot object to be written.
    /// @param name
    ///   parameter describing the object. Handling will be format specific
    /// @param descr
    ///   parameter describing the object. Handling will be format specific
    ///
    virtual bool WriteAnnot( 
        const CSeq_annot& annot,
        const string& name="",
        const string& descr="" )
    {
        cerr << "Object type not supported!" << endl;
        return false;
    };

    /// Write a raw Seq-align to the internal output stream.
    /// This implementation will just generate an error and then exit. It should
    /// be re-implemented in format specific subclasses.
    /// @param align
    ///   the Seq-align object to be written.
    /// @param name
    ///   parameter describing the object. Handling will be format specific.
    /// @param descr
    ///   parameter describing the object. Handling will be format specific.
    ///
    virtual bool WriteAlign( 
        const CSeq_align&,
        const string& name="",
        const string& descr="" ) 
    {
        cerr << "Object type not supported!" << endl;
        return false;
    };

    /// Write a Seq-entry handle to the internal output stream.
    /// This implementation will just generate an error and then exit. It should
    /// be re-implemented in format specific subclasses.
    /// @param seh
    ///   the Seq-entry handle to be written.
    /// @param name
    ///   parameter describing the object. Handling will be format specific.
    /// @param descr
    ///   parameter describing the object. Handling will be format specific.
    ///
    virtual bool WriteSeqEntryHandle(
        CSeq_entry_Handle seh,
        const string& = "",
        const string& = "" )
    {
        cerr << "Object type not supported!" << endl;
        return false;
    };

    /// Write a Bioseq handle to the internal output stream.
    /// This implementation will just generate an error and then exit. It should
    /// be re-implemented in format specific subclasses.
    /// @param bsh
    ///   the Bioseq handle to be written.
    /// @param name
    ///   parameter describing the object. Handling will be format specific.
    /// @param descr
    ///   parameter describing the object. Handling will be format specific.
    ///
    virtual bool WriteBioseqHandle(
        CBioseq_Handle bsh,
        const string& = "",
        const string& = "" )
    {
        cerr << "Object type not supported!" << endl;
        return false;
    };

    /// Write a Seq-annot handle to the internal output stream.
    /// This implementation will just generate an error and then exit. It should
    /// be re-implemented in format specific subclasses.
    /// @param sah
    ///   the Seq-annot handle to be written.
    /// @param name
    ///   parameter describing the object. Handling will be format specific.
    /// @param descr
    ///   parameter describing the object. Handling will be format specific.
    ///
    virtual bool WriteSeqAnnotHandle(
        CSeq_annot_Handle sah,
        const string& = "",
        const string& = "" )
    {
        cerr << "Object type not supported!" << endl;
        return false;
    };

    /// Write a file header.
    /// Header syntax and rules depend on the file format. This do-nothing
    /// implementation should therefore be re-implemented in format specific
    /// subclasses.
    ///
    virtual bool WriteHeader() { return true; };

    /// Write a file trailer.
    /// Trailer syntax and rules depend on the file format. This do-nothing
    /// implementation should therefore be re-implemented in format specific
    /// subclasses.
    ///
    virtual bool WriteFooter() { return true; };


protected:
    CNcbiOstream& m_Os;
    unsigned int m_uFlags;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___WRITER__HPP
