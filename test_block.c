/*
 * test_block.c
 * 
 * Testing module for the block reader module (shasm_block).
 * 
 * This testing program reads from standard input using the block
 * reader.  The testing program is invoked like this:
 * 
 *   test_block [mode] [...]
 * 
 * The [mode] argument must be a string selecting one of the testing
 * modes (defined below).  The [...] represents zero or more additional
 * arguments specific to the testing mode.
 * 
 * Testing modes
 * =============
 * 
 * This section describes each of the testing modes that this testing
 * module supports.
 * 
 * token
 * -----
 * 
 * Token testing mode is invoked as follows:
 * 
 *   test_block token
 * 
 * There are no additional parameters beyond the mode name.  The testing
 * mode name "token" is case sensitive.
 * 
 * In token mode, the testing program will use the block reader to read
 * one or more tokens from input, stopping when the token "|;" is
 * encountered.  For each token, the line number will be reported along
 * with the contents of the token.
 * 
 * Note that this mode can't fully parse all the tokens in a normal
 * Shastina file because it does not handle interpolated string data.
 * 
 * string
 * ------
 * 
 * Regular string testing mode is invoked as follows:
 * 
 *   test_block string [type] [outover]
 * 
 * Both the [type] and [outover] parameters are required.
 * 
 * The [type] parameter must be either "q" "a" or "c" (case sensitive)
 * for double-quoted strings "", apostrophe-quoted strings '', or curly
 * bracket strings {}, respectively.
 * 
 * The [outover] parameter must be either "none", "utf8", "cesu8",
 * "utf16le", "utf16be", "utf32le", or "utf32be" to select one of the
 * output overrides or specify that no output override is in effect.  If
 * an output override is selected, it will be in strict mode.
 * 
 * The program reads string data from standard input beginning
 * immediately with the first byte.  This does not include the opening
 * quote or curly bracket (which would come at the end of a token
 * introducing the string data rather than being part of the string
 * data), but it must include the closing quote or curly bracket.  Zero
 * or more additional bytes may follow the string data.  However,
 * nothing may precede the string data in input.
 * 
 * The program reports the resulting string that was read from the
 * string data, with escapes of the form "<0a>" used for characters
 * outside of US-ASCII printing range (0x21-0x7e).  The program also
 * reports any additional bytes that were read after the string data,
 * using the same escaping system for byte values outside of printing
 * range.  If there was an error, the program reports it.
 * 
 * The parameters passed on the command line do not fully specify all of
 * the details of the string type.  Some of the parameters are hardwired
 * into the testing program.  The remainder of this testing mode
 * documentation specifies the hardwired testing parameters.
 * 
 * The hardwired decoding map is as follows.  All printing US-ASCII
 * characters (0x21-0x7e) except for backslash (0x5c), ampersand (0x26),
 * and asterisk (0x2a) have a decoding map key consisting just of that
 * character, mapping the character to an entity value of the same
 * numeric value as the ASCII code.  ASCII Space (0x20) and Line Feed
 * (0x0a) also have a decoding map key consisting just of that
 * character, mapping the character to an entity value of the same
 * numeric value as the ASCII code.
 * 
 * There are a set of decoding map keys beginning with the backslash
 * character, representing escapes.  They are:
 * 
 *   \\     - literal backslash
 *   \&     - literal ampersand
 *   \"     - literal double quote
 *   \'     - literal apostrophe
 *   \{     - literal opening curly bracket
 *   \}     - literal closing curly bracket
 *   \n     - literal LF
 *   \<LF>  - line continuation (see below)
 *   \:a    - lowercase a-umlaut
 *   \:A    - uppercase a-umlaut
 *   \:o    - lowercase o-umlaut
 *   \:O    - uppercase o-umlaut
 *   \:u    - lowercase u-umlaut
 *   \:U    - uppercase u-umlaut
 *   \ss    - German eszett
 *   \u#### - Unicode codepoint (see below)
 * 
 * The line continuation escape is a backslash as the last character of
 * a line in the string data.  This maps to the entity code
 * corresponding to ASCII space.  It allows for line breaks within
 * string data that only appear as a single space in the output string.
 * 
 * The Unicode codepoint escape has four to six base-16 digits (####)
 * that represent the numeric value of a Unicode codepoint, with
 * surrogates disallowed but supplemental characters okay.  The opening
 * "u" is case sensitive, but the base-16 digits can be either uppercase
 * or lowercase.  This is handled as a numeric escape.
 * 
 * All of the other backslash escapes map to an entity code matching the
 * Unicode/ASCII codepoint of the character they represent.
 * 
 * There are also a set of decoding map keys beginning with the
 * ampersand character, representing escapes.  They are:
 * 
 *   &amp;  - literal ampersand
 *   &###;  - decimal Unicode codepoint (see below)
 *   &x###; - base-16 Unicode codepoint (see below)
 * 
 * The ampersand escape maps to an entity code matching the ASCII code
 * for an ampersand character.
 * 
 * The decimal codepoint escape has one or more decimal digits (###)
 * terminated with a semicolon, representing the Unicode codepoint, with
 * surrogates disallowed but supplemental characters okay.
 * 
 * The base-16 codepoint escape has one or more base-16 digits (###)
 * terminated with a semicolon, representing the Unicode codepoint, with
 * surrogates disallowed but supplemental characters okay.
 * 
 * Finally, there are a set of decoding map keys beginning with the
 * asterisk.  They are:
 * 
 *   **                              - literal asterisk
 *   *                               - special key #1
 *   *hello                          - special key #2
 *   *helloWorld                     - special key #3
 *   *helloEvery                     - special key #4
 *   *helloEveryone                  - special key #5
 *   *helloEveryoneOut               - special key #6
 *   *helloEveryoneOutThere          - special key #7
 *   *helloEveryoneOutThereSome      - special key #8
 *   *helloEveryoneOutThereSomewhere - special key #9
 * 
 * The literal asterisk key maps to an entity matching the ASCII code
 * for an asterisk.  Special keys 1-9 map to special entity codes
 * outside of Unicode range.
 * 
 * The numeric escape list string format parameter is defined for the
 * ampersand and backslash numeric escapes described above.
 * 
 * The encoding table is defined such that all entity codes
 * corresponding printing to US-ASCII characters (0x21-0x7e) generate
 * the equivalent ASCII codes, except that uppercase letters map to
 * lowercase letters (thereby making all characters lowercase), and the
 * tilde character is not defined (meaning it gets dropped from output).
 * ASCII Space (0x20) and Line Feed (0x0a) are also defined, and map to
 * their equivalent ASCII codes.  The umlaut characters and eszett
 * character defined in the backslash escapes are defined and map to
 * their 8-bit ISO 8859-1 values.  Finally, the special keys are defined
 * such that they yield a sequence of :-) emoticons with the number of
 * emoticons in the sequence equal to the number of the special key.
 * 
 * If a UTF-16 or UTF-32 output override is in effect, each character in
 * the special key definitions has padding zeros added in the
 * appropriate place to make the ASCII characters work in the desired
 * encoding scheme.
 * 
 * @@TODO: additional testing modes
 * 
 * Compilation
 * ===========
 * 
 * This must be compiled with shasm_block and shasm_input.  Sample build
 * line:
 * 
 *   cc -o test_block test_block.c shasm_input.c shasm_block.c
 */

