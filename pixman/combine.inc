#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "pixman-private.h"

/*
 * Helper macros.
 */

#define FbGen(x,y,i,ax,ay,t,u,v) ((t) = (FbIntMult(FbGet8(y,i),ay,(u)) + \
					 FbIntMult(FbGet8(x,i),ax,(v))), \
				  (uint32_t) ((uint8_t) ((t) |		\
							 (0 - ((t) >> 8)))) << (i))


/*
  The methods below use some tricks to be able to do two color
  components at the same time.
*/

/*
  x_c = (x_c * a) / 255
*/
#define FbByteMul(x, a) do {					    \
        uint32_t t = ((x & 0xff00ff) * a) + 0x800080;               \
        t = (t + ((t >> 8) & 0xff00ff)) >> 8;			    \
        t &= 0xff00ff;						    \
								    \
        x = (((x >> 8) & 0xff00ff) * a) + 0x800080;		    \
        x = (x + ((x >> 8) & 0xff00ff));			    \
        x &= 0xff00ff00;					    \
        x += t;							    \
    } while (0)

/*
  x_c = (x_c * a) / 255 + y
*/
#define FbByteMulAdd(x, a, y) do {				    \
        uint32_t t = ((x & 0xff00ff) * a) + 0x800080;               \
        t = (t + ((t >> 8) & 0xff00ff)) >> 8;			    \
        t &= 0xff00ff;						    \
        t += y & 0xff00ff;					    \
        t |= 0x1000100 - ((t >> 8) & 0xff00ff);			    \
        t &= 0xff00ff;						    \
								    \
        x = (((x >> 8) & 0xff00ff) * a) + 0x800080;                 \
        x = (x + ((x >> 8) & 0xff00ff)) >> 8;                       \
        x &= 0xff00ff;                                              \
        x += (y >> 8) & 0xff00ff;                                   \
        x |= 0x1000100 - ((x >> 8) & 0xff00ff);                     \
        x &= 0xff00ff;                                              \
        x <<= 8;                                                    \
        x += t;                                                     \
    } while (0)

/*
  x_c = (x_c * a + y_c * b) / 255
*/
#define FbByteAddMul(x, a, y, b) do {                                   \
        uint32_t t;							\
        uint32_t r = (x >> 24) * a + (y >> 24) * b + 0x80;		\
        r += (r >> 8);                                                  \
        r >>= 8;                                                        \
									\
        t = (x & 0xff00) * a + (y & 0xff00) * b;                        \
        t += (t >> 8) + 0x8000;                                         \
        t >>= 16;                                                       \
									\
        t |= r << 16;                                                   \
        t |= 0x1000100 - ((t >> 8) & 0xff00ff);                         \
        t &= 0xff00ff;                                                  \
        t <<= 8;                                                        \
									\
        r = ((x >> 16) & 0xff) * a + ((y >> 16) & 0xff) * b + 0x80;     \
        r += (r >> 8);                                                  \
        r >>= 8;                                                        \
									\
        x = (x & 0xff) * a + (y & 0xff) * b + 0x80;                     \
        x += (x >> 8);                                                  \
        x >>= 8;                                                        \
        x |= r << 16;                                                   \
        x |= 0x1000100 - ((x >> 8) & 0xff00ff);                         \
        x &= 0xff00ff;                                                  \
        x |= t;                                                         \
    } while (0)

/*
  x_c = (x_c * a + y_c *b) / 256
*/
#define FbByteAddMul_256(x, a, y, b) do {                               \
        uint32_t t = (x & 0xff00ff) * a + (y & 0xff00ff) * b;		\
        t >>= 8;                                                        \
        t &= 0xff00ff;                                                  \
									\
        x = ((x >> 8) & 0xff00ff) * a + ((y >> 8) & 0xff00ff) * b;      \
        x &= 0xff00ff00;                                                \
        x += t;                                                         \
    } while (0)

/*
  x_c = (x_c * a_c) / 255
*/
#define FbByteMulC(x, a) do {				  \
        uint32_t t;                                       \
        uint32_t r = (x & 0xff) * (a & 0xff);             \
        r |= (x & 0xff0000) * ((a >> 16) & 0xff);	  \
	r += 0x800080;					  \
        r = (r + ((r >> 8) & 0xff00ff)) >> 8;		  \
        r &= 0xff00ff;					  \
							  \
        x >>= 8;					  \
        t = (x & 0xff) * ((a >> 8) & 0xff);		  \
        t |= (x & 0xff0000) * (a >> 24);		  \
        t += 0x800080;					  \
        t = t + ((t >> 8) & 0xff00ff);			  \
        x = r | (t & 0xff00ff00);			  \
							  \
    } while (0)

