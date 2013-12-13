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
 * File Description:  Write source quailiers
 *
 */

#ifndef OBJTOOLS_WRITERS___SRC_WRITER__HPP
#define OBJTOOLS_READERS___SRC_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seqtable/Seq_table.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CSrcWriter:
    public CObject
//  ============================================================================
{
public:
    typedef map<string, int> COLUMNMAP;
    typedef vector<string> FIELDS;
    typedef bool (CSrcWriter::*HANDLER)(const CBioSource&);

public:
    static const string DELIMITER;
    static const string keyId;
    static const string keyTaxname;
    static const string keyGenome;
    static const string keyOrigin;
    static const string keyDivision;
    static const string keyPcrPrimersFwdNames;
    static const string keyPcrPrimersFwdSequences;
    static const string keyPcrPrimersRevNames;
    static const string keyPcrPrimersRevSequences;
    static const string keyOrgCommon;
    static const string keyOrgnameLineage;

    CSrcWriter(
        unsigned int flags=0 ) :
        mFlags( flags ) {
        xInit();
    };

    virtual ~CSrcWriter()
    {};

    virtual bool WriteBioseqHandle( 
        CBioseq_Handle,
        const FIELDS&,
        CNcbiOstream&);

    virtual bool WriteBioseqHandles( 
        const vector<CBioseq_Handle>&,
        const FIELDS&,
        CNcbiOstream&);

    static bool ValidateFields(
        const FIELDS fields);

protected:
    void xInit();

    virtual bool xGather(CBioseq_Handle, const FIELDS&);
    virtual bool xGatherId(CBioseq_Handle);
    virtual bool xHandleSourceField(const CBioSource&, const string&);

    virtual bool xGatherTaxname(const CBioSource&);
    virtual bool xGatherDivision(const CBioSource&);
    virtual bool xGatherGenome(const CBioSource&);
    virtual bool xGatherOrigin(const CBioSource&);
    virtual bool xGatherSubtype(const CBioSource&);
    virtual bool xGatherOrgMod(const CBioSource&);
    virtual bool xGatherOrgCommon(const CBioSource&);
    virtual bool xGatherOrgnameLineage(const CBioSource&);
    virtual bool xGatherPcrPrimers(const CBioSource&);

    virtual bool xFormatTabDelimited(CNcbiOstream&);

    HANDLER xGetHandler(const string&);
    static string xPrimerSetNames(const CPCRPrimerSet&);
    static string xPrimerSetSequences(const CPCRPrimerSet&);

    void xPrepareTableColumn(const string&, const string&, const string& ="");
    void xAppendColumnValue(const string&, const string&);

public:
    static const FIELDS sRecognizedFields;
    static const FIELDS sDefaultFields;

protected:
    CRef<CSeq_table> mSrcTable;
    COLUMNMAP mColnameToIndex;
    unsigned int mFlags;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___SRC_WRITER__HPP
