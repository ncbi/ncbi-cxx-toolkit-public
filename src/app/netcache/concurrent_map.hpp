#ifndef NETCACHE__CONCURRENT_MAP__HPP
#define NETCACHE__CONCURRENT_MAP__HPP
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

#include "nc_utils.hpp"
#include <corelib/ncbiatomic.hpp>


BEGIN_NCBI_SCOPE


template <class Value>
struct SConstructAllocator
{
    template <class Other>
    struct rebind
    {
        typedef SConstructAllocator<Other>  other;
    };

    static Value* allocate(void)
    {
        return new Value;
    }
    static void deallocate(Value* value)
    {
        delete value;
    }
};



template <class Key,
          class Value,
          class Comparator       = less<Key>,
          class Allocator        = SConstructAllocator<Key>,
          Uint1 CntChildsInNode  = 8,
          Uint1 MaxTreeHeight    = 16,
          Uint1 DeletionDelay    = 3,
          Uint1 DelStoreCapacity = 20>
class CConcurrentMap
{
    struct SCallContext;
    class CFinalNodeDeleter;

    enum EValueStatus NCBI_PACKED_ENUM_TYPE(Uint1) {
        eValueDeleted = 0,  /// Must be 0
        eValuePassive,
        eValueActive
    } NCBI_PACKED_ENUM_END();

public:
    enum {
        kCntChildsInNode    = CntChildsInNode,
        kMaxTreeHeight      = MaxTreeHeight,
        kDeletionDelay      = DeletionDelay,
        kDelStoreCapacity   = DelStoreCapacity,
        kLeafTreeLevel      = 1
    };

    struct SRefedKey;
    struct SNode;
    struct SLeafNode;

    typedef Uint1       TTreeHeight;
    typedef Uint1       TNodeIndex;
    typedef Key         TKey;
    typedef Value       TValue;
    typedef Comparator  TComparator;
    typedef Allocator   TKeyAlloc;
    typedef typename Allocator::template rebind<SRefedKey>::other   TRefedKeyAlloc;
    typedef typename Allocator::template rebind<SNode>::other       TNodeAlloc;
    typedef typename Allocator::template rebind<SLeafNode>::other   TLeafAlloc;

    typedef CConcurrentMap<TKey, TValue, TComparator, TKeyAlloc,
                           kCntChildsInNode, kMaxTreeHeight,
                           kDeletionDelay, kDelStoreCapacity>       TTree;
    typedef map<TKey, TValue, TComparator>                          TMap;


    enum EGetValueType NCBI_PACKED_ENUM_TYPE(Uint1) {
        eGetOnlyActive,
        eGetActiveAndPassive
    } NCBI_PACKED_ENUM_END();


    CConcurrentMap(void) : m_NodesDeleter(CFinalNodeDeleter(this))
    {
        x_Initialize();
    }
    ~CConcurrentMap(void)
    {
        x_Finalize();
    }

    bool Get(const TKey& key, TValue& value) const
    {
        TNodeIndex key_index;
        SCallContext call_ctx(key);
        x_InitializeCallContext(call_ctx);
        bool result = x_DiveAndFindKey(call_ctx, eReadLock, key_index);
        if (result)
            value = x_Value(call_ctx.tree_path[kLeafTreeLevel], key_index);
        x_FinalizeCallContext(call_ctx);
        return result;
    }
    void Put(const TKey& key, const TValue& value)
    {
        TNodeIndex key_index;
        TValue* value_ptr;
        SCallContext call_ctx(key);
        x_InitializeCallContext(call_ctx);
        if (!x_DiveAndCreateValue(call_ctx, value, key_index, value_ptr))
            *value_ptr = value;
        x_FinalizeCallContext(call_ctx);
    }
    bool Erase(const TKey& key)
    {
        return x_EraseIf(key, eValueActive);
    }
    bool PutOrGet(const TKey&   key,
                  const TValue& value,
                  EGetValueType get_type,
                  TValue&       ret_value)
    {
        bool result;
        TNodeIndex key_index;
        TValue* value_ptr;
        SCallContext call_ctx(key);
        x_InitializeCallContext(call_ctx);
        if (!x_DiveAndCreateValue(call_ctx, value, key_index, value_ptr)) {
            SNode* node = call_ctx.tree_path[kLeafTreeLevel];
            result = node->status[key_index] != eValueActive  &&  get_type == eGetOnlyActive;
            if (result)
                *value_ptr = value;
            node->status[key_index] = eValueActive;
        }
        x_FinalizeCallContext(call_ctx);
        return result;
    }
    bool PassivateKey(const TKey& key)
    {
        return x_SetValueStatus(key, eValuePassive);
    }
    bool ActivateKey(const TKey& key)
    {
        return x_SetValueStatus(key, eValueActive);
    }
    bool EraseIfPassive(const TKey& key)
    {
        return x_EraseIf(key, eValuePassive);
    }