/*
  x_c = (x_c * a) / 255 + y
*/
#define FbByteMulAddC(x, a, y) do {				      \
        uint32_t t;                                                   \
        uint32_t r = (x & 0xff) * (a & 0xff);                         \
        r |= (x & 0xff0000) * ((a >> 16) & 0xff);		      \
	r += 0x800080;						      \
	r = (r + ((r >> 8) & 0xff00ff)) >> 8;			      \
        r &= 0xff00ff;						      \
        r += y & 0xff00ff;					      \
        r |= 0x1000100 - ((r >> 8) & 0xff00ff);			      \
        r &= 0xff00ff;						      \
								      \
        x >>= 8;                                                       \
        t = (x & 0xff) * ((a >> 8) & 0xff);                            \
        t |= (x & 0xff0000) * (a >> 24);                               \
	t += 0x800080;                                                 \
        t = (t + ((t >> 8) & 0xff00ff)) >> 8;			       \
        t &= 0xff00ff;                                                 \
        t += (y >> 8) & 0xff00ff;                                      \
        t |= 0x1000100 - ((t >> 8) & 0xff00ff);                        \
        t &= 0xff00ff;                                                 \
        x = r | (t << 8);                                              \
    } while (0)

/*
  x_c = (x_c * a_c + y_c * b) / 255
*/
#define FbByteAddMulC(x, a, y, b) do {                                  \
        uint32_t t;							\
        uint32_t r = (x >> 24) * (a >> 24) + (y >> 24) * b;		\
        r += (r >> 8) + 0x80;                                           \
        r >>= 8;                                                        \
									\
        t = (x & 0xff00) * ((a >> 8) & 0xff) + (y & 0xff00) * b;        \
        t += (t >> 8) + 0x8000;                                         \
        t >>= 16;                                                       \
									\
        t |= r << 16;                                                   \
        t |= 0x1000100 - ((t >> 8) & 0xff00ff);                         \
        t &= 0xff00ff;                                                  \
        t <<= 8;                                                        \
									\
        r = ((x >> 16) & 0xff) * ((a >> 16) & 0xff) + ((y >> 16) & 0xff) * b + 0x80; \
        r += (r >> 8);                                                  \
        r >>= 8;                                                        \
									\
        x = (x & 0xff) * (a & 0xff) + (y & 0xff) * b + 0x80;            \
        x += (x >> 8);                                                  \
        x >>= 8;                                                        \
        x |= r << 16;                                                   \
        x |= 0x1000100 - ((x >> 8) & 0xff00ff);                         \
        x &= 0xff00ff;                                                  \
        x |= t;                                                         \
    } while (0)

/*
  x_c = min(x_c + y_c, 255)
*/
#define FbByteAdd(x, y) do {                                            \
        uint32_t t;							\
        uint32_t r = (x & 0xff00ff) + (y & 0xff00ff);			\
        r |= 0x1000100 - ((r >> 8) & 0xff00ff);                         \
        r &= 0xff00ff;                                                  \
									\
        t = ((x >> 8) & 0xff00ff) + ((y >> 8) & 0xff00ff);              \
        t |= 0x1000100 - ((t >> 8) & 0xff00ff);                         \
        r |= (t & 0xff00ff) << 8;                                       \
        x = r;                                                          \
    } while (0)


/*
 * There are two ways of handling alpha -- either as a single unified value or
 * a separate value for each component, hence each macro must have two
 * versions.  The unified alpha version has a 'U' at the end of the name,
 * the component version has a 'C'.  Similarly, functions which deal with
 * this difference will have two versions using the same convention.
 */


/*
 * Combine src and mask
 */
FASTCALL static void
pixman_fbCombineMaskU (uint32_t *src, const uint32_t *mask, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t a = *(mask + i) >> 24;
        uint32_t s = *(src + i);
        FbByteMul(s, a);
        *(src + i) = s;
    }
}

/*
 * All of the composing functions
 */

FASTCALL static void
fbCombineClear (uint32_t *dest, const uint32_t *src, int width)
{
    memset(dest, 0, width*sizeof(uint32_t));
}

FASTCALL static void
fbCombineSrcU (uint32_t *dest, const uint32_t *src, int width)
{
    memcpy(dest, src, width*sizeof(uint32_t));
}

/* if the Src is opaque, call fbCombineSrcU */
FASTCALL static void
fbCombineOverU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t ia = Alpha(~s);

        FbByteMulAdd(d, ia, s);
	*(dest + i) = d;
    }
}

/* if the Dst is opaque, this is a noop */
FASTCALL static void
fbCombineOverReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t ia = Alpha(~*(dest + i));
        FbByteMulAdd(s, ia, d);
	*(dest + i) = s;
    }
}

/* if the Dst is opaque, call fbCombineSrcU */
FASTCALL static void
fbCombineInU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t a = Alpha(*(dest + i));
        FbByteMul(s, a);
	*(dest + i) = s;
    }
}

