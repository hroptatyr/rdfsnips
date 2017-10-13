/**************************** FNV256.h **************************/
/***************** See RFC NNNN for details. ********************/
/*
 * Copyright (c) 2016 IETF Trust and the persons identified as
 * authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

#ifndef _FNV256_H_
#define _FNV256_H_

/*
 *  Description:
 *      This file provides headers for the 256-bit version of the
 *      FNV-1a non-cryptographic hash algorithm.
 */
#define FNV_64bitIntegers

#include <stdint.h>
#define FNV256size (256/8)

/* If you do not have the ISO standard stdint.h header file, then you
 * must typedef the following types:
 *
 *    type              meaning
 *  uint64_t         unsigned 64 bit integer (ifdef FNV_64bitIntegers)
 *  uint32_t         unsigned 32 bit integer
 *  uint16_t         unsigned 16 bit integer
 *  uint8_t          unsigned 8 bit integer (i.e., unsigned char)
 */

#ifndef _FNV_ErrCodes_
#define _FNV_ErrCodes_
/*********************************************************************
 *  All FNV functions provided return as integer as follows:
 *       0 -> success
 *      >0 -> error as listed below
 */
enum {    /* success and errors */
    fnvSuccess = 0,
    fnvNull,            /* Null pointer parameter */
    fnvStateError,      /* called Input after Result or before Init */
    fnvBadParam         /* passed a bad parameter */
};
#endif /* _FNV_ErrCodes_ */

/*
 *  This structure holds context information for an FNV256 hash
 */
#ifdef FNV_64bitIntegers
    /* version if 64 bit integers supported */
typedef struct FNV256context_s {
        int Computed;  /* state */
        uint32_t Hash[FNV256size/4];
} FNV256context;

#else
    /* version if 64 bit integers NOT supported */

typedef struct FNV256context_s {
        int Computed;  /* state */
        uint16_t Hash[FNV256size/2];
} FNV256context;

#endif /* FNV_64bitIntegers */


/*
 *  Function Prototypes
 *    FNV256string: hash a zero terminated string not including
 *                  the zero
 *    FNV256block: FNV256 hash a specified length byte vector
 *    FNV256init: initializes an FNV256 context
 *    FNV256initBasis:  initializes an FNV256 context with a provided
 *                     basis
 *    FNV256blockin: hash in a specified length byte vector
 *    FNV256stringin: hash in a zero terminated string not
 *                    including the zero
 *    FNV256result: returns the hash value
 *
 *    Hash is returned as an array of 8-bit integers
 */

#ifdef __cplusplus
extern "C" {
#endif

/* FNV256 */
extern int FNV256string ( const char *in,
                          uint8_t out[FNV256size] );
extern int FNV256block ( const void *in,
                         long int length,
                         uint8_t out[FNV256size] );
extern int FNV256init ( FNV256context *);
extern int FNV256initBasis ( FNV256context * const,
                             const uint8_t basis[FNV256size] );
extern int FNV256blockin ( FNV256context * const,
                           const void *in,
                           long int length );
extern int FNV256stringin ( FNV256context * const,
                            const char *in );
extern int FNV256result ( FNV256context * const,
                          uint8_t out[FNV256size] );

#ifdef __cplusplus
}
#endif

#endif /* _FNV256_H_ */
