#ifndef NETCACHE__CONCURRENT_BTREE__INL
#define NETCACHE__CONCURRENT_BTREE__INL
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
 * Authors:  Pavel Ivanov
 */


BEGIN_NCBI_SCOPE


#define CONCURMAP_TEMPLATE template <class         Key,             \
                                     class         Value,           \
                                     class         Comparator,      \
                                     unsigned int  NodesMaxFill,    \
                                     unsigned int  DeletionDelay,   \
                                     unsigned int  DelStoreCapacity,\
                                     unsigned int  MaxTreeHeight>
#define CONCURMAP          CConcurrentMap<Key,                      \
                                          Value,                    \
                                          Comparator,               \
                                          NodesMaxFill,             \
                                          DeletionDelay,            \
                                          DelStoreCapacity,         \
                                          MaxTreeHeight>


CONCURMAP_TEMPLATE
inline
CONCURMAP::SKeyData::SKeyData(const TKey& _key)
    : key(_key)
{
    ref_cnt.Set(1);
}


CONCURMAP_TEMPLATE
inline void
CONCURMAP::CKeyRef::x_AddRef(void)
{
    if (m_KeyData)
        m_KeyData->ref_cnt.Add(1);
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CKeyRef::x_RemoveRef(void)
{
    if (m_KeyData != NULL  &&  m_KeyData->ref_cnt.Add(-1) == 0) {
        delete m_KeyData;
    }
}

CONCURMAP_TEMPLATE
inline
CONCURMAP::CKeyRef::CKeyRef(void)
    : m_KeyData(NULL)
{}

CONCURMAP_TEMPLATE
inline
CONCURMAP::CKeyRef::CKeyRef(const CKeyRef& key_ref)
    : m_KeyData(key_ref.m_KeyData)
{
    x_AddRef();
}

CONCURMAP_TEMPLATE
inline
CONCURMAP::CKeyRef::~CKeyRef(void)
{
    x_RemoveRef();
}

CONCURMAP_TEMPLATE
inline typename CONCURMAP::CKeyRef&
CONCURMAP::CKeyRef::operator= (const CKeyRef& key_ref)
{
    x_RemoveRef();
    m_KeyData = key_ref.m_KeyData;
    x_AddRef();
    return *this;
}

CONCURMAP_TEMPLATE
inline typename CONCURMAP::CKeyRef&
CONCURMAP::CKeyRef::operator= (const TKey& key)
{
    if (m_KeyData  &&  m_KeyData->ref_cnt.Get() == 1) {
        m_KeyData->key = key;
    }
    else {
        x_RemoveRef();
        m_KeyData = new SKeyData(key);
    }
    return *this;
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::CKeyRef::IsMaximum(void) const
{
    return m_KeyData == NULL;
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::CKeyRef::IsEqual(const CKeyRef& other) const
{
    // A bit hack-ish but it should suffice for the internal usage.
    return m_KeyData == other.m_KeyData;
}

CONCURMAP_TEMPLATE
inline const Key&
CONCURMAP::CKeyRef::GetKey(void) const
{
    return m_KeyData->key;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CKeyRef::Swap(CKeyRef& other)
{
    swap(m_KeyData, other.m_KeyData);
}


CONCURMAP_TEMPLATE
inline
CONCURMAP::CNode::CNode(const TTree* const  tree,
                        const CKeyRef&      max_key,
                        unsigned int        deep_level)
    : m_Tree(tree),
      m_MaxKey(max_key),
      m_DeepLevel(deep_level),
      m_CntFilled(0),
      m_RightNode(NULL)
{
    for (int i = 0; i < kNodesMaxFill; ++i) {
        m_Keys  [i] = max_key;
        m_Values[i] = NULL;
        m_Status[i] = eValueDeleted;
    }
}

CONCURMAP_TEMPLATE
inline
CONCURMAP::CNode::~CNode(void)
{
    _ASSERT(m_CntFilled == 0);
    if (m_DeepLevel == 0)
        x_ClearValues();
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CNode::Lock(ERWLockType lock_type)
{
    if (lock_type == eReadLock)
        m_NodeLock.ReadLock();
    else
        m_NodeLock.WriteLock();
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CNode::Unlock(ERWLockType lock_type)
{
    if (lock_type == eReadLock)
        m_NodeLock.ReadUnlock();
    else
        m_NodeLock.WriteUnlock();
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::CNode::GetCntFilled(void)
{
    return m_CntFilled;
}

CONCURMAP_TEMPLATE
inline const typename CONCURMAP::CKeyRef&
CONCURMAP::CNode::GetMaxKey(void)
{
    return m_MaxKey;
}

CONCURMAP_TEMPLATE
inline const typename CONCURMAP::CKeyRef&
CONCURMAP::CNode::GetKey(unsigned int index)
{
    return m_Keys[index];
}

CONCURMAP_TEMPLATE
inline Value*
CONCURMAP::CNode::GetValue(unsigned int index)
{
    return reinterpret_cast<TValue*>(m_Values[index]);
}

CONCURMAP_TEMPLATE
inline typename CONCURMAP::EValueStatus
CONCURMAP::CNode::GetValueStatus(unsigned int index)
{
    return m_Status[index];
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CNode::SetValueStatus(unsigned int index, EValueStatus status)
{
    swap(m_Status[index], status);
    if (status == eValueDeleted)
        ++m_CntFilled;
}

CONCURMAP_TEMPLATE
inline typename CONCURMAP::CNode*
CONCURMAP::CNode::GetLowerNode(unsigned int index)
{
    return m_Values[index];
}

CONCURMAP_TEMPLATE
inline typename CONCURMAP::CNode*
CONCURMAP::CNode::GetRightNode(void)
{
    return m_RightNode;
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::CNode::GetNextIndex(unsigned int index)
{
    do {
        ++index;
    }
    while (index < kNodesMaxFill  &&  m_Status[index] == eValueDeleted);
    return index;
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::CNode::x_FindKeyIndex(const TKey& key)
{
    unsigned int low_bound = 0, high_bound = kNodesMaxFill;
    while (low_bound != high_bound) {
        unsigned int mid = (low_bound + high_bound) / 2;
        if (m_Tree->x_IsKeyLess(GetKey(mid), key))
            low_bound = mid + 1;
        else
            high_bound = mid;
    }
    return low_bound;
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::CNode::x_GetIndexOfMaximum(void)
{
    unsigned int index = kMaxNodeIndex;
    while (m_Keys[index].IsMaximum()  &&  m_Status[index] == eValueDeleted)
        --index;
    return m_Keys[index].IsMaximum()? index: index + 1;
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::CNode::FindContainIndex(const TKey& key)
{
    unsigned int index = x_FindKeyIndex(key);
    if (index != kNodesMaxFill  &&  m_Status[index] == eValueDeleted) {
        index = GetNextIndex(index);
    }
    return index;
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::CNode::FindContainIndex(const CKeyRef& key_ref)
{
    if (key_ref.IsMaximum())
        return x_GetIndexOfMaximum();
    return FindContainIndex(key_ref.GetKey());
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::CNode::FindExactIndex(const TKey& key, unsigned int& insert_index)
{
    insert_index = x_FindKeyIndex(key);
    return insert_index != kNodesMaxFill
           &&  !m_Tree->x_IsKeyLess(key, m_Keys[insert_index]);
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::CNode::FindExactIndex(const CKeyRef& key_ref,
                                 unsigned int&  insert_index)
{
    if (key_ref.IsMaximum()) {
        if (!m_MaxKey.IsMaximum()) {
            insert_index = kNodesMaxFill;
            return false;
        }
        insert_index = x_GetIndexOfMaximum();
        return insert_index != kNodesMaxFill;
    }
    return FindExactIndex(key_ref.GetKey(), insert_index);
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CNode::Delete(unsigned int index)
{
    m_Status[index] = eValueDeleted;
    --m_CntFilled;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CNode::x_ClearValues(void)
{
    for (unsigned int i = 0; i < kNodesMaxFill; ++i) {
        m_Status[i] = eValueDeleted;
        delete reinterpret_cast<TValue*>(m_Values[i]);
        m_Values   [i] = NULL;
    }
    m_CntFilled = 0;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CNode::x_ClearLowerNodes(void)
{
    for (unsigned int i = 0; i < kNodesMaxFill; ++i) {
        if (m_Status[i] != eValueDeleted) {
            m_Status[i] = eValueDeleted;
            CNode* node = m_Values[i];
            m_Values[i] = NULL;
            node->Clear();
            const_cast<TTree*>(m_Tree)->x_DeleteNode(node);
        }
    }
    m_CntFilled = 0;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CNode::Clear(void)
{
    if (m_DeepLevel == 0)
        x_ClearValues();
    else
        x_ClearLowerNodes();
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CNode::x_FindInsertSpace(unsigned int& index,
                                    unsigned int& insert_index)
{
    if (index != kNodesMaxFill  &&  m_Status[index] == eValueDeleted) {
        insert_index = index;
    }
    else if (index != 0  &&  m_Status[index - 1] == eValueDeleted) {
        insert_index = index - 1;
    }
    else {
        unsigned int right_index = index + 1;
        while (right_index < kNodesMaxFill
               &&  m_Status[right_index] != eValueDeleted)
        {
            ++right_index;
        }
        if (right_index < kNodesMaxFill) {
            for (; right_index != index; --right_index) {
                swap(m_Values[right_index], m_Values[right_index - 1]);
                m_Keys[right_index].Swap(m_Keys[right_index - 1]);
                m_Status[right_index] = m_Status[right_index - 1];
            }
            insert_index = index;
            ++index;
        }
        else {
            _ASSERT(index > 1);
            insert_index = index - 1;
            unsigned int left_index = insert_index - 1;
            while (m_Status[left_index] != eValueDeleted) {
                _ASSERT(left_index != 0);
                --left_index;
            }
            for (; left_index != insert_index; ++left_index) {
                swap(m_Values[left_index], m_Values[left_index + 1]);
                m_Keys[left_index].Swap(m_Keys[left_index + 1]);
                m_Status[left_index] = m_Status[left_index + 1];
            }
        }
    }
}

CONCURMAP_TEMPLATE
inline Value*
CONCURMAP::CNode::Insert(unsigned int  index,
                         const TKey&   key,
                         const TValue& value)
{
    _ASSERT(index == kNodesMaxFill
            ||  m_Tree->x_IsKeyLess(key, m_Keys[index])
            ||  m_Status[index] == eValueDeleted);

    unsigned int insert_index;
    x_FindInsertSpace(index, insert_index);
    m_Status[insert_index] = eValueActive;
    m_Keys  [insert_index] = key;
    TValue* my_value = reinterpret_cast<TValue*>(m_Values[insert_index]);
    if (my_value)
        *my_value = value;
    else
        m_Values[insert_index] = reinterpret_cast<CNode*>(new TValue(value));
    ++m_CntFilled;
    return GetValue(insert_index);
}

CONCURMAP_TEMPLATE
inline Value*
CONCURMAP::CNode::Insert(const TKey& key, const TValue& value)
{
    return Insert(x_FindKeyIndex(key), key, value);
}

CONCURMAP_TEMPLATE
inline typename CONCURMAP::CNode*
CONCURMAP::CNode::Split(void)
{
    const unsigned int split_index = kNodesMaxFill / 2 - 1;
    const CKeyRef& split_key_ref   = m_Keys[split_index];
    CNode* right_node   = m_Tree->x_CreateNode(split_key_ref, m_DeepLevel);
    for (unsigned int i = split_index + 1; i < kNodesMaxFill; ++i) {
        m_Keys[i].Swap(right_node->m_Keys[i]);
        swap(m_Values[i], right_node->m_Values[i]);
        right_node->m_Status[i] = m_Status[i];
        m_Status[i] = eValueDeleted;
    }
    m_CntFilled             = split_index + 1;
    right_node->m_CntFilled = kMaxNodeIndex - split_index;
    right_node->m_MaxKey    = m_MaxKey;
    m_MaxKey                = split_key_ref;
    right_node->m_RightNode = m_RightNode;
    m_RightNode             = right_node;
    return right_node;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CNode::AddSplit(const CKeyRef& left_key,
                           const CKeyRef& right_key,
                           CNode*         right_node)
{
    unsigned int right_index;
    if (right_key.IsMaximum()) {
        right_index = x_GetIndexOfMaximum();
        _ASSERT(m_Keys[right_index].IsMaximum()
                &&  m_Status[right_index] != eValueDeleted);
    }
    else {
        right_index = x_FindKeyIndex(right_key.GetKey());
    }
    unsigned int left_index;
    x_FindInsertSpace(right_index, left_index);
    while (left_index != 0
           &&  !m_Tree->x_IsKeyLess(m_Keys[left_index - 1], left_key))
    {
        --left_index;
        _ASSERT(m_Status[left_index] == eValueDeleted);
    }
    m_Keys  [left_index]  = left_key;
    m_Values[left_index]  = m_Values[right_index];
    m_Values[right_index] = right_node;
    m_Status[left_index]  = m_Status[right_index];
    ++m_CntFilled;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CNode::AddRootSplit(const CKeyRef& left_key,
                               CNode*         left_node,
                               CNode*         right_node)
{
    _ASSERT(m_Keys[0].IsMaximum()  &&  m_Keys[1].IsMaximum());

    // When root is split new root will always contain all maximums. It seems
    // that for optimization purposes we need to add one new node to the left
    // and another to the right but to be consistent with all other insert and
    // search procedures we need to add to the first 2 elements.
    m_Keys  [0] = left_key;
    m_Values[0] = left_node;
    m_Values[1] = right_node;
    m_Status[0] = eValueActive;
    m_Status[1] = eValueActive;
    m_CntFilled    = 2;
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::CNode::AddMaxChild(void)
{
    _ASSERT(!m_MaxKey.IsMaximum()  &&  m_DeepLevel != 0);
    unsigned int max_index = x_FindKeyIndex(m_MaxKey.GetKey());
    unsigned int insert_index;
    x_FindInsertSpace(max_index, insert_index);
    m_Keys  [insert_index] = m_MaxKey;
    m_Values[insert_index] = m_Tree->x_CreateNode(m_MaxKey, m_DeepLevel - 1);
    m_Status[insert_index] = eValueActive;
    ++m_CntFilled;
    return insert_index;
}


CONCURMAP_TEMPLATE
inline void
CONCURMAP::CTreeWalker::x_LockNode(CNode*& node, ERWLockType lock_type)
{
    node->Lock(lock_type);
    while (m_Tree->x_IsKeyLess(node->GetMaxKey(), m_Key)) {
        CNode* right_node = node->GetRightNode();
        node->Unlock(lock_type);
        node = right_node;
        node->Lock(lock_type);
    }
    m_LockedNode = node;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CTreeWalker::x_UnlockNode(CNode* node, ERWLockType lock_type)
{
    _ASSERT(node == m_LockedNode);
    node->Unlock(lock_type);
    m_LockedNode = NULL;
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::CTreeWalker::x_AddRootSplit(const CKeyRef& left_key,
                                       CNode*         left_node,
                                       const CKeyRef& right_key,
                                       CNode*         right_node)
{
    if (m_DeepLevel == kMaxTreeHeight - 1) {
        CNcbiDiag::DiagTrouble(DIAG_COMPILE_INFO,
                               "Concurrent map is too deep");
        return true;
    }
    // We need to check that root wasn't split already earlier by someone else.
    CNode* top_node;
    unsigned int cur_deep;
    m_Tree->x_GetRoot(top_node, cur_deep);
    m_Tree->x_RemoveRootRef();
    if (cur_deep != m_DeepLevel) {
        _ASSERT(cur_deep > m_DeepLevel);
        // We just find the path to our split node and return. Actual adding
        // new node to the parent will happen in the caller method.
        for (unsigned int level = cur_deep; level > m_DeepLevel; --level) {
            m_PathNodes[level] = top_node;
            x_LockNode(top_node, eReadLock);
            unsigned int key_index = top_node->FindContainIndex(right_key);
            _ASSERT(key_index != kNodesMaxFill);
            CNode* next_node = top_node->GetLowerNode(key_index);
            x_UnlockNode(top_node, eReadLock);
            top_node = next_node;
        }
        _ASSERT(top_node == left_node);
        return false;
    }
    else {
        _ASSERT(top_node == left_node);
        ++m_DeepLevel;
        CNode* root_node = m_Tree->x_CreateNode(CKeyRef(), m_DeepLevel);
        m_PathNodes[m_DeepLevel] = root_node;
        root_node->AddRootSplit(left_key, left_node, right_node);
        const_cast<TTree*>(m_Tree)->x_ChangeRoot(root_node, m_DeepLevel);
        return true;
    }
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::CTreeWalker::x_WaitForKeyAppear(CNode*         search_node,
                                           const CKeyRef& key,
                                           CNode*         key_node)
{
#ifdef _DEBUG
    int cnt = 0;
#endif
    unsigned int key_index;
    while (!search_node->FindExactIndex(key, key_index)
           ||  search_node->GetLowerNode(key_index) != key_node)
    {
        // This means that our split node was just created and is not
        // yet added to the parent. So we need to wait and give to another
        // thread a chance to add it.
#ifdef _DEBUG
        if (++cnt >= 5)
            abort();
#endif
        x_UnlockNode(search_node, eWriteLock);
        NCBI_SCHED_YIELD();
        x_LockNode(search_node, eWriteLock);
    }
    return key_index;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CTreeWalker::x_AddSplitToAbove(CNode*       left_node,
                                          CNode*       right_node,
                                          unsigned int above_level)
{
    for (unsigned int level = above_level; ; ++level) {
        CKeyRef left_key (left_node ->GetMaxKey());
        CKeyRef right_key(right_node->GetMaxKey());
        x_UnlockNode(left_node, eWriteLock);
        if (level > m_DeepLevel) {
            if (x_AddRootSplit(left_key, left_node, right_key, right_node))
                return;
            --level;
            continue;
        }
        CNode*& parent = m_PathNodes[level];
        x_LockNode(parent, eWriteLock);
        x_WaitForKeyAppear(parent, right_key, left_node);
        if (parent->GetCntFilled() != kNodesMaxFill) {
            parent->AddSplit(left_key, right_key, right_node);
            x_UnlockNode(parent, eWriteLock);
            return;
        }
        CNode* right_parent = parent->Split();
        if (m_Tree->x_IsKeyLess(parent->GetMaxKey(), right_key))
            right_parent->AddSplit(left_key, right_key, right_node);
        else
            parent->AddSplit(left_key, right_key, right_node);
        left_node = parent;
        right_node = right_parent;
    }
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CTreeWalker::x_FindBiggerKey(CNode*&       node,
                                        unsigned int& node_level)
{
    while (m_KeyIndex == kNodesMaxFill) {
        ERWLockType lock_type = (node_level == 0? m_LockType: eReadLock);
        CKeyRef was_key(node->GetMaxKey());
        if (was_key.IsMaximum()) {
            // There's no node on the right, key is not found.
            return;
        }
        if (node_level == m_DeepLevel) {
            // If this is root level and key in the node is not maximum then
            // there should be right node to look into.
            CNode* right_node = node->GetRightNode();
            x_UnlockNode(node, lock_type);
            node = right_node;
            m_PathNodes[node_level] = node;
        }
        else {
            x_UnlockNode(node, lock_type);
            node = m_PathNodes[++node_level];
            lock_type = eReadLock;
        }
        _ASSERT(node);
        x_LockNode(node, lock_type);
        m_KeyIndex = node->FindContainIndex(m_Key);
        if (m_KeyIndex != kNodesMaxFill) {
            if (m_Tree->x_IsKeyEqual(was_key, node->GetKey(m_KeyIndex)))
                m_KeyIndex = node->GetNextIndex(m_KeyIndex);
            else
                return;
        }
    }
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CTreeWalker::x_AddExactKeyNode(CNode*&       node,
                                          unsigned int& node_level)
{
    x_UnlockNode(node, eReadLock);
    x_LockNode(node, eWriteLock);
    while (node_level != 0) {
        m_KeyIndex = node->FindContainIndex(m_Key);
        while (m_KeyIndex == kNodesMaxFill) {
            if (node->GetCntFilled() == kNodesMaxFill) {
                CNode* right_node = node->Split();
                x_AddSplitToAbove(node, right_node, node_level + 1);
                node = right_node;
                x_LockNode(node, eWriteLock);
                m_KeyIndex = node->FindContainIndex(m_Key);
            }
            else {
                m_KeyIndex = node->AddMaxChild();
            }
        }
        CNode* next_node = node->GetLowerNode(m_KeyIndex);
        x_UnlockNode(node, eWriteLock);
        node = next_node;
        x_LockNode(node, eWriteLock);
        --node_level;
        m_PathNodes[node_level] = node;
    }
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CTreeWalker::x_FindKey(void)
{
    CNode* node;
    unsigned int deep_level;
    m_Tree->x_GetRoot(node, deep_level);
    m_DeepLevel = deep_level;

    for (;;) {
        ERWLockType lock_type = (deep_level == 0? m_LockType: eReadLock);
        x_LockNode(node, lock_type);

        if (node->GetCntFilled() == 0  &&  !node->GetMaxKey().IsMaximum()) {
            // This could be deletion of this node or it's just added by
            // another thread forcing its existence. So we need to try
            // once more at the above level and see what case is it exactly.
            if (deep_level == m_DeepLevel) {
                // This means that while we were waiting for lock root was
                // split and then all elements were deleted from it. So we
                // need to look into the right node.
                CNode* right_node = node->GetRightNode();
                x_UnlockNode(node, lock_type);
                node = right_node;
            }
            else {
                x_UnlockNode(node, lock_type);
                node = m_PathNodes[++deep_level];
            }
            _ASSERT(deep_level <= m_DeepLevel  &&  node != NULL);
            continue;
        }
        m_PathNodes[deep_level] = node;
        if (deep_level == 0  &&  m_ForceType != eForceBiggerKey) {
            // For eForceBiggerKey algorithm on the leaf level is the same as
            // on non-leaf level. For other types of searching leaf level is
            // different.
            break;
        }

        m_KeyIndex = node->FindContainIndex(m_Key);
        if (m_KeyIndex == kNodesMaxFill) {
            switch (m_ForceType) {
            case eForceBiggerKey:
                x_FindBiggerKey(node, deep_level);
                break;
            case eDoNotForceKey:
                // We will be here only on non-leaf level, so we can be sure
                // that we used read lock.
                x_UnlockNode(node, eReadLock);
                m_KeyFound = false;
                return;
            case eForceExactKey:
                _ASSERT(m_LockType == eWriteLock);
                x_AddExactKeyNode(node, deep_level);
                _ASSERT(deep_level == 0);
                break;
            default:
                _TROUBLE;
            }
        }
        if (deep_level == 0) {
            // We'll be here for eForceBiggerKey or for eForceExactKey when we
            // had to add new nodes to contain our key.
            break;
        }
        _ASSERT(m_KeyIndex != kNodesMaxFill);
        CNode* next_node = node->GetLowerNode(m_KeyIndex);
        _ASSERT(next_node);
        x_UnlockNode(node, lock_type);
        node = next_node;
        --deep_level;
    }
    if (m_ForceType == eForceBiggerKey) {
        m_KeyFound = m_KeyIndex != kNodesMaxFill;
    }
    else {
        m_KeyFound = node->FindExactIndex(m_Key, m_KeyIndex);
    }
}

CONCURMAP_TEMPLATE
inline Value*
CONCURMAP::CTreeWalker::Insert(const TValue& value)
{
    _ASSERT(m_LockType == eWriteLock  &&  m_LockedNode
            &&  m_LockedNode == m_PathNodes[0]);

    CNode* left_node = m_PathNodes[0];
    if (left_node->GetCntFilled() != kNodesMaxFill) {
        return left_node->Insert(m_KeyIndex, m_Key, value);
    }
    CNode*  right_node = left_node->Split();
    TValue* result;
    if (m_Tree->x_IsKeyLess(left_node->GetMaxKey(), m_Key)) {
        m_PathNodes[0] = right_node;
        result = right_node->Insert(m_Key, value);
    }
    else {
        result = left_node->Insert(m_Key, value);
    }
    x_AddSplitToAbove(left_node, right_node, 1);
    return result;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CTreeWalker::Delete(void)
{
    _ASSERT(m_LockType == eWriteLock  &&  m_LockedNode
            &&  m_LockedNode == m_PathNodes[0]);

    unsigned int level = 0;
    CNode* prev_node = m_PathNodes[0];
    prev_node->Delete(m_KeyIndex);
    while (prev_node->GetCntFilled() == 0
           &&  !prev_node->GetMaxKey().IsMaximum())
    {
        CKeyRef prev_key(prev_node->GetMaxKey());
        CNode*& next_node = m_PathNodes[++level];
        _ASSERT(next_node);
        x_UnlockNode(prev_node, eWriteLock);
        // We already have prev_node.m_MaxKey locked and the node itself is
        // used by pointer only, so we can delete it already here.
        const_cast<TTree*>(m_Tree)->x_DeleteNode(prev_node);
        x_LockNode(next_node, eWriteLock);
        unsigned int key_index
                        = x_WaitForKeyAppear(next_node, prev_key, prev_node);
        next_node->Delete(key_index);
        prev_node = next_node;
    }
    if (m_Tree->x_CanShrinkTree()) {
        // As prev_node can be root we need to unlock it before shrinking as
        // it can be already deleted on return.
        x_UnlockNode(prev_node, eWriteLock);
        const_cast<TTree*>(m_Tree)->x_ShrinkTree();
    }
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::CTreeWalker::SetValueStatus(EValueStatus status)
{
    m_PathNodes[0]->SetValueStatus(m_KeyIndex, status);
}

CONCURMAP_TEMPLATE
inline
CONCURMAP::CTreeWalker::CTreeWalker(const TTree* const  tree,
                                    CNode** const       path_nodes,
                                    const TKey&         key,
                                    ERWLockType         lock_type,
                                    EForceKeyType       force_type)
    : m_Tree(tree),
      m_PathNodes(path_nodes),
      m_Key(key),
      m_LockType(lock_type),
      m_ForceType(force_type),
      m_KeyIndex(kNodesMaxFill)
{
    x_FindKey();
}

CONCURMAP_TEMPLATE
inline
CONCURMAP::CTreeWalker::~CTreeWalker(void)
{
    if (m_LockedNode)
        m_LockedNode->Unlock(m_LockType);
    m_Tree->x_RemoveRootRef();
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::CTreeWalker::IsKeyFound(void)
{
    return m_KeyFound;
}

CONCURMAP_TEMPLATE
inline const Key&
CONCURMAP::CTreeWalker::GetKey(void)
{
    const CKeyRef& key_ref = m_PathNodes[0]->GetKey(m_KeyIndex);
    _ASSERT(!key_ref.IsMaximum());
    return key_ref.GetKey();
}

CONCURMAP_TEMPLATE
inline Value*
CONCURMAP::CTreeWalker::GetValue(void)
{
    return m_PathNodes[0]->GetValue(m_KeyIndex);
}

CONCURMAP_TEMPLATE
inline typename CONCURMAP::EValueStatus
CONCURMAP::CTreeWalker::GetValueStatus(void)
{
    return m_PathNodes[0]->GetValueStatus(m_KeyIndex);
}


CONCURMAP_TEMPLATE
inline bool
CONCURMAP::x_IsKeyLess(const TKey& left, const TKey& right) const
{
    return m_Comparator(left, right);
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::x_IsKeyLess(const CKeyRef& left_ref, const TKey& right) const
{
    if (left_ref.IsMaximum())
        return false;
    return x_IsKeyLess(left_ref.GetKey(), right);
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::x_IsKeyLess(const TKey& left, const CKeyRef& right_ref) const
{
    if (right_ref.IsMaximum())
        return true;
    return x_IsKeyLess(left, right_ref.GetKey());
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::x_IsKeyLess(const CKeyRef& left_ref, const CKeyRef& right_ref) const
{
    if (left_ref.IsMaximum())
        return false;
    else if (right_ref.IsMaximum())
        return true;
    return x_IsKeyLess(left_ref.GetKey(), right_ref.GetKey());
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::x_IsKeyEqual(const CKeyRef& left_ref, const CKeyRef& right_ref) const
{
    return left_ref.IsEqual(right_ref);
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::x_GetRoot(CNode*& node, unsigned int& deep_level) const
{
    CSpinReadGuard guard(m_RootLock);
    node       = m_RootNode;
    deep_level = m_DeepLevel;
    m_CntRootRefs.Add(1);
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::x_ChangeRoot(CNode* node, unsigned int deep_level)
{
    CSpinWriteGuard guard(m_RootLock);
    _ASSERT(deep_level == m_DeepLevel + 1);
    m_RootNode  = node;
    m_DeepLevel = deep_level;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::x_RemoveRootRef(void) const
{
    m_CntRootRefs.Add(-1);
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::x_Initialize(void)
{
    m_DeepLevel = 0;
    m_CntRootRefs.Set(0);
    m_CntNodes.Set(0);
    m_CntValues.Set(0);
    m_IsDeleting = false;

    m_RootNode = x_CreateNode(CKeyRef(), 0);
}

CONCURMAP_TEMPLATE
inline
CONCURMAP::CConcurrentMap(void)
    : m_NodesDeleter(Deleter<CNode>())
{
    x_Initialize();
}

CONCURMAP_TEMPLATE
inline
CONCURMAP::CConcurrentMap(const TComparator& comparator)
    : m_Comparator(comparator),
      m_NodesDeleter(Deleter<CNode>())
{
    x_Initialize();
}

CONCURMAP_TEMPLATE
inline
CONCURMAP::~CConcurrentMap(void)
{
    m_IsDeleting = true;
    m_RootNode->Clear();
    delete m_RootNode;
    // We didn't decrease counter for root node, so it should be 1 here
    _ASSERT(m_CntNodes.Get() == 1);
}

CONCURMAP_TEMPLATE
inline typename CONCURMAP::CNode*
CONCURMAP::x_CreateNode(const CKeyRef& key_ref, unsigned int deep_level) const
{
    m_CntNodes.Add(1);
    return new CNode(this, key_ref, deep_level);
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::x_DeleteNode(CNode* node)
{
    m_CntNodes.Add(-1);
    if (!m_IsDeleting)
        m_NodesDeleter.AddElement(node);
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::HeartBeat(void)
{
    m_NodesDeleter.HeartBeat();
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::x_CanShrinkTree(void) const
{
    return m_DeepLevel != 0  &&  m_RootNode->GetCntFilled() == 1
           &&  m_CntRootRefs.Get() == 1;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::x_ShrinkTree(void)
{
    CSpinWriteGuard guard(m_RootLock);
    if (!x_CanShrinkTree())
        return;
    // Next node is for the maximum value of key, so it has to be at the end
    // of the node, i,e, with index kMaxNodeIndex.
    unsigned int index = m_RootNode->FindContainIndex(CKeyRef());
    _ASSERT(index != kNodesMaxFill);
    CNode* next_root = m_RootNode->GetLowerNode(index);
    m_RootNode->Delete(index);
    x_DeleteNode(m_RootNode);
    m_RootNode = next_root;
    --m_DeepLevel;
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::Get(const TKey& key, TValue* value) const
{
    CNode* path_nodes[kMaxTreeHeight];
    CTreeWalker walker(this, path_nodes, key, eReadLock, eDoNotForceKey);
    if (walker.IsKeyFound()) {
        if (value)
            *value = walker.GetValue();
        return true;
    }
    return false;
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::Exists(const TKey& key) const
{
    return Get(key, NULL);
}

CONCURMAP_TEMPLATE
inline Value*
CONCURMAP::GetPtr(const TKey& key, EGetValueType get_type)
{
    CNode* path_nodes[kMaxTreeHeight];
    ERWLockType lock_typ    = (get_type == eGetOrCreate? eWriteLock: eReadLock);
    EForceKeyType force_typ = (get_type == eGetOrCreate? eForceExactKey: eDoNotForceKey);
    CTreeWalker walker(this, path_nodes, key, lock_typ, force_typ);
    if (walker.IsKeyFound()) {
        EValueStatus val_status = walker.GetValueStatus();
        switch (get_type) {
        case eGetOnlyActive:
            if (val_status == eValueActive)
                return walker.GetValue();
            else
                return NULL;
        case eGetActiveAndPassive:
            if (val_status != eValueDeleted)
                return walker.GetValue();
            else
                return NULL;
        case eGetOrCreate:
            switch (val_status) {
            case eValueDeleted:
                m_CntValues.Add(1);
                if (walker.GetValue()) {
                    walker.SetValueStatus(eValueActive);
                    return walker.GetValue();
                }
                else {
                    return walker.Insert(TValue());
                }
            case eValuePassive:
                walker.SetValueStatus(eValueActive);
                // fall through
            case eValueActive:
                return walker.GetValue();
            }
        }
    }
    else if (get_type == eGetOrCreate) {
        m_CntValues.Add(1);
        return walker.Insert(TValue());
    }
    return NULL;
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::InsertOrGetPtr(const TKey&   key,
                          const TValue& value,
                          EGetValueType get_type,
                          TValue**      value_ptr)
{
    _ASSERT(get_type != eGetOrCreate);
    CNode* path_nodes[kMaxTreeHeight];
    CTreeWalker walker(this, path_nodes, key, eWriteLock, eForceExactKey);
    if (walker.IsKeyFound()) {
        switch (walker.GetValueStatus()) {
        case eValueDeleted:
            m_CntValues.Add(1);
            if (walker.GetValue()) {
                walker.SetValueStatus(eValueActive);
                *walker.GetValue() = value;
                *value_ptr = walker.GetValue();
            }
            else {
                *value_ptr = walker.Insert(value);
            }
            return true;
        case eValuePassive:
            walker.SetValueStatus(eValueActive);
            if (get_type == eGetOnlyActive)
                *walker.GetValue() = value;
            // fall through
        case eValueActive:
            *value_ptr = walker.GetValue();
            return false;
        }
    }
    else {
        m_CntValues.Add(1);
        *value_ptr = walker.Insert(value);
        return true;
    }
    // This is never reachable but compiler complains that some execution paths
    // do not return value.
    abort();
    return false;
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::GetLowerBound(const TKey&    search_key,
                         TKey&          stored_key,
                         EGetValueType  get_type) const
{
    _ASSERT(get_type != eGetOrCreate);

    CNode* path_nodes[kMaxTreeHeight];
    CTreeWalker walker(this, path_nodes, search_key, eReadLock, eForceBiggerKey);
    if (walker.IsKeyFound()  &&  (walker.GetValueStatus() == eValueActive
                                  ||  get_type == eGetActiveAndPassive))
    {
        stored_key = walker.GetKey();
        return true;
    }
    return false;
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::SetValueStatus(const TKey& key, EValueStatus status)
{
    _ASSERT(status != eValueDeleted);

    CNode* path_nodes[kMaxTreeHeight];
    CTreeWalker walker(this, path_nodes, key, eWriteLock, eDoNotForceKey);
    if (walker.IsKeyFound()  &&  walker.GetValueStatus() != eValueDeleted) {
        walker.SetValueStatus(status);
        return true;
    }
    return false;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::Put(const TKey& key, const TValue& value)
{
    CNode* path_nodes[kMaxTreeHeight];
    CTreeWalker walker(this, path_nodes, key, eWriteLock, eForceExactKey);
    if (walker.IsKeyFound()) {
        walker.GetValue() = value;
    }
    else {
        walker.Insert(value);
        m_CntValues.Add(1);
    }
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::Insert(const TKey& key, const TValue& value, TValue* stored_value)
{
    CNode* path_nodes[kMaxTreeHeight];
    CTreeWalker walker(this, path_nodes, key, eWriteLock, eForceExactKey);
    if (walker.IsKeyFound()) {
        if (stored_value)
            *stored_value = *walker.GetValue();
        return false;
    }
    else {
        walker.Insert(value);
        m_CntValues.Add(1);
        return true;
    }
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::Change(const TKey& key, const TValue& value)
{
    CNode* path_nodes[kMaxTreeHeight];
    CTreeWalker walker(this, path_nodes, key, eWriteLock, eDoNotForceKey);
    if (walker.IsKeyFound()) {
        walker.GetValue() = value;
        return true;
    }
    return false;
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::Erase(const TKey& key)
{
    CNode* path_nodes[kMaxTreeHeight];
    CTreeWalker walker(this, path_nodes, key, eWriteLock, eDoNotForceKey);
    if (walker.IsKeyFound()  &&  walker.GetValueStatus() != eValueDeleted) {
        walker.Delete();
        m_CntValues.Add(-1);
        return true;
    }
    return false;
}

CONCURMAP_TEMPLATE
inline bool
CONCURMAP::EraseIfPassive(const TKey& key)
{
    CNode* path_nodes[kMaxTreeHeight];
    CTreeWalker walker(this, path_nodes, key, eWriteLock, eDoNotForceKey);
    if (walker.IsKeyFound()  &&  walker.GetValueStatus() == eValuePassive) {
        walker.Delete();
        m_CntValues.Add(-1);
        return true;
    }
    return false;
}

CONCURMAP_TEMPLATE
inline void
CONCURMAP::Clear(void)
{
    m_IsDeleting = true;
    m_RootNode->Clear();
    x_DeleteNode(m_RootNode);
    m_IsDeleting = false;
    m_CntValues.Set(0);
    m_RootNode  = x_CreateNode(CKeyRef(), 0);
    m_DeepLevel = 0;
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::CountValues(void) const
{
    return static_cast<unsigned int>(m_CntValues.Get());
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::CountNodes(void) const
{
    return static_cast<unsigned int>(m_CntNodes.Get());
}

CONCURMAP_TEMPLATE
inline unsigned int
CONCURMAP::TreeHeight(void) const
{
    return m_DeepLevel + 1;
}


#undef  CONCURMAP_TEMPLATE
#undef  CONCURMAP


END_NCBI_SCOPE

#endif /* NETCACHE__CONCURRENT_BTREE__INL */