/* if the Src is opaque, this is a noop */
FASTCALL static void
fbCombineInReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint32_t a = Alpha(*(src + i));
        FbByteMul(d, a);
	*(dest + i) = d;
    }
}

/* if the Dst is opaque, call fbCombineClear */
FASTCALL static void
fbCombineOutU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t a = Alpha(~*(dest + i));
        FbByteMul(s, a);
	*(dest + i) = s;
    }
}

/* if the Src is opaque, call fbCombineClear */
FASTCALL static void
fbCombineOutReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint32_t a = Alpha(~*(src + i));
        FbByteMul(d, a);
	*(dest + i) = d;
    }
}

/* if the Src is opaque, call fbCombineInU */
/* if the Dst is opaque, call fbCombineOverU */
/* if both the Src and Dst are opaque, call fbCombineSrcU */
FASTCALL static void
fbCombineAtopU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t dest_a = Alpha(d);
        uint32_t src_ia = Alpha(~s);

        FbByteAddMul(s, dest_a, d, src_ia);
	*(dest + i) = s;
    }
}

/* if the Src is opaque, call fbCombineOverReverseU */
/* if the Dst is opaque, call fbCombineInReverseU */
/* if both the Src and Dst are opaque, call fbCombineDstU */
FASTCALL static void
fbCombineAtopReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t src_a = Alpha(s);
        uint32_t dest_ia = Alpha(~d);

        FbByteAddMul(s, dest_ia, d, src_a);
	*(dest + i) = s;
    }
}

/* if the Src is opaque, call fbCombineOverU */
/* if the Dst is opaque, call fbCombineOverReverseU */
/* if both the Src and Dst are opaque, call fbCombineClear */
FASTCALL static void
fbCombineXorU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t src_ia = Alpha(~s);
        uint32_t dest_ia = Alpha(~d);

        FbByteAddMul(s, dest_ia, d, src_ia);
	*(dest + i) = s;
    }
}

FASTCALL static void
fbCombineAddU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        FbByteAdd(d, s);
	*(dest + i) = d;
    }
}

/* if the Src is opaque, call fbCombineAddU */
/* if the Dst is opaque, call fbCombineAddU */
/* if both the Src and Dst are opaque, call fbCombineAddU */
FASTCALL static void
fbCombineSaturateU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t  s = *(src + i);
        uint32_t d = *(dest + i);
        uint16_t  sa, da;

        sa = s >> 24;
        da = ~d >> 24;
        if (sa > da)
        {
            sa = FbIntDiv(da, sa);
            FbByteMul(s, sa);
        };
        FbByteAdd(d, s);
	*(dest + i) = d;
    }
}


/*
 * All of the disjoint composing functions

 The four entries in the first column indicate what source contributions
 come from each of the four areas of the picture -- areas covered by neither
 A nor B, areas covered only by A, areas covered only by B and finally
 areas covered by both A and B.

 Disjoint			Conjoint
 Fa		Fb		Fa		Fb
 (0,0,0,0)	0		0		0		0
 (0,A,0,A)	1		0		1		0
 (0,0,B,B)	0		1		0		1
 (0,A,B,A)	1		min((1-a)/b,1)	1		max(1-a/b,0)
 (0,A,B,B)	min((1-b)/a,1)	1		max(1-b/a,0)	1
 (0,0,0,A)	max(1-(1-b)/a,0) 0		min(1,b/a)	0
 (0,0,0,B)	0		max(1-(1-a)/b,0) 0		min(a/b,1)
 (0,A,0,0)	min(1,(1-b)/a)	0		max(1-b/a,0)	0
 (0,0,B,0)	0		min(1,(1-a)/b)	0		max(1-a/b,0)
 (0,0,B,A)	max(1-(1-b)/a,0) min(1,(1-a)/b)	 min(1,b/a)	max(1-a/b,0)
 (0,A,0,B)	min(1,(1-b)/a)	max(1-(1-a)/b,0) max(1-b/a,0)	min(1,a/b)
 (0,A,B,0)	min(1,(1-b)/a)	min(1,(1-a)/b)	max(1-b/a,0)	max(1-a/b,0)

*/

#define CombineAOut 1
#define CombineAIn  2
#define CombineBOut 4
#define CombineBIn  8

#define CombineClear	0
#define CombineA	(CombineAOut|CombineAIn)
#define CombineB	(CombineBOut|CombineBIn)
#define CombineAOver	(CombineAOut|CombineBOut|CombineAIn)
#define CombineBOver	(CombineAOut|CombineBOut|CombineBIn)
#define CombineAAtop	(CombineBOut|CombineAIn)
#define CombineBAtop	(CombineAOut|CombineBIn)
#define CombineXor	(CombineAOut|CombineBOut)