#include "shasm_block.h"
#include "shasm_input.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* @@TODO: additional testing modes */

/*
 * The long asterisk keys for the decoding map.
 * 
 * This also includes the common *hello prefix
 */
static const char *m_pLongKey1 = "*helloWorld";
static const char *m_pLongKey2 = "*helloEveryoneOutThereSomewhere";
static const char *m_pCommonKey = "*hello";

/*
 * Pass-through structure for storing state information for the decoding
 * map.
 */
typedef struct {
  
  /*
   * The key of the current node.
   * 
   * This is a null-terminated string.  The empty string is allowed,
   * meaning the root node.
   */
  char key[32];
  
} DECMAP_STATE;

/* Function prototypes */
static int longkey_branch(const char *pKey, int len, int c);
static void decmap_reset(DECMAP_STATE *pv);
static int decmap_branch(DECMAP_STATE *pv, int c);
static long decmap_entity(DECMAP_STATE *pv);

static int esclist_query(
    void *pCustom,
    long entity,
    SHASM_BLOCK_NUMESCAPE *pe);

static long enc_map(
    void *pCustom,
    long entity,
    unsigned char *pBuffer,
    long buf_len);

static int rawInput(void *pCustom);
static int test_token(void);

/* @@TODO: implement decmap, esclist, and enc_map functions */
/* @@TODO: add string testing mode */

/*
 * Given a long key value, a current key length, and an unsigned byte
 * value in range 0-255, figure out whether a branch in the long key
 * corresponding to the unsigned byte value is available.
 * 
 * len must be in range zero up to and including the length of the
 * string pKey.
 * 
 * This function returns non-zero if len is less than the length of the
 * string pKey and the character at offset len in the string is equal to
 * c.  Else it returns zero.
 * 
 * This function assumes that the length of the long key can fit within
 * a signed int.  Undefined behavior occurs if this is not the case.
 * 
 * Parameters:
 * 
 *   pKey - the long key to check (null-terminated)
 * 
 *   len - the current key length
 * 
 *   c - the unsigned byte value to find a branch for
 * 
 * Return:
 * 
 *   non-zero if branch available in this long key, zero if not
 */
