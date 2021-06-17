#ifndef OLIGOFAR_CTRANSCRIPT__HPP
#define OLIGOFAR_CTRANSCRIPT__HPP

#include "debug.hpp"
#include "defs.hpp"

BEGIN_OLIGOFAR_SCOPES

class CTrBase
{
public:
    enum EEvent { 
        eEvent_Match     = 0x01, // bases match
        eEvent_Replaced  = 0x02, // replacement
        eEvent_Insertion = 0x03, // query has extra base
        eEvent_Deletion  = 0x04, // subject ahs an extra base
        eEvent_Splice    = 0x05, // few subject bases are skiped with no penalty
        eEvent_Overlap   = 0x06, // same subject bases are mathed twice by different bases of the read
        eEvent_Padding   = 0x07, // CIGAR - for multisequence alignment, no action
        eEvent_SoftMask  = 0x08, // end of the query is not aligned, but is present in SAM/BAM
        eEvent_HardMask  = 0x09, // end of the query is not aligned and is not present in SAM/BAM
        eEvent_Changed   = 0x0a, // allowed replacement (C->T with bisulfite treatment)
        eEvent_NULL      = 0,
    };
    static EEvent Char2Event( char x );
    static char Event2Char( EEvent x ) { return ":MRIDNBPSHC....."[x]; }
protected:
    static char s_char2event[];
};

inline CTrBase::EEvent CTrBase::Char2Event( char x ) {
    EEvent y = (EEvent) 
        "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0" "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0"
        "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0" "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0"
        //    A  B  C  D  E  F  G    H  I  J  K  L  M  N  O     P  Q  R  S  T  U  V  W    X  Y  Z
        "\x0\x0\x6\xa\x4\x0\x0\x0""\x9\x3\x0\x0\x0\x1\x5\x0" "\x7\x0\x2\x8\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0"
        "\x0\x0\x6\xa\x4\x0\x0\x0""\x9\x3\x0\x0\x0\x1\x5\x0" "\x7\x0\x2\x8\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0"
        "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0" "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0"
        "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0" "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0"
        "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0" "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0"
        "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0" "\x0\x0\x0\x0\x0\x0\x0\x0""\x0\x0\x0\x0\x0\x0\x0\x0"
        [int( (unsigned char)x)];
    if( y == eEvent_NULL ) THROW( logic_error, "Unknown CIGAR event char ascii" << int(x) );
    return y;
}


template<int bitst = 4, int bitsc = 12>
class CTrItemPacked : public CTrBase
{
public:
    CTrItemPacked() : m_event(0), m_count(0) {}
    CTrItemPacked( EEvent t, unsigned c = 1 ) : m_event( t ), m_count( c ) { ASSERT( c <= GetMaxCount() ); }
    CTrItemPacked( const CTrItemPacked& other, unsigned cnt ) : m_event( other.GetEvent() ), m_count( cnt ) { ASSERT( cnt <= GetMaxCount() ); }
    CTrItemPacked( const CTrItemPacked& other ) : m_event( other.GetEvent() ), m_count( other.GetCount() ) {}
    template<class TOther>
    CTrItemPacked( const TOther& other ) : m_event( other.GetEvent() ), m_count( other.GetCount() ) { 
        if( other.GetCount() > (int)GetMaxCount() ) { THROW( logic_error, "Event count limit exceeded: " << other.GetCount() << " > " << (int)GetMaxCount() << " for event " << Event2Char( other.GetEvent() ) ); } }
    int AddCount( int count ) {
        unsigned ncnt = count + GetCount();
        if( ncnt > GetMaxCount() ) {
            m_count = GetMaxCount();
            return ncnt - GetMaxCount();
        } else {
            m_count = ncnt;
            return 0;
        }
    }
    template<class TOtherItem>
    TOtherItem Add( const TOtherItem& o ) {
        if( GetCount() == 0 ) m_event = o.GetEvent();
        else if( o.GetEvent() != GetEvent() ) return o;
        return TOtherItem( o, AddCount( o.GetCount() ) ); 
    }
    void SetEvent( EEvent ev ) { m_event = ev; }
    EEvent GetEvent() const { return EEvent( m_event ); }
    int GetCount() const { return m_count; }
    static size_t GetMaxCount() { return ~((~size_t(0)) << bitsc); }
    static char GetCigar( EEvent t ) { return Event2Char( t ); } // considering that NULL can be at the end only
    char GetCigar() const { return GetCigar( GetEvent() ); }
    CTrItemPacked<bitst,bitsc>& DecrCount() { if( m_count == 0 ) THROW( logic_error, "Attempt to decrement count below 0" ); --m_count; return *this; }
    CTrItemPacked<bitst,bitsc>& IncrCount() { if( m_count == GetMaxCount() ) THROW( logic_error, "Attempt to increment count above max" ); ++m_count; return *this; }
    bool operator == ( const CTrItemPacked<bitst,bitsc>& other ) const { return m_event == other.m_event && m_count == other.m_count; }
    bool operator != ( const CTrItemPacked<bitst,bitsc>& other ) const { return m_event != other.m_event || m_count != other.m_count; }
protected:
    unsigned int m_event:bitst;
    unsigned int m_count:bitsc;
};