/* portion covered by a but not b */
FASTCALL static uint8_t
fbCombineDisjointOutPart (uint8_t a, uint8_t b)
{
    /* min (1, (1-b) / a) */

    b = ~b;		    /* 1 - b */
    if (b >= a)		    /* 1 - b >= a -> (1-b)/a >= 1 */
	return 0xff;	    /* 1 */
    return FbIntDiv(b,a);   /* (1-b) / a */
}

/* portion covered by both a and b */
FASTCALL static uint8_t
fbCombineDisjointInPart (uint8_t a, uint8_t b)
{
    /* max (1-(1-b)/a,0) */
    /*  = - min ((1-b)/a - 1, 0) */
    /*  = 1 - min (1, (1-b)/a) */

    b = ~b;		    /* 1 - b */
    if (b >= a)		    /* 1 - b >= a -> (1-b)/a >= 1 */
	return 0;	    /* 1 - 1 */
    return ~FbIntDiv(b,a);  /* 1 - (1-b) / a */
}

/* portion covered by a but not b */
FASTCALL static uint8_t
fbCombineConjointOutPart (uint8_t a, uint8_t b)
{
    /* max (1-b/a,0) */
    /* = 1-min(b/a,1) */

    /* min (1, (1-b) / a) */

    if (b >= a)		    /* b >= a -> b/a >= 1 */
	return 0x00;	    /* 0 */
    return ~FbIntDiv(b,a);   /* 1 - b/a */
}

/* portion covered by both a and b */
FASTCALL static uint8_t
fbCombineConjointInPart (uint8_t a, uint8_t b)
{
    /* min (1,b/a) */

    if (b >= a)		    /* b >= a -> b/a >= 1 */
	return 0xff;	    /* 1 */
    return FbIntDiv(b,a);   /* b/a */
}

FASTCALL static void
fbCombineDisjointGeneralU (uint32_t *dest, const uint32_t *src, int width, uint8_t combine)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t m,n,o,p;
        uint16_t Fa, Fb, t, u, v;
        uint8_t sa = s >> 24;
        uint8_t da = d >> 24;

        switch (combine & CombineA) {
        default:
            Fa = 0;
            break;
        case CombineAOut:
            Fa = fbCombineDisjointOutPart (sa, da);
            break;
        case CombineAIn:
            Fa = fbCombineDisjointInPart (sa, da);
            break;
        case CombineA:
            Fa = 0xff;
            break;
        }

        switch (combine & CombineB) {
        default:
            Fb = 0;
            break;
        case CombineBOut:
            Fb = fbCombineDisjointOutPart (da, sa);
            break;
        case CombineBIn:
            Fb = fbCombineDisjointInPart (da, sa);
            break;
        case CombineB:
            Fb = 0xff;
            break;
        }
        m = FbGen (s,d,0,Fa,Fb,t, u, v);
        n = FbGen (s,d,8,Fa,Fb,t, u, v);
        o = FbGen (s,d,16,Fa,Fb,t, u, v);
        p = FbGen (s,d,24,Fa,Fb,t, u, v);
        s = m|n|o|p;
	*(dest + i) = s;
    }
}

FASTCALL static void
fbCombineDisjointOverU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t  s = *(src + i);
        uint16_t  a = s >> 24;

        if (a != 0x00)
        {
            if (a != 0xff)
            {
                uint32_t d = *(dest + i);
                a = fbCombineDisjointOutPart (d >> 24, a);
                FbByteMulAdd(d, a, s);
                s = d;
            }
	    *(dest + i) = s;
        }
    }
}

FASTCALL static void
fbCombineDisjointInU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineAIn);
}

FASTCALL static void
fbCombineDisjointInReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineBIn);
}

FASTCALL static void
fbCombineDisjointOutU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineAOut);
}

FASTCALL static void
fbCombineDisjointOutReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineBOut);
}

FASTCALL static void
fbCombineDisjointAtopU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineAAtop);
}

FASTCALL static void
fbCombineDisjointAtopReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineBAtop);
}

FASTCALL static void
fbCombineDisjointXorU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineXor);
}

