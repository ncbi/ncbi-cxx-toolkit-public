/*
Copyright(c) 2002-2005 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)


Permission is hereby granted, free of charge, to any person 
obtaining a copy of this software and associated documentation 
files (the "Software"), to deal in the Software without restriction, 
including without limitation the rights to use, copy, modify, merge, 
publish, distribute, sublicense, and/or sell copies of the Software, 
and to permit persons to whom the Software is furnished to do so, 
subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
OTHER DEALINGS IN THE SOFTWARE.


For more information please visit:   http://bmagic.sourceforge.net

*/

#ifndef ENCODING_H__INCLUDED__
#define ENCODING_H__INCLUDED__

#include <memory.h>

namespace bm
{

// ----------------------------------------------------------------
/*!
   \brief Memory encoding.
   
   Class for encoding data into memory. 
   Properly handles aligment issues with integer data types.
*/
class encoder
{
public:

    encoder(unsigned char* buf, unsigned size);
    void put_8(unsigned char c);
    void put_16(bm::short_t  s);
    void put_16(const bm::short_t* s, unsigned count);
    void put_32(bm::word_t  w);
    void put_32(const bm::word_t* w, unsigned count);
    unsigned size() const;

private:

    unsigned char*  buf_;
    unsigned char*  start_;
    unsigned int    size_;
};

// ----------------------------------------------------------------
/**
   Class for decoding data from memory buffer.
   Properly handles aligment issues with integer data types.
*/
class decoder
{
public:
    decoder(const unsigned char* buf);
    unsigned char get_8();
    bm::short_t get_16();
    bm::word_t get_32();
    void get_32(bm::word_t* w, unsigned count);
    void get_16(bm::short_t* s, unsigned count);
    unsigned size() const;
private:
   const unsigned char*   buf_;
   const unsigned char*   start_;
};



// ----------------------------------------------------------------
// Implementation details. 
// ----------------------------------------------------------------

/*! 
    \fn encoder::encoder(unsigned char* buf, unsigned size) 
    \brief Construction.
    \param buf - memory buffer pointer.
    \param size - size of the buffer
*/
inline encoder::encoder(unsigned char* buf, unsigned size)
: buf_(buf), start_(buf), size_(size)
{
}

/*!
   \fn void encoder::put_8(unsigned char c) 
   \brief Puts one character into the encoding buffer.
   \param c - character to encode
*/
inline void encoder::put_8(unsigned char c)
{
    *buf_++ = c;
}

/*!
   \fn encoder::put_16(bm::short_t s)
   \brief Puts short word (16 bits) into the encoding buffer.
   \param s - short word to encode
*/
inline void encoder::put_16(bm::short_t s)
{
    *buf_++ = (unsigned char) s;
    s >>= 8;
    *buf_++ = (unsigned char) s;
}

/*!
   \brief Method puts array of short words (16 bits) into the encoding buffer.
*/
inline void encoder::put_16(const bm::short_t* s, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) 
    {
        put_16(s[i]);
    }
}


/*!
   \fn unsigned encoder::size() const
   \brief Returns size of the current encoding stream.
*/
inline unsigned encoder::size() const
{
    return (unsigned)(buf_ - start_);
}

/*!
   \fn void encoder::put_32(bm::word_t w)
   \brief Puts 32 bits word into encoding buffer.
   \param w - word to encode.
*/
inline void encoder::put_32(bm::word_t w)
{
    *buf_++ = (unsigned char) w;
    *buf_++ = (unsigned char) (w >> 8);
    *buf_++ = (unsigned char) (w >> 16);
    *buf_++ = (unsigned char) (w >> 24);
}

/*!
    \brief Encodes array of 32-bit words
*/
inline void encoder::put_32(const bm::word_t* w, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) 
    {
        put_32(w[i]);
    }
}


// ---------------------------------------------------------------------

/*!
   \fn decoder::decoder(const unsigned char* buf) 
   \brief Construction
   \param buf - pointer to the decoding memory. 
*/
inline decoder::decoder(const unsigned char* buf) 
: buf_(buf), start_(buf)
{
}

/*!
   \fn unsigned char decoder::get_8()  
   \brief Reads character from the decoding buffer.
*/
inline unsigned char decoder::get_8() 
{
    return *buf_++;
}

/*!
   \fn bm::short_t decoder::get_16()
   \brief Reads 16bit word from the decoding buffer.
*/
inline bm::short_t decoder::get_16() 
{
    bm::short_t a = (bm::short_t)(buf_[0] + ((bm::short_t)buf_[1] << 8));
    buf_ += sizeof(a);
    return a;
}

/*!
   \fn bm::word_t decoder::get_32()
   \brief Reads 32 bit word from the decoding buffer.
*/
inline bm::word_t decoder::get_32() 
{
    bm::word_t a = buf_[0]+ ((unsigned)buf_[1] << 8) +
                   ((unsigned)buf_[2] << 16) + ((unsigned)buf_[3] << 24);
    buf_+=sizeof(a);
    return a;
}


/*!
   \fn void decoder::get_32(bm::word_t* w, unsigned count)
   \brief Reads block of 32-bit words from the decoding buffer.
   \param w - pointer on memory block to read into.
   \param count - size of memory block in words.
*/
inline void decoder::get_32(bm::word_t* w, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) 
    {
        w[i] = get_32();
    }
}

/*!
   \fn void decoder::get_16(bm::short_t* s, unsigned count)
   \brief Reads block of 32-bit words from the decoding buffer.
   \param s - pointer on memory block to read into.
   \param count - size of memory block in words.
*/
inline void decoder::get_16(bm::short_t* s, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) 
    {
        s[i] = get_16();
    }
}

/*!
   \fn unsigned decoder::size() const
   \brief Returns size of the current decoding stream.
*/
inline unsigned decoder::size() const
{
    return (unsigned)(buf_ - start_);
}


} // namespace bm

#endif


