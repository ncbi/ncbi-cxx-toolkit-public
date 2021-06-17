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
* Authors:  Chris Lanczycki
*
* File Description:
*      Edit the blocks of a BlockMultipleAlignment object, and provide information
*      on the possible block edits allowed under the block-alignment constraints.
*
* ===========================================================================
*/

#ifndef AR_BLOCK_EDITOR__HPP
#define AR_BLOCK_EDITOR__HPP

#include <string>
#include <vector>
#include <set>
#include <corelib/ncbistd.hpp>

#include <algo/structure/bma_refine/BMAUtils.hpp>
#include <algo/structure/bma_refine/BlockBoundaryAlgorithm.hpp>
//#include <algo/structure/bma_refine/ColumnScorer.hpp>

BEGIN_SCOPE(struct_util)
class AlignmentUtility;
//class BlockMultipleAlignment;
//class UngappedAlignedBlock;
//class Block;
END_SCOPE(struct_util)

//USING_SCOPE(struct_util);
BEGIN_SCOPE(align_refine)


class NCBI_BMAREFINE_EXPORT CBlockedAlignmentEditor
{
public:

//    typedef BlockMultipleAlignment BMA;

    //  Extension location possibilities for blocks
    enum ExtensionLoc {
        eNTerm = 0,  //  n-terminal block extension
        eCTerm,      //  c-terminal block extension
        eBoth,       //  n- and c-terminal block extensions
        eEither,     //  n- or c-terminal block extensions
        eNeither,    //  neither n- nor c-terminal block extensions
        eAny = 255   //  any n- or c-terminal block extensions, including those of size 0
    };

    CBlockedAlignmentEditor(struct_util::AlignmentUtility* au);  //  editor is configured w/ a COPY of au's bma
    CBlockedAlignmentEditor(const BMA* bma);  //  editor is configured w/ a COPY of bma

    const BMA* GetAlignment() const {return m_bma;}  
    void Init(const BMA* bma);  // replace existing alignment w/ a COPY of bma

    virtual ~CBlockedAlignmentEditor() {delete m_bma;}

    unsigned int GetWidth() const;  // return alignment width
    std::string BoundsToString(unsigned int pad = 0) const;  //  pad output w/ spaces

    //  Scan specified column; return true if there is a residue at that position for 
    //  every row.  
    bool IsResidueAtIndexOnAllRows(unsigned alignmentIndex) const;
    unsigned GetSeqIndexForColumn(unsigned alignmentIndex, unsigned row) const;
    bool GetCharacterForColumn(unsigned alignmentIndex, unsigned row, char* residue) const;

    //  By default, only return blocks where either an n- or c-terminal extension is possible.
    //  The 'ed' parameter allows for customization of which types of block extensions to
    //  consider (by default, blocks extendable at either n- or c-terminal end are returned).
    //  ExtendableBlock items contain only those extensions consistent with eloc.  (E.g., eloc = eCTerm
    //  returns ExtendableBlocks with nExt = 0; eEither may have nExt and cExt non-zero.)
    //  If ed = eAny, return an entry for each aligned block even if both extensions are zero.
    //  Returns the size of vector returned.
    unsigned GetExtendableBlocks(std::vector<ExtendableBlock>& extBlocks, ExtensionLoc eloc = eEither) const;

    //  True is block is extendable at ends specified by eloc.  Note that eEither only requires
    //  one terminii be extendible, eBoth requires extensions at both ends; eAny is always true;
    //  eNeither is true only when there are no extensions at both terminii.
    bool IsBlockExtendable(unsigned blockNum, ExtensionLoc eloc = eEither) const;

    //  Globally compute and execute all block boundary movements according to the 
    //  specific BlockBoundaryAlgorithm object passed in, which should already be
    //  configured with a ColumnScorer.  By default, 'eLoc' extends on either end of each block.
    //  If 'editableBlocks' is non-NULL, blocks not in that list are not modified; if NULL, all are modifyable.
    //  Return number of blocks whose boundaries were changed, along w/ the specific
    //  block moves if 'changedBlocks' is non-NULL.
    unsigned int MoveBlockBoundaries(BlockBoundaryAlgorithm& algorithm, ExtensionLoc eLoc, 
                                     const set<unsigned int>* editableBlocks = NULL, vector<ExtendableBlock>* changedBlocks = NULL);

    //  Return true if the boundary moved.
    //  If shift > 0 and exceeds max extension, apply the maximal extension.  
    //  If shift < 0 and in magnitude equals or exceeds the block length, take no action 
    //  and return false.  For shift = 0 or an extension location of eEither, eNeither or eAny, 
    //  return false (i.e. no movement), as does any other ExtensionLoc if the move fails or
    //  maximal extension is zero.  
    bool MoveBlockBoundary(unsigned blockNum, ExtensionLoc eloc, int shift = kMax_Int);

private:

    //  Extend the declared block bounds into unaligned blocks, up to half-way.
    //  Stop the extention at the last alignment index where all sequences have a valid residue.
    void SetExtensionBounds();

    //  Wrapper for Block::GetIndexAt.
    //  Used to hide the block justification parameter;  blockIndex is zero-based; 
    //  blockNum is zero-based (*not* the aligned block number)
    unsigned GetSeqIndexForColumn(unsigned blockIndex, unsigned row, const Block* block, unsigned blockNum) const;

    //  Store the n- and c-terminal-most alignment index to which an aligned block may be extended.
    typedef struct {
        ncbi::CRef<Block> block;  //  will always be an UngappedAlignedBlock*
        unsigned int from;    // alignment index of n-terminal end of block 
        unsigned int to;      // alignment index of c-terminal end of block 
        unsigned int nBound;  // alignment index of n-terminal-most extension
        unsigned int cBound;  // alignment index of c-terminal-most extension
    } BlockExtensionBound;

    BMA* m_bma;
    std::vector<BlockExtensionBound> m_extBound;  // for each aligned block, save the bounds

};

END_SCOPE(align_refine)

#endif // AR_BLOCK_EDITOR__HPP
