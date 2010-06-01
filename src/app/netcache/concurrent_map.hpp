#ifndef NETCACHE__CONCURRENT_BTREE__HPP
#define NETCACHE__CONCURRENT_BTREE__HPP
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

#include <corelib/ncbiatomic.hpp>


BEGIN_NCBI_SCOPE

///
template <class         Key,
          class         Value,
          class         Comparator       = less<Key>,
          unsigned int  NodesMaxFill     = 8,
          unsigned int  DeletionDelay    = 3,
          unsigned int  DelStoreCapacity = 20,
          unsigned int  MaxTreeHeight    = 16>
class CConcurrentMap
{
public:
    typedef CConcurrentMap<Key, Value, Comparator>  TTree;
    typedef Key                                     TKey;
    typedef Value                                   TValue;
    typedef Comparator                              TComparator;

    ///
    enum {
        kNodesMaxFill  = NodesMaxFill,
        kMaxTreeHeight = MaxTreeHeight
    };


    ///
    CConcurrentMap(void);
    ///
    CConcurrentMap(const TComparator& comparator);
    ~CConcurrentMap(void);

    ///
    bool Exists(const TKey& key) const;
    ///
    bool Get(const TKey& key, TValue* value) const;
    ///
    void Put(const TKey& key, const TValue& value);
    ///
    bool Insert(const TKey& key, const TValue& value, TValue* stored_value);
    ///
    bool Change(const TKey& key, const TValue& value);
    ///
    bool Erase(const TKey& key);
    ///
    void Clear(void);

    ///
    enum EValueStatus {
        eValueDeleted,  ///<
        eValuePassive,  ///<
        eValueActive    ///<
    };

    ///
    bool SetValueStatus(const TKey& key, EValueStatus status);
    ///
    bool EraseIfPassive(const TKey& key);

    ///
    enum EGetValueType {
        eGetOnlyActive,         ///<
        eGetActiveAndPassive,   ///<
        eGetOrCreate            ///<
    };

    ///
    TValue* GetPtr(const TKey& key, EGetValueType get_type);
    ///
    bool InsertOrGetPtr(const TKey&   key,
                        const TValue& value,
                        EGetValueType get_type,
                        TValue**      value_ptr);
    ///
    bool GetLowerBound(const TKey&   search_key,
                       TKey&         stored_key,
                       EGetValueType get_type) const;

    ///
    void HeartBeat(void);
    ///
    unsigned int CountValues(void) const;
    ///
    unsigned int CountNodes(void) const;
    ///
    unsigned int TreeHeight(void) const;

private:
    ///
    CConcurrentMap(const CConcurrentMap&);
    CConcurrentMap& operator= (const CConcurrentMap&);


    ///
    enum {
        kMaxNodeIndex = kNodesMaxFill - 1   ///<
    };


    ///
    struct SKeyData
    {
        ///
        SKeyData(const TKey& key);

        ///
        TKey            key;
        ///
        CAtomicCounter  ref_cnt;
    };

    ///
    class CKeyRef
    {
    public:
        ///
        CKeyRef(void);
        ~CKeyRef(void);
        ///
        CKeyRef& operator= (const TKey& key);
        ///
        CKeyRef(const CKeyRef& key_ref);
        CKeyRef& operator= (const CKeyRef& key_ref);

        ///
        bool IsMaximum(void) const;
        ///
        bool IsEqual(const CKeyRef& other) const;
        ///
        const TKey& GetKey(void) const;
        ///
        void Swap(CKeyRef& other);

    private:
        ///
        void x_AddRef(void);
        ///
        void x_RemoveRef(void);


        ///
        SKeyData* m_KeyData;
    };

    ///
    class CNode
    {
    public:
        ///
        CNode(const TTree* const tree,
              const CKeyRef&     max_key,
              unsigned int       deep_level);
        ~CNode(void);

        ///
        void Lock(ERWLockType lock_type);
        ///
        void Unlock(ERWLockType lock_type);
        ///
        unsigned int FindContainIndex(const TKey& key);
        ///
        unsigned int FindContainIndex(const CKeyRef& key_ref);
        ///
        bool FindExactIndex(const TKey&    key,
                            unsigned int&  insert_index);
        ///
        bool FindExactIndex(const CKeyRef& key_ref,
                            unsigned int&  insert_index);
        ///
        unsigned int GetNextIndex(unsigned int index);

        ///
        unsigned int    GetCntFilled  (void);
        ///
        const CKeyRef&  GetMaxKey     (void);
        ///
        unsigned int    GetDeepLevel  (void);
        ///
        const CKeyRef&  GetKey        (unsigned int index);
        ///
        TValue*         GetValue      (unsigned int index);
        ///
        EValueStatus    GetValueStatus(unsigned int index);
        ///
        CNode*          GetLowerNode  (unsigned int index);
        ///
        CNode*          GetRightNode  (void);

        ///
        void SetValueStatus(unsigned int index, EValueStatus status);
        ///
        TValue* Insert(unsigned int index, const TKey& key, const TValue& value);
        ///
        TValue* Insert(const TKey& key, const TValue& value);
        ///
        void Delete(unsigned int index);
        ///
        CNode* Split(void);
        ///
        void AddSplit(const CKeyRef& left_key,
                      const CKeyRef& right_key,
                      CNode*         right_node);
        ///
        void AddRootSplit(const CKeyRef& left_key,
                          CNode*         left_node,
                          CNode*         right_node);
        ///
        unsigned int AddMaxChild(void);
        ///
        void Clear(void);

