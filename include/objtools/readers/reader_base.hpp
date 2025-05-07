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
#include <objtools/readers/track_data.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/reader_message.hpp>
#include <objtools/readers/read_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CSeq_entry;
class ILineErrorListener;
class CObjReaderLineException;
class CTrackData;
class CReaderListener;
class CReaderMessageHandler;

//  ----------------------------------------------------------------------------
/// Defines and provides stubs for a general interface to a variety of file
/// readers. These readers are assumed to read information in some foreign
/// format from an input stream, and render it as an NCBI Genbank object.
///
class NCBI_XOBJREAD_EXPORT CReaderBase
//  ----------------------------------------------------------------------------
{
public:
    struct TReaderLine
    {
        unsigned int mLine = 0;
        string mData;
    };

    using TReaderData = vector<TReaderLine>;
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
    using TReaderFlags = long;
    enum ObjectType {
        OT_UNKNOWN,
        OT_SEQANNOT,
        OT_SEQENTRY
    };
    typedef list< CRef< CSeq_annot > > TAnnotList;
    typedef TAnnotList TAnnots;
    typedef TAnnots::iterator TAnnotIt;
    typedef TAnnots::const_iterator TAnnotCit;

    using SeqIdResolver = CRef<CSeq_id> (*)(const string&, long, bool);

protected:
    /// Protected constructor. Use GetReader() to get an actual reader object.
    CReaderBase(
        TReaderFlags flags = 0,     //flags
        const string& name = "",    //annot name
        const string& title = "",   //annot title
        SeqIdResolver seqresolver = CReadUtil::AsSeqId,
        CReaderListener* pListener = nullptr);

    CReaderBase(
        const CReaderBase&) = delete;

    CReaderBase(
        CReaderBase&&) = delete;

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
        TReaderFlags flags = 0,
        CReaderListener* = nullptr);

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
        ILineErrorListener* pErrors = nullptr);

    /// Read an object from a given line reader, render it as the most
    /// appropriate Genbank object. This will be Seq-annot by default
    /// but may be something else (Bioseq, Seq-entry, ...) in derived
    /// classes.
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
        ILineErrorListener* pErrors = nullptr);

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
        ILineErrorListener* pErrors = nullptr);

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
        ILineErrorListener* pErrors = nullptr);

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
        TAnnots& annots,
        CNcbiIstream& istr,
        ILineErrorListener* pErrors = nullptr);

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
        TAnnots& annots,
        ILineReader& lr,
        ILineErrorListener* pErrors = nullptr);

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
        ILineErrorListener* pErrors = nullptr);

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
        ILineErrorListener* pErrors = nullptr);

    void
    SetProgressReportInterval(
        unsigned int intv );

    void
    SetCanceler(
        ICanceled* = nullptr);

    bool
    IsCanceled() const { return m_pCanceler && m_pCanceler->IsCanceled(); };

protected:
    void xGuardedGetData(
        ILineReader&,
        TReaderData&,
        ILineErrorListener*);

    virtual void xGuardedProcessData(
        const TReaderData&,
        CSeq_annot&,
        ILineErrorListener*);

    virtual CRef<CSeq_annot> xCreateSeqAnnot();

    virtual void xGetData(
        ILineReader&,
        TReaderData&);

    virtual void xProcessData(
        const TReaderData&,
        CSeq_annot&);

    virtual void xValidateAnnot(
        const CSeq_annot&) {};

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

    virtual bool xIsTrackTerminator(
        const CTempString& );

    virtual void xAssignTrackData(
        CSeq_annot& );

    virtual bool xParseBrowserLine(
        const CTempString&,
        CSeq_annot&);

    virtual bool xParseTrackLine(
        const CTempString&);

    virtual void xSetBrowserRegion(
        const CTempString&,
        CAnnot_descr&);

    virtual void xPostProcessAnnot(
        CSeq_annot&);

    virtual void xAddConversionInfo(
        CSeq_annot&,
        ILineErrorListener*);

    bool xParseComment(
        const CTempString&,
        CRef<CSeq_annot>&);

    virtual bool xReadInit();

    virtual bool xProgressInit(
        ILineReader& istr);

    bool xIsReportingProgress() const;

    bool xIsOperationCanceled() const;
    void xReportProgress(
        ILineErrorListener* = nullptr );

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

    void
    xProcessReaderMessage(
        CReaderMessage&,
        ILineErrorListener*);

    void
    xProcessLineError(
        const ILineError&,
        ILineErrorListener*);

    void
    xProcessUnknownException(
        const CException&);

    static void
    xAddStringFlagsWithMap(
        const list<string>& stringFlags,
        const  map<string, TReaderFlags> flagMap,
        TReaderFlags& baseFlags);


    //
    //  Data:
    //
    unsigned int m_uLineNumber;
    std::atomic<unsigned int> m_uDataCount = 0;
    unsigned int m_uProgressReportInterval;
    unsigned int m_uNextProgressReport;

    TReaderFlags m_iFlags;
    string m_AnnotName;
    string m_AnnotTitle;
    string m_PendingLine;

    unique_ptr<CTrackData>  m_pTrackDefaults;
    ILineReader* m_pReader;
    ICanceled* m_pCanceler;
    SeqIdResolver mSeqIdResolve;
    unique_ptr<CReaderMessageHandler> m_pMessageHandler;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___READERBASE__HPP
