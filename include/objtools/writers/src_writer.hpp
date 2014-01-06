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
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqtable/Seq_table.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CSrcError:
    public CLineError
//  ============================================================================
{   
protected: 
    //creates known deprecation warning for now
    CSrcError(const CLineError& other):CLineError(other){};
    CSrcError(
        ncbi::EDiagSev severity,
        const std::string&);

public:
    static CSrcError* Create(
        ncbi::EDiagSev severity,
        const std::string&);
};

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CSrcWriter:
    public CObject
//  ============================================================================
{    
public:
    typedef map<string, int> COLUMNMAP;
    typedef vector<string> FIELDS;
    typedef bool (CSrcWriter::*HANDLER)(const CBioSource&, IMessageListener*);
    typedef map<string, CSrcWriter::HANDLER> HANDLERMAP;

public:
    CSrcWriter(
            unsigned int flags=0) :
        mFlags(flags),
        mDelimiter("\t") {
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
        CNcbiOstream&,
        IMessageListener* = 0);

    void SetDelimiter(
        const string& delimiter) {
        mDelimiter = delimiter;
    };

    static bool ValidateFields(
        const FIELDS fields,
        IMessageListener* = 0);

protected:
    void xInit();

    virtual bool xGather(CBioseq_Handle, const FIELDS&, IMessageListener* =0);
    virtual bool xGatherId(CBioseq_Handle, IMessageListener* =0);
    virtual bool xHandleSourceField(const CBioSource&, const string&, IMessageListener* =0);

    virtual bool xGatherTaxname(const CBioSource&, IMessageListener* =0);
    virtual bool xGatherDivision(const CBioSource&, IMessageListener* =0);
    virtual bool xGatherGenome(const CBioSource&, IMessageListener* =0);
    virtual bool xGatherOrigin(const CBioSource&, IMessageListener* =0);
    virtual bool xGatherSubtype(const CBioSource&, IMessageListener* =0);
    virtual bool xGatherOrgMod(const CBioSource&, IMessageListener* =0);
    virtual bool xGatherOrgCommon(const CBioSource&, IMessageListener* =0);
    virtual bool xGatherOrgnameLineage(const CBioSource&, IMessageListener* =0);
    virtual bool xGatherPcrPrimers(const CBioSource&, IMessageListener* =0);
    virtual bool xGatherDb(const CBioSource&, IMessageListener* =0);

    virtual bool xFormatTabDelimited(CNcbiOstream&);

    static HANDLER xGetHandler(const string&);
    static string xPrimerSetNames(const CPCRPrimerSet&);
    static string xPrimerSetSequences(const CPCRPrimerSet&);
    static bool xIsSubsourceTypeSuppressed(CSubSource::TSubtype);
    static bool xIsOrgmodTypeSuppressed(COrgMod::TSubtype);

    void xPrepareTableColumn(const string&, const string&, const string& ="");
    void xAppendColumnValue(const string&, const string&);
    bool xValueNeedsQuoting(const string&);
    string xDequotedValue(const string&);

public:
    static const FIELDS sDefaultFields;

protected:
    static HANDLERMAP sHandlerMap;
    CRef<CSeq_table> mSrcTable;
    COLUMNMAP mColnameToIndex;
    unsigned int mFlags;
    string mDelimiter;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___SRC_WRITER__HPP