FASTCALL static void
fbCombineConjointGeneralU (uint32_t *dest, const uint32_t *src, int width, uint8_t combine)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t  s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t  m,n,o,p;
        uint16_t  Fa, Fb, t, u, v;
        uint8_t sa = s >> 24;
        uint8_t da = d >> 24;

        switch (combine & CombineA) {
        default:
            Fa = 0;
            break;
        case CombineAOut:
            Fa = fbCombineConjointOutPart (sa, da);
            break;
        case CombineAIn:
            Fa = fbCombineConjointInPart (sa, da);
            break;
        case CombineA:
            Fa = 0xff;
            break;
        }

        switch (combine & CombineB) {
        default:
            Fb = 0;
            break;
        case CombineBOut:
            Fb = fbCombineConjointOutPart (da, sa);
            break;
        case CombineBIn:
            Fb = fbCombineConjointInPart (da, sa);
            break;
        case CombineB:
            Fb = 0xff;
            break;
        }
        m = FbGen (s,d,0,Fa,Fb,t, u, v);
        n = FbGen (s,d,8,Fa,Fb,t, u, v);
        o = FbGen (s,d,16,Fa,Fb,t, u, v);
        p = FbGen (s,d,24,Fa,Fb,t, u, v);
        s = m|n|o|p;
	*(dest + i) = s;
    }
}

FASTCALL static void
fbCombineConjointOverU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineAOver);
}


FASTCALL static void
fbCombineConjointOverReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineBOver);
}


FASTCALL static void
fbCombineConjointInU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineAIn);
}


FASTCALL static void
fbCombineConjointInReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineBIn);
}

FASTCALL static void
fbCombineConjointOutU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineAOut);
}

FASTCALL static void
fbCombineConjointOutReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineBOut);
}

FASTCALL static void
fbCombineConjointAtopU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineAAtop);
}

FASTCALL static void
fbCombineConjointAtopReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineBAtop);
}

FASTCALL static void
fbCombineConjointXorU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineXor);
}

/********************************************************************************/
/*************************** Per Channel functions ******************************/
/********************************************************************************/

FASTCALL static void
fbCombineMaskC (uint32_t *src, uint32_t *mask)
{
    uint32_t a = *mask;

    uint32_t	x;
    uint16_t	xa;

    if (!a)
    {
	*(src) = 0;
	return;
    }

    x = *(src);
    if (a == 0xffffffff)
    {
	x = x >> 24;
	x |= x << 8;
	x |= x << 16;
	*(mask) = x;
	return;
    }

    xa = x >> 24;
    FbByteMulC(x, a);
    *(src) = x;
    FbByteMul(a, xa);
    *(mask) = a;
}

FASTCALL static void
fbCombineMaskValueC (uint32_t *src, const uint32_t *mask)
{
    uint32_t a = *mask;
    uint32_t	x;

    if (!a)
    {
	*(src) = 0;
	return;
    }

    if (a == 0xffffffff)
	return;

    x = *(src);
    FbByteMulC(x, a);
    *(src) =x;
}

FASTCALL static void
fbCombineMaskAlphaC (const uint32_t *src, uint32_t *mask)
{
    uint32_t a = *(mask);
    uint32_t	x;

    if (!a)
	return;

    x = *(src) >> 24;
    if (x == 0xff)
	return;
    if (a == 0xffffffff)
    {
	x = x >> 24;
	x |= x << 8;
	x |= x << 16;
	*(mask) = x;
	return;
    }

    FbByteMul(a, x);
    *(mask) = a;
}



FASTCALL static void
fbCombineClearC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    memset(dest, 0, width*sizeof(uint32_t));
}

FASTCALL static void
fbCombineSrcC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
	uint32_t s = *(src + i);
	uint32_t m = *(mask + i);

	fbCombineMaskValueC (&s, &m);

	*(dest) = s;
    }
}

FASTCALL static void
fbCombineOverC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
	uint32_t s = *(src + i);
	uint32_t m = *(mask + i);
	uint32_t a;

	fbCombineMaskC (&s, &m);

	a = ~m;
        if (a != 0xffffffff)
        {
            if (a)
            {
                uint32_t d = *(dest + i);
                FbByteMulAddC(d, a, s);
                s = d;
            }
	    *(dest + i) = s;
        }
    }
}

FASTCALL static void
fbCombineOverReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint32_t a = ~d >> 24;

        if (a)
        {
            uint32_t s = *(src + i);
	    uint32_t m = *(mask + i);

	    fbCombineMaskValueC (&s, &m);

            if (a != 0xff)
            {
                FbByteMulAdd(s, a, d);
            }
	    *(dest + i) = s;
        }
    }
}

FASTCALL static void
fbCombineInC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint16_t a = d >> 24;
        uint32_t s = 0;
        if (a)
        {
	    uint32_t m = *(mask + i);

	    s = *(src + i);
	    fbCombineMaskValueC (&s, &m);
            if (a != 0xff)
            {
                FbByteMul(s, a);
            }
        }
	*(dest + i) = s;
    }
}

FASTCALL static void
fbCombineInReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t m = *(mask + i);
        uint32_t a;

	fbCombineMaskAlphaC (&s, &m);

	a = m;
        if (a != 0xffffffff)
        {
            uint32_t d = 0;
            if (a)
            {
                d = *(dest + i);
                FbByteMulC(d, a);
            }
	    *(dest + i) = d;
        }
    }
}