static int longkey_branch(const char *pKey, int len, int c) {
  
  int result = 0;
  const unsigned char *pc = NULL;
  
  /* Check parameters */
  if ((pKey == NULL) || (len < 0) || (c < 0) || (c > 255)) {
    abort();
  }
  
  /* Check that len is in range for the string */
  if (strlen(pKey) < len) {
    abort();
  }
  
  /* Recast const char pointer as const unsigned char pointer */
  pc = (const unsigned char *) pKey;
  
  /* Check whether the current key length equals the length of the full
   * key */
  if (len < strlen(pKey)) {
    /* Current key length less than length of the full key, so check
     * character at offset len to determine whether there is a branch */
    if (pc[len] == c) {
      result = 1;
    } else {
      result = 0;
    }
    
  } else {
    /* Current key length equals length of the full key, so no further
     * branching */
    result = 0;
  }
  
  /* Return result */
  return result;
}

/*
 * Reset a decoding map state structure to its initial state.
 * 
 * This sets the key to empty, corresponding to the root of the map.
 * 
 * Parameters:
 * 
 *   pv - the decoding map structure to initialize
 */
static void decmap_reset(DECMAP_STATE *pv) {
  /* Check parameter */
  if (pv == NULL) {
    abort();
  }
  
  /* Initialize */
  memset(pv, 0, sizeof(DECMAP_STATE));
  pv->key[0] = (char) 0;
}

/*
 * Follow a branch in the decoding map, if available.
 * 
 * c is the unsigned byte value (0-255) of the branch to follow.  If
 * such a branch exists from the current node, it will be followed and a
 * non-zero value returned.  If no such branch exists from the current
 * node, the position will stay on the current node and zero will be
 * returned.
 * 
 * If the current key is empty, then branches for each printing US-ASCII
 * character (0x21-0x7e), as well as Space (0x20) and Line Feed (0x0a)
 * are available.
 * 
 * If the current key has length one or greater, then examine the first
 * character of the key to determine what branches are available.
 * 
 * If the first character is neither backslash nor ampersand nor
 * asterisk, then no branches are available.
 * 
 * If the first character is backslash, then the current key length must
 * be one, two, or three.  If it is three, there are no branches
 * available.  If it is two and the second character is a colon, then
 * branches for characters aAoOuU are available.  If it is two and the
 * second character is s, then a branch for s is available.  If it is
 * two and the second character is neither colon nor s, then no branches
 * are available.  If the current key length is one, then branches are
 * available for \&"'{}n:su as well as for Line Feed.
 * 
 * If the first character is ampersand, then the current key length is
 * from one to five.  If five, then no branches available.  If four,
 * then a ; branch available.  If three, then a p branch available.  If
 * two and the second character is a then an m branch available, else no
 * branches available.  If one then a and x branches are available.
 * 
 * If the first character is asterisk, then the current key length is
 * from one to 31.  If one, then branches for * and h are available.  If
 * two and second character is h then a branch for e is available, else
 * no branches are available.  If three to five, then branches available
 * corresponding to next character in prefix "*hello".  If six, then
 * branches for W and E available.  If seven to ten and the seventh
 * character is W then branches available corresponding to next
 * character in prefix "*helloWorld".  If seven to 30 and the seventh
 * character is E then branches available corresponding to next
 * character in prefix "*helloEveryoneOutThereSomewhere".  If 31, then
 * no branches available.
 * 
 * Parameters:
 * 
 *   pv - the decoding map structure
 * 
 *   c - the unsigned byte value of the branch to follow
 * 
 * Return:
 * 
 *   non-zero if branch available and followed; zero if no such branch
 *   and no change in state
 */
static int decmap_branch(DECMAP_STATE *pv, int c) {
  
  int branch = 0;
  int keylen = 0;
  
  /* Check parameters */
  if ((pv == NULL) || (c < 0) || (c > 255)) {
    abort();
  }
  
  /* Get length of current key */
  keylen = (int) strlen(&(pv->key[0]));
  
  /* Determine whether to branch */
  if (keylen > 0) {
    /* Current key not empty -- check first character to determine what
     * to do */
    if (pv->key[0] == '\\') {
      /* Backslash is first character -- check current key length */
      if (keylen == 1) {
        /* One character in backslash escape -- branches are available
         * for \&"'{}n:su and line feed */
        if ((c == '\\') || (c == '&') || (c == '"') || (c == '\'') ||
            (c == '{' ) || (c == '}') || (c == 'n') || (c == ':' ) ||
            (c == 's' ) || (c == 'u') || (c == 0xa)) {
          branch = 1;
        } else {
          branch = 0;
        }
        
      } else if (keylen == 2) {
        /* Two characters in backslash escape -- check second
         * character */
        if (pv->key[1] == ':') {
          /* Two-character backslash escape and second character is :,
           * so aAoOuU branches available */
          if ((c == 'a') || (c == 'A') ||
              (c == 'o') || (c == 'O') ||
              (c == 'u') || (c == 'U')) {
            branch = 1;
          } else {
            branch = 0;
          }
          
        } else if (pv->key[1] == 's') {
          /* Two-character backslash escape and second character is s,
           * so only an s branch is available */
          if (c == 's') {
            branch = 1;
          } else {
            branch = 0;
          }
          
        } else {
          /* Two-character backslash escape and second character is
           * neither colon nor s, so no further branches */
          branch = 0;
        }
        
      } else if (keylen == 3) {
        /* Three characters in backslash escape, so no further branch */
        branch = 0;
        
      } else {
        /* Shouldn't happen */
        abort();
      }
      
    } else if (pv->key[0] == '&') {
      /* Ampersand is first character -- check current key length */
      if (keylen == 1) {
        /* One-character ampersand key -- a and x branches available */
        if ((c == 'a') || (c == 'x')) {
          branch = 1;
        } else {
          branch = 0;
        }
        
      } else if (keylen == 2) {
        /* Two-character ampersand key -- m branch available if second
         * character is a */
        if ((pv->key[1] == 'a') && (c == 'm')) {
          branch = 1;
        } else {
          branch = 0;
        }
         
      } else if (keylen == 3) {
        /* Three-character ampersand key -- p branch available */
        if (c == 'p') {
          branch = 1;
        } else {
          branch = 0;
        }
      
      } else if (keylen == 4) {
        /* Four-character ampersand key -- ; branch available */
        if (c == ';') {
          branch = 1;
        } else {
          branch = 0;
        }
      
      } else if (keylen == 5) {
        /* Five-character ampersand key -- no branches available */
        branch = 0;
      
      } else {
        /* Shouldn't happen */
        abort();
      }
      
      
    } else if (pv->key[0] == '*') {
      /* Asterisk is first character -- check current key length */
      if (keylen == 1) {
        /* Current key length is 1, so branches for * and h */
        if ((c == '*') || (c == 'h')) {
          branch = 1;
        } else {
          branch = 0;
        }
        
      } else if (keylen == 2) {
        /* Two-character key, so branch for e if second character is
         * h */
        if ((pv->key[1] == 'h') && (c == 'e')) {
          branch = 1;
        } else {
          branch = 0;
        }
        
      } else if ((keylen >= 3) && (keylen <= 5)) {
        /* Current key length is 3-5, so check for available branch in
         * the common prefix key */
        branch = longkey_branch(m_pCommonKey, keylen, c);
        
      } else if (keylen == 6) {
        /* Six-character key, so branches for W and E available */
        if ((c == 'W') || (c == 'E')) {
          branch = 1;
        } else {
          branch = 0;
        }
        
      } else if ((keylen >= 7) && (keylen <= 11)) {
        /* Current key length is 7-11, so branches available for one of
         * the two long keys, depending on the seventh character */
        if (pv->key[6] == 'W') {
          /* Check for available branch in first long key */
          branch = longkey_branch(m_pLongKey1, keylen, c);
          
        } else if (pv->key[6] == 'E') {
          /* Check for available branch in second long key */
          branch = longkey_branch(m_pLongKey2, keylen, c);
          
        } else {
          /* Shouldn't happen */
          abort();
        }
        
      } else if ((keylen >= 12) && (keylen <= 30)) {
        /* Current key length is 12-30, so check for available branch in
         * the second long key */
        branch = longkey_branch(m_pLongKey2, keylen, c);
      
      } else if (keylen == 31) {
        /* Current key length is 31, so no branches available */
        branch = 0;
      
      } else {
        /* Shouldn't happen */
        abort();
      }
      
    } else {
      /* First character neither backslash nor ampersand nor asterisk,
       * so no branch available */
      branch = 0;
    }
    
  } else {
    /* Current key is empty -- branch if provided character in printing
     * US-ASCII range or space or line feed */
    if (((c >= 0x20) && (c <= 0x7e)) || (c == 0xa)) {
      branch = 1;
    } else {
      branch = 0;
    }
  }
  
  /* If branching, then append character to key */
  if (branch) {
    pv->key[keylen] = (unsigned char) c;
  }
  
  /* Return whether or not a branch was performed */
  return branch;
}

/*
 * Implementation of the raw input callback.
 * 
 * Reads the next byte of input from standard input, or SHASM_INPUT_EOF
 * for EOF, or SHASM_INPUT_IOERR for I/O error.
 * 
 * The custom pass-through parameter is not used.
 * 
 * Parameters:
 * 
 *   pCustom - (ignored)
 * 
 * Return:
 * 
 *   the unsigned value of the next byte of input (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int rawInput(void *pCustom) {
  int c = 0;
  
  /* Ignore parameter */
  (void) pCustom;
  
  /* Read next byte from standard input */
  c = getchar();
  if (c == EOF) {
    if (feof(stdin)) {
      c = SHASM_INPUT_EOF;
    } else {
      c = SHASM_INPUT_IOERR;
    }
  }
  
  /* Return result */
  return c;
}

/*
 * Use the block reader to read one or more tokens from standard input,
 * ending with the "|;" token.
 * 
 * For each token, report the line number and the token data.
 * 
 * If an error occurs, this function will report the specifics.
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int test_token(void) {
  
  int status = 1;
  long lx = 0;
  int errcode = 0;
  unsigned char *ptk = NULL;
  SHASM_IFLSTATE *ps = NULL;
  SHASM_BLOCK *pb = NULL;
  
  /* Allocate a new input filter chain on standard input */
  ps = shasm_input_alloc(&rawInput, NULL);
  
  /* Allocate a new block reader */
  pb = shasm_block_alloc();
  
  /* Read all the tokens */
  while (status) {
    
    /* Read the next token */
    if (!shasm_block_token(pb, ps)) {
      status = 0;
      errcode = shasm_block_status(pb, &lx);
      if (lx != LONG_MAX) {
        fprintf(stderr, "Error %d at line %ld!\n", errcode, lx);
      } else {
        fprintf(stderr, "Error %d at unknown line!\n", errcode);
      }
    }
    
    /* Get the token and line number */
    if (status) {
      ptk = shasm_block_ptr(pb, 1);
      if (ptk == NULL) {
        abort();  /* shouldn't happen as tokens never have null bytes */
      }
      lx = shasm_block_line(pb);
    }
    
    /* Report the token */
    if (status) {
      if (lx != LONG_MAX) {
        printf("@%ld: %s\n", lx, ptk);
      } else {
        printf("@???: %s\n", ptk);
      }
    }
    
    /* Break out of loop if token is "|;" */
    if (status) {
      if (strcmp(ptk, "|;") == 0) {
        break;
      }
    }
  }
  
  /* Free the block reader */
  shasm_block_free(pb);
  pb = NULL;
  
  /* Free the input filter chain */
  shasm_input_free(ps);
  ps = NULL;
  
  /* Return status */
  return status;
}

/*
 * Program entrypoint.
 * 
 * See the documentation at the top of this source file for a
 * description of how this testing program works.
 * 
 * The main function parses the command line and invokes the appropriate
 * testing function.
 * 
 * Parameters:
 * 
 *   argc - the number of elements in argv
 * 
 *   argv - the program arguments, where the first argument is the
 *   module name and any arguments beyond that are parameters passed on
 *   the command line; each argument must be null terminated
 * 
 * Return:
 * 
 *   zero if successful, one if error
 */
int main(int argc, char *argv[]) {
  
  int status = 1;
  
  /* Fail if less than one extra argument */
  if (argc < 2) {
    status = 0;
    fprintf(stderr,
      "Expecting a program argument choosing the testing mode!\n");
  }
  
  /* Verify that extra argument is present */
  if (status) {
    if (argv == NULL) {
      abort();
    }
    if (argv[1] == NULL) {
      abort();
    }
  }
  
  /* Invoke the appropriate testing function based on the command
   * line */
  if (status) {
    if (strcmp(argv[1], "token") == 0) {
      /* Token mode -- make sure no additional parameters */
      if (argc != 2) {
        status = 0;
        fprintf(stderr, "Too many parameters for token mode!\n");
      }
      
      /* Invoke token mode */
      if (status) {
        if (!test_token()) {
          status = 0;
        }
      }
      
    } else {
      /* Unrecognized mode */
      status = 0;
      fprintf(stderr, "Unrecognized testing mode!\n");
    }
  }
  
  /* Invert status */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  
  /* Return inverted status */
  return status;
}
