#ifndef NETCACHE_NC_UTILS__HPP
#define NETCACHE_NC_UTILS__HPP
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
 *
 * File Description: 
 *   Utility classes that now are used only in NetCache but can be used
 *   anywhere else.
 */


#include <util/simple_buffer.hpp>


#ifdef NCBI_COMPILER_GCC
# define ATTR_PACKED    __attribute__ ((packed))
# define ATTR_ALIGNED_8 __attribute__ ((aligned(8)))
#else
# define ATTR_PACKED
# define ATTR_ALIGNED_8
#endif



BEGIN_NCBI_SCOPE


static const char* const kNCPeerClientName = "nc_peer";


typedef map<string, string> TStringMap;
typedef map<Uint8, string>  TNCPeerList;
typedef vector<Uint8>       TServersList;
typedef CSimpleBufferT<char> TNCBufferType;


/// Type of access to NetCache blob
enum ENCAccessType {
    eNCRead,        ///< Read meta information only
    eNCReadData,    ///< Read blob data
    eNCCreate,      ///< Create blob or re-write its contents
    eNCCopyCreate   ///< (Re-)write blob from another NetCache (as opposed to
                    ///< writing from client)
} NCBI_PACKED_ENUM_END();


/// Statuses of commands to be set in diagnostics' request context
/// Additional statuses can be taken from
/// http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
enum EHTTPStatus {
    /// Command is ok and execution is good.
    eStatus_OK          = 200,
    /// Inactivity timeout on connection.
    eStatus_Inactive    = 204,
    /// SYNC_START command is ok but synchronization by blobs list will be
    /// performed.
    eStatus_SyncBList   = 205,
    /// Operation is not done as the same or newer blob is present on the
    /// server.
    eStatus_NewerBlob   = 206,
    /// "Fake" connection detected -- no data received from client at all.
    eStatus_FakeConn    = 207,
    /// Status for PUT2 command or connection that used PUT2 command.
    eStatus_PUT2Used    = 301,
    /// SETVALID cannot be executed as new version was already written.
    eStatus_RaceCond    = 302,
    /// Synchronization cannot start because server is busy doing cleaning or
    /// some other synchronization on this slot.
    eStatus_SyncBusy    = 303,
    /// Synchronization is rejected because both servers tried to start it
    /// simultaneously.
    eStatus_CrossSync   = 305,
    /// Synchronization is aborted because cleaning thread waited for too much
    /// or because server needs to shutdown.
    eStatus_SyncAborted = 306,
    /// Caching of the database was canceled because server needs to shutdown.
    eStatus_OperationCanceled = 306,
    /// Command cannot be executed because NetCache didn't cache the database
    /// contents.
    eStatus_JustStarted = 308,
    /// Connection was forcefully closed because server needs to shutdown.
    eStatus_ShuttingDown = 309,
    /// Command is incorrect.
    eStatus_BadCmd      = 400,
    /// Bad password for accessing the blob.
    eStatus_BadPassword = 401,
    /// Client was disabled in configuration.
    eStatus_Disabled    = 403,
    /// Blob was not found.
    eStatus_NotFound    = 404,
    /// Operation not allowed with current settings (e.g. password given but
    /// ini-file says that only blobs without password are allowed)
    eStatus_NotAllowed  = 405,
    /// Command requires admin privileges.
    eStatus_NeedAdmin   = 407,
    /// Command timeout is exceeded.
    eStatus_CmdTimeout  = 408,
    /// Precondition stated in command has failed (size of blob was given but
    /// data has a different size).
    eStatus_CondFailed  = 412,
    /// Connection was closed too early (client didn't send all data or didn't
    /// get confirmation about successful execution).
    eStatus_PrematureClose = 499,
    /// Internal server error.
    eStatus_ServerError = 500,
    /// Command is not implemented.
    eStatus_NoImpl      = 501,
    /// Synchronization command belongs to session that was already canceled
    eStatus_StaleSync   = 502,
    /// Command should be proxied to peers but it's impossible to connect to
    /// peers.
    eStatus_PeerError   = 503,
    /// There's not enough disk space to execute the command.
    eStatus_NoDiskSpace = 504,
    /// Command was aborted because server needs to shutdown.
    eStatus_CmdAborted  = 505
};


/// Maps between status codes and error texts sent to client
/// to explain these codes.
extern map<int, string> s_MsgForStatus;
extern map<string, int> s_StatusForMsg;

