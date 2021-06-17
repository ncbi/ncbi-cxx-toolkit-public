/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *            National Center for Biotechnology Information (NCBI)
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government do not place any restriction on its use or reproduction.
 *  We would, however, appreciate having the NCBI and the author cited in
 *  any work or product based on this material
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 * ===========================================================================
 *
 * Author:  Michael Kimelman
 *
 * (Asn) Stream processing utilities - compressor/cacher and AsnIoPtr to fci merger.
 *                   
 * Modifications:  
 * --------------------------------------------------------------------------
 * $Log: streamprocs.c,v $
 * Revision 1.8  2002/08/14 15:51:52  kimelman
 * asserts added
 *
 * Revision 1.7  2001/05/09 23:40:26  kimelman
 * reader effeciency improved
 *
 * Revision 1.6  2001/05/09 21:32:45  kimelman
 * bugfix: check for null 'close' method before running
 *
 * Revision 1.5  2001/05/09 00:57:42  kimelman
 * cosmetics
 *
 * Revision 1.4  2001/03/01 21:20:51  kimelman
 * make it less noisy
 *
 * Revision 1.3  1998/06/25 19:24:29  kimelman
 * changed coef. of cache grow
 *
 * Revision 1.2  1998/05/15 19:05:19  kimelman
 * all old 'gzip' names changed to be suitable for library names.
 * prefix Nlmzip_ is now used for all of this local stuff.
 * interface headers changed their names, moving from artdb/ur to nlmzip
 *
 * Revision 1.1  1998/05/14 20:21:17  kimelman
 * ct_init --> Nlmzip_ct_init
 * added stream& AsnIo processing functions
 * makefile changed
 *
 *
 *
 * ==========================================================================
 */


#include <ncbi_pch.hpp>
#include <ctools/ctransition/ncbimem.hpp>
#include <ctools/ctransition/ncbierr.hpp>
#include <assert.h>
#include "ct_nlmzip_i.h"

// Ignore warnings for ncbi included code
#ifdef __GNUC__ // if gcc or g++
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif //__GNUC__


BEGIN_CTRANSITION_SCOPE



/*
 * File common interface
 */

fci_t LIBCALL
fci_open(  Pointer data,
           Int4 (*proc_buf)(Pointer ptr, CharPtr buf, Int4 count),
           Int4 (*pclose)(Pointer ptr, int commit)
           )
{
  fci_t obj = (fci_t)Nlm_MemNew(sizeof(*obj));

  obj->data      = data;
  obj->proc_buf  = proc_buf;
  obj->close     = pclose;
  return obj;
}


/*
 *  AsnIoPtr  asnio2fci.open(compressor.open(cacher.open(100,dbio.open(db))))

 * fci_t  asnio2fci ;
 * fci_t  compressor;
 * fci_t  cacher;
 * fci_t  dbio;

 */

/*
 *   CACHER
 */

typedef struct {
  fci_t src;
  int   read;
  char *buf;
  int   len;
  int   start;
  int   size;
  int   cache_size;
  int   eos;
} cacher_t;
  
static Int4 LIBCALLBACK
cacher_read(Pointer ptr, CharPtr buf, Int4 count)
{
  cacher_t *db = (cacher_t*)ptr;
  Int4      bytes = 0;
  
  while (count > bytes)
    {
      assert(bytes>=0);
      assert(bytes<=count);
      if ( db->len == db->start ) /* if cache is empty */
        {
          Int4 len = 0;
//          int direct_read = 0;
          if (db->eos)  
            break;      /* end of stream EXIT */
          if ( count-bytes > db->cache_size / 2)
            { /* read directly to caller's buffer */
              len = db->src->proc_buf(db->src->data, buf+bytes, count-bytes);
              /* negative 'len<0' answer means request for larger buffer size */
              if ( len == 0 )
                db->eos = 1;
              if ( len > 0 )
                bytes += len ;
            }
          if (count>bytes)
            { /* cache input stream */
              if ( db->cache_size < count )
                db->cache_size = count;
              if ( db->cache_size < - len )
                db->cache_size = - len;
              if ( db->cache_size > db->size )
                db->cache_size = db->size;
              db->start = 0;
              len = db->src->proc_buf(db->src->data, db->buf,db->cache_size);
              if (len < 0 && db->cache_size < - len )
                {
                  /* negative 'len<0' answer means request for larger buffer size */
                  db->cache_size = - len;
                  if ( db->cache_size >  db->size )
                    { /* try to adjust cache size - that case seems to be request
                         for larger buffer from underlying decompressor */
                      CharPtr newb = (CharPtr)Nlm_MemNew( - len );
                      if ( ! newb )
                        {
                          ErrPostEx(SEV_ERROR,0,0,"memory is exhausted - can't allocate %d bytes",-len);
                          return len;
                        }
                      Nlm_MemFree(db->buf);
                      db->buf = newb;
                      db->size = - len;
                    }
                  len = db->src->proc_buf(db->src->data, db->buf,db->cache_size);
                }
              db->cache_size *= 2;
              if (len < 0 )
                return len;
              if (len == 0 )
                db->eos = 1 ;
              db->len = len;
            }
        }
      if ( db->len - db->start > 0 )
        {
          int sz = db->len - db->start;
          if ( bytes + sz  > count )
            sz = count - bytes ;
          memcpy(buf+bytes, db->buf + db->start, sz );
          db->start +=sz;
          bytes += sz;
        }
    }
  assert(bytes>=0);
  assert(bytes<=count);
  return bytes;
}

