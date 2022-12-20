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
* Authors:  Jonathan Kans, Clifford Clausen,
*           Aaron Ucko, Sergiy Gotvyanskyy
*
* File Description:
*   table2asn thread state
*
*/

BEGIN_NCBI_SCOPE

using namespace objects;

class CSerialObject;

namespace objects::feature {
    class CFeatTree;
}

struct TAsyncToken
{
    using TFeatMap = map<string, CRef<objects::CSeq_feat>>;

    CRef<CSeq_entry>  entry;
    CRef<CSeq_submit> submit;
    CRef<CSeq_entry>  top_entry;
    CRef<CScope>      scope;
    CSeq_entry_Handle seh;
    CRef<objects::CBioseq> bioseq;
    CRef<objects::feature::CFeatTree> feat_tree;
    TFeatMap map_transcript_to_mrna;
    TFeatMap map_protein_to_mrna;
    TFeatMap map_locus_to_gene;

    operator CConstRef<CSeq_entry>() const;
    void Clear();

    CRef<feature::CFeatTree> FeatTree();
};

END_NCBI_SCOPE