/// Initializes maps between status codes and error texts sent to client
/// to explain these codes.
void InitClientMessages(void);


/*
/// Type to use for bit masks
typedef size_t  TNCBitMaskElem;
/// Number of bits in one element of type TNCBitMaskElem
static const unsigned int kNCMaskElemSize = SIZEOF_SIZE_T * 8;

/// Base class for bit masks of any size.
/// Bit mask is a set of bits with indexes starting with 0.
/// General template class implements functionality suitable for working with
/// bit masks of any size although for special cases of 1 or 2 elements it can
/// be optimized (see template's specializations) and for now only
/// 1- and 2-elements specializations are used in NetCache.
///
/// @param num_elems
///   Number of elements of type TNCBitMaskElem that class should contain.
template <int num_elems>
class CNCBitMaskBase
{
public:
    /// Empty constructor.
    /// All initialization is made via Initialize() method.
    CNCBitMaskBase            (void);
    /// Initialize bit mask with all zeros.
    void         Initialize   (void);
    /// Check if bit with given number is set.
    bool         IsBitSet     (unsigned int bit_num);
    /// Invert value of bits_cnt bits starting from start_bit.
    void         InvertBits   (unsigned int start_bit, unsigned int bits_cnt);
    /// Get number of bits set in the mask
    unsigned int GetCntSet    (void);
    /// Get index of lowest set bit with index greater or equal to bit_num.
    /// If there's no such bit in the mask then return -1.
    int          GetClosestSet(unsigned int bit_num);

private:
    /// Calculate coordinates of the bit with index bit_index (calculate index
    /// of byte it located in and index of bit in that byte.
    void x_CalcCoords(unsigned int  bit_index,
                      unsigned int& byte_num,
                      unsigned int& bit_num);

    /// Bit mask itself
    TNCBitMaskElem m_Mask[num_elems];
};

/// Optimized specialization of bit mask that fits entirely in 1 element of
/// type TNCBitMaskElem.
template <>
class CNCBitMaskBase<1>
{
public:
    CNCBitMaskBase            (void);
    void         Initialize   (void);
    bool         IsBitSet     (unsigned int bit_num);
    void         InvertBits   (unsigned int start_bit, unsigned int bits_cnt);
    unsigned int GetCntSet    (void);
    int          GetClosestSet(unsigned int bit_num);

private:
    TNCBitMaskElem m_Mask;
};

/// Optimized specialization of bit mask that fits entirely in 2 elements of
/// type TNCBitMaskElem.
template <>
class CNCBitMaskBase<2>
{
public:
    CNCBitMaskBase            (void);
    void         Initialize   (void);
    bool         IsBitSet     (unsigned int bit_num);
    void         InvertBits   (unsigned int start_bit, unsigned int bits_cnt);
    unsigned int GetCntSet    (void);
    int          GetClosestSet(unsigned int bit_num);

private:
    TNCBitMaskElem m_Mask[2];
};

/// Class for working with bit masks containing given number of bits.
template <int num_bits>
class CNCBitMask : public CNCBitMaskBase<(num_bits + kNCMaskElemSize - 1)
                                         / kNCMaskElemSize>
{
    // Expression here should be a copy of expression in declaration.
    typedef CNCBitMaskBase<(num_bits + kNCMaskElemSize - 1)
                           / kNCMaskElemSize>                  TBase;

public:
    /// Empty constructor.
    /// All initialization happens in Initialize().
    CNCBitMask                (void);
    /// Initialize bit mask.
    ///
    /// @param init_value
    ///   Value for initialization of all bits. If this value is 0 then all
    ///   bits are initialized with 0, otherwise all bits are initialized
    ///   with 1.
    void         Initialize   (unsigned int init_value);

    // Redirections to methods of base class performing additional checks in
    // Debug mode.
    bool         IsBitSet     (unsigned int bit_num);
    void         InvertBits   (unsigned int start_bit, unsigned int bits_cnt);
    unsigned int GetCntSet    (void);
    int          GetClosestSet(unsigned int bit_num);
};
*/