static Int4 LIBCALLBACK
cacher_write(Pointer ptr, CharPtr buf, Int4 count)
{
  cacher_t *db = (cacher_t*)ptr;
  Int4      bytes = 0;
//  int       flush_it = 0;

  if(count<=0)
    return 0;
  /* cache size adjustments */
  if ( db->cache_size < count )
    {   
      db->cache_size = count;
      if ( db->cache_size > db->size )
        db->cache_size = db->size;
    }
  
  while (count > bytes)
    {
      int len = 0;
      if ( db->len == db->cache_size ||  /* if cache is full */
           count-bytes > db->size / 2 )  /* or new data is too large for this cache */
        { /* flush cache */
          if (db->len > 0)
            len = db->src->proc_buf(db->src->data, db->buf,db->len);
          if (len != db->len)
            {
              ErrPostEx(SEV_ERROR,0,0,"Failure to write data from cache (%d of %d written)",len,db->len);
              return -1;
            }
          db->cache_size *=2;
          if ( db->cache_size < 2 * count )
            db->cache_size = 2 * count;
          if ( db->cache_size > db->size )
            db->cache_size = db->size;
          db->start = db->len = 0;
        }
      if ( count - bytes > db->cache_size && db->len) /* if there are a lot of data */
        { /*remains and cache is empty  -- do uncached write                        */
          len = db->src->proc_buf(db->src->data, buf+bytes,count-bytes);
          if (len != count-bytes)
            return -1;
          bytes += len;
          assert (bytes == count);
        }
      else
        { /* cached write */
          len = db->cache_size - db->len;
          assert( len > 0 );
          if ( count-bytes < len)
            len = count - bytes;
          memcpy(db->buf + db->len, buf+bytes, len);
          db->len += len ;
          bytes += len;
        }
    }
  return bytes;
}

static Int4 LIBCALLBACK
cacher_close(Pointer ptr, int commit)
{
  cacher_t *db = (cacher_t*)ptr;
  Int4 rc;

  if (!db->read)
    {
      Int4 len, len1 = db->len-db->start;
      if (commit>0)
        {
          len = db->src->proc_buf(db->src->data, db->buf+db->start,len1);
          commit = (len == len1) ;
        }
    }
  rc=commit;
  if(db->src->close)
    rc = db->src->close(db->src->data,commit);
  if (commit>=0)
    {
      Nlm_MemFree(db->src);
      Nlm_MemFree(db->buf);
      Nlm_MemFree(db);
    }
  else
    {
      db->len = db->start = db->cache_size = db->eos = 0 ;
    }
  return rc ;
}

fci_t LIBCALL
cacher_open(fci_t stream, int max_cache_size,int read)
{
  cacher_t *data = (cacher_t*)Nlm_MemNew(sizeof(*data));

  data->read = read ;
  data->src  = stream ;
  data->size = max_cache_size;
  data->cache_size = max_cache_size/10;
  if (data->cache_size < 2048 && max_cache_size > 2048)
    data->cache_size=2048;
  
  while ((data->buf = (char*)Nlm_MemNew(data->size)) == NULL)
    {
      data->size /= 2;
      if (data->size <= 1024)
        {
          Nlm_MemFree(data);
          ErrPostEx(SEV_ERROR, 0,0,"\n%s:%d: memory exhausted '%d' ",
                    __FILE__,__LINE__,data->size*2);
          return NULL;
        }
    }
  return fci_open(data,(read?cacher_read:cacher_write),cacher_close);
}

/*
 *   COMPRESSOR
 */

typedef struct {
  fci_t          src;
  int            mode; /* 0 - uninitialized. 1 - compressed ; -1 - uncompressed */
  unsigned char *dbuf;
  Int4           bsize;
  Int4           compr_size;
  Int4           decomp_size;
} compressor_t;

