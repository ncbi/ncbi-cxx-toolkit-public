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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author: Frank Ludwig, NCBI
 *
 * File Description:
 *   Convenience wrapper around some of the other feature processing finctions
 *   in the xobjedit library.
 */


#ifndef _FEATTABLE_EDIT_H_
#define _FEATTABLE_EDIT_H_

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objmgr/scope.hpp>
#include <objtools/readers/message_listener.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

class NCBI_XOBJEDIT_EXPORT CFeatTableEdit
{
    typedef list<CRef<CSeq_feat> > FEATS;

public:
    CFeatTableEdit(
        CSeq_annot&,
		const string& = "",
        IMessageListener* =0);
    ~CFeatTableEdit();

	void GenerateLocusTags();
    void InferParentMrnas();
    void InferParentGenes();
    void InferPartials();
    void EliminateBadQualifiers();
    void GenerateProteinAndTranscriptIds();
    void GenerateOrigProteinAndOrigTranscriptIds();
    void GenerateLocusIds();

protected:
    void xGenerateLocusIdsUseExisting();
    void xGenerateLocusIdsRegenerate();

    string xNextFeatId();
	string xNextLocusTag();
	string xNextProteinId(
		const CSeq_feat&);
	string xNextTranscriptId(
		const CSeq_feat&);
    string xCurrentTranscriptId(
        const CSeq_feat&);

    void xPutErrorMissingLocustag(
        CMappedFeat);
    void xPutErrorMissingTranscriptId(
        CMappedFeat);
    void xPutErrorMissingProteinId(
        CMappedFeat);

	CConstRef<CSeq_feat> xGetGeneParent(
		const CSeq_feat&);
	CConstRef<CSeq_feat> xGetMrnaParent(
		const CSeq_feat&);
    CRef<CSeq_feat> xMakeGeneForMrna(
        const CSeq_feat&);

    CSeq_annot& mAnnot;
    CRef<CScope> mpScope;
    CSeq_annot_Handle mHandle;
    CSeq_annot_EditHandle mEditHandle;
    IMessageListener* mpMessageListener;
    unsigned int mNextFeatId;
	unsigned int mLocusTagNumber;
	string mLocusTagPrefix;

	map<string, int> mMapProtIdCounts;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