    Uint4 GetCntValues(void) const
    {
        return static_cast<Uint4>(m_CntValues.Get());
    }
    Uint4 GetCntNodes(void) const
    {
        return static_cast<Uint4>(m_CntNodes.Get());
    }
    Uint4 GetCntLeafNodes(void) const
    {
        return static_cast<Uint4>(m_CntLeafNodes.Get());
    }
    TTreeHeight GetTreeHeight(void) const
    {
        return m_TreeHeight;
    }
    void HeartBeat(void)
    {
        m_NodesDeleter.HeartBeat();
    }

    /// Caller is responsible to not make this call concurrent with any other
    /// method call (the same as calling ctor and dtor).
    void Clear(void)
    {
        x_Finalize();
        x_Initialize();
    }

public:
    struct SRefedKey
    {
        TKey            key;
        CAtomicCounter  ref_cnt;
    };

    struct SNodeBase
    {
        SRefedKey*   max_key;
        SNode*       right_node;
        CSpinRWLock  node_lock;
        TTreeHeight  tree_level;
        TNodeIndex   cnt_filled;
        EValueStatus status[kCntChildsInNode];
        SRefedKey*   keys  [kCntChildsInNode];
    };
    struct SNode : public SNodeBase
    {
        SNode* childs[kCntChildsInNode];
    };
    struct SLeafNode : public SNodeBase
    {
        TValue values[kCntChildsInNode];
    };

private:
    CConcurrentMap(const CConcurrentMap&);
    CConcurrentMap& operator= (const CConcurrentMap&);

    class CFinalNodeDeleter
    {
    public:
        CFinalNodeDeleter(TTree* tree) : m_Tree(tree)
        {}
        void Delete(SNode* node)
        {
            m_Tree->x_FinalDeleteNode(node);
        }

    private:
        TTree* m_Tree;
    };

    struct SCallContext
    {
        const TKey& lookup_key;
        SNode*      locked_node;
        ERWLockType locked_type;
        TTreeHeight tree_height;
        TTreeHeight cur_level;
        TTreeHeight split_level;
        SNode*      tree_path[kMaxTreeHeight + 1];
        SNode*      left_node;
        SNode*      right_node;
        SRefedKey*  left_key;
        SRefedKey*  right_key;

        SCallContext(const TKey& key) : lookup_key(key)
        {}
    };

    typedef CNCDeferredDeleter<SNode*, CFinalNodeDeleter,
                               kDeletionDelay, kDelStoreCapacity> TDeferredDeleter;
    friend class CFinalNodeDeleter;


    void x_AddKeyRef(SRefedKey* key_ref, Uint4 cnt_ref = 1)
    {
        if (key_ref)
            key_ref->ref_cnt.Add(cnt_ref);
    }
    void x_DerefKey(SRefedKey* key_ref)
    {
        if (key_ref  &&  key_ref->ref_cnt.Add(-1) == 0)
            m_RefedKeyAlloc.deallocate(key_ref);
    }
    void x_AssignKeyRef(SRefedKey*& to_ref, SRefedKey* from_ref)
    {
        x_DerefKey(to_ref);
        x_AddKeyRef(from_ref);
        to_ref = from_ref;
    }
    void x_AssignKeyRef(SRefedKey*& to_ref, const TKey& key)
    {
        if (!to_ref  ||  to_ref->ref_cnt.Add(-1) != 0)
            to_ref = m_RefedKeyAlloc.allocate();
        to_ref->key = key;
        to_ref->ref_cnt.Set(1);
    }

    bool x_IsKeyLess(const TKey& left_key, const TKey& right_key) const
    {
        return m_Comparator(left_key, right_key);
    }
    bool x_IsKeyLess(const SRefedKey* left_ref, const TKey& right_key) const
    {
        if (left_ref == NULL)
            return false;
        return x_IsKeyLess(left_ref->key, right_key);
    }
    bool x_IsKeyLess(const TKey& left_key, const SRefedKey* right_ref) const
    {
        if (right_ref == NULL)
            return true;
        return x_IsKeyLess(left_key, right_ref->key);
    }
    bool x_IsKeyLess(const SRefedKey* left_ref, const SRefedKey* right_ref) const
    {
        if (right_ref == NULL)
            return true;
        else if (left_ref == NULL)
            return false;
        return x_IsKeyLess(left_ref->key, right_ref->key);
    }

