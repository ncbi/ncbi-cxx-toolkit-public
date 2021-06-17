/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* interface CAli -> new Spliced-seg version of Seq-align
*
* =========================================================================
*/

#ifndef ALISEQALIGN_H
#define ALISEQALIGN_H

#include<objects/seqalign/seqalign__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CScope;
END_SCOPE(objects)
BEGIN_SCOPE(prosplign)
USING_SCOPE(ncbi::objects);

//class CCompart;
class CAli;
class CPosAli;

class CAliToSeq_align
{
    const CAli& m_ali; 
    CScope& m_scope;
    const CSeq_id& m_protein;
    const CSeq_loc& m_genomic;

public:
    CAliToSeq_align(const CAli& ali, CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic);
    CRef<CSeq_align> MakeSeq_align(const CPSeq& cpseq, const CNSeq& cnseq) const;
    static CRef<CSeq_id> StringToSeq_id(const string& id);
    static string Seq_idToString(CRef<CSeq_id> seqid);
    //Takes care of strand
    void SetExonBioStart(CRef<CSpliced_exon> exon, int nulpos, int nultripos) const;
    void SetExonBioEnd(CRef<CSpliced_exon> exon, int nulpos, int nultripos) const;
private:
    int NucPosOut(int pos) const;

};

/// Convert linear coordinate into (amin,frame)
CRef<CProduct_pos> NultriposToProduct_pos(int nultripos);

END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif//ALISEQALIGN_H
