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
* Revision 1.4  2001/03/28 23:01:38  thiessen
* first working full threading
*
* Revision 1.3  2001/03/22 00:32:36  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.2  2001/02/13 01:03:03  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.1  2001/02/08 23:01:24  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* ===========================================================================
*/

#ifndef CN3D_THREADER__HPP
#define CN3D_THREADER__HPP

#include <corelib/ncbistl.hpp>

#include <map>
#include <list>
#include <vector>

#include "cn3d/vector_math.hpp"

// to avoid having to include many C-toolkit headers here
struct bioseq;
typedef struct bioseq Bioseq_fwddecl;
struct _Seq_Mtf;
typedef struct _Seq_Mtf Seq_Mtf_fwddecl;
struct _Cor_Def;
typedef struct _Cor_Def Cor_Def_fwddecl;
struct _Qry_Seq;
typedef struct _Qry_Seq Qry_Seq_fwddecl;
struct _Rcx_Ptl;
typedef struct _Rcx_Ptl Rcx_Ptl_fwddecl;
struct _Gib_Scd;
typedef struct _Gib_Scd Gib_Scd_fwddecl;
struct _Fld_Mtf;
typedef struct _Fld_Mtf Fld_Mtf_fwddecl;


BEGIN_SCOPE(Cn3D)

class BlockMultipleAlignment;
class Sequence;
class Molecule;

class Threader
{
public:
    Threader(void);
    ~Threader(void);

    // create new BlockMultipleAlignments from the given multiple and master/slave pairs
    typedef std::list < BlockMultipleAlignment * > AlignmentList;
    bool Realign(const BlockMultipleAlignment *masterMultiple,
        const AlignmentList *originalAlignments, AlignmentList *newAlignments);

    // to hold virtual residue, sidechain positions
    enum { MISSING_COORDINATE = 0, VIRTUAL_RESIDUE, VIRTUAL_PEPTIDE };
    typedef struct {
        unsigned char type;
        Vector coord;
        int disulfideWith;    // if Cysteine, virtual coord index of any disulfide-bound Cys; -1 otherwise
    } VirtualCoordinate;
    typedef std::vector < VirtualCoordinate > VirtualCoordinateList;

    // for (temporary) storage of contacts
    typedef struct {
        int
          vc1, vc2,     // virtual coord index
          distanceBin;
    } Contact;
    typedef std::list < Contact > ContactList;

private:
    // holds Bioseqs associated with the current Sequences
    typedef std::map < const Sequence *, Bioseq_fwddecl * > BioseqMap;
    BioseqMap bioseqs;

    // holds Fld_Mtf structures already calculated for a given Molecule
    typedef std::map < const Molecule *, Fld_Mtf_fwddecl * > ContactMap;
    ContactMap contacts;

    // threading structure setups
    Seq_Mtf_fwddecl * CreateSeqMtf(const BlockMultipleAlignment *multiple, double weightPSSM);
    Cor_Def_fwddecl * CreateCorDef(const BlockMultipleAlignment *multiple, double loopLengthMultiplier);
    Qry_Seq_fwddecl * CreateQrySeq(const BlockMultipleAlignment *multiple,
        const BlockMultipleAlignment *pairwise);
    Rcx_Ptl_fwddecl * CreateRcxPtl(double weightContacts);
    Gib_Scd_fwddecl * CreateGibScd(bool fast, int nRandomStarts);
    Fld_Mtf_fwddecl * CreateFldMtf(const Sequence *masterSequence);

    // creates Bioseq from Sequence; registed with SeqMgr and stored locally
    void CreateBioseq(const Sequence *sequence);
    void CreateBioseqs(const BlockMultipleAlignment *multiple);

    Bioseq_fwddecl * GetBioseq(const Sequence *sequence)
    {
        BioseqMap::const_iterator b = bioseqs.find(sequence);
        return ((b != bioseqs.end()) ? b->second : NULL);
    }
};

END_SCOPE(Cn3D)

#endif // CN3D_THREADER__HPP