    bool x_IsNodeToBeDeleted(SNode* node) const
    {
        return node->cnt_filled == 0  &&  node->max_key != NULL;
    }
    TValue& x_Value(SNode* node, TNodeIndex index) const
    {
        return reinterpret_cast<SLeafNode*>(node)->values[index];
    }
    bool x_IsKeyFound(SNode* node, const TKey& key, TNodeIndex index)
    {
        return index != kCntChildsInNode  &&  !x_IsKeyLess(key, node->keys[index])
               &&  node->status[index] != eValueDeleted;
    }

    TNodeIndex x_FindKeyIndex(SNode* node, const TKey& key) const
    {
        unsigned int low_bound = 0, high_bound = kCntChildsInNode;
        while (low_bound != high_bound) {
            unsigned int mid = (low_bound + high_bound) / 2;
            if (x_IsKeyLess(node->keys[mid], key))
                low_bound = mid + 1;
            else
                high_bound = mid;
        }
        return low_bound;
    }
    TNodeIndex x_FindKeyIndex(SNode* node, const SRefedKey* key_ref) const
    {
        if (key_ref != NULL)
            return x_FindKeyIndex(node, key_ref->key);

        unsigned int index = kCntChildsInNode - 1;
        while (index > 0  &&  node->keys[index - 1] == NULL) {
            _ASSERT(node->status[index] == eValueDeleted);
            --index;
        }
        return index;
}
    TNodeIndex x_GetNextIndex(SNode* node, TNodeIndex index) const
    {
        do {
            ++index;
        }
        while (index < kCntChildsInNode  &&  node->status[index] == eValueDeleted);
        return index;
    }
    TNodeIndex x_FindContainingIndex(SNode* node, const TKey& key) const
    {
        TNodeIndex index = x_FindKeyIndex(node, key);
        if (index != kCntChildsInNode  &&  node.status[index] == eValueDeleted)
            index = x_GetNextIndex(node, index);
        return index;
    }
    void x_ScanForInsertSpace(SNode* node, TNodeIndex& index, TNodeIndex& ins_index)
    {
        TNodeIndex right_index = index + 1;
        while (right_index < kCntChildsInNode
               &&  node->status[right_index] != eValueDeleted)
        {
            ++right_index;
        }
        SRefedKey* key_ref;
        SNode* child;
        if (right_index < kCntChildsInNode) {
            ins_index = index;
            ++index;
            key_ref = node->keys[right_index];
            child = node->childs[right_index];
            size_t sz_ptrs = (right_index - ins_index) * sizeof(void*);
            size_t sz_statuses = (right_index - ins_index) * sizeof(EValueStatus);
            memmove(&node->childs[ins_index + 1], &node->childs[ins_index], sz_ptrs);
            memmove(&node->keys[ins_index + 1], &node->keys[ins_index], sz_ptrs);
            memmove(&node->status[ins_index + 1], &node->status[ins_index], sz_statuses);
        }
        else {
            _ASSERT(index > 1);
            ins_index = index - 1;
            unsigned int left_index = ins_index - 1;
            while (node->status[left_index] != eValueDeleted) {
                _ASSERT(left_index != 0);
                --left_index;
            }
            key_ref = node->keys[left_index];
            child = node->childs[left_index];
            size_t sz_ptrs = (ins_index - left_index) * sizeof(void*);
            size_t sz_statuses = (ins_index - left_index) * sizeof(EValueStatus);
            memmove(&node->childs[left_index], &node->childs[left_index + 1], sz_ptrs);
            memmove(&node->keys[left_index], &node->keys[left_index + 1], sz_ptrs);
            memmove(&node->status[left_index], &node->status[left_index + 1], sz_statuses);
        }
        node->childs[ins_index] = child;
        node->keys[ins_index] = key_ref;
    }
    void x_FindInsertSpace(SNode* node, TNodeIndex& index, TNodeIndex& ins_index)
    {
        if (index != kCntChildsInNode  &&  node->status[index] == eValueDeleted)
            ins_index = index;
        else if (index != 0  &&  node->status[index - 1] == eValueDeleted)
            ins_index = index - 1;
        else
            x_ScanForInsertSpace(node, index, ins_index);
    }