static void
compressor_header(compressor_t  *db,UcharPtr header,int read)
{
  UcharPtr dbuf;
  Uint4 val;
  int bytes;

  dbuf = (UcharPtr) header;
  if(read)
    { /* header --> db */
#if 0
      fprintf(stderr,"scanned buffer");
      for(bytes=0; bytes<8; bytes++)
        fprintf(stderr,"'%x',",header[bytes]);
      fprintf(stderr,"\n");
#endif 
      for(val=0, bytes=0; bytes<4; bytes++,dbuf++)
        val = (val<<8) + *dbuf ;
      db->compr_size = val;
      for(val=0         ; bytes<8; bytes++,dbuf++)
        val = (val<<8) + *dbuf ;
      db->decomp_size = val;
#if 0
      fprintf (stderr,"decompr(%x-%d)-->%x-%d\n",db->compr_size,db->compr_size,db->decomp_size,db->decomp_size);
      if (read == 1)
        {/* QA */
          Uchar buf[8];
          compressor_header(db,buf,0);
          assert(memcmp(buf,header,8)==0);
        }
#endif
    }
  else
    { /* write compressed block header */
      /* db --> header */
      val = db->compr_size;
      for(bytes=0; bytes<4; bytes++,dbuf++)
        *dbuf = (val >> (3-bytes)*8) & 0xff ;
      val = db->decomp_size;
      for(      ; bytes<8; bytes++,dbuf++)
        *dbuf = (val >> (7-bytes)*8) & 0xff ;
#if 0
      fprintf (stderr,"compr(%x)-->%x ",db->decomp_size,db->compr_size);
      fprintf(stderr,"written buffer");
      for(bytes=0; bytes<8; bytes++)
        fprintf(stderr,"'%x',",header[bytes]);
      fprintf(stderr,"\n");
      
      {/* QA */
        Int4 dc = db->decomp_size, cm = db->compr_size;
        compressor_header(db,header,2);
        assert(cm == db->compr_size);
        assert(dc == db->decomp_size);
      }
#endif
    }
}

static Int4 LIBCALLBACK
compressor_read(Pointer ptr, CharPtr obuf, Int4 count)
{
  compressor_t  *db          = (compressor_t*)ptr   ;
  unsigned char  lens[8];
  Int4           bytes       = 0 ;

  switch(db->mode)
    {
    case 0:
      assert(count>=4);
      bytes = db->src->proc_buf(db->src->data, (CharPtr)obuf,4);
      if (bytes!=4)
        return -1;
      if (strcmp(obuf,"ZIP")==0)
        {
          db->mode=1; /* compresseed mode */
          break;
        }
      db->mode=-1; /*uncompresseed mode */
      obuf+=4;
      count -=4;
    case -1:
      {
        int rc;
        rc = db->src->proc_buf(db->src->data, (CharPtr)obuf,count);
        if (rc < 0)
          return rc;
        return bytes+ rc;
      }
    case 1:
    default:
      break;
    }
  assert(db->mode == 1);
  if ( db->compr_size == 0 )
    {
      bytes = db->src->proc_buf(db->src->data, (CharPtr)lens,8);
      if (bytes<=0)
        return bytes;
      assert ( bytes == 8 );
      compressor_header(db,lens,1);
    }
  if ( db->decomp_size > count )
    {
#if 0 
      ErrPostEx(SEV_INFO, 0,0,"\n%s:%d: small compressor output buffer('%d' - required %d) ",
                __FILE__,__LINE__,count,db->decomp_size);
#endif
      return - db->decomp_size ;  /* unsufficient space problem */
    }
  if ( db->compr_size > db->bsize)
    {
      unsigned char *nb = (unsigned char*)Nlm_MemNew(db->compr_size);
      if (!nb)
        {
          ErrPostEx(SEV_ERROR, 0,0,"\n%s:%d: memory exhausted (required %d) ",
                    __FILE__,__LINE__,db->compr_size);
          return -db->compr_size;
        }
      Nlm_MemFree(db->dbuf);
      db->dbuf = nb;
      db->bsize = db->compr_size;
    }
  bytes = db->src->proc_buf(db->src->data, (CharPtr)db->dbuf,db->compr_size);
  if ( bytes < db->compr_size )
    {
      ErrPostEx(SEV_ERROR, 0,0,"\n%s:%d: broken data in input stream compressed(%d) != returned(%d)",
                __FILE__,__LINE__,db->compr_size,bytes);
      return -1;
    }
  assert (bytes == db->compr_size);
  if (Nlmzip_Uncompress (db->dbuf, db->compr_size,obuf,count,&bytes) != NLMZIP_OKAY )
    {
      ErrPostEx(SEV_ERROR, 0,0,"can't uncompress data");
      return -1;
    }
  assert(bytes==db->decomp_size);
  db->decomp_size=db->compr_size=0; /* clean buffer reading lock */
  return bytes;
}

