#ifndef OBJTOOLS_ALNMGR___ALN_ASN_READER__HPP
#define OBJTOOLS_ALNMGR___ALN_ASN_READER__HPP
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
* Authors:  Kamen Todorov, NCBI
*
* File Description:
*   Reading Seq-aligns from an ASN.1 input stream.
*
*   The stream can be in any CObjectIStream format: text or binary
*   ASN.1, or XML.  If binary, since the type of object cannot be
*   automatically recognized, the top_level_asn_object needs to be
*   provided.
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/iterator.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <objmgr/scope.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// Helper class for reading seq-align objects from a CObjectIStream.
class CAlnAsnReader
{
public:
    /// Create alignment reader. If the scope is not null, the loaded seq-entry
    /// objects are added to the scope.
    CAlnAsnReader(CScope* scope = NULL)
        : m_Scope(scope),
          m_Verbose(false)
    {
    }

    /// Switch verbose report about loaded objects on/off.
    void SetVerbose(bool verbose = true)
    {
        m_Verbose = verbose;
    }

    /// Read all seq-align objects from the stream.
    /// @param obj_in_stream
    ///   The object stream to read from.
    /// @param callback
    ///   Callback receiving the loaded seq-aligns. The callback must accept
    ///   const CSeq_align*.
    /// @param top_level_asn_object
    ///   Name of the top level object type. Required for binary ASN.1 files
    ///   to force the type.
    template <class TCallback>
    void Read(CObjectIStream* obj_in_stream,
              TCallback callback,
              const string& top_level_asn_object = kEmptyStr)
    {
        while ( !obj_in_stream->EndOfData() ) {
            // determine the ASN.1 object type
            string obj = obj_in_stream->ReadFileHeader();
            if ( obj.empty() ) {
                // auto-detection is not possible in ASN.1 binary mode
                // hopefully top_level_asn_object was specified by the user
                if ( top_level_asn_object.empty() ) {
                    NCBI_THROW(CException, eUnknown,
                        "ReadFileHeader() returned empty.  "
                        "Binary ASN.1 file?  "
                        "Please supply the top_level_asn_object.");
                }
                else {
                    obj = top_level_asn_object;
                }
            }
            else {
                if (!top_level_asn_object.empty()  &&  obj != top_level_asn_object) {
                    // object differs from the specified, skip it
                    continue;
                }
            }

            CTypesIterator i;
            CType<CSeq_align>::AddTo(i);

            if (obj == "Seq-entry") {
                CRef<CSeq_entry> se(new CSeq_entry);
                obj_in_stream->Read(Begin(*se), CObjectIStream::eNoFileHeader);
                if ( m_Scope ) {
                    m_Scope->AddTopLevelSeqEntry(*se);
                }
                for (i = Begin(*se); i; ++i) {
                    if ( CType<CSeq_align>::Match(i) ) {
                        callback(CType<CSeq_align>::Get(i));
                    }
                }
            }
            else if (obj == "Seq-submit") {
                CRef<CSeq_submit> ss(new CSeq_submit);
                obj_in_stream->Read(Begin(*ss), CObjectIStream::eNoFileHeader);
                CType<CSeq_entry>::AddTo(i);
                int tse_cnt = 0;
                for (i = Begin(*ss); i; ++i) {
                    if ( CType<CSeq_align>::Match(i) ) {
                        callback(CType<CSeq_align>::Get(i));
                    }
                    else if ( CType<CSeq_entry>::Match(i) ) {
                        if ( !(tse_cnt++) ) {
                            //m_Scope.AddTopLevelSeqEntry
                            (*(CType<CSeq_entry>::Get(i)));
                        }
                    }
                }
            }
            else if (obj == "Seq-align") {
                CRef<CSeq_align> sa(new CSeq_align);
                obj_in_stream->Read(Begin(*sa), CObjectIStream::eNoFileHeader);
                for (i = Begin(*sa); i; ++i) {
                    if ( CType<CSeq_align>::Match(i) ) {
                        callback(CType<CSeq_align>::Get(i));
                    }
                }
            }
            else if (obj == "Seq-align-set") {
                CRef<CSeq_align_set> sas(new CSeq_align_set);
                obj_in_stream->Read(Begin(*sas), CObjectIStream::eNoFileHeader);
                for (i = Begin(*sas); i; ++i) {
                    if ( CType<CSeq_align>::Match(i) ) {
                        callback(CType<CSeq_align>::Get(i));
                    }
                }
            }
            else if (obj == "Seq-annot") {
                CRef<CSeq_annot> san(new CSeq_annot);
                obj_in_stream->Read(Begin(*san), CObjectIStream::eNoFileHeader);
                for (i = Begin(*san); i; ++i) {
                    if ( CType<CSeq_align>::Match(i) ) {
                        callback(CType<CSeq_align>::Get(i));
                    }
                }
            }
            else if (obj == "Dense-seg") {
                CRef<CDense_seg> ds(new CDense_seg);
                obj_in_stream->Read(Begin(*ds), CObjectIStream::eNoFileHeader);
                CRef<CSeq_align> sa(new CSeq_align);
                sa->SetType(CSeq_align::eType_not_set);
                sa->SetSegs().SetDenseg(*ds);
                sa->SetDim(ds->GetDim());
                callback(sa);
            }
            else {
                if ( obj.empty() ) {
                    NCBI_THROW(CException, eUnknown,
                        "ReadFileHeader() returned empty.  "
                        "Binary ASN.1 file?  "
                        "Please supply the top_level_asn_object.");
                }
                else {
                    cerr << "Don't know how to extract alignments from: " << obj << endl;
                    cerr << "Do you know?  Please contact us at aln-mgr@ncbi.nlm.nih.gov." << endl;
                }
                return;
            }

            if (m_Verbose) {
                cerr << "Finished reading " << obj << "." << endl;
            }

        }            
    }

private:
    CScope* m_Scope;
    bool    m_Verbose;
};


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_ASN_READER__HPP