    SNode* x_CreateNode(SRefedKey* max_key, TTreeHeight tree_level)
    {
        SNode* node;
        if (tree_level == kLeafTreeLevel)
            node = reinterpret_cast<SNode*>(m_LeafAlloc.allocate());
        else
            node = m_NodeAlloc.allocate();
        node->max_key    = max_key;
        node->right_node = NULL;
        node->tree_level = tree_level;
        node->cnt_filled = 0;
        memset(node->status, eValueDeleted, sizeof(node->status));
        for (int i = 0; i < kCntChildsInNode; ++i) {
            node->keys[i] = max_key;
        }
        x_AddKeyRef(max_key, kCntChildsInNode + 1);
        m_CntNodes.Add(1);
        if (tree_level == kLeafTreeLevel)
            m_CntLeafNodes.Add(1);
        return node;
    }
    void x_DeleteNode(SNode* node)
    {
        m_NodesDeleter.AddElement(node);
        m_CntNodes.Add(-1);
        if (node->tree_level == kLeafTreeLevel)
            m_CntLeafNodes.Add(-1);
    }
    void x_FinalDeleteNode(SNode* node)
    {
        _ASSERT(node->cnt_filled == 0);
        for (TNodeIndex i = 0; i < kCntChildsInNode; ++i)
            x_DerefKey(node->keys[i]);
        x_DerefKey(node->max_key);
        if (node->tree_level == kLeafTreeLevel)
            m_LeafAlloc.deallocate(reinterpret_cast<SLeafNode*>(node));
        else
            m_NodeAlloc.deallocate(node);
    }

    void x_Initialize(void)
    {
        m_TreeHeight = 1;
        m_CntRootRefs.Set(0);
        m_CntNodes.Set(0);
        m_CntValues.Set(0);

        m_RootNode = x_CreateNode(NULL, kLeafTreeLevel);
    }
    void x_Finalize(void)
    {
        SNode* node_stack[kMaxTreeHeight];
        TNodeIndex ind_in_node[kMaxTreeHeight];
        node_stack [0] = m_RootNode;
        ind_in_node[0] = 0;
        for (TTreeHeight node_ind = 0; node_ind + 1 > 0; ) {
            SNode* node = node_stack[node_ind];
            if (node->tree_level != kLeafTreeLevel) {
                TNodeIndex child_ind = ind_in_node[node_ind];
                while (child_ind < kCntChildsInNode
                       &&  node->status[child_ind] == eValueDeleted)
                {
                    ++child_ind;
                }
                if (child_ind < kCntChildsInNode) {
                    SNode* child = node->childs[child_ind];
                    ind_in_node[node_ind] = ++child_ind;
                    ++node_ind;
                    node_stack [node_ind] = child;
                    ind_in_node[node_ind] = 0;
                    continue;
                }
            }
            x_FinalDeleteNode(node);
            --node_ind;
        }
        m_RootNode = NULL;
        _ASSERT(m_CntNodes.Get() == 0);
    }
    bool x_SetValueStatus(const TKey& key, EValueStatus status)
    {
        TNodeIndex key_index;
        SCallContext call_ctx(key);
        x_InitializeCallContext(call_ctx);
        bool result = x_DiveAndFindKey(call_ctx, eWriteLock, key_index);
        if (result)
            call_ctx.tree_path[kLeafTreeLevel]->status[key_index] = status;
        x_FinalizeCallContext(call_ctx);
        return result;
    }
    bool x_EraseIf(const TKey& key, EValueStatus status)
    {
        bool result = false;
        TNodeIndex key_index;
        SCallContext call_ctx(key);
        x_InitializeCallContext(call_ctx);
        if (x_DiveAndFindKey(call_ctx, eWriteLock, key_index)) {
            SNode* node = call_ctx.tree_path[kLeafTreeLevel];
            result = node->status[key_index] == status;
            if (result)
                x_DeleteKey(call_ctx, key_index);
        }
        x_FinalizeCallContext(call_ctx);
        return result;
    }

    void x_GetRootAndHeight(SNode*& node, TTreeHeight& height, bool add_ref) const
    {
        m_RootLock.ReadLock();
        node    = m_RootNode;
        height  = m_TreeHeight;
        if (add_ref)
            m_CntRootRefs.Add(1);
        m_RootLock.ReadUnlock();
    }
    void x_ChangeRoot(SNode* new_root, TTreeHeight new_height)
    {
        m_RootLock.WriteLock();
        _ASSERT(new_height == m_TreeHeight + 1);
        m_RootNode   = new_root;
        m_TreeHeight = new_height;
        m_RootLock.WriteUnlock();
    }
    bool x_CanShrinkTree(void) const
    {
        return m_TreeHeight != 1  &&  m_RootNode->cnt_filled == 1
               &&  m_CntRootRefs.Get() == 1;
    }
    void x_ShrinkTree(void)
    {
        m_RootLock.WriteLock();
        if (UNLIKELY(!x_CanShrinkTree())) {
            m_RootLock.WriteUnlock();
            return;
        }
        TNodeIndex index = x_FindContainingIndex(m_RootNode, NULL);
        _ASSERT(index != kCntChildsInNode);
        SNode* next_root = m_RootNode->childs[index];
        SNode* prev_root = m_RootNode;
        m_RootNode = next_root;
        --m_TreeHeight;
        m_RootLock.WriteUnlock();
        x_DeleteNode(prev_root);
    }

