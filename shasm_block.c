/*
 * shasm_block.c
 * 
 * Implementation of shasm_block.h
 * 
 * See the header for further information.
 */
#include "shasm_block.h"
#include "shasm_ascii.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * The initial capacity of the block buffer in bytes, as a signed long
 * constant.
 * 
 * This includes space for the terminating null character.
 * 
 * This value must be at least two, and not be greater than
 * SHASM_BLOCK_MAXBUFFER.
 */
#define SHASM_BLOCK_MINBUFFER (32L)

/*
 * The maximum capacity of the block buffer in bytes, as a signed long
 * constant.
 * 
 * This includes space for the terminating null character.  Blocks may
 * be no longer than one less than this value in length.  Since the
 * maximum block size supported by Shastina is 32,766 bytes, this value
 * is one greater than that.
 * 
 * This value must not be less than SHASM_BLOCK_MINBUFFER.  It must also
 * be less than half of LONG_MAX, so that doubling the capacity never
 * results in overflow.
 */
#define SHASM_BLOCK_MAXBUFFER (32767L)

/*
 * The initial capacity of a temporary buffer (SHASM_BLOCK_TBUF) in
 * bytes.
 * 
 * This value must be at least one, and not be greater than 
 * SHASM_BLOCK_MAXBUFFER.
 */
#define SHASM_BLOCK_MINTBUF (8)

/*
 * The maximum Unicode codepoint value.
 */
#define SHASM_BLOCK_MAXCODE (0x10ffffL)

/*
 * The minimum and maximum Unicode surrogate codepoints.
 */
#define SHASM_BLOCK_MINSURROGATE (0xd800L)
#define SHASM_BLOCK_MAXSURROGATE (0xdfffL)

/*
 * The first high surrogate and the first low surrogate codepoints.
 * 
 * The high surrogate encodes the ten most significant bits of the
 * supplemental offset and comes first in the pair, while the low
 * surrogate encodes the ten least significant bits of the supplemental
 * offset and comes second in the pair.
 */
#define SHASM_BLOCK_HISURROGATE (0xd800L)
#define SHASM_BLOCK_LOSURROGATE (0xdc00L)

/*
 * The minimum Unicode codepoint that is in supplemental range.
 */
#define SHASM_BLOCK_MINSUPPLEMENTAL (0x10000L)

/*
 * The minimum codepoints for which 2-byte, 3-byte, and 4-byte UTF-8
 * encodings are used.
 */
#define SHASM_BLOCK_UTF8_2BYTE (0x80L)
#define SHASM_BLOCK_UTF8_3BYTE (0x800L)
#define SHASM_BLOCK_UTF8_4BYTE (0x10000L)

/*
 * The leading byte masks for 2-byte, 3-byte, and 4-byte UTF-8
 * encodings.
 */
#define SHASM_BLOCK_UTF8_2MASK (0xC0)
#define SHASM_BLOCK_UTF8_3MASK (0xE0)
#define SHASM_BLOCK_UTF8_4MASK (0XF0)

/*
 * SHASM_BLOCK structure for storing block reader state.
 * 
 * The prototype of this structure is given in the header.
 */
struct SHASM_BLOCK_TAG {
  
  /*
   * The status of the block reader.
   * 
   * This is one of the codes from shasm_error.h
   * 
   * If SHASM_OKAY (the initial value), then there is no error and the
   * block reader is in a function state.  Otherwise, the block reader
   * is in an error state and code indicates the error.
   */
  int code;
  
  /*
   * The line number.
   * 
   * If the status code is SHASM_OKAY, this is the line number that the
   * most recently read block begins at, or one if no blocks have been
   * read yet (the initial value), or LONG_MAX if the line count has
   * overflowed.
   * 
   * If the status code indicates an error state, this is the line
   * number that the error occurred at, or LONG_MAX if the line count
   * has overflowed.
   */
  long line;
  
  /*
   * The capacity of the allocated buffer in bytes.
   * 
   * This includes space for a terminating null character.  The buffer
   * starts out allocated at a capacity of SHASM_BLOCK_MINBUFFER.  It
   * grows by doubling as necessary, to a maximum capacity value of
   * SHASM_BLOCK_MAXBUFFER.
   */
  long buf_cap;
  
  /*
   * The length of data stored in the buffer, in bytes.
   * 
   * This does not include the terminating null character.  If zero, it
   * means the string is empty.  This starts out at zero.
   */
  long buf_len;
  
  /*
   * Flag indicating whether a null byte has been written as data to the
   * buffer.
   * 
   * The buffer will always be given a terminating null byte regardless
   * of whether a null byte is present in the data.  However, the ptr
   * function will check this flag to determine whether it's safe for
   * the client to treat the string as null-terminated.
   * 
   * This flag starts out as zero to indicate no null byte has been
   * written yet.
   */
  int null_present;
  
  /*
   * Pointer to the dynamically allocated buffer.
   * 
   * This must be freed when the block structure is freed.  Its capacity
   * is stored in buf_cap, the actual data length is buf_len, and
   * null_present indicates whether the data includes a null byte.
   * 
   * The data always has a null termination byte following it, even if
   * the data includes null bytes as data.
   */
  unsigned char *pBuf;
  
};

/*
 * Structure for storing a temporary buffer.
 * 
 * Use the shasm_block_tbuf functions to initialize and interact with
 * this structure.
 */
typedef struct {
  
  /*
   * The length of the buffer in bytes.
   */
  long len;
  
  /*
   * Pointer to the buffer.
   * 
   * This is NULL if len is zero, else it is a dynamically allocated
   * pointer.
   */
  unsigned char *pBuf;
  
} SHASM_BLOCK_TBUF;

/*
 * Structure for storing decoding map overlay state.
 * 
 * Use the shasm_decoder_overlay functions to work with this structure.
 */
typedef struct {
  
  /*
   * The decoding map that this overlay is set on top of.
   */
  SHASM_BLOCK_DECODER dec;
  
  /*
   * The most recent branch taken, or -1 to indicate that no branches
   * have been taken from root.
   * 
   * This starts out at -1 to indicate the decoding map is at the root
   * node and no branches have been taken.
   * 
   * When a successful branch is taken, the unsigned byte value (0-255)
   * of the branch is stored in this field.
   */
  int recent;
  
  /*
   * The type of string currently being decoded.
   * 
   * This determines whether the string data is a "" '' or {} string.
   * 
   * It must be one of the SHASM_BLOCK_STYPE constants.
   */
  int stype;
  
  /*
   * The input override mode of the string currently being decoded.
   * 
   * This must be one of the SHASM_BLOCK_IMODE constants.  Use the
   * constant SHASM_BLOCK_IMODE_NONE if there are no input overrides.
   */
  int i_over;
  
  /*
   * The bracket nesting level.
   * 
   * This starts out at one and may never go below one.  An error occurs
   * if this reaches LONG_MAX.
   * 
   * The nesting level may only be changed in SHASM_BLOCK_STYPE_CURLY {}
   * strings.  Faults occur if the nesting level is changed for other
   * string types.
   */
  long nest_level;
  
} SHASM_DECODER_OVERLAY;

/* 
 * Local functions
 * ===============
 */

static void shasm_block_seterr(
    SHASM_BLOCK *pb,
    SHASM_IFLSTATE *ps,
    int code);
static void shasm_block_clear(SHASM_BLOCK *pb);
static int shasm_block_addByte(SHASM_BLOCK *pb, int c);

static void shasm_block_tbuf_init(SHASM_BLOCK_TBUF *pt);
static void shasm_block_tbuf_reset(SHASM_BLOCK_TBUF *pt);
static int shasm_block_tbuf_widen(SHASM_BLOCK_TBUF *pt, long tlen);
static unsigned char *shasm_block_tbuf_ptr(SHASM_BLOCK_TBUF *pt);
static long shasm_block_tbuf_len(SHASM_BLOCK_TBUF *pt);

static void shasm_block_pair(long code, long *pHi, long *pLo);

static int shasm_block_ereg(
    SHASM_BLOCK *pb,
    long entity,
    const SHASM_BLOCK_ENCODER *penc,
    SHASM_BLOCK_TBUF *pt);

static int shasm_block_utf8(SHASM_BLOCK *pb, long entity, int cesu8);
static int shasm_block_utf16(SHASM_BLOCK *pb, long entity, int big);
static int shasm_block_utf32(SHASM_BLOCK *pb, long entity, int big);

static int shasm_block_encode(
    SHASM_BLOCK *pb,
    long entity,
    const SHASM_BLOCK_ENCODER *penc,
    int o_over,
    int o_strict,
    SHASM_BLOCK_TBUF *pt);

/*
 * Set a block reader into an error state.
 * 
 * The provided code must not be SHASM_OKAY.  It can be anything else,
 * though it should be defined in shasm_error.
 * 
 * If the block reader is already in an error state, this function
 * performs no further action besides checking that the code is not
 * SHASM_OKAY.
 * 
 * If the block reader is not currently in an error state, this function
 * clears the buffer of the block reader to empty, sets the error
 * status, and sets the line number field to the current line number of
 * the input filter chain.
 * 
 * Parameters:
 * 
 *   pb - the block reader to set to error state
 * 
 *   ps - the input filter chain to query for the line number
 * 
 *   code - the error code to set
 */
static void shasm_block_seterr(
    SHASM_BLOCK *pb,
    SHASM_IFLSTATE *ps,
    int code) {
  
  /* Check parameters */
  if ((pb == NULL) || (ps == NULL) || (code == SHASM_OKAY)) {
    abort();
  }
  
  /* Only set error state if not already in error state */
  if (pb->code == SHASM_OKAY) {
    /* Set error state */
    shasm_block_clear(pb);
    pb->code = code;
    pb->line = shasm_input_count(ps);
  }
}

/*
 * Clear the block reader's internal buffer to an empty string.
 * 
 * This does not reset the error status of the block reader (see
 * shasm_block_status).
 * 
 * Parameters:
 * 
 *   pb - the block reader to clear
 */
static void shasm_block_clear(SHASM_BLOCK *pb) {
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Reset buf_len and null_present */
  pb->buf_len = 0;
  pb->null_present = 0;
  
  /* Clear the buffer to all zero */
  memset(pb->pBuf, 0, (size_t) pb->buf_cap);
}

/*
 * Append an unsigned byte value (0-255) to the end of the block
 * reader's internal buffer.
 * 
 * This might cause the buffer to be reallocated, so pointers returned
 * by shasm_block_ptr become invalid after calling this function.
 * 
 * The function will fail if there is no more room for another
 * character.  This function does *not* set an error state within the
 * block reader if it fails, leaving that for the client.
 * 
 * If this function is called when the block reader is already in an
 * error state, the function fails and does nothing further.
 * 
 * Parameters:
 * 
 *   pb - the block reader to append a byte to
 * 
 *   c - the unsigned byte value to append (0-255)
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int shasm_block_addByte(SHASM_BLOCK *pb, int c) {
  int status = 1;
  
  /* Check parameters */
  if ((pb == NULL) || (c < 0) || (c > 255)) {
    abort();
  }
  
  /* Fail immediately if already in error state */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* If buf_len is one less than buf_cap, buffer capacity needs to be
   * increased -- the "one less" is to account for the terminating
   * null */
  if (status && (pb->buf_len >= pb->buf_cap - 1)) {
    /* If buffer capacity already at maximum capacity, fail */
    if (pb->buf_len >= SHASM_BLOCK_MAXBUFFER) {
      status = 0;
    }
    
    /* Room to grow still -- new capacity is minimum of double the
     * current capacity and the maximum capacity */
    if (status) {
      pb->buf_cap *= 2;
      if (pb->buf_cap > SHASM_BLOCK_MAXBUFFER) {
        pb->buf_cap = SHASM_BLOCK_MAXBUFFER;
      }
    }
    
    /* Expand the size of the buffer to the new capacity */
    if (status) {
      pb->pBuf = (unsigned char *) realloc(
                    pb->pBuf, (size_t) pb->buf_cap);
      if (pb->pBuf == NULL) {
        abort();
      }
    }
    
    /* Clear the expanded buffer contents to zero */
    if (status) {
      memset(&(pb->pBuf[pb->buf_len]), 0, pb->buf_cap - pb->buf_len);
    }
  }
  
  /* If we're adding a null byte, set the null_present flag */
  if (status && (c == 0)) {
    pb->null_present = 1;
  }
  
  /* Append the new byte */
  if (status) {
    pb->pBuf[pb->buf_len] = (unsigned char) c;
    (pb->buf_len)++;
  }
  
  /* Return status */
  return status;
}

/*
 * Initialize a temporary buffer.
 * 
 * This initializes the temporary buffer to zero length with no actual
 * buffer allocated.
 * 
 * Do not call this function on a temporary buffer that has already been
 * initialized or a memory leak may occur.
 * 
 * Parameters:
 * 
 *   pt - the uninitialized temporary buffer to initialize
 */
static void shasm_block_tbuf_init(SHASM_BLOCK_TBUF *pt) {
  /* Check parameter */
  if (pt == NULL) {
    abort();
  }
  
  /* Initialize */
  memset(pt, 0, sizeof(SHASM_BLOCK_TBUF));
  pt->len = 0;
  pt->pBuf = NULL;
}

/*
 * Reset a temporary buffer.
 * 
 * This returns the temporary buffer to zero length, freeing any
 * dynamically allocated buffer.
 * 
 * Undefined behavior occurs if this is called on a temporary buffer
 * that has not yet been initialized.
 * 
 * Parameters:
 * 
 *   pt - the initialized temporary buffer to reset
 */
static void shasm_block_tbuf_reset(SHASM_BLOCK_TBUF *pt) {
  /* Check parameter */
  if (pt == NULL) {
    abort();
  }
  
  /* Free the memory buffer if allocated */
  if (pt->pBuf != NULL) {
    free(pt->pBuf);
    pt->pBuf = NULL;
  }
  
  /* Set the length to zero */
  pt->len = 0;
}

/*
 * Widen a temporary buffer if necessary to be at least the given size.
 * 
 * The buffer must have been initialized with shasm_block_tbuf_init or
 * undefined behavior occurs.  After calling this widening function, the
 * shasm_block_tbuf_reset function must eventually be called to free the
 * dynamically allocated buffer.
 * 
 * tlen is the number of bytes that the buffer should at least have.  It
 * must be zero or greater.
 * 
 * If tlen is greater than SHASM_BLOCK_MAXBUFFER, then this function
 * will fail.
 * 
 * If the current buffer size is greater than or equal to tlen, then
 * this function call does nothing.
 * 
 * If the current buffer size is zero and tlen is greater than zero,
 * then the target size will start out at SHASM_BLOCK_MINTBUF.  Else, if
 * the current buffer size is greater than zero and tlen is greater than
 * the current buffer size, the target size will start out at the
 * current size.  The target size is doubled until it is greater than
 * tlen, with the maximum size clamped at SHASM_BLOCK_MAXBUFFER.  The
 * buffer is then reallocated to this size and the function returns
 * successfully.
 * 
 * This function will always clear the temporary buffer to all zero
 * contents, regardless of whether the buffer was actually widened.
 * 
 * Parameters:
 * 
 *   pt - the temporary buffer to widen if necessary
 * 
 *   tlen - the minimum size in bytes
 * 
 * Return:
 * 
 *   non-zero if successful, zero if requested length was too large
 */
static int shasm_block_tbuf_widen(SHASM_BLOCK_TBUF *pt, long tlen) {
  int status = 1;
  long tl = 0;
  
  /* Check parameters */
  if ((pt == NULL) || (tlen < 0)) {
    abort();
  }
  
  /* Fail if requested length exceeds maximum buffer length */
  if (tlen > SHASM_BLOCK_MAXBUFFER) {
    status = 0;
  }
  
  /* Only widen if not yet large enough */
  if (status && (tlen > pt->len)) {
    
    /* Start target length at current length */
    tl = pt->len;
    
    /* If target length is zero, expand to initial capacity */
    if (tl < 1) {
      tl = SHASM_BLOCK_MINTBUF;
    }
    
    /* Keep doubling until greater than tlen */
    for( ; tl < tlen; tl *= 2);
    
    /* If result is greater than maximum buffer size, shrink to maximum
     * possible */
    if (tl > SHASM_BLOCK_MAXBUFFER) {
      tl = SHASM_BLOCK_MAXBUFFER;
    }
    
    /* Set size */
    pt->len = tl;
    
    /* (Re)allocate buffer to new size */
    if (pt->pBuf == NULL) {
      pt->pBuf = (unsigned char *) malloc((size_t) pt->len);
    } else {
      pt->pBuf = (unsigned char *) realloc(pt->pBuf, (size_t) pt->len);
    }
    if (pt->pBuf == NULL) {
      abort();
    }
  }
  
  /* If buffer not zero length, clear its contents */
  if (status && (pt->len > 0)) {
    memset(pt->pBuf, 0, (size_t) pt->len);
  }
  
  /* Return status */
  return status;
}

/* 
 * Get a pointer to the temporary buffer.
 * 
 * Undefined behavior occurs if the temporary buffer has not been
 * initialized with shasm_block_tbuf_init.
 * 
 * If the buffer length is greater than zero, a pointer to the internal
 * buffer is returned.  Else, NULL is returned.
 * 
 * The returned pointer is valid until the buffer is widened or reset.
 * 
 * Parameters:
 * 
 *   pt - the temporary buffer
 * 
 * Return:
 * 
 *   a pointer to the internal buffer, or NULL if the buffer is zero
 *   length
 */
static unsigned char *shasm_block_tbuf_ptr(SHASM_BLOCK_TBUF *pt) {
  
  /* Check parameter */
  if (pt == NULL) {
    abort();
  }
  
  /* Return pointer to internal buffer or NULL */
  return pt->pBuf;
}

/*
 * Return the current length of the temporary buffer in bytes.
 * 
 * The temporary buffer must have been initialized with
 * shasm_block_tbuf_init or undefined behavior occurs.
 * 
 * Parameters:
 * 
 *   pt - the temporary buffer to query
 * 
 * Return:
 * 
 *   the length of the temporary buffer in bytes
 */
static long shasm_block_tbuf_len(SHASM_BLOCK_TBUF *pt) {
  
  /* Check parameter */
  if (pt == NULL) {
    abort();
  }
  
  /* Return length */
  return pt->len;
}

/*
 * Encode a supplemental Unicode codepoint into a Surrogate pair.
 * 
 * The provided code must be in range SHASM_BLOCK_MINSUPPLEMENTAL up to
 * and including SHASM_BLOCK_MAXCODE.
 * 
 * The provided pointers must not be NULL, and they must not be equal to
 * each other.
 * 
 * To compute the surrogates, first determine the supplemental offset by
 * subtracting SHASM_BLOCK_MINSUPPLEMENTAL from the provided code.
 * 
 * Add the ten most significant bits of the supplemental offset to
 * SHASM_BLOCK_HISURROGATE to get the high surrogate.  Add the ten least
 * significant bits of the supplemental offset to the constant
 * SHASM_BLOCK_LOSURROGATE to get the low surrogate.
 * 
 * The high surrogate should appear before the low surrogate in the
 * output.
 * 
 * Parameters:
 * 
 *   code - the supplemental codepoint
 * 
 *   pHi - pointer to the long to receive the high surrogate
 * 
 *   pLo - pointer to the long to receive the low surrogate
 */
static void shasm_block_pair(long code, long *pHi, long *pLo) {
  
  long offs = 0;
  
  /* Check parameters */
  if ((code < SHASM_BLOCK_MINSUPPLEMENTAL) ||
      (code > SHASM_BLOCK_MAXCODE) ||
      (pHi == NULL) || (pLo == NULL) || (pHi == pLo)) {
    abort();
  }
  
  /* Get supplemental offset */
  offs = code - SHASM_BLOCK_MINSUPPLEMENTAL;
  
  /* Split offset into low and high surrogates */
  *pLo = offs & 0x3ffL;
  *pHi = (offs >> 10) & 0x3ffL;
  
  /* Add the surrogate offsets */
  *pLo += SHASM_BLOCK_LOSURROGATE;
  *pHi += SHASM_BLOCK_HISURROGATE;
}

