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
#include <util/line_reader.hpp>
#include <util/icanceled.hpp>
#include <objtools/readers/line_error.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CSeq_entry;
class ILineErrorListener;
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
        /// numeric identifiers are local IDs
        fNumericIdsAsLocal  = 1<<0,
        /// all identifiers are local IDs
        fAllIdsAsLocal      = 1<<1,

        fNextInLine = 1<<2,

        fAsRaw = 1<<3,
    };
    typedef unsigned int TReaderFlags;
    enum ObjectType {
        OT_UNKNOWN,
        OT_SEQANNOT,
        OT_SEQENTRY
    };

protected:
    /// Protected constructor. Use GetReader() to get an actual reader object.
    CReaderBase(
        TReaderFlags flags = 0,     //flags
        const string& name = "",    //annot name
        const string& title = "" ); //annot title


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
        TReaderFlags flags =0 );

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
        ILineErrorListener* pErrors=0 );
                
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
        ILineErrorListener* pErrors=0 ) =0;
    
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
        ILineErrorListener* pErrors=0 );
                
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
        ILineErrorListener* pErrors=0 );

    /// Read all objects from given insput stream, returning them as a vector of
    /// Seq-annots.
    /// @param annots
    ///   (out) vector containing read Seq-annots
    /// @param istr
    ///   input stream to read from.
    /// @param pErrors
    ///   pointer to optional error container object. 
    ///  
    virtual void
    ReadSeqAnnots(
        vector< CRef<CSeq_annot> >& annots,
        CNcbiIstream& istr,
        ILineErrorListener* pErrors=0 );
                        
    /// Read all objects from given insput stream, returning them as a vector of
    /// Seq-annots.
    /// @param annots
    ///   (out) vector containing read Seq-annots
    /// @param lr
    ///   line reader to read from.
    /// @param pErrors
    ///   pointer to optional error container object. 
    ///  
    virtual void
    ReadSeqAnnots(
        vector< CRef<CSeq_annot> >& annots,
        ILineReader& lr,
        ILineErrorListener* pErrors=0 );
                        
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
        ILineErrorListener* pErrors=0 );
                
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
        ILineErrorListener* pErrors=0 );
       
    void
    SetProgressReportInterval(
        unsigned int intv );

    void
    SetCanceler(
        ICanceled* =0);

    bool
    IsCanceled() const { return m_pCanceler && m_pCanceler->IsCanceled(); };

protected:
    virtual bool xGetLine(
        ILineReader&,
        string&);

    virtual bool xUngetLine(
        ILineReader&);

    virtual bool xIsCommentLine(
        const CTempString& );

    virtual bool xIsTrackLine(
        const CTempString& );

    virtual bool xIsBrowserLine(
        const CTempString& );

    virtual void xAssignTrackData(
        CRef<CSeq_annot>& );
                
    virtual bool xParseBrowserLine(
        const string&,
        CRef<CSeq_annot>&,
        ILineErrorListener*);
        
    virtual bool xParseTrackLine(
        const string&,
        ILineErrorListener*);
        
    virtual bool xParseBrowserLine(
        const string&,
        ILineErrorListener*);
        
    virtual void xSetBrowserRegion(
        const string&,
        CAnnot_descr&,
        ILineErrorListener*);

    virtual void xSetTrackData(
        CRef<CSeq_annot>&,
        CRef<CUser_object>&,
        const string&,
        const string&);
                
    virtual void xAddConversionInfo(
        CRef< CSeq_annot >&,
        ILineErrorListener* );
                    
    virtual void xAddConversionInfo(
        CRef< CSeq_entry >&,
        ILineErrorListener*);
    
    virtual CRef<CUser_object> xMakeAsnConversionInfo(
        ILineErrorListener*);

    bool xParseComment(
        const CTempString&,
        CRef<CSeq_annot>&);

    virtual bool xReadInit();

    virtual bool xProgressInit(
        ILineReader& istr);

    void xReportProgress(
        ILineErrorListener* =0 );

    bool xIsReportingProgress() const;

    
    bool xIsOperationCanceled() const;

    void
    ProcessError(
        CObjReaderLineException&,
        ILineErrorListener* );
        
    void
    ProcessError(
        CLineError&,
        ILineErrorListener* );
        
    void
    ProcessWarning(
        CObjReaderLineException&,
        ILineErrorListener* );
        
    void
    ProcessWarning(
        CLineError&,
        ILineErrorListener* );
        
    //
    //  Data:
    //
    unsigned int m_uLineNumber;
    unsigned int m_uProgressReportInterval;
    unsigned int m_uNextProgressReport;

    TReaderFlags       m_iFlags;
    string m_AnnotName;
    string m_AnnotTitle;

    CTrackData*  m_pTrackDefaults;
    ILineReader* m_pReader;
    ICanceled* m_pCanceler;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___READERBASE__HPP
