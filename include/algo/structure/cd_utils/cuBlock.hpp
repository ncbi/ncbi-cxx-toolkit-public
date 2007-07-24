#ifndef CU_BLOCK_HPP
#define CU_BLOCK_HPP

#include <algo/structure/cd_utils/cuCppNCBI.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

//result of SubjectBlock - objectBlock
struct DeltaBlock
{
	int subjectBlockID;
	int objectBlockID;
	int deltaStart;
	int deltaLen;
	//to be used by sorting on subjectBlockID
	bool operator<(const DeltaBlock& rhs)const { return subjectBlockID < rhs.subjectBlockID;}
};

class NCBI_CDUTILS_EXPORT Block
{
public:
	Block ();
	Block(const Block& rhs);
	Block (int start, int len, int id);
	Block (int start, int len);
	const Block& operator=(const Block& rhs);
	//for sorting on m_start
	bool operator<(const Block rhs)const { return m_start < rhs.m_start;}
	bool operator==(const Block& rhs) const;
	bool operator!=(const Block& rhs) const;
	bool contain(const Block& rhs) const;
	bool isIntersecting(const Block& rhs)const ;
	bool intersect(Block& rhs)const ;
	//this + delta
	Block operator+(const DeltaBlock& deltaBlock) const;
	//this-object
	DeltaBlock operator-(const Block& object)const;
	//friend ResidualBlocks operator- (const Block& rhs);
	int getLen() const { return m_len;}
	int getStart() const { return m_start;}
	int getEnd() const { return m_start + m_len -1;}
	void setEnd(int end) { m_len = end - m_start + 1;}
	int getId() const {return m_id;}
	void setId(int id)  {m_id = id;}
	Block extend (int nExt, int cExt) const;
	void extendSelf (int nExt, int cExt);
	void addOffset(int nExt);
	typedef set<Block> SortedBlocks;
	static bool concatenate(const SortedBlocks& blocks, Block& comBlock);
	//make sure this contain(this+delta); shrink it if necessary
	//return true if shrinking happened
	//bool retrofit(DeltaBlock& delta)const;
	bool isValid() const {return m_len > 0 && m_start >= 0;}
private:
	int m_len;
	int m_start;
	int m_id;  //-1 is a invalid block
};


typedef multiset<DeltaBlock> DeltaBlockModel;
string DeltaBlockModelToString(const DeltaBlockModel& dbm);
//typedef auto_ptr<DeltaBlockModel> DeltaBlockModelPtr;

class NCBI_CDUTILS_EXPORT BlockModel
{
public:
	BlockModel() ;
	BlockModel(const CRef< CSeq_align> seqAlign, bool forSlave=true);
	//withOneBlock=true add one (0,1) block
	BlockModel(CRef< CSeq_id > sedId, bool withOneBlock=true);
	BlockModel(const BlockModel& rhs);

	void addBlock(Block& block);
    //  True if same seq_ids and same number of blocks.
	bool isAlike(const BlockModel& rhs) const ;
	bool operator==(const BlockModel& rhs) const;

