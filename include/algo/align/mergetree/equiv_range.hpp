#ifndef EQUIV_RANGE__HPP
#define EQUIV_RANGE__HPP

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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <objects/seqloc/seqloc__.hpp>
#include <objmgr/scope.hpp>
#include <util/range.hpp>



BEGIN_SCOPE(ncbi)

BEGIN_SCOPE(objects)
class CSeq_id;
class CSeq_align;
END_SCOPE(objects)

class CEquivRange;
typedef vector<CEquivRange> TEquivList;

class CEquivRange
{
public:
    CEquivRange() : AlignId(0),SegmtId(0),SplitId(0) { ; }
    
	CRange<TSeqPos> Query;
	CRange<TSeqPos> Subjt;
	objects::ENa_strand Strand;
	int Intercept;
	int Matches;
    int MisMatches;
    vector<TSeqPos> MisMatchSubjtPoints;

    int AlignId;
    int SegmtId;
    int SplitId;

	bool Empty() const {
		return (Query.Empty() || Subjt.Empty());
	}

	bool NotEmpty() const {
		return (Query.NotEmpty() && Subjt.NotEmpty());
	}

	bool IntersectingWith(const CEquivRange& Other) const {
		return (Query.IntersectingWith(Other.Query) ||
				Subjt.IntersectingWith(Other.Subjt));
	}

    bool AbuttingWith(const CEquivRange& Other) const {
        if(Strand != Other.Strand)
            return false;
        
        if (!Query.AbuttingWith(Other.Query) || 
            !Subjt.AbuttingWith(Other.Subjt)) 
            return false;
        

        return true;
    }
	
    
    enum ERelative {
        eWtf = 0x00,
		eIntersects = 0x01,
        eInterQuery = 0x02,
        eInterSubjt = 0x04,
		eBefore = 0x10,
		eAfter  = 0x20,
		eAbove  = 0x40,
		eUnder  = 0x80
	};

    // How is Check positioned relative to this
	ERelative CalcRelative(const CEquivRange& Check) const;
	ERelative CalcRelativeDuo(const CEquivRange& Check) const;

    static TSeqPos Distance(const CEquivRange& A, const CEquivRange& B);
    static TSeqPos Distance(const TEquivList& A,  const TEquivList& B);
};

class CEquivRangeBuilder 
{
public:

    CEquivRangeBuilder();

    bool SplitIntersections(const TEquivList& Originals,
								   TEquivList& Splits);
    bool MergeAbuttings(const TEquivList& Originals, TEquivList& Merges);


	void ExtractRangesFromSeqAlign(const objects::CSeq_align& Alignment, 
											TEquivList& Equivs);
    
    void CalcMatches(objects::CBioseq_Handle QueryHandle, 
                            objects::CBioseq_Handle SubjtHandle,
                            TEquivList& Equivs);
     
private:
    int x_GAlignId;
    int x_GSegmtId;
    int x_GSplitId;
  
    CEquivRange SliceOnQuery(const CEquivRange& Original, const CRange<TSeqPos>& pQuery);
	CEquivRange SliceOnSubjt(const CEquivRange& Original, const CRange<TSeqPos>& pSubjt);
    CEquivRange Merge(const CEquivRange& First, const CEquivRange& Second);

};


CNcbiOstream& operator<<(CNcbiOstream& out, const CEquivRange& range);

string s_RelToStr(CEquivRange::ERelative rel);
	
bool operator==(const CEquivRange& A, const CEquivRange& B);	
bool operator<(const CEquivRange& A, const CEquivRange& B);

bool s_SortEquivBySubjt(const CEquivRange& A, const CEquivRange& B);

END_SCOPE(ncbi)

#endif // end EQUIV_RANGE__HPP