template<unsigned bbits, unsigned cbits>
inline std::ostream& operator << ( std::ostream& out, const CTrItemPacked<bbits,cbits>& ti ) 
{
    return out << ti.GetCount() << ti.GetCigar();
}

template<class T>
class CTrVector : public CTrBase, public vector<T>
{
public:
    typedef T value_type;
    typedef CTrVector<T> self_type;

    static size_t GetMaxItemCount() { return value_type::GetMaxCount(); }

    template<class TT>
    self_type& Assign( const TT& tt ) { self_type::clear(); return AppendFwd( tt ); }
    self_type& Reverse() { reverse( self_type::begin(), self_type::end() ); return *this; }

    self_type& AppendItem( EEvent t, unsigned cnt ) {
        if( cnt == 0 ) return *this;
        //ASSERT( t != eEvent_NULL );
        if( (!self_type::empty()) && self_type::back().GetEvent() == t ) cnt = self_type::back().AddCount( cnt );
        for( ; cnt > GetMaxItemCount() ; cnt -= GetMaxItemCount() ) push_back( T( t, GetMaxItemCount() ) );
        if( cnt ) push_back( T( t, cnt ) );
        return *this;
    }

    template<class TT>
    self_type& AppendItem( TT tt ) {  
        if( tt.GetCount() == 0 ) return *this;
        //ASSERT( tt.GetEvent() != eEvent_NULL );
        if( (!self_type::empty()) && self_type::back().GetEvent() == tt.GetEvent() ) tt = self_type::back().Add( tt );
        for( ; (unsigned)tt.GetCount() > GetMaxItemCount() ; tt = TT( tt, tt.GetCount() - GetMaxItemCount() ) ) 
            push_back( T( tt, GetMaxItemCount() ) );
        if( tt.GetCount() ) push_back( T( tt ) );
        return *this;
    }

    template<class TT>
    self_type& AppendFwd( const TT& tt ) { return Append( tt.begin(), tt.end() ); }
    template<class TT>
    self_type& AppendRev( const TT& tt ) { return Append( tt.rbegin(), tt.rend() ); }

    template<class Iter>
    self_type& Append( Iter begin, Iter end ) {
        for( ; begin != end ; ++begin ) AppendItem( *begin );
        return *this;
    }

    CTrVector() {}
    CTrVector( const char * cigar ) { Assign( cigar ); }
    CTrVector( const string& cigar ) { Assign( cigar ); }
    template<class Other>
    CTrVector( const Other& other ) { AppendFwd( other ); }

    self_type& Assign( const string& cigar ) { return Assign( cigar.c_str() ); }
    self_type& Assign( const char * cigar ) {
        self_type::clear(); 
        if( cigar == 0 ) return *this;
        for( const char * x = cigar; *x; ++x ) {
            int count = isdigit( *x ) ? strtol( x, const_cast<char**>(&x), 10 ) : 1;
            EEvent event = Char2Event(*x);
            if( count > 0 ) AppendItem( event, count );
        }
        return *this;
    }
    pair<int,int> ComputeLengths() const {
        int qlen = 0, slen = 0;
        for( typename self_type::const_iterator iter = self_type::begin(); iter != self_type::end(); ++iter ) {
            switch( iter->GetEvent() ) {
            case eEvent_NULL:
            case eEvent_Padding:
            case eEvent_SoftMask:
            case eEvent_HardMask: break;
            case eEvent_Insertion: qlen += iter->GetCount(); break;
            case eEvent_Overlap: slen -= iter->GetCount(); break;
            case eEvent_Splice: 
            case eEvent_Deletion: slen += iter->GetCount(); break;
            case eEvent_Match:
            case eEvent_Changed:
            case eEvent_Replaced: qlen += iter->GetCount(); slen += iter->GetCount(); break;
            }
        }
        return make_pair( qlen, slen );
    }
    unsigned ComputeSubjectLength() const {
        int len = 0;
        for( typename self_type::const_iterator iter = self_type::begin(); iter != self_type::end(); ++iter ) {
            switch( iter->GetEvent() ) {
            case eEvent_NULL:
            case eEvent_Padding:
            case eEvent_SoftMask:
            case eEvent_HardMask:
            case eEvent_Insertion: break;
            case eEvent_Overlap: len -= iter->GetCount(); break;
            case eEvent_Splice:
            case eEvent_Deletion:
            case eEvent_Match:
            case eEvent_Changed:
            case eEvent_Replaced: len += iter->GetCount(); break;
            }
        }
        ASSERT( len >= 0 );
        return len;
    }
            
    template<class Iter>
    static ostream& Print( ostream& o, Iter begin, Iter end ) {
        for( Iter i = begin; i != end; ++i ) {
            if( i->GetCount() ) o << i->GetCount() << i->GetCigar();
        }
        return o;
    }
    ostream& PrintFwd( ostream& o ) const { return Print( o, self_type::begin(), self_type::end() ); }
    ostream& PrintRev( ostream& o ) const { return Print( o, self_type::rbegin(), self_type::rend() ); }
    string ToString() const { ostringstream o; PrintFwd( o ); return o.str(); }
};

template<unsigned bbits, unsigned cbits>
inline std::ostream& operator << ( std::ostream& out, const CTrVector<CTrItemPacked<bbits,cbits> >& tr ) 
{
    return tr.PrintFwd( out );
}

template<int bitst = 4, int bitsc = 12>
class CTrSequence : public CTrBase
{
public:
    // TODO: optimize allocation/deallocation to minimize dry rest 
    typedef CTrSequence<bitst,bitsc> self_type;

    typedef CTrItemPacked<bitst,bitsc> value_type;
    typedef CTrItemPacked<bitst,bitsc> data_type;
    
    typedef CTrItemPacked<bitst,bitsc> * self_pointer_type;
    typedef CTrItemPacked<bitst,bitsc> & self_reference_type;
    
    typedef const CTrItemPacked<bitst,bitsc> * self_const_pointer_type;
    typedef const CTrItemPacked<bitst,bitsc> & self_const_reference_type;
    
    typedef value_type * pointer_type;
    typedef value_type & reference_type;
    typedef const value_type * const_pointer_type;
    typedef const value_type & const_reference_type;
    
    typedef pointer_type iterator;
    typedef const_pointer_type const_iterator;

    enum { kSizeIncrement = 4 };

    template<class Item>
    class C_ReverseIterator
    {
    public:
        typedef Item value_type;
        typedef Item * pointer_type;
        typedef Item & reference_type;
        typedef C_ReverseIterator<Item> self_type;

        C_ReverseIterator( Item * i = 0 ) : m_item( i ) {}
        C_ReverseIterator( const self_type* i ) : m_item( i->m_item ) {}
        C_ReverseIterator( const self_type& i ) : m_item( i.m_item ) {}
        operator pointer_type () const { return m_item; }
        pointer_type operator -> () const { return m_item; }
        reference_type operator * () const { return *m_item; }
        self_type& operator ++ () { --m_item; return *this; }
        self_type& operator -- () { ++m_item; return *this; }
        self_type  operator ++ ( int ) { self_type ret( *this ); --m_item; return ret; }
        self_type  operator -- ( int ) { self_type ret( *this ); ++m_item; return ret; }
        self_type& operator += ( int i ) { m_item -= i; return *this; }
        self_type& operator -= ( int i ) { m_item += i; return *this; }
        bool operator == ( const self_type& other ) const { return m_item == other.m_item; }
        bool operator != ( const self_type& other ) const { return m_item != other.m_item; }
        bool operator <  ( const self_type& other ) const { return m_item >  other.m_item; }
        bool operator >  ( const self_type& other ) const { return m_item <  other.m_item; }
        bool operator <= ( const self_type& other ) const { return m_item >= other.m_item; }
        bool operator >= ( const self_type& other ) const { return m_item <= other.m_item; }
        friend       int operator - ( const self_type& a, const self_type& b ) { return b.m_item - a.m_item; }
        friend self_type operator - ( const self_type& a, int b ) { return self_type(a.m_item + b); }
        friend self_type operator + ( const self_type& a, int b ) { return self_type(a.m_item - b); }
        friend self_type operator + ( int b, const self_type& a ) { return self_type(a.m_item - b); }
    protected:
        Item * m_item;
    };

    typedef C_ReverseIterator<value_type> reverse_iterator;
    typedef C_ReverseIterator<const value_type> const_reverse_iterator;
    