    //  True if isAlike passes, and each block of this contains the 
    //  corresponding block of rhs.
	bool contain(const BlockModel& rhs) const;
	//delta = this - bm
	//attempt to represent the block structure of this in delta
	pair<DeltaBlockModel*, bool> operator-(const BlockModel& bm) const;
	pair<DeltaBlockModel*, bool> intersect(const BlockModel& bm) const;
	//return this + delta
	// the result will take the block structure in delta
	pair<BlockModel*, bool> operator+(const DeltaBlockModel& delta) const;
	BlockModel& operator=(const BlockModel& rhs);
	//complete cast this to target.  return 0 if fails
	BlockModel* completeCastTo(const BlockModel& target) const;
	//BlockModel& operator+=(const BlockModel& delta);
	//BlockModel operator-(const BlockModel& rhs);
	void findIntersectingBlocks(const Block& target, vector<int>& result)const ;
	vector<Block>& getBlocks() {return m_blocks;}
	Block& getBlock(int bn) {return m_blocks[bn];}
//	int getBlockNumber(int pos);
	int getBlockNumber(int pos) const;
	const vector<Block>& getBlocks() const{return m_blocks;}
	CRef< CSeq_id > getSeqId() const{ return m_seqId;}
	void setSeqId(CRef< CSeq_id > seqId) {m_seqId = seqId;}
	CRef<CSeq_align> toSeqAlign(const BlockModel& master) const;
    //  True if number of blocks and each of the block's lengths are the same.
    bool blockMatch(const BlockModel& bm)const;
    //  True if the complete model (from completeCastTo, operator+) would imply inclusion 
    //  of residues from the gapped region (beyond some maximum number allowed) of 'this' 
    //  into the casted block model.  'commonBlockExt' gives the maximum permitted extension
    //  of a block of this:  if NULL, zero extension permitted.  If non-NULL, the first entry
    //  is the max N-terminal extension to the N-terminal-most block, and the remaining
    //  entries are max C-terminal extensions of each block.
    bool completeModelExtendsIntoUnallowedGappedRegion(const BlockModel& completeModel, int sequenceLength, const vector<int>* commonBlockExt = NULL) const;
	int getLastAlignedPosition()const ;
	int getFirstAlignedPosition()const;
	int getTotalBlockLength() const;
	int getGapToNTerminal(int bn) const;
	int getGapToCTerminal(int bn, int len=-1)const ;
	bool isValid(int seqLen, int& errBlock) const;
	bool overlap(const BlockModel& bm)const;
	void addOffset(int nExt);

    //  Both versions of 'mask' modify the blocks so that no residues in 'maskBlocks' remain.
    //  In the first version, the seqId of this object and maskBlockModel must match to proceed.
    //  Both return true if this object was modified; false if there was no effect. 
    bool mask(const BlockModel& maskBlockModel);
    bool mask(const vector<Block>& maskBlocks);

    //  Truncate the current block model to be within the interval [min, max].
    void clipToRange(unsigned int min, unsigned max);


    string toString() const;
    static string toString(const BlockModel& bm);
	
private:
	vector<Block> m_blocks;
	CRef< CSeq_id > m_seqId;
	//assume delta = AnotherBlockModel - this
	//void retrofit(DeltaBlockModel& delta) const;
	bool intersectOneBlock(const Block& aBlock, DeltaBlockModel& delta) const;
	bool minusOneBlock(const Block& aBlock, DeltaBlockModel& delta) const;
};

class NCBI_CDUTILS_EXPORT BlockModelPair
{
public:
	BlockModelPair();
	BlockModelPair(const CRef< CSeq_align> seqAlign);
	//deep copy
	BlockModelPair(const BlockModelPair& rhs);
	~BlockModelPair();

    BlockModelPair& operator=(const BlockModelPair& rhs);

	BlockModel& getMaster();
	const BlockModel& getMaster()const;
	BlockModel& getSlave();
	const BlockModel& getSlave()const;

	void degap();
	CRef<CSeq_align> toSeqAlign() const;
//	int mapToMaster(int slavePos);
	int mapToMaster(int slavePos) const;
	int mapToSlave(int masterPos) const;
	bool isValid()const;
	//assume this.master is the same as guide.master
	//change this.master to guide.slave
	int remaster(const BlockModelPair& guide);

    //  Returns true if this object was modified; false if there was no effect.
    //  Mask using blocks and not a BlockModel to factor out logic to verify
    //  SeqIds are common between the pair and the mask.  Assumes this 
    //  was already done.  Also assumes that the this BlockModelPair is valid.
    bool mask(const vector<Block>& maskBlocks, bool maskBasedOnMaster);

	//reverse the master vs slave
	void reverse();
    //  clear out m_master, m_slave.
    void reset();
private:
	void extendMidway(int blockNum);
	
	BlockModel* m_master;
	BlockModel* m_slave;
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif

