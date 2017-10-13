/***************************** FNV256.c ****************************/
/******************** See RFC NNNN for details *********************/
/* Copyright (c) 2016 IETF Trust and the persons identified as
 * authors of the code.  All rights
 * See fnv-private.h for terms of use and redistribution.
 */

/* This file implements the FNV (Fowler, Noll, Vo) non-cryptographic
 * hash function FNV-1a for 256-bit hashes.
 */

#ifndef _FNV256_C_
#define _FNV256_C_

#include "fnv.h"

/*
 *      Six FNV-1a hashes are defined with these sizes:
 *              FNV32          32 bits, 4 bytes
 *              FNV64          64 bits, 8 bytes
 *              FNV128         128 bits, 16 bytes
 *              FNV256         256 bits, 32 bytes
 *              FNV512         512 bits, 64 bytes
 *              FNV1024        1024 bits, 128 bytes
 */

/* Private stuff used by this implementation of the FNV
 * (Fowler, Noll, Vo) non-cryptographic hash function FNV-1a.
 * External callers don't need to know any of this.  */

enum {  /* State value bases for context->Computed */
    FNVinited = 22,
    FNVcomputed = 76,
    FNVemptied = 220,
    FNVclobber = 122 /* known bad value for testing */
};

/* Deltas to assure distinct state values for different lengths */
enum {
   FNV32state = 1,
   FNV32Bstate = 17,
   FNV64state = 3,
   FNV64Bstate = 19,
   FNV128state = 5,
   FNV256state = 7,
   FNV512state = 11,
   FNV1024state = 13
};


/* common code for 64 and 32 bit modes */

/* FNV256 hash a null terminated string  (64/32 bit)
 ********************************************************************/
int FNV256string ( const char *in, uint8_t out[FNV256size] )
{
FNV256context    ctx;
int              err;

if ( (err = FNV256init ( &ctx )) != fnvSuccess )
    return err;
if ( (err = FNV256stringin ( &ctx, in )) != fnvSuccess )
    return err;
return FNV256result ( &ctx, out );
}   /* end FNV256string */

/* FNV256 hash a counted block  (64/32 bit)
 ********************************************************************/
int FNV256block ( const void *in,
                  long int length,
                  uint8_t out[FNV256size] )
{
FNV256context    ctx;
int              err;

if ( (err = FNV256init ( &ctx )) != fnvSuccess )
    return err;
if ( (err = FNV256blockin ( &ctx, in, length)) != fnvSuccess )
    return err;
return FNV256result ( &ctx, out );
}   /* end FNV256block */


/********************************************************************
 *        START VERSION FOR WHEN YOU HAVE 64 BIT ARITHMETIC         *
 ********************************************************************/
#ifdef FNV_64bitIntegers
/* 256 bit FNV_prime = 2^168 + 2^8 + 0x63 */
/* 0x0000000000000000 0000010000000000
     0000000000000000 0000000000000163 */
#define FNV256primeX 0x0163
#define FNV256shift 8

/* 0xDD268DBCAAC55036 2D98C384C4E576CC
     C8B1536847B6BBB3 1023B4C8CAEE0535 */
uint32_t FNV256basis[FNV256size/4] = {
         0xDD268DBC, 0xAAC55036, 0x2D98C384, 0xC4E576CC,
         0xC8B15368, 0x47B6BBB3, 0x1023B4C8, 0xCAEE0535 };

/********************************************************************
 *         Set of init, input, and output functions below           *
 *         to incrementally compute FNV256                          *
 ********************************************************************/

/* initialize context  (64 bit)
 ********************************************************************/
int FNV256init ( FNV256context *ctx )
{
int      i;

if ( ctx )
    {
    for ( i=0; i<FNV256size/4; ++i )
        ctx->Hash[i] = FNV256basis[i];
    ctx->Computed = FNVinited+FNV256state;
    return fnvSuccess;
    }
return fnvNull;
}   /* end FNV256init */

/* initialize context with a provided basis  (64 bit)
 ********************************************************************/
int FNV256initBasis ( FNV256context* const ctx,
                      const uint8_t basis[FNV256size] )
{
int      i;
const uint8_t  *ui8p;
uint32_t  temp;

if ( ctx )
    {
#ifndef __BYTE_ORDER__
# error no byte order defined
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    ui8p = basis;
    for ( i=0; i < FNV256size/4; ++i )
        {
            temp = (*ui8p++)<<8;
            temp = (temp + *ui8p++)<<8;
            temp = (temp + *ui8p++)<<8;
        ctx->Hash[i] = temp +  *ui8p;
        }
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    ui8p = basis + (FNV256size/4 - 1);
    for ( i=0; i < FNV256size/4; ++i )
        {
            temp = (*ui8p--)<<8;
            temp = (temp + *ui8p--)<<8;
            temp = (temp + *ui8p--)<<8;
        ctx->Hash[i] = temp +  *ui8p;
        }
#else
# error no byte order defined
#endif
    ctx->Computed = FNVinited+FNV256state;
    return fnvSuccess;
    }
return fnvNull;
}   /* end FNV256initBasis */

/* hash in a counted block  (64 bit)
 ********************************************************************/
int FNV256blockin ( FNV256context *ctx,
                    const void *vin,
                    long int length )
{
const uint8_t *in = (const uint8_t*)vin;
uint64_t    temp[FNV256size/4];
uint64_t    temp2[3];
int         i;

if ( ctx && in )
    {
    if ( length < 0 )
        return fnvBadParam;
    switch ( ctx->Computed )
        {
        case FNVinited+FNV256state:
            ctx->Computed = FNVcomputed+FNV256state;
        case FNVcomputed+FNV256state:
            break;
        default:
            return fnvStateError;
        }
    for ( i=0; i<FNV256size/4; ++i )
         temp[i] = ctx->Hash[i];
    for ( ; length > 0; length-- )
        {
        /* temp = FNV256prime * ( temp ^ *in++ ); */
        temp[7] ^= *in++;
        temp2[2] = temp[7] << FNV256shift;
        temp2[1] = temp[6] << FNV256shift;
        temp2[0] = temp[5] << FNV256shift;
        for ( i=0; i<FNV256size/4; ++i )
            temp[i] *= FNV256primeX;
        temp[2] += temp2[2];
        temp[1] += temp2[1];
        temp[0] += temp2[0];
        for ( i=FNV256size/4-1; i>0; --i )
            {
            temp[i-1] += temp[i] >> 16;
            temp[i] &= 0xFFFF;
            }
       }
    for ( i=0; i<FNV256size/4; ++i )
        ctx->Hash[i] = temp[i];
    return fnvSuccess;
    }
return fnvNull;
}   /* end FNV256input */

/* hash in a string  (64 bit)
 ********************************************************************/
int FNV256stringin ( FNV256context *ctx, const char *in )
{
uint64_t   temp[FNV256size/4];
uint64_t   temp2[3];
int        i;
uint8_t    ch;

if ( ctx && in )
    {
    switch ( ctx->Computed )
        {
        case FNVinited+FNV256state:
            ctx->Computed = FNVcomputed+FNV256state;
        case FNVcomputed+FNV256state:
            break;
        default:
             return fnvStateError;
         }
    for ( i=0; i<FNV256size/4; ++i )
         temp[i] = ctx->Hash[i];
    while ( (ch = (uint8_t)*in++) )
        {
        /* temp = FNV256prime * ( temp ^ ch ); */
        temp[7] ^= ch;
        temp2[2] = temp[7] << FNV256shift;
        temp2[1] = temp[6] << FNV256shift;
        temp2[0] = temp[5] << FNV256shift;
        for ( i=0; i<FNV256size/4; ++i )
            temp[i] *= FNV256primeX;
        temp[2] += temp2[2];
        temp[1] += temp2[1];
        temp[0] += temp2[0];
        for ( i=FNV256size/4-1; i>0; --i )
            {
            temp[i-1] += temp[i] >> 16;
            temp[i] &= 0xFFFF;
            }
       }
    for ( i=0; i<FNV256size/4; ++i )
        ctx->Hash[i] = temp[i];
    return fnvSuccess;
    }
return fnvNull;
}   /* end FNV256stringin */

/* return hash  (64 bit)
 ********************************************************************/
int FNV256result ( FNV256context *ctx, uint8_t out[FNV256size] )
{
int     i;

if ( ctx && out )
    {
    if ( ctx->Computed != FNVcomputed+FNV256state )
        return fnvStateError;
    for ( i=0; i<FNV256size/4; ++i )
        {
#ifndef __BYTE_ORDER__
# error no byte order defined
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        out[31-4*i] = (uint8_t)(ctx->Hash[i]);
        out[31-4*i] = (uint8_t)(ctx->Hash[i] >> 8);
        out[31-4*i] = (uint8_t)(ctx->Hash[i] >> 16);
        out[31-4*i] = (uint8_t)(ctx->Hash[i] >> 24);
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        out[4*i] = (uint8_t)(ctx->Hash[i]);
        out[4*i+1] = (uint8_t)(ctx->Hash[i] >> 8);
        out[4*i+2] = (uint8_t)(ctx->Hash[i] >> 16);
	out[4*i+3] = (uint8_t)(ctx->Hash[i] >> 24);
#else
# error unknown endianness
#endif
        ctx -> Hash[i] = 0;
        }
    ctx->Computed = FNVemptied+FNV256state;
    return fnvSuccess;
    }
return fnvNull;
}   /* end FNV256result */

/******************************************************************
 *        END VERSION FOR WHEN YOU HAVE 64 BIT ARITHMETIC         *
 ******************************************************************/
#else    /*  FNV_64bitIntegers */
/******************************************************************
 *      START VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC    *
 ******************************************************************/

/* version for when you only have 32-bit arithmetic
 ********************************************************************/

/* 256 bit FNV_prime = 2^168 + 2^8 + 0x63 */
/* 0x00000000 00000000 00000100 00000000
     00000000 00000000 00000000 00000163 */
#define FNV256primeX 0x0163
#define FNV256shift 8

/* 0xDD268DBCAAC55036 2D98C384C4E576CC
     C8B1536847B6BBB3 1023B4C8CAEE0535 */
uint16_t FNV256basis[FNV256size/2] = {
         0xDD26, 0x8DBC, 0xAAC5, 0x5036,
         0x2D98, 0xC384, 0xC4E5, 0x76CC,
         0xC8B1, 0x5368, 0x47B6, 0xBBB3,
         0x1023, 0xB4C8, 0xCAEE, 0x0535 };

/********************************************************************
 *         Set of init, input, and output functions below           *
 *         to incrementally compute FNV256                          *
 ********************************************************************/

/* initialize context  (32 bit)
 ********************************************************************/
int FNV256init ( FNV256context *ctx )
{
int     i;

if ( ctx )
    {
    for ( i=0; i<FNV256size/2; ++i )
        ctx->Hash[i] = FNV256basis[i];
    ctx->Computed = FNVinited+FNV256state;
    return fnvSuccess;
    }
return fnvNull;
}   /* end FNV256init */

/* initialize context with a provided basis  (32 bit)
 ********************************************************************/
int FNV256initBasis ( FNV256context *ctx,
                      const uint8_t basis[FNV256size] )
{
int      i;
const uint8_t  *ui8p;
uint32_t temp;

if ( ctx )
    {
#ifndef __BYTE_ORDER__
# error no byte order defined
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    ui8p = basis;
    for ( i=0; i < FNV256size/2; ++i )
        {
        temp = *ui8p++;
        ctx->Hash[i] = ( temp<<8 ) + (*ui8p++);
        }
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    ui8p = basis + FNV256size/2 -1;
    for ( i=0; i < FNV256size/2; ++i )
        {
        temp = *ui8p--;
        ctx->Hash[i] = ( temp<<8 ) +  (*ui8p--);
        }
#else
# error no byte order defined
#endif
    ctx->Computed = FNVinited+FNV256state;
    return fnvSuccess;
    }
return fnvNull;
}   /* end FNV256initBasis */