FASTCALL static void
fbCombineOutC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint16_t a = ~d >> 24;
        uint32_t s = 0;
        if (a)
        {
	    uint32_t m = *(mask + i);

	    s = *(src + i);
	    fbCombineMaskValueC (&s, &m);

            if (a != 0xff)
            {
                FbByteMul(s, a);
            }
        }
	*(dest + i) = s;
    }
}

FASTCALL static void
fbCombineOutReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
	uint32_t s = *(src + i);
	uint32_t m = *(mask + i);
	uint32_t a;

	fbCombineMaskAlphaC (&s, &m);

        a = ~m;
        if (a != 0xffffffff)
        {
            uint32_t d = 0;
            if (a)
            {
                d = *(dest + i);
                FbByteMulC(d, a);
            }
	    *(dest + i) = d;
        }
    }
}

FASTCALL static void
fbCombineAtopC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint32_t s = *(src + i);
        uint32_t m = *(mask + i);
        uint32_t ad;
        uint16_t as = d >> 24;

	fbCombineMaskC (&s, &m);

        ad = ~m;

        FbByteAddMulC(d, ad, s, as);
	*(dest + i) = d;
    }
}

FASTCALL static void
fbCombineAtopReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {

        uint32_t d = *(dest + i);
        uint32_t s = *(src + i);
        uint32_t m = *(mask + i);
        uint32_t ad;
        uint16_t as = ~d >> 24;

	fbCombineMaskC (&s, &m);

	ad = m;

        FbByteAddMulC(d, ad, s, as);
	*(dest + i) = d;
    }
}

FASTCALL static void
fbCombineXorC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint32_t s = *(src + i);
        uint32_t m = *(mask + i);
        uint32_t ad;
        uint16_t as = ~d >> 24;

	fbCombineMaskC (&s, &m);

	ad = ~m;

        FbByteAddMulC(d, ad, s, as);
	*(dest + i) = d;
    }
}

FASTCALL static void
fbCombineAddC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t m = *(mask + i);
        uint32_t d = *(dest + i);

	fbCombineMaskValueC (&s, &m);

        FbByteAdd(d, s);
	*(dest + i) = d;
    }
}

FASTCALL static void
fbCombineSaturateC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
        uint32_t  s, d;
        uint16_t  sa, sr, sg, sb, da;
        uint16_t  t, u, v;
        uint32_t  m,n,o,p;

        d = *(dest + i);
        s = *(src + i);
	m = *(mask + i);

	fbCombineMaskC (&s, &m);

        sa = (m >> 24);
        sr = (m >> 16) & 0xff;
        sg = (m >>  8) & 0xff;
        sb = (m      ) & 0xff;
        da = ~d >> 24;

        if (sb <= da)
            m = FbAdd(s,d,0,t);
        else
            m = FbGen (s, d, 0, (da << 8) / sb, 0xff, t, u, v);

        if (sg <= da)
            n = FbAdd(s,d,8,t);
        else
            n = FbGen (s, d, 8, (da << 8) / sg, 0xff, t, u, v);

        if (sr <= da)
            o = FbAdd(s,d,16,t);
        else
            o = FbGen (s, d, 16, (da << 8) / sr, 0xff, t, u, v);

        if (sa <= da)
            p = FbAdd(s,d,24,t);
        else
            p = FbGen (s, d, 24, (da << 8) / sa, 0xff, t, u, v);

	*(dest + i) = m|n|o|p;
    }
}