/*
/// Get number of least bit set.
/// For instance for the binary number 10010100 it will return 2.
inline unsigned int
g_GetLeastSetBit(size_t value)
{
    value ^= value - 1;
    value >>= 1;
    ++value;
    return g_GetLogBase2(value);
}

/// Get number of bits set.
/// Implementation uses minimum number of operations without using additional
/// variables or memory. Trick was taken from
/// http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel.
inline unsigned int
g_GetBitsCnt(size_t value)
{
#if SIZEOF_SIZE_T == 8
    value = value - ((value >> 1) & 0x5555555555555555);
    value = (value & 0x3333333333333333) + ((value >> 2) & 0x3333333333333333);
    value = (value + (value >> 4)) & 0x0F0F0F0F0F0F0F0F;
    value = (value * 0x0101010101010101) >> 56;
    return static_cast<unsigned int>(value);
#elif SIZEOF_SIZE_T == 4
    value = value - ((value >> 1) & 0x55555555);
    value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
    value = (value + (value >> 4)) & 0x0F0F0F0F;
    value = (value * 0x01010101) >> 24;
    return value;
#else
# error "Cannot compile with this size_t"
#endif
}
*/

//////////////////////////////////////////////////////////////////////////
// Inline methods
//////////////////////////////////////////////////////////////////////////
/*
template <int num_elems>
inline
CNCBitMaskBase<num_elems>::CNCBitMaskBase(void)
{}

template <int num_elems>
inline void
CNCBitMaskBase<num_elems>::Initialize(void)
{
    memset(m_Mask, 0, sizeof(m_Mask));
}

template <int num_elems>
inline void
CNCBitMaskBase<num_elems>::x_CalcCoords(unsigned int  bit_index,
                                        unsigned int& byte_num,
                                        unsigned int& bit_num)
{
    byte_num = bit_index / kNCMaskElemSize;
    bit_num  = bit_index - byte_num * kNCMaskElemSize;
}

template <int num_elems>
inline bool
CNCBitMaskBase<num_elems>::IsBitSet(unsigned int bit_num)
{
    unsigned int byte_num, bit_index;
    x_CalcCoords(bit_num, byte_num, bit_index);
    return (m_Mask[byte_num] & (TNCBitMaskElem(1) << bit_index)) != 0;
}

template <int num_elems>
inline void
CNCBitMaskBase<num_elems>::InvertBits(unsigned int start_bit,
                                      unsigned int bits_cnt)
{
    unsigned int byte_num, bit_num;
    x_CalcCoords(start_bit, byte_num, bit_num);
    do {
        TNCBitMaskElem inv_mask;
        if (bits_cnt >= kNCMaskElemSize)
            inv_mask = TNCBitMaskElem(-1);
        else
            inv_mask = (TNCBitMaskElem(1) << bits_cnt) - 1;
        m_Mask[byte_num] ^= inv_mask << bit_num;
        unsigned int num_inved = kNCMaskElemSize - bit_num;
        bits_cnt = (bits_cnt > num_inved? bits_cnt - num_inved: 0);
        ++byte_num;
        bit_num = 0;
    }
    while (bits_cnt != 0);
}

template <int num_elems>
inline unsigned int
CNCBitMaskBase<num_elems>::GetCntSet(void)
{
    unsigned int cnt_set = 0;
    for (unsigned int i = 0; i < num_elems; ++i) {
        cnt_set += g_GetBitsCnt(m_Mask[i]);
    }
    return cnt_set;
}

template <int num_elems>
inline int
CNCBitMaskBase<num_elems>::GetClosestSet(unsigned int bit_num)
{
    unsigned int byte_num, bit_index;
    x_CalcCoords(bit_num, byte_num, bit_index);
    TNCBitMaskElem mask = 0;
    if (bit_index < kNCMaskElemSize - 1) {
        mask = m_Mask[byte_num] & ~((TNCBitMaskElem(1) << bit_index) - 1);
    }
    if (mask == 0) {
        ++byte_num;
        while (byte_num < kNCMaskElemSize  &&  (mask = m_Mask[byte_num]) == 0)
        {
            ++byte_num;
        }
    }
    if (mask == 0) {
        return -1;
    }
    else {
        return g_GetLeastSetBit(mask) + byte_num * kNCMaskElemSize;
    }
}


inline
CNCBitMaskBase<1>::CNCBitMaskBase(void)
{}

inline void
CNCBitMaskBase<1>::Initialize(void)
{
    m_Mask = 0;
}

inline bool
CNCBitMaskBase<1>::IsBitSet(unsigned int bit_num)
{
    return (m_Mask & (TNCBitMaskElem(1) << bit_num)) != 0;
}

inline void
CNCBitMaskBase<1>::InvertBits(unsigned int start_bit,
                              unsigned int bits_cnt)
{
    TNCBitMaskElem inv_mask = (bits_cnt == kNCMaskElemSize? 0:
                                           (TNCBitMaskElem(1) << bits_cnt));
    --inv_mask;
    m_Mask ^= inv_mask << start_bit;
}

inline unsigned int
CNCBitMaskBase<1>::GetCntSet(void)
{
    return g_GetBitsCnt(m_Mask);
}

inline int
CNCBitMaskBase<1>::GetClosestSet(unsigned int bit_num)
{
    TNCBitMaskElem mask = m_Mask & ~((TNCBitMaskElem(1) << bit_num) - 1);
    if (mask == 0) {
        return -1;
    }
    else {
        return g_GetLeastSetBit(mask);
    }
}


inline
CNCBitMaskBase<2>::CNCBitMaskBase(void)
{}

inline void
CNCBitMaskBase<2>::Initialize(void)
{
    m_Mask[0] = m_Mask[1] = 0;
}

inline bool
CNCBitMaskBase<2>::IsBitSet(unsigned int bit_num)
{
    unsigned int byte_num = (bit_num >= kNCMaskElemSize? 1: 0);
    return (m_Mask[byte_num] & (TNCBitMaskElem(1) << bit_num)) != 0;
}

inline void
CNCBitMaskBase<2>::InvertBits(unsigned int start_bit,
                              unsigned int bits_cnt)
{
    TNCBitMaskElem inv_mask;
    if (start_bit < kNCMaskElemSize) {
        if (bits_cnt >= kNCMaskElemSize)
            inv_mask = TNCBitMaskElem(-1);
        else
            inv_mask = (TNCBitMaskElem(1) << bits_cnt) - 1;
        m_Mask[0] ^= inv_mask << start_bit;
        unsigned int last_bit = start_bit + bits_cnt;
        if (last_bit > kNCMaskElemSize) {
            inv_mask = (TNCBitMaskElem(1) << (last_bit - kNCMaskElemSize)) - 1;
            m_Mask[1] ^= inv_mask;
        }
    }
    else {
        inv_mask = (TNCBitMaskElem(1) << bits_cnt) - 1;
        m_Mask[1] ^= inv_mask << (start_bit - kNCMaskElemSize);
    }
}

inline unsigned int
CNCBitMaskBase<2>::GetCntSet(void)
{
    return g_GetBitsCnt(m_Mask[0]) + g_GetBitsCnt(m_Mask[1]);
}

inline int
CNCBitMaskBase<2>::GetClosestSet(unsigned int bit_num)
{
    TNCBitMaskElem mask;
    if (bit_num < kNCMaskElemSize) {
        mask = m_Mask[0] & ~((TNCBitMaskElem(1) << bit_num) - 1);
        if (mask != 0)
            return g_GetLeastSetBit(mask);
        mask = m_Mask[1];
    }
    else {
        unsigned int second_num = bit_num - kNCMaskElemSize;
        mask = m_Mask[1] & ~((TNCBitMaskElem(1) << (second_num)) - 1);
    }
    if (mask == 0) {
        return -1;
    }
    else {
        return g_GetLeastSetBit(mask) + kNCMaskElemSize;
    }
}


template <int num_bits>
inline
CNCBitMask<num_bits>::CNCBitMask(void)
{}

template <int num_bits>
inline void
CNCBitMask<num_bits>::Initialize(unsigned int init_value)
{
    TBase::Initialize();
    if (init_value)
        InvertBits(0, num_bits);
}

template <int num_bits>
inline bool
CNCBitMask<num_bits>::IsBitSet(unsigned int bit_num)
{
    _ASSERT(bit_num < num_bits);
    return TBase::IsBitSet(bit_num);
}

template <int num_bits>
inline void
CNCBitMask<num_bits>::InvertBits(unsigned int start_bit,
                                 unsigned int bits_cnt)
{
    _ASSERT(start_bit + bits_cnt <= num_bits);
    TBase::InvertBits(start_bit, bits_cnt);
}

template <int num_bits>
inline unsigned int
CNCBitMask<num_bits>::GetCntSet(void)
{
    return TBase::GetCntSet();
}

template <int num_bits>
inline int
CNCBitMask<num_bits>::GetClosestSet(unsigned int bit_num)
{
    _ASSERT(bit_num < num_bits);
    return TBase::GetClosestSet(bit_num);
}
*/
END_NCBI_SCOPE

#endif /* NETCACHE_NC_UTILS__HPP */
