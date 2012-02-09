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
 *   Basic reader interface
 *
 */

#ifndef OBJTOOLS_READERS___READERBASE__HPP
#define OBJTOOLS_READERS___READERBASE__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <util/format_guess.hpp>
#include <objtools/readers/line_error.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class IErrorContainer;
class CObjReaderLineException;
class CTrackData;

//  ----------------------------------------------------------------------------
/// Defines and provides stubs for a general interface to a variety of file
/// readers. These readers are assumed to read information in some foreign
/// format from an input stream, and render it as an NCBI Genbank object.
///
class NCBI_XOBJREAD_EXPORT CReaderBase
//  ----------------------------------------------------------------------------
{
public:
    /// Customization flags that are relevant to all CReaderBase derived readers.
    ///
    enum EFlags {
        fNormal = 0,
        ///< numeric identifiers are local IDs
        fNumericIdsAsLocal  = 1<0,
        ///< all identifiers are local IDs
        fAllIdsAsLocal      = 1<1,

        fNextInLine = 1<2,
    };
    enum ObjectType {
        OT_UNKNOWN,
        OT_SEQANNOT,
        OT_SEQENTRY
    };

protected:
    /// Protected constructor. Use GetReader() to get an actual reader object.
    CReaderBase(
        unsigned int flags =0);    //flags


public:
    virtual ~CReaderBase();

    /// Allocate a CReaderBase derived reader object based on the given
    /// file format.
    /// @param format
    ///   format specifier as defined in the class CFormatGuess
    /// @param flags
    ///   bit flags as defined in EFlags
    ///
    static CReaderBase* GetReader(
        CFormatGuess::EFormat format,
        unsigned int flags =0 );

    /// Read an object from a given input stream, render it as the most
    /// appropriate Genbank object.
    /// @param istr
    ///   input stream to read from.
    /// @param pErrors
    ///   pointer to optional error container object. 
    ///
    virtual CRef< CSerialObject >
    ReadObject(
        CNcbiIstream& istr,
        IErrorContainer* pErrors=0 );
                
    /// Read an object from a given line reader, render it as the most
    /// appropriate Genbank object.
    /// This is the only function that does not come with a default 
    /// implementation. That is, an implementation must be provided in the
    /// derived class.
    /// @param lr
    ///   line reader to read from.
    /// @param pErrors
    ///   pointer to optional error container object. 
    ///
    virtual CRef< CSerialObject >
    ReadObject(
        ILineReader& lr,
        IErrorContainer* pErrors=0 ) =0;
    
    /// Read an object from a given input stream, render it as a single
    /// Seq-annot. Return empty Seq-annot otherwise.
    /// @param istr
    ///   input stream to read from.
    /// @param pErrors
    ///   pointer to optional error container object. 
    ///  
    virtual CRef< CSeq_annot >
    ReadSeqAnnot(
        CNcbiIstream& istr,
        IErrorContainer* pErrors=0 );
                
    /// Read an object from a given line reader, render it as a single
    /// Seq-annot, if possible. Return empty Seq-annot otherwise.
    /// @param lr
    ///   line reader to read from.
    /// @param pErrors
    ///   pointer to optional error container object. 
    ///  
    virtual CRef< CSeq_annot >
    ReadSeqAnnot(
        ILineReader& lr,
        IErrorContainer* pErrors=0 );

    /// Read an object from a given input stream, render it as a single
    /// Seq-entry, if possible. Return empty Seq-entry otherwise.
    /// @param istr
    ///   input stream to read from.
    /// @param pErrors
    ///   pointer to optional error container object. 
    ///                
    virtual CRef< CSeq_entry >
    ReadSeqEntry(
        CNcbiIstream& istr,
        IErrorContainer* pErrors=0 );
                
    /// Read an object from a given line reader, render it as a single
    /// Seq-entry, if possible. Return empty Seq-entry otherwise.
    /// @param lr
    ///   line reader to read from.
    /// @param pErrors
    ///   pointer to optional error container object. 
    ///                
    virtual CRef< CSeq_entry >
    ReadSeqEntry(
        ILineReader& lr,
        IErrorContainer* pErrors=0 );
                
protected:
    virtual void x_AssignTrackData(
        CRef<CSeq_annot>& );
                
    virtual bool x_ParseBrowserLine(
        const string&,
        CRef<CSeq_annot>& );
        
    virtual bool x_ParseTrackLine(
        const string&,
        CRef<CSeq_annot>& );
        
    virtual void x_SetBrowserRegion(
        const string&,
        CAnnot_descr& );

    virtual void x_SetTrackData(
        CRef<CSeq_annot>&,
        CRef<CUser_object>&,
        const string&,
        const string& );
                
    virtual void x_AddConversionInfo(
        CRef< CSeq_annot >&,
        IErrorContainer* );
                    
    virtual void x_AddConversionInfo(
        CRef< CSeq_entry >&,
        IErrorContainer* );
    
    virtual CRef<CUser_object> x_MakeAsnConversionInfo(
        IErrorContainer* );

    void
    ProcessError(
        CObjReaderLineException&,
        IErrorContainer* );
        
    void
    ProcessError(
        CLineError&,
        IErrorContainer* );
        
    //
    //  Data:
    //
    unsigned int m_uLineNumber;
    int m_iFlags;
    CTrackData* m_pTrackDefaults;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___READERBASE__HPP