FASTCALL static void
fbCombineDisjointGeneralC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width, uint8_t combine)
{
    int i;

    for (i = 0; i < width; ++i) {
        uint32_t  s, d;
        uint32_t  m,n,o,p;
        uint32_t  Fa, Fb;
        uint16_t  t, u, v;
        uint32_t  sa;
        uint8_t   da;

        s = *(src + i);
        m = *(mask + i);
        d = *(dest + i);
        da = d >> 24;

	fbCombineMaskC (&s, &m);

	sa = m;

        switch (combine & CombineA) {
        default:
            Fa = 0;
            break;
        case CombineAOut:
            m = fbCombineDisjointOutPart ((uint8_t) (sa >> 0), da);
            n = fbCombineDisjointOutPart ((uint8_t) (sa >> 8), da) << 8;
            o = fbCombineDisjointOutPart ((uint8_t) (sa >> 16), da) << 16;
            p = fbCombineDisjointOutPart ((uint8_t) (sa >> 24), da) << 24;
            Fa = m|n|o|p;
            break;
        case CombineAIn:
            m = fbCombineDisjointInPart ((uint8_t) (sa >> 0), da);
            n = fbCombineDisjointInPart ((uint8_t) (sa >> 8), da) << 8;
            o = fbCombineDisjointInPart ((uint8_t) (sa >> 16), da) << 16;
            p = fbCombineDisjointInPart ((uint8_t) (sa >> 24), da) << 24;
            Fa = m|n|o|p;
            break;
        case CombineA:
            Fa = 0xffffffff;
            break;
        }

        switch (combine & CombineB) {
        default:
            Fb = 0;
            break;
        case CombineBOut:
            m = fbCombineDisjointOutPart (da, (uint8_t) (sa >> 0));
            n = fbCombineDisjointOutPart (da, (uint8_t) (sa >> 8)) << 8;
            o = fbCombineDisjointOutPart (da, (uint8_t) (sa >> 16)) << 16;
            p = fbCombineDisjointOutPart (da, (uint8_t) (sa >> 24)) << 24;
            Fb = m|n|o|p;
            break;
        case CombineBIn:
            m = fbCombineDisjointInPart (da, (uint8_t) (sa >> 0));
            n = fbCombineDisjointInPart (da, (uint8_t) (sa >> 8)) << 8;
            o = fbCombineDisjointInPart (da, (uint8_t) (sa >> 16)) << 16;
            p = fbCombineDisjointInPart (da, (uint8_t) (sa >> 24)) << 24;
            Fb = m|n|o|p;
            break;
        case CombineB:
            Fb = 0xffffffff;
            break;
        }
        m = FbGen (s,d,0,FbGet8(Fa,0),FbGet8(Fb,0),t, u, v);
        n = FbGen (s,d,8,FbGet8(Fa,8),FbGet8(Fb,8),t, u, v);
        o = FbGen (s,d,16,FbGet8(Fa,16),FbGet8(Fb,16),t, u, v);
        p = FbGen (s,d,24,FbGet8(Fa,24),FbGet8(Fb,24),t, u, v);
        s = m|n|o|p;
	*(dest + i) = s;
    }
}

FASTCALL static void
fbCombineDisjointOverC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineAOver);
}

FASTCALL static void
fbCombineDisjointInC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineAIn);
}

FASTCALL static void
fbCombineDisjointInReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineBIn);
}

FASTCALL static void
fbCombineDisjointOutC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineAOut);
}

FASTCALL static void
fbCombineDisjointOutReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineBOut);
}

FASTCALL static void
fbCombineDisjointAtopC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineAAtop);
}

FASTCALL static void
fbCombineDisjointAtopReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineBAtop);
}

FASTCALL static void
fbCombineDisjointXorC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineXor);
}

FASTCALL static void
fbCombineConjointGeneralC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width, uint8_t combine)
{
    int i;

    for (i = 0; i < width; ++i) {
        uint32_t  s, d;
        uint32_t  m,n,o,p;
        uint32_t  Fa, Fb;
        uint16_t  t, u, v;
        uint32_t  sa;
        uint8_t   da;

        s = *(src + i);
        m = *(mask + i);
        d = *(dest + i);
        da = d >> 24;

	fbCombineMaskC (&s, &m);

        sa = m;

        switch (combine & CombineA) {
        default:
            Fa = 0;
            break;
        case CombineAOut:
            m = fbCombineConjointOutPart ((uint8_t) (sa >> 0), da);
            n = fbCombineConjointOutPart ((uint8_t) (sa >> 8), da) << 8;
            o = fbCombineConjointOutPart ((uint8_t) (sa >> 16), da) << 16;
            p = fbCombineConjointOutPart ((uint8_t) (sa >> 24), da) << 24;
            Fa = m|n|o|p;
            break;
        case CombineAIn:
            m = fbCombineConjointInPart ((uint8_t) (sa >> 0), da);
            n = fbCombineConjointInPart ((uint8_t) (sa >> 8), da) << 8;
            o = fbCombineConjointInPart ((uint8_t) (sa >> 16), da) << 16;
            p = fbCombineConjointInPart ((uint8_t) (sa >> 24), da) << 24;
            Fa = m|n|o|p;
            break;
        case CombineA:
            Fa = 0xffffffff;
            break;
        }

        switch (combine & CombineB) {
        default:
            Fb = 0;
            break;
        case CombineBOut:
            m = fbCombineConjointOutPart (da, (uint8_t) (sa >> 0));
            n = fbCombineConjointOutPart (da, (uint8_t) (sa >> 8)) << 8;
            o = fbCombineConjointOutPart (da, (uint8_t) (sa >> 16)) << 16;
            p = fbCombineConjointOutPart (da, (uint8_t) (sa >> 24)) << 24;
            Fb = m|n|o|p;
            break;
        case CombineBIn:
            m = fbCombineConjointInPart (da, (uint8_t) (sa >> 0));
            n = fbCombineConjointInPart (da, (uint8_t) (sa >> 8)) << 8;
            o = fbCombineConjointInPart (da, (uint8_t) (sa >> 16)) << 16;
            p = fbCombineConjointInPart (da, (uint8_t) (sa >> 24)) << 24;
            Fb = m|n|o|p;
            break;
        case CombineB:
            Fb = 0xffffffff;
            break;
        }
        m = FbGen (s,d,0,FbGet8(Fa,0),FbGet8(Fb,0),t, u, v);
        n = FbGen (s,d,8,FbGet8(Fa,8),FbGet8(Fb,8),t, u, v);
        o = FbGen (s,d,16,FbGet8(Fa,16),FbGet8(Fb,16),t, u, v);
        p = FbGen (s,d,24,FbGet8(Fa,24),FbGet8(Fb,24),t, u, v);
        s = m|n|o|p;
	*(dest + i) = s;
    }
}