/* hash in a counted block  (32 bit)
 *******************************************************************/
int FNV256blockin ( FNV256context *ctx,
                    const void *vin,
                    long int length )
{
const uint8_t *in = (const uint8_t*)vin;
uint32_t   temp[FNV256size/2];
uint32_t   temp2[6];
int        i;

if ( ctx && in )
    {
    if ( length < 0 )
        return fnvBadParam;
    switch ( ctx->Computed )
        {
        case FNVinited+FNV256state:
            ctx->Computed = FNVcomputed+FNV256state;
        case FNVcomputed+FNV256state:
            break;
        default:
            return fnvStateError;
        }
    for ( i=0; i<FNV256size/2; ++i )
        temp[i] = ctx->Hash[i];
    for ( ; length > 0; length-- )
        {
        /* temp = FNV256prime * ( temp ^ *in++ ); */
        temp[15] ^= *in++;
        for ( i=0; i<6; ++i )
            temp2[i] = temp[10+i] << FNV256shift;
        for ( i=0; i<FNV256size/2; ++i )
            temp[i] *= FNV256primeX;
        for ( i=0; i<6; ++i )
            temp[10+i] += temp2[i];
        for ( i=15; i>0; --i )
            {
            temp[i-1] += temp[i] >> 16;
            temp[i] &= 0xFFFF;
            }
        }
    for ( i=0; i<FNV256size/2; ++i )
        ctx->Hash[i] = temp[i];
    return fnvSuccess;
    }
return fnvNull;
}   /* end FNV256blockin */

/* hash in a string  (32 bit)
 *******************************************************************/
int FNV256stringin ( FNV256context *ctx, const char *in )
{
uint32_t   temp[FNV256size/2];
uint32_t   temp2[6];
int        i;
uint8_t    ch;

if ( ctx && in )
    {
    switch ( ctx->Computed )
        {
        case FNVinited+FNV256state:
            ctx->Computed = FNVcomputed+FNV256state;
        case FNVcomputed+FNV256state:
            break;
        default:
             return fnvStateError;
         }
    for ( i=0; i<FNV256size/2; ++i )
         temp[i] = ctx->Hash[i];
    while ( ( ch = (uint8_t)*in++ ) != 0)
        {
        /* temp = FNV256prime * ( temp ^ *in++ ); */
        temp[15] ^= ch;
        for ( i=0; i<6; ++i )
            temp2[i] = temp[10+i] << FNV256shift;
        for ( i=0; i<FNV256size/2; ++i )
            temp[i] *= FNV256primeX;
        for ( i=0; i<6; ++i )
            temp[10+i] += temp2[i];
        for ( i=15; i>0; --i )
            {
            temp[i-1] += temp[i] >> 16;
            temp[i] &= 0xFFFF;
            }
         }
    for ( i=0; i<FNV256size/2; ++i )
        ctx->Hash[i] = temp[i];
    return fnvSuccess;
    }
return fnvNull;
}   /* end FNV256stringin */

/* return hash  (32 bit)
 ********************************************************************/
int FNV256result ( FNV256context *ctx, uint8_t out[FNV256size] )
{
int    i;

if ( ctx && out )
    {
    if ( ctx->Computed != FNVcomputed+FNV256state )
        return fnvStateError;
    for ( i=0; i<FNV256size/2; ++i )
        {
#ifndef __BYTE_ORDER__
# error no byte order defined
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        out[31-2*i] = ctx->Hash[i];
        out[30-2*i] = ctx->Hash[i] >> 8;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        out[2*i] = ctx->Hash[i];
        out[2*i+1] = ctx->Hash[i] >> 8;
#else
# error no byte order defined
#endif
        ctx->Hash[i] = 0;
        }
    ctx->Computed = FNVemptied+FNV256state;
    return fnvSuccess;
    }
return fnvNull;
}   /* end FNV256result */

#endif    /*  Have64bitIntegers */
/********************************************************************
 *        END VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC      *
 ********************************************************************/

#endif    /* _FNV256_C_ */
