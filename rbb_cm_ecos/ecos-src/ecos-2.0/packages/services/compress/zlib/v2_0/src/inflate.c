/* inflate.c -- zlib interface to inflate modules
 * Copyright (C) 1995-1998 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

#include "zutil.h"
#include "infblock.h"

struct inflate_blocks_state {int dummy;}; /* for buggy compilers */

typedef enum {
      METHOD,   /* waiting for method byte */
#ifdef __ECOS__
      GZ_HDR2,
      GZ_METHOD,
      GZ_FLAG,
      GZ_TIME,
      GZ_EXTRA,
      GZ_EXTRA_HI,
      GZ_EXTRA_ZAP,
      GZ_NAME,
      GZ_COMMENT,
      GZ_HEAD_CRC_LO,
      GZ_HEAD_CRC_HI,
      GZ_DONE,
#endif // __ECOS__
      FLAG,     /* waiting for flag byte */
      DICT4,    /* four dictionary check bytes to go */
      DICT3,    /* three dictionary check bytes to go */
      DICT2,    /* two dictionary check bytes to go */
      DICT1,    /* one dictionary check byte to go */
      DICT0,    /* waiting for inflateSetDictionary */
      BLOCKS,   /* decompressing blocks */
      CHECK4,   /* four check bytes to go */
      CHECK3,   /* three check bytes to go */
      CHECK2,   /* two check bytes to go */
      CHECK1,   /* one check byte to go */
      DONE,     /* finished check, done */
      BAD}      /* got an error--stay here */
inflate_mode;

/* inflate private state */
struct internal_state {

  /* mode */
  inflate_mode  mode;   /* current inflate mode */
#ifdef __ECOS__
    inflate_mode gz_mode;
    uInt gz_flag;
    int gz_cnt;
    unsigned char* gz_start;
    uLong gz_sum;
#endif

  /* mode dependent information */
  union {
    uInt method;        /* if FLAGS, method byte */
    struct {
      uLong was;                /* computed check value */
      uLong need;               /* stream check value */
    } check;            /* if CHECK, check values to compare */
    uInt marker;        /* if BAD, inflateSync's marker bytes count */
  } sub;        /* submode */

  /* mode independent information */
  int  nowrap;          /* flag for no wrapper */
  uInt wbits;           /* log2(window size)  (8..15, defaults to 15) */
  inflate_blocks_statef 
    *blocks;            /* current inflate_blocks state */

};

#ifdef __ECOS__
/* gzip flag byte */
#define _GZ_ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define _GZ_HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define _GZ_EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define _GZ_ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define _GZ_COMMENT      0x10 /* bit 4 set: file comment present */
#define _GZ_RESERVED     0xE0 /* bits 5..7: reserved */
#endif // __ECOS__


int ZEXPORT inflateReset(z)
z_streamp z;
{
  if (z == Z_NULL || z->state == Z_NULL)
    return Z_STREAM_ERROR;
  z->total_in = z->total_out = 0;
  z->msg = Z_NULL;
  z->state->mode = z->state->nowrap ? BLOCKS : METHOD;
  inflate_blocks_reset(z->state->blocks, z, Z_NULL);
  Tracev((stderr, "inflate: reset\n"));
  return Z_OK;
}


int ZEXPORT inflateEnd(z)
z_streamp z;
{
  if (z == Z_NULL || z->state == Z_NULL || z->zfree == Z_NULL)
    return Z_STREAM_ERROR;
  if (z->state->blocks != Z_NULL)
    inflate_blocks_free(z->state->blocks, z);
  ZFREE(z, z->state);
  z->state = Z_NULL;
  Tracev((stderr, "inflate: end\n"));
  return Z_OK;
}


