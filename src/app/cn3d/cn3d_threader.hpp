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
* Authors:  Paul Thiessen
*
* File Description:
*       class to isolate and run the threader
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/02/08 23:01:24  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* ===========================================================================
*/

#ifndef CN3D_THREADER__HPP
#define CN3D_THREADER__HPP

#include <corelib/ncbistl.hpp>

#include <map>

// to avoid having to include many C-toolkit headers here
struct bioseq;
typedef struct bioseq Bioseq_fwddecl;
struct _Seq_Mtf;
typedef struct _Seq_Mtf Seq_Mtf_fwddecl;


BEGIN_SCOPE(Cn3D)

class BlockMultipleAlignment;
class Sequence;

class Threader
{
public:
    Threader(void) { }
    ~Threader(void);

    void Test(const BlockMultipleAlignment* multiple);

    // construct a PSSM for the given alignment
    Seq_Mtf_fwddecl * CreateSeqMtf(const BlockMultipleAlignment* multiple, double weightPSSM);

    // creates Bioseq from Sequence; registed with SeqMgr and stored locally
    void CreateBioseq(const Sequence *sequence);
    void CreateBioseqs(const BlockMultipleAlignment *multiple);

private:
    typedef std::map < const Sequence *, Bioseq_fwddecl* > BioseqMap;
    BioseqMap bioseqs;

public:
    Bioseq_fwddecl * GetBioseq(const Sequence *sequence)
    {
        BioseqMap::const_iterator b = bioseqs.find(sequence);
        return ((b != bioseqs.end()) ? b->second : NULL);
    }
};

END_SCOPE(Cn3D)

#endif // CN3D_THREADER__HPP