static Int4 LIBCALLBACK
compressor_write(Pointer ptr, CharPtr buf, Int4 count)
{
  compressor_t  *db          = (compressor_t*)ptr   ;
  Int4           bytes       = 0 ;

  if (count<=0)
    return 0;
  
  switch (db->mode)
    {
    case 0  :
      bytes = db->src->proc_buf(db->src->data,(char*)"ZIP",4);
      if (bytes!=4)
        return -1;
      db->mode=1; /* compresseed mode */
      break ;
    case -1 :      /* uncompresseed mode */
      return db->src->proc_buf(db->src->data, (CharPtr)buf,count);
    case 1  :
    default :
      break;
    }
  
  while (Nlmzip_Compress (buf, count,db->dbuf+8,db->bsize-8,&bytes) !=NLMZIP_OKAY)
    {
      unsigned char *nb = (unsigned char*)Nlm_MemNew(2*db->bsize);
      if (!nb)
        {
          ErrPostEx(SEV_ERROR, 0,0,"\n%s:%d: memory exhausted (required %d) ",
                    __FILE__,__LINE__,db->compr_size);
          return -db->compr_size;
        }
      Nlm_MemFree(db->dbuf);
      db->dbuf = nb;
      db->bsize *=2;
    }
  
  db->decomp_size = count;
  db->compr_size  = bytes;

  compressor_header(db,db->dbuf,0);
  bytes = db->src->proc_buf(db->src->data, (CharPtr)db->dbuf,db->compr_size+8);
  if ( bytes != db->compr_size+8)
    {
      ErrPostEx(SEV_ERROR, 0,0,"\n%s:%d: broken data in output stream",
                __FILE__,__LINE__);
      return -1;
    }
  return count;
}

static Int4 LIBCALLBACK
compressor_close(Pointer ptr, int commit)
{
  compressor_t *db = (compressor_t*)ptr;
  Int4 rc = commit;

  if(db->src->close)
    rc = db->src->close(db->src->data,commit);
  if (commit>=0)
    {
      if (db->src)
          Nlm_MemFree(db->src);
      if (db->dbuf)
          Nlm_MemFree(db->dbuf);
      Nlm_MemFree(db);
    }
  else
    {
      db->mode = 0;
      db->decomp_size=db->compr_size=0; /* clean buffer reading lock */
    }
  return rc ;
}

fci_t LIBCALL
compressor_open(fci_t stream, int max_buffer_size, int read)
{
  compressor_t *data = (compressor_t*)Nlm_MemNew(sizeof(*data));

  if (max_buffer_size<1024)
    max_buffer_size = 1024;
  data->src  = stream ;
  data->mode = 0;
  data->dbuf = (unsigned char*)Nlm_MemNew(max_buffer_size);
  if(data->dbuf)
    data->bsize = max_buffer_size;
  return cacher_open( /* add one more cache which will read data */
                     fci_open(data,(read?compressor_read:compressor_write),compressor_close),
                     max_buffer_size,read);
}


#if 0
/*
 *   ASNIO2FCI
 */

static Int2 LIBCALLBACK
asnio2fci_proc(Pointer ptr, CharPtr buf, Uint2 count)
{
  fci_t f = (fci_t)ptr;
  
  assert(count <= 0x7fff );
  return f->proc_buf(f->data, buf,count);
}

Int4 LIBCALL
asnio2fci_close(AsnIoPtr aip,Int4 commit)
{
  fci_t stream = aip ->iostruct;
  Int4  rc = commit;

  if(commit>=0)
    AsnIoClose (aip);
  else
    AsnIoReset (aip);
  if(stream->close)
    rc = stream->close(stream->data,commit);
  if (commit>=0)
    MemFree (stream);
  return rc;
}

AsnIoPtr LIBCALL
asnio2fci_open(int read, fci_t stream)
{
  if (read)
    return AsnIoNew(ASNIO_BIN_IN, NULL, stream, asnio2fci_proc, NULL);
  else
    return AsnIoNew(ASNIO_BIN_OUT, NULL, stream, NULL, asnio2fci_proc);
}
#endif


END_CTRANSITION_SCOPE


// Re-enable warnings
#ifdef __GNUC__ // if gcc or g++
#  pragma GCC diagnostic pop
#endif //__GNUC__