FASTCALL static void
fbCombineConjointOverC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineAOver);
}

FASTCALL static void
fbCombineConjointOverReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineBOver);
}

FASTCALL static void
fbCombineConjointInC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineAIn);
}

FASTCALL static void
fbCombineConjointInReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineBIn);
}

FASTCALL static void
fbCombineConjointOutC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineAOut);
}

FASTCALL static void
fbCombineConjointOutReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineBOut);
}

FASTCALL static void
fbCombineConjointAtopC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineAAtop);
}

FASTCALL static void
fbCombineConjointAtopReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineBAtop);
}

FASTCALL static void
fbCombineConjointXorC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineXor);
}

static CombineFuncU pixman_fbCombineFuncU[] = {
    fbCombineClear,
    fbCombineSrcU,
    NULL, /* CombineDst */
    fbCombineOverU,
    fbCombineOverReverseU,
    fbCombineInU,
    fbCombineInReverseU,
    fbCombineOutU,
    fbCombineOutReverseU,
    fbCombineAtopU,
    fbCombineAtopReverseU,
    fbCombineXorU,
    fbCombineAddU,
    fbCombineSaturateU,
    NULL,
    NULL,
    fbCombineClear,
    fbCombineSrcU,
    NULL, /* CombineDst */
    fbCombineDisjointOverU,
    fbCombineSaturateU, /* DisjointOverReverse */
    fbCombineDisjointInU,
    fbCombineDisjointInReverseU,
    fbCombineDisjointOutU,
    fbCombineDisjointOutReverseU,
    fbCombineDisjointAtopU,
    fbCombineDisjointAtopReverseU,
    fbCombineDisjointXorU,
    NULL,
    NULL,
    NULL,
    NULL,
    fbCombineClear,
    fbCombineSrcU,
    NULL, /* CombineDst */
    fbCombineConjointOverU,
    fbCombineConjointOverReverseU,
    fbCombineConjointInU,
    fbCombineConjointInReverseU,
    fbCombineConjointOutU,
    fbCombineConjointOutReverseU,
    fbCombineConjointAtopU,
    fbCombineConjointAtopReverseU,
    fbCombineConjointXorU,
};

static CombineFuncC pixman_fbCombineFuncC[] = {
    fbCombineClearC,
    fbCombineSrcC,
    NULL, /* Dest */
    fbCombineOverC,
    fbCombineOverReverseC,
    fbCombineInC,
    fbCombineInReverseC,
    fbCombineOutC,
    fbCombineOutReverseC,
    fbCombineAtopC,
    fbCombineAtopReverseC,
    fbCombineXorC,
    fbCombineAddC,
    fbCombineSaturateC,
    NULL,
    NULL,
    fbCombineClearC,	    /* 0x10 */
    fbCombineSrcC,
    NULL, /* Dest */
    fbCombineDisjointOverC,
    fbCombineSaturateC, /* DisjointOverReverse */
    fbCombineDisjointInC,
    fbCombineDisjointInReverseC,
    fbCombineDisjointOutC,
    fbCombineDisjointOutReverseC,
    fbCombineDisjointAtopC,
    fbCombineDisjointAtopReverseC,
    fbCombineDisjointXorC,  /* 0x1b */
    NULL,
    NULL,
    NULL,
    NULL,
    fbCombineClearC,
    fbCombineSrcC,
    NULL, /* Dest */
    fbCombineConjointOverC,
    fbCombineConjointOverReverseC,
    fbCombineConjointInC,
    fbCombineConjointInReverseC,
    fbCombineConjointOutC,
    fbCombineConjointOutReverseC,
    fbCombineConjointAtopC,
    fbCombineConjointAtopReverseC,
    fbCombineConjointXorC,
};

FbComposeFunctions pixman_composeFunctions = {
    pixman_fbCombineFuncU,
    pixman_fbCombineFuncC,
    pixman_fbCombineMaskU
};