/*
 * Encode an entity value with an encoding table and append the output
 * bytes to the block reader buffer.
 * 
 * This function does not account for output overrides.
 * 
 * If the block reader is already in an error state when this function
 * is called, this function fails immediately.
 * 
 * The given entity code must be zero or greater.
 * 
 * The provided encoder callback defines the encoding table that will be
 * used.  The encoding table defines the mapping of entity codes to
 * sequences of zero or more output bytes.  Unrecognized entity codes
 * are mapped to zero-length output byte sequences.
 * 
 * The temporary buffer must be allocated by the caller and initialized.
 * This allows the same temporary buffer to be used across multiple
 * encoding calls for sake of efficiency.  The caller should reset the
 * temporary buffer when finished to prevent a memory leak.
 * 
 * This function fails if the block reader buffer runs out of space.  In
 * this case, the block reader buffer state is undefined, and only part
 * of the output bytecode may have been written.
 * 
 * However, this function does *not* set an error state in the block
 * reader.  This is the caller's responsibility.
 * 
 * Parameters:
 * 
 *   pb - the block reader
 * 
 *   entity - the entity code to encode
 * 
 *   penc - the encoding table
 * 
 *   pt - an initialized temporary buffer
 * 
 * Return:
 * 
 *   non-zero if successful, zero if the block reader was already in an
 *   error state or this function ran out of space in the block reader
 *   buffer
 */