    void x_InitializeCallContext(SCallContext& call_ctx) const
    {
        SNode* cur_node;
        x_GetRootAndHeight(cur_node, call_ctx.tree_height, true);
        call_ctx.cur_level = call_ctx.tree_height;
        call_ctx.tree_path[call_ctx.cur_level] = cur_node;
        call_ctx.split_level = 0;
    }
    void x_FinalizeCallContext(SCallContext& call_ctx) const
    {
        if (call_ctx.locked_node)
            x_UnlockCurNode(call_ctx);
        m_CntRootRefs.Add(-1);
    }

    SNode* x_LockCurNode(SCallContext& call_ctx, ERWLockType lock_type) const
    {
        SNode* node = call_ctx.tree_path[call_ctx.cur_level];
        node->node_lock.Lock(lock_type);
        while (x_IsKeyLess(node->max_key, call_ctx.lookup_key)) {
            SNode* right_node = node->right_node;
            node->node_lock.Unlock(lock_type);
            node = right_node;
            node->node_lock.Lock(lock_type);
        }
        call_ctx.tree_path[call_ctx.cur_level] = node;
        call_ctx.locked_node = node;
        call_ctx.locked_type = lock_type;
        return node;
    }
    void x_UnlockCurNode(SCallContext& call_ctx) const
    {
        call_ctx.locked_node->node_lock.Unlock(call_ctx.locked_type);
        call_ctx.locked_node = NULL;
    }
    void x_ExchangeNodeLocks(SCallContext& call_ctx, SNode* node_to_lock) const
    {
        _ASSERT(call_ctx.locked_node == call_ctx.tree_path[call_ctx.cur_level]
                &&  call_ctx.locked_type == eWriteLock);
        node_to_lock->node_lock.Lock(eWriteLock);
        call_ctx.locked_node->node_lock.Unlock(eWriteLock);
        call_ctx.locked_node = node_to_lock;
    }
    TNodeIndex x_LockNodeAndWaitKey(SCallContext&  call_ctx,
                                    SRefedKey*     wait_key_ref,
                                    SNode*         wait_child_value)
    {
        SNode* node = x_LockCurNode(call_ctx, eWriteLock);
        TNodeIndex index = x_FindKeyIndex(node, wait_key_ref);
#ifdef _DEBUG
        int cnt = 0;
#endif
        while (UNLIKELY(index == kCntChildsInNode
                        // Check for strong equality of pointers should suffice here
                        // because value we wait should be identical to max_key
                        // in the node we wait.
                        //||  x_IsKeyLess(wait_key_ref, node->keys[index])
                        ||  node->keys  [index] != wait_key_ref
                        ||  node->childs[index] != wait_child_value))
        {
            // This means that wait_child_value was just created and is not
            // yet added to the parent. So we need to wait and give another
            // thread a chance to add it.
#ifdef _DEBUG
            if (++cnt >= 5)
                abort();
#endif
            x_UnlockCurNode(call_ctx);
            NCBI_SCHED_YIELD();
            node = x_LockCurNode(call_ctx, eWriteLock);
            index = x_FindKeyIndex(node, wait_key_ref);
        }
        return index;
    }