    ~CTrSequence() { free( m_data ); }
    CTrSequence() : m_data(0) {}
    CTrSequence( const self_type& other ) : m_data(0) { Assign( other ); }
    CTrSequence( const char *  trans ) : m_data(0) { Assign( trans ); }
    CTrSequence( const string& trans ) : m_data(0) { Assign( trans ); }
    
    size_t size() const { return m_data ? m_data->GetCount() : 0; }
    size_t capacity() const { return size(); } //m_data ? m_data->GetCount() | (kSizeIncrement - 1) : 0; }
    
    reference_type front() { return m_data[1]; }
    reference_type back() { return m_data[size()]; } // since size() is always 1 less then allocated, and first element contains size
    const_reference_type front() const { return m_data[1]; }
    const_reference_type back() const { return m_data[size()]; }
    
    iterator begin() { return m_data ? m_data + 1 : 0; }
    iterator end() { return m_data ? m_data + size() + 1 : 0; }
    const_iterator begin() const { return m_data ? m_data + 1 : 0; }
    const_iterator end() const { return m_data ? m_data + size() + 1 : 0; }

    reverse_iterator rbegin() { return m_data ? m_data + size() : 0; }
    reverse_iterator rend() { return m_data ? m_data : 0; }
    const_reverse_iterator rbegin() const { return m_data ? m_data + size() : 0; }
    const_reverse_iterator rend() const { return m_data ? m_data : 0; }

    void clear() { free( m_data ); m_data = 0; } //delete[] m_data; m_data = 0; }
    void resize( unsigned long x ) { 
        ASSERT( x <= size() );
        ASSERT( x >= 0 );
        if( x == size() ) return;
        if( x == 0 ) clear();
        else m_data[0] = value_type( eEvent_NULL, x );
    }
    void reserve( unsigned long ) {}
    void erase( iterator from, iterator to ) {
        copy( to, end(), from );
        resize( size() - ( to - from ) );
    }

    bool operator != ( const self_type& other ) const { return !operator == ( other ); }
    bool operator == ( const self_type& other ) const { 
        const_iterator a = begin(), b = other.begin();
        const_iterator A = end(), B = other.end();
        while( a != A && b != B ) if( *a++ != *b++ ) return false;
        return( a == A && b == B );
    }

    self_type& operator = ( const self_type& other ) { return Assign( other ); }

    reference_type operator [] ( int i ) { return m_data[i + 1]; }
    const_reference_type operator [] ( int i ) const { return m_data[i + 1]; }
    