static int shasm_block_ereg(
    SHASM_BLOCK *pb,
    long entity,
    const SHASM_BLOCK_ENCODER *penc,
    SHASM_BLOCK_TBUF *pt) {
  
  int status = 1;
  long lv = 0;
  long x = 0;
  unsigned char *pc = NULL;
  
  /* Check parameters */
  if ((pb == NULL) || (entity < 0) || (penc == NULL) || (pt == NULL)) {
    abort();
  }
  
  /* Check that encoder pointer is defined */
  if (penc->fpMap == NULL) {
    abort();
  }
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* Call the mapping function until the code has been read into the
   * temporary buffer with lv as the length of the code */
  while (status) {

    /* Try to map entity with current temporary buffer */
    lv = (*(penc->fpMap))(
            penc->pCustom,
            entity,
            shasm_block_tbuf_ptr(pt),
            shasm_block_tbuf_len(pt));

    /* If the temporary buffer was large enough, break from the loop */
    if (shasm_block_tbuf_len(pt) >= lv) {
      break;
    }
    
    /* Buffer wasn't large enough, so widen it before trying again */
    if (!shasm_block_tbuf_widen(pt, lv)) {
      status = 0;
    }
  }
  
  /* Write each byte to the buffer */
  if (status) {
    pc = shasm_block_tbuf_ptr(pt);
    for(x = 0; x < lv; x++) {
      if (!shasm_block_addByte(pb, pc[x])) {
        status = 0;
        break;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Encode an entity value according to the UTF-8 or CESU-8 encoding
 * systems and append the output bytes to the block reader buffer.
 * 
 * If the block reader is already in an error state when this function
 * is called, this function fails immediately.
 * 
 * The given entity code must be in range zero up to and including
 * SHASM_BLOCK_MAXCODE.  Surrogates are allowed, and will just be
 * encoded like any other codepoint.
 * 
 * If cesu8 is zero, then supplemental characters will be encoded
 * directly in UTF-8, which is standard.  If cesu8 is non-zero, then
 * supplemental characters will first be encoded as a surrogate pair
 * using shasm_block_pair, and then each surrogate codepoint will be
 * encoded in UTF-8.  This is not standard behavior, but it is sometimes
 * used.
 * 
 * This function fails if the block reader buffer runs out of space.  In
 * this case, the block reader buffer state is undefined, and only part
 * of the output bytecode may have been written.
 * 
 * However, this function does *not* set an error state in the block
 * reader.  This is the caller's responsibility.
 * 
 * The UTF-8 encoding system works as follows.  First, determine the
 * total number of output bytes for the codepoint c according to the
 * following table:
 * 
 *   (                               c < SHASM_BLOCK_UTF8_2BYTE) -> 1
 *   (c >= SHASM_BLOCK_UTF8_2BYTE && c < SHASM_BLOCK_UTF8_3BYTE) -> 2
 *   (c >= SHASM_BLOCK_UTF8_3BYTE && c < SHASM_BLOCK_UTF8_4BYTE) -> 3
 *   (c >= SHASM_BLOCK_UTF8_4BYTE                              ) -> 4
 * 
 * Second, extract zero to three continuation bytes, with the number of
 * continuation bytes as one less than the total number of bytes.  To
 * extract a continuation byte, take the six least significant bits of
 * the codepoint, put them in a byte, set the most significant bit of
 * that byte, and shift the codepoint right six bits.
 * 
 * Third, define the leading byte as the remaining bits after the
 * continuation byte extraction.  If the total number of bytes is
 * greater than one, then OR the leading byte with one of the following
 * masks:
 * 
 *   SHASM_BLOCK_UTF8_2MASK for two-byte UTF-8 codes
 *   SHASM_BLOCK_UTF8_3MASK for three-byte UTF-8 codes
 *   SHASM_BLOCK_UTF8_4MASK for four-byte UTF-8 codes
 * 
 * Finally, output the leading byte first.  Then, output any
 * continuation bytes, but output them in the reverse order from which
 * they were extracted.
 * 
 * Parameters:
 * 
 *   pb - the block reader
 * 
 *   entity - the entity code to encode
 * 
 *   cesu8 - non-zero for CESU-8 mode, zero for standard UTF-8
 * 
 * Return:
 * 
 *   non-zero if successful, zero if the block reader was already in an
 *   error state or this function ran out of space in the block reader
 *   buffer
 */
static int shasm_block_utf8(SHASM_BLOCK *pb, long entity, int cesu8) {
  
  int status = 1;
  long s1 = 0;
  long s2 = 0;
  int codelen = 0;
  unsigned char contb[3];
  int i = 0;
  
  /* Initialize buffer */
  memset(&(contb[0]), 0, 3);
  
  /* Check parameters */
  if ((pb == NULL) || (entity < 0) || (entity > SHASM_BLOCK_MAXCODE)) {
    abort();
  }
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* If CESU-8 mode is active and the entity code is in supplemental
   * range, then split into a surrogate pair and recursively call this
   * function in regular UTF-8 mode to encode the high surrogate and
   * then use this function call to encode the low surrogate */
  if (status && cesu8 && (entity >= SHASM_BLOCK_MINSUPPLEMENTAL)) {
    /* Split into surrogates */
    shasm_block_pair(entity, &s1, &s2);

    /* Recursively encode the high surrogate */
    if (!shasm_block_utf8(pb, s1, 0)) {
      status = 0;
    }
    
    /* Encode the low surrogate in this function call */
    entity = s2;
  }
  
  /* Determine the total number of bytes in the UTF-8 encoding */
  if (status) {
    if (entity < SHASM_BLOCK_UTF8_2BYTE) {
      codelen = 1;
    
    } else if (entity < SHASM_BLOCK_UTF8_3BYTE) {
      codelen = 2;
      
    } else if (entity < SHASM_BLOCK_UTF8_4BYTE) {
      codelen = 3;
      
    } else {
      codelen = 4;
    }
  }
  
  /* Extract continuation bytes (if any) */
  if (status) {
    for(i = 0; i < (codelen - 1); i++) {
      contb[i] = (unsigned char) ((entity & 0x3f) | 0x80);
      entity >>= 6;
    }
  }
  
  /* Reverse order of extracted continuation bytes so they are in output
   * order -- this is only needed if there is more than one continuation
   * byte (codelen 3 and 4) */
  if (status && (codelen == 3)) {
    /* Two continuation bytes, so swap them */
    i = contb[0];
    contb[0] = contb[1];
    contb[1] = (unsigned char) i;
    
  } else if (status && (codelen == 4)) {
    /* Three continuation bytes, so swap first and third */
    i = contb[0];
    contb[0] = contb[2];
    contb[2] = (unsigned char) i;
  }
  
  /* Append the leading byte to the block reader buffer */
  if (status) {
    /* Leading byte is remaining bits */
    i = (int) entity;
    
    /* If codelen more than one, add appropriate mask */
    if (codelen == 2) {
      i |= SHASM_BLOCK_UTF8_2MASK;
    
    } else if (codelen == 3) {
      i |= SHASM_BLOCK_UTF8_3MASK;
    
    } else if (codelen == 4) {
      i |= SHASM_BLOCK_UTF8_4MASK;
    }
    
    /* Append leading byte */
    if (!shasm_block_addByte(pb, i)) {
      status = 0;
    }
  }
  
  /* Append any trailing continuation bytes */
  if (status) {
    for(i = 0; i < (codelen - 1); i++) {
      if (!shasm_block_addByte(pb, contb[i])) {
        status = 0;
        break;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Encode an entity value according to the UTF-16 encoding system and
 * append the output bytes to the block reader buffer.
 * 
 * If the block reader is already in an error state when this function
 * is called, this function fails immediately.
 * 
 * The given entity code must be in range zero up to and including
 * SHASM_BLOCK_MAXCODE.  Surrogates are allowed, and will just be
 * encoded like any other codepoint.  Supplemental characters are always
 * encoded as surrogate pairs using shasm_block_pair.
 * 
 * If big is non-zero, each UTF-16 character will be encoded in big
 * endian order, with the most significant byte first.  If big is zero,
 * each UTF-16 character will be encoded in little endian order, with
 * the least significant byte first.
 * 
 * This function fails if the block reader buffer runs out of space.  In
 * this case, the block reader buffer state is undefined, and only part
 * of the output bytecode may have been written.
 * 
 * However, this function does *not* set an error state in the block
 * reader.  This is the caller's responsibility.
 * 
 * Parameters:
 * 
 *   pb - the block reader
 * 
 *   entity - the entity code to encode
 * 
 *   big - non-zero for big endian, zero for little endian
 * 
 * Return:
 * 
 *   non-zero if successful, zero if the block reader was already in an
 *   error state or this function ran out of space in the block reader
 *   buffer
 */
static int shasm_block_utf16(SHASM_BLOCK *pb, long entity, int big) {
  
  int status = 1;
  long s1 = 0;
  long s2 = 0;
  unsigned char vb[2];
  int c = 0;
  
  /* Initialize buffer */
  memset(&(vb[0]), 0, 2);
  
  /* Check parameters */
  if ((pb == NULL) || (entity < 0) || (entity > SHASM_BLOCK_MAXCODE)) {
    abort();
  }
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* If the entity code is in supplemental range, then split into a
   * surrogate pair and recursively call this function to encode the
   * high surrogate and then use this function call to encode the low
   * surrogate */
  if (status && (entity >= SHASM_BLOCK_MINSUPPLEMENTAL)) {
    /* Split into surrogates */
    shasm_block_pair(entity, &s1, &s2);

    /* Recursively encode the high surrogate */
    if (!shasm_block_utf16(pb, s1, big)) {
      status = 0;
    }
    
    /* Encode the low surrogate in this function call */
    entity = s2;
  }
  
  /* Entity should now be in range 0x0-0xffff -- split into two bytes in
   * vb, in little endian order */
  if (status) {
    vb[0] = (unsigned char) (entity & 0xff);
    vb[1] = (unsigned char) ((entity >> 8) & 0xff);
  }
  
  /* If in big endian order, reverse the two bytes */
  if (status && big) {
    c = vb[0];
    vb[0] = vb[1];
    vb[1] = (unsigned char) c;
  }
  
  
  /* Append the bytes to block reader buffer */
  if (status) {
    if (!shasm_block_addByte(pb, vb[0])) {
      status = 0;
    }
      
    if (status) {
      if (!shasm_block_addByte(pb, vb[1])) {
        status = 0;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Encode an entity value according to the UTF-32 encoding system and
 * append the output bytes to the block reader buffer.
 * 
 * If the block reader is already in an error state when this function
 * is called, this function fails immediately.
 * 
 * The given entity code must be in range zero up to and including
 * SHASM_BLOCK_MAXCODE.  Surrogates are allowed, and will just be
 * encoded like any other codepoint.
 * 
 * If big is non-zero, each UTF-32 character will be encoded in big
 * endian order, with the most significant byte first.  If big is zero,
 * each UTF-32 character will be encoded in little endian order, with
 * the least significant byte first.
 * 
 * This function fails if the block reader buffer runs out of space.  In
 * this case, the block reader buffer state is undefined, and only part
 * of the output bytecode may have been written.
 * 
 * However, this function does *not* set an error state in the block
 * reader.  This is the caller's responsibility.
 * 
 * Parameters:
 * 
 *   pb - the block reader
 * 
 *   entity - the entity code to encode
 * 
 *   big - non-zero for big endian, zero for little endian
 * 
 * Return:
 * 
 *   non-zero if successful, zero if the block reader was already in an
 *   error state or this function ran out of space in the block reader
 *   buffer
 */
static int shasm_block_utf32(SHASM_BLOCK *pb, long entity, int big) {
  
  int status = 1;
  unsigned char vb[4];
  int c = 0;
  
  /* Initialize buffer */
  memset(&(vb[0]), 0, 4);
  
  /* Check parameters */
  if ((pb == NULL) || (entity < 0) || (entity > SHASM_BLOCK_MAXCODE)) {
    abort();
  }
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* Split entity into four bytes in vb, in little endian order */
  if (status) {
    vb[0] = (unsigned char) (entity & 0xff);
    vb[1] = (unsigned char) ((entity >> 8) & 0xff);
    vb[2] = (unsigned char) ((entity >> 16) & 0xff);
    vb[3] = (unsigned char) ((entity >> 24) & 0xff);
  }
  
  /* If in big endian order, reverse the four bytes */
  if (status && big) {
    c = vb[0];
    vb[0] = vb[3];
    vb[3] = (unsigned char) c;
    
    c = vb[1];
    vb[1] = vb[2];
    vb[2] = (unsigned char) c;
  }
  
  /* Append the bytes to block reader buffer */
  if (status) {
    if (!shasm_block_addByte(pb, vb[0])) {
      status = 0;
    }
      
    if (status) {
      if (!shasm_block_addByte(pb, vb[1])) {
        status = 0;
      }
    }
    
    if (status) {
      if (!shasm_block_addByte(pb, vb[2])) {
        status = 0;
      }
    }
    
    if (status) {
      if (!shasm_block_addByte(pb, vb[3])) {
        status = 0;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Encode an entity value using the regular string method and append the
 * output bytes to the block reader buffer.
 * 
 * If the block reader is already in an error state when this function
 * is called, this function fails immediately.
 * 
 * The given entity code must be zero or greater.
 * 
 * The provided encoder callback defines the encoding table that will be
 * used.  The encoding table defines the mapping of entity codes to
 * sequences of zero or more output bytes.  Unrecognized entity codes
 * are mapped to zero-length output byte sequences.
 * 
 * o_over must be one of the SHASM_BLOCK_OMODE constants.  They have the
 * following meanings:
 * 
 *   NONE -- the encoding table will be used for all entity codes.  The
 *   o_strict parameter is ignored.
 * 
 *   UTF8 -- entity codes in range zero up to and including
 *   SHASM_BLOCK_MAXCODE will be output in their UTF-8 encoding,
 *   ignoring the encoding table for this range of entity codes.  If
 *   o_strict is non-zero, then the surrogate range is excluded (see
 *   below).
 * 
 *   CESU8 -- same as UTF8, except supplemental codepoints are first
 *   encoded as a surrogate pair, and then each surrogate is encoded in
 *   UTF-8.  This is not standard UTF-8, but it is sometimes used when
 *   full Unicode support is lacking.
 * 
 *   U16LE -- entity codes in range zero up to and including
 *   SHASM_BLOCK_MAXCODE will be output in their UTF-16 encoding, in
 *   little endian order (least significant byte first).  The encoding
 *   table is ignored for entity codes in this range.  Supplemental 
 *   characters are encoded as a surrogate pair, as is standard for
 *   UTF-16.  If o_strict is non-zero, then the surrogate range is
 *   excluded (see below).
 * 
 *   U16BE -- same as U16LE, except big endian order (most significant
 *   byte first) is used.
 * 
 *   U32LE -- entity codes in range zero up to and including
 *   SHASM_BLOCK_MAXCODE will be output in their UTF-32 encoding, in
 *   little endian order (least significant byte first).  The encoding
 *   table is ignored for entity codes in this range.  If o_strict is
 *   non-zero, then the surrogate range is excluded (see below).
 * 
 *   U32BE -- same as U32LE, except big endian order (most significant
 *   byte first) is used.
 * 
 * For UTF8, CESU8, U16LE, U16BE, U32LE, and U32BE, the o_strict flag
 * can be used to exclude the surrogate range.  If the o_strict flag is
 * non-zero for these modes, then entity codes in Unicode surrogate
 * range (SHASM_BLOCK_MINSURROGATE to SHASM_BLOCK_MAXSURROGATE) will be
 * handled by the encoding table rather than by the UTF encoder.  If
 * o_strict is zero for these mdoes, then all entity codes in the range
 * zero to SHASM_BLOCK_MAXCODE will be handled by the UTF encoder.
 * 
 * The temporary buffer must be allocated by the caller and initialized.
 * This allows the same temporary buffer to be used across multiple
 * encoding calls for sake of efficiency.  The caller should reset the
 * temporary buffer when finished to prevent a memory leak.
 * 
 * This function fails if the block reader buffer runs out of space.  In
 * this case, the block reader buffer state is undefined, and only part
 * of the output bytecode may have been written.
 * 
 * However, this function does *not* set an error state in the block
 * reader.  This is the caller's responsibility.
 * 
 * Parameters:
 * 
 *   pb - the block reader
 * 
 *   entity - the entity code to encode
 * 
 *   penc - the encoding table
 * 
 *   o_over - the output override selection
 * 
 *   o_strict - non-zero for strict output override mode, zero for loose
 *   output override mode
 * 
 *   pt - an initialized temporary buffer
 * 
 * Return:
 * 
 *   non-zero if successful, zero if the block reader was already in an
 *   error state or this function ran out of space in the block reader
 *   buffer
 */
static int shasm_block_encode(
    SHASM_BLOCK *pb,
    long entity,
    const SHASM_BLOCK_ENCODER *penc,
    int o_over,
    int o_strict,
    SHASM_BLOCK_TBUF *pt) {
  
  int status = 1;
  
  /* Check parameters, except for o_over */
  if ((pb == NULL) || (entity < 0) || (penc == NULL) || (pt == NULL)) {
    abort();
  }
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* If entity is outside of Unicode codepoint range, set o_over to
   * NONE since output overrides are never used outside of that range */
  if (status && (entity > SHASM_BLOCK_MAXCODE)) {
    o_over = SHASM_BLOCK_OMODE_NONE;
  }
  
  /* If strict flag is on and entity is in Unicode surrogate range, set
   * o_over to NONE since output overrides are never used on surrogates
   * in strict mode */
  if (status && o_strict &&
      (entity >= SHASM_BLOCK_MINSURROGATE) &&
        (entity <= SHASM_BLOCK_MAXSURROGATE)) {
    o_over = SHASM_BLOCK_OMODE_NONE;
  }
  
  /* o_over now has the actual mode to use for this entity -- dispatch
   * to appropriate routine */
  if (status) {
    switch (o_over) {
      
      /* Regular string encoding */
      case SHASM_BLOCK_OMODE_NONE:
        if (!shasm_block_ereg(pb, entity, penc, pt)) {
          status = 0;
        }
        break;
      
      /* UTF-8 encoding */
      case SHASM_BLOCK_OMODE_UTF8:
        if (!shasm_block_utf8(pb, entity, 0)) {
          status = 0;
        }
        break;
      
      /* CESU-8 encoding */
      case SHASM_BLOCK_OMODE_CESU8:
        if (!shasm_block_utf8(pb, entity, 1)) {
          status = 0;
        }
        break;
      
      /* UTF-16 LE encoding */
      case SHASM_BLOCK_OMODE_U16LE:
        if (!shasm_block_utf16(pb, entity, 0)) {
          status = 0;
        }
        break;
      
      /* UTF-16 BE encoding */
      case SHASM_BLOCK_OMODE_U16BE:
        if (!shasm_block_utf16(pb, entity, 1)) {
          status = 0;
        }
        break;
      
      /* UTF-32 LE encoding */
      case SHASM_BLOCK_OMODE_U32LE:
        if (!shasm_block_utf32(pb, entity, 0)) {
          status = 0;
        }
        break;
      
      /* UTF-32 BE encoding */
      case SHASM_BLOCK_OMODE_U32BE:
        if (!shasm_block_utf32(pb, entity, 1)) {
          status = 0;
        }
        break;
      
      /* Unrecognized mode */
      default:
        abort();
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Public functions
 * ================
 * 
 * See the header for specifications.
 */

/*
 * shasm_block_alloc function.
 */
SHASM_BLOCK *shasm_block_alloc(void) {
  SHASM_BLOCK *pb = NULL;
  
  /* Allocate a block */
  pb = (SHASM_BLOCK *) malloc(sizeof(SHASM_BLOCK));
  if (pb == NULL) {
    abort();
  }
  
  /* Clear the block */
  memset(pb, 0, sizeof(SHASM_BLOCK));
  
  /* Initialize fields */
  pb->code = SHASM_OKAY;
  pb->line = 1;
  pb->buf_cap = SHASM_BLOCK_MINBUFFER;
  pb->buf_len = 0;
  pb->null_present = 0;
  pb->pBuf = NULL;
  
  /* Allocate the initial dynamic buffer */
  pb->pBuf = (unsigned char *) malloc((size_t) pb->buf_cap);
  if (pb->pBuf == NULL) {
    abort();
  }
  
  /* Clear the dynamic buffer to all zero */
  memset(pb->pBuf, 0, (size_t) pb->buf_cap);
  
  /* Return the new block object */
  return pb;
}

/*
 * shasm_block_free function.
 */
void shasm_block_free(SHASM_BLOCK *pb) {
  /* Free object if not NULL */
  if (pb != NULL) {
    /* Free dynamic buffer if not NULL */
    if (pb->pBuf != NULL) {
      free(pb->pBuf);
      pb->pBuf = NULL;
    }
    
    /* Free the main structure */
    free(pb);
  }
}

/*
 * shasm_block_status function.
 */
int shasm_block_status(SHASM_BLOCK *pb, long *pLine) {
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Write line number if in error state and pLine provided */
  if ((pb->code != SHASM_OKAY) && (pLine != NULL)) {
    *pLine = pb->line;
  }
  
  /* Return status */
  return pb->code;
}

/*
 * shasm_block_count function.
 */
long shasm_block_count(SHASM_BLOCK *pb) {
  long result = 0;
  
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Use the count field or a value of zero, depending on error state */
  if (pb->code == SHASM_OKAY) {
    /* Not in error state -- use length field of structure */
    result = pb->buf_len;
  } else {
    /* Error state -- use value of zero */
    result = 0;
  }
  
  /* Return result */
  return result;
}

/*
 * shasm_block_ptr function.
 */
unsigned char *shasm_block_ptr(SHASM_BLOCK *pb, int null_term) {
  unsigned char *pc = NULL;
  
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Get pointer to buffer */
  pc = pb->pBuf;
  
  /* Special handling for a few cases */
  if (pb->code != SHASM_OKAY) {
    /* Error state -- write a terminating null into the start of the
     * buffer to make sure there's an empty string */
    *pc = (unsigned char) 0;
  
  } else if (null_term) {
    /* Client expecting null-terminated string -- clear pc to NULL if
     * null is present in the data */
    if (pb->null_present) {
      pc = NULL;
    }
  }
  
  /* Return the buffer pointer */
  return pc;
}

/*
 * shasm_block_line function.
 */
long shasm_block_line(SHASM_BLOCK *pb) {
  long result = 0;
  
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Determine result depending on error state */
  if (pb->code == SHASM_OKAY) {
    /* Not in error state -- return line field */
    result = pb->line;
  
  } else {
    /* In error state -- return LONG_MAX */
    result = LONG_MAX;
  }
  
  /* Return result */
  return result;
}

/*
 * shasm_block_token function.
 */
int shasm_block_token(SHASM_BLOCK *pb, SHASM_IFLSTATE *ps) {
  
  int status = 1;
  int c = 0;
  
  /* Check parameters */
  if ((pb == NULL) || (ps == NULL)) {
    abort();
  }
  
  /* Fail immediately if block reader in error state */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* Read zero or more bytes of whitespace and comments */
  while (status) {
    
    /* Read zero or more filtered HT, SP, or LF characters until either
     * a character that is not one of those three has been read, or EOF
     * or I/O error */
    for(c = shasm_input_get(ps);
        (c == SHASM_ASCII_HT) || (c == SHASM_ASCII_SP) ||
          (c == SHASM_ASCII_LF);
        c = shasm_input_get(ps));
    
    /* Fail if stopped on EOF or I/O error */
    if (c == SHASM_INPUT_EOF) {
      /* Fail on EOF */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
      
    } else if (c == SHASM_INPUT_IOERR) {
      /* Fail on I/O error */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_IO);
    }
    
    /* If the non-whitespace character that was read is not the
     * ampersand, then this is the first character of the token -- in
     * this case, set the line number field of the block reader to the
     * current line number, unread the character, and break out of this
     * loop */
    if (status && (c != SHASM_ASCII_AMPERSAND)) {
      /* First token character found */
      pb->line = shasm_input_count(ps);
      shasm_input_back(ps);
      break;
    }
    
    /* If we got here, we just read an ampersand beginning a comment --
     * proceed by reading characters until either LF, EOF, or I/O is
     * encountered; leave the LF read as it is part of the comment */
    if (status) {
      /* Read the comment characters */
      for(c = shasm_input_get(ps);
          (c != SHASM_ASCII_LF) && (c != SHASM_INPUT_EOF) &&
            (c != SHASM_INPUT_IOERR);
          c = shasm_input_get(ps));
      
      /* Fail if EOF or I/O error */
      if (c == SHASM_INPUT_EOF) {
        /* Fail on EOF */
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
      
      } else if (c == SHASM_INPUT_IOERR) {
        /* Fail on I/O error */
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_IO);
      }
    }
    
    /* Now that we've read the comment, loop back to read any further
     * whitespace and comments */
  }
  
  /* If we got here successfully, we just unread the first character of
   * the token and set the line number of the token -- now clear the
   * buffer to prepare for reading the token characters */
  if (status) {
    shasm_block_clear(pb);
  }
  
  /* Read the first character of the token into the buffer, verifying
   * that it is in visible, printing US-ASCII range */
  if (status) {
    /* Read the character and check for error */
    c = shasm_input_get(ps);
    if (c == SHASM_INPUT_EOF) {
      /* Fail on EOF */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
    
    } else if (c == SHASM_INPUT_IOERR) {
      /* Fail on I/O error */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_IO);
    }
    
    /* Check range */
    if (status &&
          ((c < SHASM_ASCII_VISPRINT_MIN) ||
            (c > SHASM_ASCII_VISPRINT_MAX))) {
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_TOKENCHAR);
    }
    
    /* Add the character to the buffer */
    if (status) {
      if (!shasm_block_addByte(pb, c)) {
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_HUGEBLOCK);
      }
    }
  }
  
  /* If the first character read was vertical bar, read the next
   * character -- if it's semicolon, then add it to the buffer to yield
   * the "|;" token; if it's anything else, unread it */
  if (status && (pb->pBuf[0] == SHASM_ASCII_BAR)) {
    /* Read the next character */
    c = shasm_input_get(ps);
    if (c == SHASM_INPUT_EOF) {
      /* Fail on EOF */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
    
    } else if (c == SHASM_INPUT_IOERR) {
      /* Fail on I/O error */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_IO);
    }
    
    /* If next character is semicolon, add it to buffer; else, unread
     * it */
    if (status && (c == SHASM_ASCII_SEMICOLON)) {
      /* Semicolon -- add it to buffer */
      if (!shasm_block_addByte(pb, c)) {
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_HUGEBLOCK);
      }
      
    } else {
      /* Not a semicolon -- unread it */
      shasm_input_back(ps);
    }
  }
  
  /* If the buffer currently is something other than ( ) [ ] , % ; " ' {
   * or the special token "|;" then read additional token characters;
   * else leave the buffer as it currently is */
  if (status &&
        (pb->pBuf[0] != SHASM_ASCII_LPAREN   ) &&
        (pb->pBuf[0] != SHASM_ASCII_RPAREN   ) &&
        (pb->pBuf[0] != SHASM_ASCII_LSQR     ) &&
        (pb->pBuf[0] != SHASM_ASCII_RSQR     ) &&
        (pb->pBuf[0] != SHASM_ASCII_COMMA    ) &&
        (pb->pBuf[0] != SHASM_ASCII_PERCENT  ) &&
        (pb->pBuf[0] != SHASM_ASCII_SEMICOLON) &&
        (pb->pBuf[0] != SHASM_ASCII_DQUOTE   ) &&
        (pb->pBuf[0] != SHASM_ASCII_SQUOTE   ) &&
        (pb->pBuf[0] != SHASM_ASCII_LCURL    ) &&
        (pb->pBuf[1] != SHASM_ASCII_SEMICOLON)) {
    
    /* Read zero or more additional token characters */
    while (status) {
      /* Read the next character */
      c = shasm_input_get(ps);
      if (c == SHASM_INPUT_EOF) {
        /* Fail on EOF */
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
      
      } else if (c == SHASM_INPUT_IOERR) {
        /* Fail on I/O error */
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_IO);
      }
      
      /* If this is a stop character, then break out of the loop */
      if (status && (
            (c == SHASM_ASCII_HT       ) ||
            (c == SHASM_ASCII_SP       ) ||
            (c == SHASM_ASCII_LF       ) ||
            (c == SHASM_ASCII_LPAREN   ) ||
            (c == SHASM_ASCII_RPAREN   ) ||
            (c == SHASM_ASCII_LSQR     ) ||
            (c == SHASM_ASCII_RSQR     ) ||
            (c == SHASM_ASCII_COMMA    ) ||
            (c == SHASM_ASCII_PERCENT  ) ||
            (c == SHASM_ASCII_SEMICOLON) ||
            (c == SHASM_ASCII_AMPERSAND) ||
            (c == SHASM_ASCII_DQUOTE   ) ||
            (c == SHASM_ASCII_SQUOTE   ) ||
            (c == SHASM_ASCII_LCURL    ))) {
        break;
      }
      
      /* Check range */
      if (status &&
            ((c < SHASM_ASCII_VISPRINT_MIN) ||
              (c > SHASM_ASCII_VISPRINT_MAX))) {
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_TOKENCHAR);
      }
      
      /* Not a stop character and range validated, so add to buffer */
      if (status) {
        if (!shasm_block_addByte(pb, c)) {
          status = 0;
          shasm_block_seterr(pb, ps, SHASM_ERR_HUGEBLOCK);
        }
      }
    }

    /* If stopped on an inclusive stop character, add it to the buffer;
     * else the stop character is exclusive so unread it */
    if (status && (
          (c == SHASM_ASCII_DQUOTE) ||
          (c == SHASM_ASCII_SQUOTE) ||
          (c == SHASM_ASCII_LCURL ))) {
      /* Inclusive stop character -- add to buffer */
      if (!shasm_block_addByte(pb, c)) {
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_HUGEBLOCK);
      }
      
    } else if (status) {
      /* Exclusive stop character -- unread it */
      shasm_input_back(ps);
    }
  }
  
  /* Return status */
  return status;
}

/*
 * shasm_block_string function.
 */
int shasm_block_string(
    SHASM_BLOCK *pb,
    SHASM_IFLSTATE *ps,
    const SHASM_BLOCK_STRING *sp) {
  /* @@TODO: placeholder */
  /*
   * For the moment, we're going to try to encode the following testing
   * sequence, which is meant to exercise the encode function:
   * 
   *   'H'
   *   'i'
   *   '~'
   *   '$'
   *       0xA2 (cent sign)
   *     0x20AC (euro sign)
   *    0x10348 (gothic letter hwair)
   *       0xDF (eszett)
   *       0x0A (line feed)
   *   0x200005 (special key #5 defined in the test encoding table)
   *     0xD801 (unpaired surrogate)
   *    0x10437 (deseret small letter yee)
   *    0x24B62 (unknown supplemental codepoint)
   *   '!'
   * 
   * Some of the supplemental characters chosen match examples given on
   * the Wikipedia pages for UTF-8 and UTF-16, such that the example
   * encodings can be checked against the output of our encoder.
   */
  
  int status = 1;
  SHASM_BLOCK_TBUF tb;
  
  /* Initialize buffer */
  shasm_block_tbuf_init(&tb);
  
  /* Check parameters */
  if ((pb == NULL) || (ps == NULL) || (sp == NULL)) {
    abort();
  }
  
  /* Send test entity codes */
  if (status) {
    if (!shasm_block_encode(pb, 'H',
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, 'i',
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, '~',
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, '$',
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, 0xA2L,
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, 0x20ACL,
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, 0x10348L,
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, 0xDFL,
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, 0xAL,
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, 0x200005L,
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, 0xD801L,
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, 0x10437L,
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, 0x24B62L,
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  if (status) {
    if (!shasm_block_encode(pb, '!',
          &(sp->enc), sp->o_over, sp->o_strict, &tb)) {
      status = 0;
    }
  }
  
  /* Set error state if error occurred */
  if (!status) {
    shasm_block_seterr(pb, ps, SHASM_ERR_HUGEBLOCK);
  }
  
  /* Reset temporary buffer */
  shasm_block_tbuf_reset(&tb);
  
  /* Return status */
  return status;
}