    private:
        ///
        CNode(const CNode&);
        CNode& operator= (const CNode&);

        ///
        unsigned int x_FindKeyIndex(const TKey& key);
        ///
        unsigned int x_GetIndexOfMaximum(void);
        ///
        void x_FindInsertSpace(unsigned int& index, unsigned int& insert_index);
        ///
        void x_ClearValues(void);
        ///
        void x_ClearLowerNodes(void);


        ///
        const TTree* const  m_Tree;
        ///
        CSpinRWLock         m_NodeLock;
        ///
        CKeyRef             m_MaxKey;
        ///
        unsigned int        m_DeepLevel;
        ///
        unsigned int        m_CntFilled;
        ///
        CNode*              m_RightNode;
        ///
        CKeyRef             m_Keys  [kNodesMaxFill];
        ///
        CNode*              m_Values[kNodesMaxFill];
        ///
        EValueStatus        m_Status[kNodesMaxFill];
    };

    ///
    enum EForceKeyType {
        eDoNotForceKey,     ///<
        eForceBiggerKey,    ///<
        eForceExactKey      ///<
    };

    ///
    class CTreeWalker
    {
    public:
        ///
        CTreeWalker(const TTree* const  tree,
                    CNode** const       path_nodes,
                    const TKey&         key,
                    ERWLockType         lock_type,
                    EForceKeyType       force_type);
        ///
        ~CTreeWalker(void);

        ///
        bool IsKeyFound(void);
        ///
        const TKey& GetKey(void);
        ///
        TValue* GetValue(void);
        ///
        EValueStatus GetValueStatus(void);
        ///
        void SetValueStatus(EValueStatus status);
        ///
        TValue* Insert(const TValue& value);
        ///
        void Delete(void);

    private:
        ///
        void x_LockNode  (CNode*& node, ERWLockType lock_type);
        ///
        void x_UnlockNode(CNode*  node, ERWLockType lock_type);
        ///
        void x_FindKey(void);
        ///
        void x_FindBiggerKey(CNode*&       node,
                             unsigned int& node_level);
        ///
        void x_AddSplitToAbove(CNode*       left_node,
                               CNode*       right_node,
                               unsigned int above_level);
        ///
        bool x_AddRootSplit(const CKeyRef& left_key,
                            CNode*         left_node,
                            const CKeyRef& right_key,
                            CNode*         right_node);
        ///
        void x_AddExactKeyNode(CNode*&       node,
                               unsigned int& node_level);
        ///
        unsigned int x_WaitForKeyAppear(CNode*         search_node,
                                        const CKeyRef& key,
                                        CNode*         key_node);


        ///
        const TTree* const   m_Tree;
        ///
        CNode** const        m_PathNodes;
        ///
        const TKey&          m_Key;
        ///
        const ERWLockType    m_LockType;
        ///
        const EForceKeyType  m_ForceType;
        ///
        unsigned int         m_DeepLevel;
        ///
        bool                 m_KeyFound;
        ///
        unsigned int         m_KeyIndex;
        ///
        CNode*               m_LockedNode;
    };

    typedef CNCDeferredDeleter<CNode*, Deleter<CNode>,
                               DeletionDelay, DelStoreCapacity> TDeferredDeleter;

    friend class CTreeWalker;
    friend class CNode;


    ///
    void x_Initialize(void);
    ///
    CNode* x_CreateNode(const CKeyRef& key_ref, unsigned int deep_level) const;
    ///
    void x_DeleteNode(CNode* node);
    ///
    void x_GetRoot(CNode*& node, unsigned int& deep_level) const;
    ///
    void x_ChangeRoot(CNode* node, unsigned int deep_level);
    ///
    void x_RemoveRootRef(void) const;
    ///
    bool x_CanShrinkTree(void) const;
    ///
    void x_ShrinkTree(void);
    ///
    bool x_IsKeyLess (const TKey& left, const TKey& right) const;
    bool x_IsKeyLess (const CKeyRef& left_ref, const TKey& right) const;
    bool x_IsKeyLess (const TKey& left, const CKeyRef& right_ref) const;
    bool x_IsKeyLess (const CKeyRef& left_ref, const CKeyRef& right_ref) const;
    ///
    bool x_IsKeyEqual(const CKeyRef& left_ref, const CKeyRef& right_ref) const;


    ///
    TComparator             m_Comparator;
    ///
    mutable CSpinRWLock     m_RootLock;
    ///
    unsigned int            m_DeepLevel;
    ///
    CNode*                  m_RootNode;
    ///
    mutable CAtomicCounter  m_CntRootRefs;
    ///
    mutable CAtomicCounter  m_CntNodes;
    ///
    CAtomicCounter          m_CntValues;
    ///
    bool                    m_IsDeleting;
    ///
    TDeferredDeleter        m_NodesDeleter;
};

END_NCBI_SCOPE


#include "concurrent_map.inl"


#endif /* NETCACHE__CONCURRENT_BTREE__HPP */
