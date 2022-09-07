#ifndef __TABLE2ASN_UTILS_HPP
#define __TABLE2ASN_UTILS_HPP

#include <mutex>
#include <map>

BEGIN_NCBI_SCOPE

bool AssignLocalIdIfEmpty(ncbi::objects::CSeq_feat& feature, int& id);

class COnceMutex
{
public:
    void lock() {
        m_mutex.lock();
        auto visited = m_counter++;
        if (visited == 0)
            m_counter++;
        else
            m_mutex.unlock();
    }

    void unlock() {
        auto visited = m_counter--;
        if (visited == 2) {
            m_mutex.unlock();
        }
    }

    bool IsFirstRun() const {
        return 2 == m_counter.load();
    }

private:
    std::mutex          m_mutex;
    std::atomic<size_t> m_counter{0};
};

class CSharedOStream;
class CSharedStreamMap: public std::map<std::string, std::tuple<std::string, unique_ptr<std::ostream>, std::mutex>>
{
public:
    CSharedOStream GetOstream(CTempString suffix);
    void ClearOstream(const CTempString& suffix);
private:
    std::mutex m_mutex;
};

class CSharedOStream
{
public:
    CSharedOStream(CSharedStreamMap::mapped_type* owner);
    CSharedOStream(const CSharedOStream&) = delete;
    CSharedOStream(CSharedOStream&&);
    CSharedOStream& operator=(CSharedOStream&&);
    CSharedOStream() = default;
    ~CSharedOStream();
    operator std::ostream&() { return get(); };
    std::ostream& get();
    std::string& filename() { return std::get<0>(*m_owner); }
private:
    CSharedStreamMap::mapped_type* m_owner = nullptr;
};

template<typename _Stream, size_t _bufsize=8192*2>
class CBufferedStream
{
public:
    CBufferedStream() {}
    ~CBufferedStream() {}

    operator _Stream&()
    {
        return get();
    }
    _Stream& get()
    {
        if (!m_buffer)
        {
            m_buffer.reset(new char[bufsize]);
            m_stream.rdbuf()->pubsetbuf(m_buffer.get(), bufsize);
        }
        return m_stream;
    }
private:
    static constexpr size_t bufsize = _bufsize;
    unique_ptr<char> m_buffer;
    _Stream m_stream;
};

//using CBufferedOutput = CBufferedStream<CNcbiOfstream>;
using CBufferedInput  = CBufferedStream<CNcbiIfstream>;

END_NCBI_SCOPE

#endif