    self_type& AppendItem( const value_type& value ) {
        if( value.GetCount() == 0 ) return *this;
        if( value.GetEvent() == eEvent_NULL ) return *this;
        if( m_data == 0 ) {
            m_data = (pointer_type)malloc( sizeof( value_type ) * 2 ); // allocate two items at once - for metadata and for first element
            //m_data = new value_type[kSizeIncrement];
            m_data[0] = value_type( eEvent_NULL, 1 );
            m_data[1] = value;
        } else {
            ASSERT( size() > 0 );
            reference_type b = back();
            value_type v = b.Add( value );
            if( v.GetCount() ) {
                size_t s = size();
                m_data = (pointer_type)realloc( m_data, (s + 2)*sizeof( value_type ) );
                m_data[s + 1] = v;
                ASSERT( m_data[0].GetCount() == (int)s );
                m_data[0].IncrCount();
            }
        }
        return *this;
    }
    self_type& AppendItem( EEvent ev, unsigned cnt ) {
        if( ev == eEvent_NULL ) return *this;
        if( cnt <= 0 ) return *this;
        while( cnt > value_type::GetMaxCount() ) {
            AppendItem( value_type( ev, value_type::GetMaxCount() ) );
            cnt -= value_type::GetMaxCount();
        }
        return AppendItem( value_type( ev, cnt ) );
    }
    template<class Iter>
    self_type& Append( const Iter& begin, const Iter& end ) {
        for( Iter x = begin, X = end; x != X; ++x ) AppendItem( *x );
        return *this;
    }
    self_type& AppendFwd( const self_type& other ) {
        Append( other.begin(), other.end() );
        return *this;
    }
    self_type& AppendRev( const self_type& other ) {
        Append( other.rbegin(), other.rend() );
        return *this;
    }
    self_type& Assign( const string& other ) { return Assign( other.c_str() ); }
    self_type& Assign( const self_type& other ) {
        if( &other == this ) return *this;
        //delete[] m_data;
        if( m_data ) { free( m_data ); m_data = 0; }
        if( int sz = other.size() ) {
            m_data = (pointer_type)malloc( (1 + sz) * sizeof( value_type ) );
            // m_data = new value_type[(sz | (kSizeIncrement - 1)) + 1];
            copy( other.m_data, other.m_data + sz + 1, m_data );
        }
        else m_data = 0;
        return *this;
    }
    self_type& Assign( const CTrVector<value_type>& other ) {
        if( m_data ) { free( m_data ); m_data = 0; }
        for( typename CTrVector<value_type>::const_iterator o = other.begin(); o != other.end(); ++o ) AppendItem( *o );
        return *this;
    }
    self_type& Assign( const char * other ) {
        //delete[] m_data;
        if( m_data ) { free( m_data ); m_data = 0; }
        if( !other ) return *this;
        for( const char * x = other; *x; ++x ) {
            unsigned cnt = isdigit( *x ) ? strtol( x, const_cast<char**>( &x ), 10 ) : 1;
            EEvent e = Char2Event( *x );
            if( e == eEvent_NULL ) THROW( runtime_error, "Undefined CIGAR char " << (isprint( *x ) ? *x : ' ') << " (ascii" << int(*x) << ") in [" << other << "]" );
            while( cnt > value_type::GetMaxCount() ) AppendItem( value_type( e, value_type::GetMaxCount() ) );
            if( cnt > 0 ) AppendItem( value_type( e, cnt ) );
        }
        return *this;
    }
    self_type& Reverse() {
        if( m_data ) reverse( m_data + 1, m_data + 1 + m_data->GetCount() );
        return *this;
    }
    template<class Iter>
    static ostream& Print( ostream& o, Iter begin, Iter end ) {
        for( Iter i = begin; i != end; ++i ) {
            if( i->GetCount() ) o << i->GetCount() << i->GetCigar();
        }
        return o;
    }
    unsigned ComputeSubjectLength() const {
        int len = 0;
        for( typename self_type::const_iterator iter = self_type::begin(); iter != self_type::end(); ++iter ) {
            switch( iter->GetEvent() ) {
            case eEvent_NULL:
            case eEvent_Padding:
            case eEvent_SoftMask:
            case eEvent_HardMask:
            case eEvent_Insertion: break;
            case eEvent_Overlap: len -= iter->GetCount(); break;
            case eEvent_Splice:
            case eEvent_Deletion:
            case eEvent_Match:
            case eEvent_Changed:
            case eEvent_Replaced: len += iter->GetCount(); break;
            }
        }
        ASSERT( len >= 0 );
        return len;
    }
    pair<int,int> ComputeLengths() const {
        int qlen = 0, slen = 0;
        for( typename self_type::const_iterator iter = self_type::begin(); iter != self_type::end(); ++iter ) {
            switch( iter->GetEvent() ) {
            case eEvent_NULL:
            case eEvent_Padding:
            case eEvent_SoftMask:
            case eEvent_HardMask: break;
            case eEvent_Insertion: qlen += iter->GetCount(); break;
            case eEvent_Overlap: slen -= iter->GetCount(); break;
            case eEvent_Splice: 
            case eEvent_Deletion: slen += iter->GetCount(); break;
            case eEvent_Match:
            case eEvent_Changed:
            case eEvent_Replaced: qlen += iter->GetCount(); slen += iter->GetCount(); break;
            }
        }
        return make_pair( qlen, slen );
    }
    ostream& PrintFwd( ostream& o ) const { return Print( o, self_type::begin(), self_type::end() ); }
    ostream& PrintRev( ostream& o ) const { return Print( o, self_type::rbegin(), self_type::rend() ); }
    
    string ToString() const { ostringstream o; PrintFwd( o ); return o.str(); }
protected:
    // data structure: first item should be NULL with count equal to number of items in list
    pointer_type m_data;
};

template<unsigned bbits, unsigned cbits>
inline std::ostream& operator << ( std::ostream& out, const CTrSequence<bbits,cbits>& tr ) 
{
    return tr.PrintFwd( out );
}

template<class Iter>
int ComputeDistance( Iter begin, Iter end )
{
    int ret = 0;
    for( ; begin != end; ++begin ) 
        switch( begin->GetEvent() ) {
            case CTrBase::eEvent_Insertion: 
            case CTrBase::eEvent_Deletion: 
            case CTrBase::eEvent_Replaced: ret += begin-> GetCount();
            default: ;
        }
    return ret;
}

template<class Container>
int ComputeDistance( const Container& cont ) 
{
    return ComputeDistance( cont.begin(), cont.end() );
}

typedef CTrItemPacked<4,12> TTrItem;
typedef CTrVector<TTrItem> TTrVector;
//typedef CTrVector<TTrItem> TTrSequence;
typedef CTrSequence<4,12> TTrSequence;

END_OLIGOFAR_SCOPES

#endif