int ZEXPORT inflateInit2_(z, w, version, stream_size)
z_streamp z;
int w;
const char *version;
int stream_size;
{
  if (version == Z_NULL || version[0] != ZLIB_VERSION[0] ||
      stream_size != sizeof(z_stream))
      return Z_VERSION_ERROR;

  /* initialize state */
  if (z == Z_NULL)
    return Z_STREAM_ERROR;
  z->msg = Z_NULL;
  if (z->zalloc == Z_NULL)
  {
    z->zalloc = zcalloc;
    z->opaque = (voidpf)0;
  }
  if (z->zfree == Z_NULL) z->zfree = zcfree;
  if ((z->state = (struct internal_state FAR *)
       ZALLOC(z,1,sizeof(struct internal_state))) == Z_NULL)
    return Z_MEM_ERROR;
  z->state->blocks = Z_NULL;

  /* handle undocumented nowrap option (no zlib header or check) */
  z->state->nowrap = 0;
  if (w < 0)
  {
    w = - w;
    z->state->nowrap = 1;
  }

  /* set window size */
  if (w < 8 || w > 15)
  {
    inflateEnd(z);
    return Z_STREAM_ERROR;
  }
  z->state->wbits = (uInt)w;

  /* create inflate_blocks state */
  if ((z->state->blocks =
      inflate_blocks_new(z, z->state->nowrap ? Z_NULL : adler32, (uInt)1 << w))
      == Z_NULL)
  {
    inflateEnd(z);
    return Z_MEM_ERROR;
  }
  Tracev((stderr, "inflate: allocated\n"));

  /* reset state */
  inflateReset(z);
  return Z_OK;
}


int ZEXPORT inflateInit_(z, version, stream_size)
z_streamp z;
const char *version;
int stream_size;
{
  return inflateInit2_(z, DEF_WBITS, version, stream_size);
}


#define NEEDBYTE {if(z->avail_in==0)return r;r=f;}
#define NEXTBYTE (z->avail_in--,z->total_in++,*z->next_in++)