    bool x_DiveAndFindKey(SCallContext& call_ctx,
                          ERWLockType   leaf_lock_type,
                          TNodeIndex&   key_index) const
    {
        for (;;) {
            if (!x_DiveToLeafLevel(call_ctx))
                return false;
            if (UNLIKELY(!x_LockLeafNode(call_ctx, leaf_lock_type)))
                continue;

            SNode* node = call_ctx.tree_path[kLeafTreeLevel];
            key_index = x_FindKeyIndex(node, call_ctx.lookup_key);
            return x_IsKeyFound(node, call_ctx.lookup_key, key_index);
        }

    }
    bool x_DiveAndCreateValue(SCallContext& call_ctx,
                              const TValue& value,
                              TNodeIndex&   key_index,
                              TValue*&      value_ptr)
    {
        for (;;) {
            if (x_DiveToLeafLevel(call_ctx)) {
                if (UNLIKELY(!x_LockLeafNode(call_ctx, eWriteLock)))
                    continue;
            }
            else {
                if (UNLIKELY(!x_CreatePathToLeaf(call_ctx)))
                    continue;
            }
            SNode* node = call_ctx.tree_path[kLeafTreeLevel];
            TNodeIndex index = x_FindKeyIndex(node, call_ctx.lookup_key);
            bool result;
            if (x_IsKeyFound(node, call_ctx.lookup_key, index)) {
                value_ptr = &x_Value(node, index);
                result = false;
            }
            else {
                value_ptr = x_InsertLeafValue(call_ctx, node, index, value);
                x_PropagateSplit(call_ctx);
                result = true;
            }
            key_index = index;
            return result;
        }
    }
    /// Delete key from the tree. No real deletion will be done unless the node
    /// becomes empty. Node is deleted then cleaning all its keys and values.
    /// Only value status is changed otherwise
    void x_DeleteKey(SCallContext& call_ctx, TNodeIndex key_index)
    {
        SNode* node = call_ctx.tree_path[kLeafTreeLevel];
        node->status[key_index] = eValueDeleted;
        --node->cnt_filled;
        if (x_IsNodeToBeDeleted(node))
            x_DeleteEmptyNodes(call_ctx);
        m_CntValues.Add(-1);
    }
    bool x_DiveToLeafLevel(SCallContext& call_ctx) const
    {
        while (call_ctx.cur_level != kLeafTreeLevel) {
            if (!x_DiveToNextLevel(call_ctx))
                return false;
        }
        return true;
    }
    bool x_DiveToNextLevel(SCallContext& call_ctx) const
    {
        bool success = false;
        SNode* node = x_LockCurNode(call_ctx, eReadLock);
        if (LIKELY(node->cnt_filled != 0)) {
            TNodeIndex key_index = x_FindContainingIndex(node, call_ctx.lookup_key);
            success = key_index != kCntChildsInNode;
            if (success)
                x_MoveOneLevelDown(call_ctx, key_index);
        }
        else if (UNLIKELY(x_IsNodeToBeDeleted(node))) {
            // This node was just deleted. So we'll
            // try once more at the above level.
            x_MoveOneLevelUp(call_ctx);
            // success is true just to continue diving.
            success = true;
        }
        x_UnlockCurNode(call_ctx);
        return success;
    }
    void x_MoveOneLevelUp(SCallContext& call_ctx) const
    {
        if (UNLIKELY(call_ctx.cur_level == call_ctx.tree_height)) {
            // This means that while we were waiting for lock root was
            // split and then all elements were deleted from it.
            // So we need to look into the right node - we don't know
            // what new root is.
            call_ctx.tree_path[call_ctx.cur_level]
                             = call_ctx.tree_path[call_ctx.cur_level]->right_node;
        }
        else {
            ++call_ctx.cur_level;
        }
    }
    void x_MoveOneLevelDown(SCallContext& call_ctx, TNodeIndex index) const
    {
        SNode* node = call_ctx.tree_path[call_ctx.cur_level];
        --call_ctx.cur_level;
        call_ctx.tree_path[call_ctx.cur_level] = node->childs[index];
    }
    bool x_LockLeafNode(SCallContext& call_ctx, ERWLockType lock_type) const
    {
        SNode* node = x_LockCurNode(call_ctx, lock_type);
        if (UNLIKELY(x_IsNodeToBeDeleted(node))) {
            x_MoveOneLevelUp(call_ctx);
            x_UnlockCurNode(call_ctx);
            return false;
        }
        return true;
    }
    bool x_CreatePathToLeaf(SCallContext& call_ctx)
    {
        SNode* node = x_LockCurNode(call_ctx, eWriteLock);
        if (UNLIKELY(x_IsNodeToBeDeleted(node))) {
            x_MoveOneLevelUp(call_ctx);
            x_UnlockCurNode(call_ctx);
            return false;
        }
        TNodeIndex key_index = x_FindContainingIndex(node, call_ctx.lookup_key);
        if (UNLIKELY(key_index != kCntChildsInNode)) {
            x_MoveOneLevelDown(call_ctx, key_index);
            x_UnlockCurNode(call_ctx);
            return false;
        }
        TTreeHeight level = node->tree_level - 1;
        SNode* add_node = x_CreateNode(node->max_key, level);
        call_ctx.tree_path[level] = add_node;
        SNode* last_node = add_node;
        for (--level; level >= kLeafTreeLevel; --level) {
            SNode* next_node = x_CreateNode(node->max_key, level);
            last_node->childs[0]  = next_node;
            last_node->status[0]  = eValueActive;
            last_node->cnt_filled = 1;
            last_node = next_node;
            call_ctx.tree_path[level] = next_node;
        }
        x_AddNodeKey(call_ctx, node, node->max_key, add_node);
        x_ExchangeNodeLocks(call_ctx, last_node);
        call_ctx.cur_level = kLeafTreeLevel;
        return true;
    }
    TValue* x_InsertLeafValue(SCallContext& call_ctx,
                              SNode*        node,
                              TNodeIndex    key_index,
                              const TValue& value)
    {
        if (node->cnt_filled == kCntChildsInNode) {
            x_SplitNode(call_ctx, node);
            node = call_ctx.locked_node;
            key_index = x_FindKeyIndex(node, call_ctx.lookup_key);
        }
        TNodeIndex ins_index = key_index;
        if (node->status[key_index] != eValueDeleted)
            x_FindInsertSpace(node, key_index, ins_index);
        node->status[ins_index] = eValueActive;
        x_AssignKeyRef(node->keys[ins_index], call_ctx.lookup_key);
        TValue* value_ptr = &x_Value(node, ins_index);
        *value_ptr = value;
        m_CntValues.Add(1);
        return value_ptr;
    }
    TNodeIndex x_AddNodeKey(SCallContext& call_ctx,
                            SNode*        node,
                            SRefedKey*    key_ref,
                            SNode*        value_node)
    {
        if (node->cnt_filled == kCntChildsInNode) {
            x_SplitNode(call_ctx, node);
            node = call_ctx.locked_node;
        }
        // This method will be called only when adding non-NULL key_ref.
        TNodeIndex key_index = x_FindKeyIndex(node, key_ref->key);
        TNodeIndex ins_index;
        x_FindInsertSpace(node, key_index, ins_index);
        node->childs[ins_index] = value_node;
        node->status[ins_index] = eValueActive;
        x_AssignKeyRef(node->keys[ins_index], key_ref);
        ++node->cnt_filled;
        return ins_index;
    }
    void x_SplitNode(SCallContext& call_ctx, SNode* node)
    {
        const TNodeIndex left_cnt = kCntChildsInNode / 2;
        const SRefedKey* left_max_key = node->keys[left_cnt - 1];
        SNode* const right_node = x_CreateNode(left_max_key, node->tree_level);
        const size_t sz_right_ptrs  = (kCntChildsInNode - left_cnt) * sizeof(void*);
        const size_t sz_right_stats = (kCntChildsInNode - left_cnt) * sizeof(EValueStatus);
        memcpy(&right_node->keys[left_cnt], &node->keys[left_cnt], sz_right_ptrs);
        memcpy(&right_node->childs[left_cnt], &node->childs[left_cnt], sz_right_ptrs);
        memcpy(&right_node->status[left_cnt], &node->status[left_cnt], sz_right_stats);
        memcpy(&node->keys[left_cnt], &right_node->keys[0], left_cnt * sizeof(void*));
        if (left_cnt * 2 < kCntChildsInNode)
            node->keys[kCntChildsInNode - 1] = left_max_key;
        memset(&node->childs[left_cnt], NULL, sz_right_ptrs);
        memset(&node->status[left_cnt], eValueDeleted, sz_right_stats);
        node->cnt_filled        = left_cnt;
        right_node->cnt_filled  = kCntChildsInNode - left_cnt;
        right_node->max_key     = node->max_key;
        node->max_key           = left_max_key;
        right_node->right_node  = node->right_node;
        node->right_node        = right_node;
        call_ctx.split_level    = node->tree_level;
        call_ctx.left_node      = node;
        call_ctx.right_node     = right_node;
        // Remembered keys shouldn't be referenced here because it's guaranteed
        // that nodes won't be deleted until we finish our split addition. Because
        // of that at least one reference to keys will exist as max_key inside
        // some nodes (probably not node and right_node but some nodes for sure).
        call_ctx.left_key       = left_max_key;
        call_ctx.right_key      = right_node->max_key;

        if (x_IsKeyLess(left_max_key, call_ctx.lookup_key)) {
            call_ctx.tree_path[node->tree_level] = right_node;
            x_ExchangeNodeLocks(call_ctx, right_node);
        }
    }
    void x_PropagateSplit(SCallContext& call_ctx)
    {
        x_UnlockCurNode(call_ctx);
        while (call_ctx.split_level != 0) {
            if (LIKELY(call_ctx.split_level != call_ctx.tree_height))
                x_AddRegularSplit(call_ctx);
            else
                x_CheckRootSplit(call_ctx);
        }
    }
    void x_AddRegularSplit(SCallContext& call_ctx)
    {
        SRefedKey* left_key  = call_ctx.left_key;
        SRefedKey* right_key = call_ctx.right_key;
        SNode* left_node     = call_ctx.left_node;
        SNode* right_node    = call_ctx.right_node;
        call_ctx.cur_level   = call_ctx.split_level + 1;
        x_LockNodeAndWaitKey(call_ctx, right_key, left_node);
        SNode* node = call_ctx.locked_node;
        call_ctx.split_level = 0;
        TNodeIndex key_index = x_AddNodeKey(call_ctx, node, left_key, left_node);
        // node can be split and changed inside x_AddNodeKey
        node = call_ctx.locked_node;
        _ASSERT(node->keys[key_index + 1] == right_key
                &&  node->childs[key_index + 1] == left_node);
        node->childs[key_index + 1] = right_node;
        x_UnlockCurNode(call_ctx);
    }
    void x_CheckRootSplit(SCallContext& call_ctx)
    {
        // We need to check that root wasn't split already by someone else.
        SNode* new_root;
        TTreeHeight new_height;
        x_GetRootAndHeight(new_root, new_height, false);
        if (LIKELY(new_height == call_ctx.tree_height)) {
            _ASSERT(new_root == call_ctx.left_node);
            x_AddNewRoot(call_ctx);
        }
        else {
            _ASSERT(new_height > call_ctx.tree_height);
            TTreeHeight old_height = call_ctx.tree_height;
            call_ctx.tree_height = new_height;
            call_ctx.cur_level = new_height;
            call_ctx.tree_path[new_height] = new_root;
            while (call_ctx.cur_level != old_height + 1) {
                _VERIFY(x_DiveToNextLevel(call_ctx));
            }
        }
    }
    void x_AddNewRoot(SCallContext& call_ctx)
    {
        if (UNLIKELY(call_ctx.tree_height == kMaxTreeHeight)) {
            CNcbiDiag::DiagTrouble(DIAG_COMPILE_INFO,
                                   "Concurrent map is too deep");
            abort();
        }
        TTreeHeight new_height = call_ctx.tree_height + 1;
        SNode* new_root = x_CreateNode(NULL, new_height);
        call_ctx.tree_path[new_height] = new_root;
        new_root->keys  [0]  = call_ctx.left_key;
        // We must add reference to the key when we add it to the root - it wasn't
        // referenced when it was added to call_ctx.
        x_AddKeyRef(call_ctx.left_key);
        _ASSERT(new_root->keys[1] == call_ctx.right_key);
        new_root->childs[0]  = call_ctx.left_node;
        new_root->childs[1]  = call_ctx.right_node;
        new_root->status[0]  = eValueActive;
        new_root->status[1]  = eValueActive;
        new_root->cnt_filled = 2;
        x_ChangeRoot(new_root, new_height);
        call_ctx.split_level = 0;
    }
    void x_DeleteEmptyNodes(SCallContext& call_ctx)
    {
        SNode* node = call_ctx.tree_path[kLeafTreeLevel];
        while (x_IsNodeToBeDeleted(node)) {
            SRefedKey* del_key = node->max_key;
            x_AddKeyRef(del_key);
            x_UnlockCurNode(call_ctx);
            call_ctx.cur_level = node->tree_level + 1;
            // We delete the node but its pointer is still used in the next call,
            // although without dereferencing. We do this to process deletion
            // outside of any node lock.
            x_DeleteNode(node);
            TNodeIndex index = x_LockNodeAndWaitKey(call_ctx, del_key, node);
            x_DerefKey(del_key);
            node = call_ctx.locked_node;
            node->status[index] = eValueDeleted;
            --node->cnt_filled;
        }
        if (x_CanShrinkTree()) {
            // As locked_node can be root we need to unlock it before shrinking -
            // it can be already deleted on return.
            x_UnlockCurNode(call_ctx);
            x_ShrinkTree();
        }
    }


    mutable CSpinRWLock     m_RootLock;
    SNode*                  m_RootNode;
    TDeferredDeleter        m_NodesDeleter;
    mutable CAtomicCounter  m_CntRootRefs;
    CAtomicCounter          m_CntNodes;
    CAtomicCounter          m_CntLeafNodes;
    CAtomicCounter          m_CntValues;
    TTreeHeight             m_TreeHeight;
    TComparator             m_Comparator;
    TNodeAlloc              m_NodeAlloc;
    TLeafAlloc              m_LeafAlloc;
    TRefedKeyAlloc          m_RefedKeyAlloc;
};

END_NCBI_SCOPE

#endif /* NETCACHE__CONCURRENT_MAP__HPP */