int ZEXPORT inflate(z, f)
z_streamp z;
int f;
{
  int r;
  uInt b;

  if (z == Z_NULL || z->state == Z_NULL || z->next_in == Z_NULL)
    return Z_STREAM_ERROR;
  f = f == Z_FINISH ? Z_BUF_ERROR : Z_OK;
  r = Z_BUF_ERROR;
  while (1) switch (z->state->mode)
  {
    case METHOD:
#ifdef __ECOS__
        // "Clear" gz_mode - if DONE at exit, this was a zlib stream.
        z->state->gz_mode = DONE;
#endif
      NEEDBYTE
      if (((z->state->sub.method = NEXTBYTE) & 0xf) != Z_DEFLATED)
      {
#ifdef __ECOS__
          if (0x1f == z->state->sub.method) {
              z->state->mode = GZ_HDR2;
              break;
          }
          // This is a hack to get a reasonable error message in
          // RedBoot if the user tries to decompress raw data.
          z->state->mode = BAD;
          z->msg = (char*)"incorrect gzip header";
          z->state->sub.marker = 5;       /* can't try inflateSync */
#else
        z->state->mode = BAD;
        z->msg = (char*)"unknown compression method";
        z->state->sub.marker = 5;       /* can't try inflateSync */
#endif // __ECOS__
        break;
      }
      if ((z->state->sub.method >> 4) + 8 > z->state->wbits)
      {
        z->state->mode = BAD;
        z->msg = (char*)"invalid window size";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      z->state->mode = FLAG;
#ifdef __ECOS__
      break;
    case GZ_HDR2:
        NEEDBYTE;
        b = NEXTBYTE;
        if (0x8b != b) {
            z->state->mode = BAD;
            z->msg = (char*)"incorrect gzip header";
            z->state->sub.marker = 5;       /* can't try inflateSync */
            break;
        }
        z->state->mode = GZ_METHOD;
  case GZ_METHOD:
      NEEDBYTE
      if ((z->state->sub.method = NEXTBYTE) != Z_DEFLATED)
      {
        z->state->mode = BAD;
        z->msg = (char*)"unknown compression method";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      if ((z->state->sub.method >> 4) + 8 > z->state->wbits)
      {
        z->state->mode = BAD;
        z->msg = (char*)"invalid window size";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      z->state->mode = GZ_FLAG;
  case GZ_FLAG:
      NEEDBYTE
      z->state->gz_flag = NEXTBYTE;
      z->state->mode = GZ_TIME;
      z->state->gz_cnt = 6;             // for GZ_TIME
  case GZ_TIME:
      // Discard time, xflags and OS code
      while (z->state->gz_cnt-- > 0) {
          NEEDBYTE;
          b = NEXTBYTE;
      }
      z->state->mode = GZ_EXTRA;
  case GZ_EXTRA:
      if (!(z->state->gz_flag & _GZ_EXTRA_FIELD)) {
          z->state->mode = GZ_NAME;
          break;
      }

      NEEDBYTE;
      z->state->gz_cnt = NEXTBYTE;
      z->state->mode = GZ_EXTRA_HI;
  case GZ_EXTRA_HI:
      NEEDBYTE;
      z->state->gz_cnt += (((uInt)NEXTBYTE)<<8);
      z->state->mode = GZ_EXTRA_ZAP;
  case GZ_EXTRA_ZAP:
      // Discard extra field
      while (z->state->gz_cnt-- > 0) {
          NEEDBYTE;
          b = NEXTBYTE;
      }
      z->state->mode = GZ_NAME;
  case GZ_NAME:
      if (!(z->state->gz_flag & _GZ_ORIG_NAME)) {
          z->state->mode = GZ_COMMENT;
          break;
      }
      // Skip the name
      do {
          NEEDBYTE;
          b = NEXTBYTE;
      } while (0 != b);
      z->state->mode = GZ_COMMENT;
  case GZ_COMMENT:
      if (!(z->state->gz_flag & _GZ_COMMENT)) {
          z->state->mode = GZ_HEAD_CRC_LO;
          break;
      }
      // Skip the comment
      do {
          NEEDBYTE;
          b = NEXTBYTE;
      } while (0 != b);
      z->state->mode = GZ_HEAD_CRC_LO;
  case GZ_HEAD_CRC_LO:
      if (!(z->state->gz_flag & _GZ_HEAD_CRC)) {
          z->state->mode = GZ_DONE;
          break;
      }
      // skip lo byte
      NEEDBYTE;
      b = NEXTBYTE;
      z->state->mode = GZ_HEAD_CRC_HI;
  case GZ_HEAD_CRC_HI:
      // skip hi byte
      NEEDBYTE;
      b = NEXTBYTE;
      z->state->mode = GZ_DONE;
  case GZ_DONE:
      // Remember where output is written to and flag that this is a
      // gz stream by setting the gz_mode.
      z->state->gz_start = z->next_out;
      z->state->gz_mode = BLOCKS;
      z->state->gz_sum = 0;

      // Depending on the flag select correct next step
      // (clone of code in FLAG)
      if (!(z->state->gz_flag & PRESET_DICT))
          z->state->mode = BLOCKS;
      else
          z->state->mode = DICT4;
      break;
#endif // __ECOS__
    case FLAG:
      NEEDBYTE
      b = NEXTBYTE;
      if (((z->state->sub.method << 8) + b) % 31)
      {
        z->state->mode = BAD;
        z->msg = (char*)"incorrect header check";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      Tracev((stderr, "inflate: zlib header ok\n"));
      if (!(b & PRESET_DICT))
      {
        z->state->mode = BLOCKS;
        break;
      }
      z->state->mode = DICT4;
    case DICT4:
      NEEDBYTE
      z->state->sub.check.need = (uLong)NEXTBYTE << 24;
      z->state->mode = DICT3;
    case DICT3:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 16;
      z->state->mode = DICT2;
    case DICT2:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 8;
      z->state->mode = DICT1;
    case DICT1:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE;
      z->adler = z->state->sub.check.need;
      z->state->mode = DICT0;
      return Z_NEED_DICT;
    case DICT0:
      z->state->mode = BAD;
      z->msg = (char*)"need dictionary";
      z->state->sub.marker = 0;       /* can try inflateSync */
      return Z_STREAM_ERROR;
    case BLOCKS:
#ifdef __ECOS__
      if (z->state->gz_mode != DONE)
        z->state->gz_start = z->next_out;
#endif
      r = inflate_blocks(z->state->blocks, z, r);
      if (r == Z_DATA_ERROR)
      {
        z->state->mode = BAD;
        z->state->sub.marker = 0;       /* can try inflateSync */
        break;
      }
      if (r == Z_OK)
        r = f;
#ifdef __ECOS__
      if (z->state->gz_mode != DONE)
        z->state->gz_sum = cyg_ether_crc32_accumulate(z->state->gz_sum,
                                                      z->state->gz_start,
                                                      z->next_out - 
                                                      z->state->gz_start);
#endif
      if (r != Z_STREAM_END)
        return r;
      r = f;
      inflate_blocks_reset(z->state->blocks, z, &z->state->sub.check.was);
      if (z->state->nowrap)
      {
        z->state->mode = DONE;
        break;
      }
      z->state->mode = CHECK4;
    case CHECK4:
      NEEDBYTE
      z->state->sub.check.need = (uLong)NEXTBYTE << 24;
      z->state->mode = CHECK3;
    case CHECK3:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 16;
      z->state->mode = CHECK2;
    case CHECK2:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 8;
      z->state->mode = CHECK1;
    case CHECK1:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE;

#ifdef __ECOS__
      if (z->state->gz_mode != DONE) {
          // Byte swap CRC since it is read in the opposite order as
          // written by gzip.
          unsigned long c = z->state->gz_sum;
          z->state->sub.check.was = (((c & 0xff000000) >> 24) | ((c & 0x00ff0000) >> 8)
                                     | ((c & 0x0000ff00) << 8) | ((c & 0x000000ff) << 24));
          
      }
#endif

      if (z->state->sub.check.was != z->state->sub.check.need)
      {
        z->state->mode = BAD;
        z->msg = (char*)"incorrect data check";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      Tracev((stderr, "inflate: zlib check ok\n"));
      z->state->mode = DONE;
    case DONE:
      return Z_STREAM_END;
    case BAD:
      return Z_DATA_ERROR;
    default:
      return Z_STREAM_ERROR;
  }
#ifdef NEED_DUMMY_RETURN
  return Z_STREAM_ERROR;  /* Some dumb compilers complain without this */
#endif
}


int ZEXPORT inflateSetDictionary(z, dictionary, dictLength)
z_streamp z;
const Bytef *dictionary;
uInt  dictLength;
{
  uInt length = dictLength;

  if (z == Z_NULL || z->state == Z_NULL || z->state->mode != DICT0)
    return Z_STREAM_ERROR;

  if (adler32(1L, dictionary, dictLength) != z->adler) return Z_DATA_ERROR;
  z->adler = 1L;

  if (length >= ((uInt)1<<z->state->wbits))
  {
    length = (1<<z->state->wbits)-1;
    dictionary += dictLength - length;
  }
  inflate_set_dictionary(z->state->blocks, dictionary, length);
  z->state->mode = BLOCKS;
  return Z_OK;
}


int ZEXPORT inflateSync(z)
z_streamp z;
{
  uInt n;       /* number of bytes to look at */
  Bytef *p;     /* pointer to bytes */
  uInt m;       /* number of marker bytes found in a row */
  uLong r, w;   /* temporaries to save total_in and total_out */

  /* set up */
  if (z == Z_NULL || z->state == Z_NULL)
    return Z_STREAM_ERROR;
  if (z->state->mode != BAD)
  {
    z->state->mode = BAD;
    z->state->sub.marker = 0;
  }
  if ((n = z->avail_in) == 0)
    return Z_BUF_ERROR;
  p = z->next_in;
  m = z->state->sub.marker;

  /* search */
  while (n && m < 4)
  {
    static const Byte mark[4] = {0, 0, 0xff, 0xff};
    if (*p == mark[m])
      m++;
    else if (*p)
      m = 0;
    else
      m = 4 - m;
    p++, n--;
  }

  /* restore */
  z->total_in += p - z->next_in;
  z->next_in = p;
  z->avail_in = n;
  z->state->sub.marker = m;

  /* return no joy or set up to restart on a new block */
  if (m != 4)
    return Z_DATA_ERROR;
  r = z->total_in;  w = z->total_out;
  inflateReset(z);
  z->total_in = r;  z->total_out = w;
  z->state->mode = BLOCKS;
  return Z_OK;
}


/* Returns true if inflate is currently at the end of a block generated
 * by Z_SYNC_FLUSH or Z_FULL_FLUSH. This function is used by one PPP
 * implementation to provide an additional safety check. PPP uses Z_SYNC_FLUSH
 * but removes the length bytes of the resulting empty stored block. When
 * decompressing, PPP checks that at the end of input packet, inflate is
 * waiting for these length bytes.
 */
int ZEXPORT inflateSyncPoint(z)
z_streamp z;
{
  if (z == Z_NULL || z->state == Z_NULL || z->state->blocks == Z_NULL)
    return Z_STREAM_ERROR;
  return inflate_blocks_sync_point(z->state->blocks);
}
