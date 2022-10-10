/*
 * SSE.H                   Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * SSE operations
 */
#ifndef SSE_H
#define SSE_H

#ifndef __SSE__
#error                                                                         \
    "Not compiled with SSE support (use '-msse...' and/or target 64-bit Linux)"
#endif

#include "xmmintrin.h"
// SSE intrinsics (kernel header); use '-msse'

#include "MatrixPos.h"
class Matrix;
class MemMatrix;

/*
 * These functions handle as much of the matrix they can (0..all), leaving the
 * rest to the caller's trivial loop (if 'i'<'n').
 */
void SSE_abs(MemMatrix &c, const Matrix &a, MatrixPos::offset_t &i,
             MatrixPos::offset_t n);

typedef void (*reduce_sum_safe_f)(float v, float &sum, unsigned &used);
typedef void (*reduce_count_safe_f)(float v, unsigned &used);

void SSE_reduce_sum(const Matrix &a, float &sum, unsigned &used,
                    MatrixPos::offset_t &i, MatrixPos::offset_t n,
                    reduce_sum_safe_f f);

void SSE_reduce_count(const Matrix &a, unsigned &used, MatrixPos::offset_t &i,
                      MatrixPos::offset_t n, reduce_count_safe_f f);

/*
 * These functions handle all of 'n' values.
 */
void SSE_fill(float *to, float v, MatrixPos::offset_t n);
void SSE_copy(float *to, const float *from, MatrixPos::offset_t n);
bool SSE_hasnan(const float *ptr, MatrixPos::offset_t n);

/*
 * The rest of the operations need to be as macros; OP operation varies.
 *
 * Performance note:
 *
 * SSE code has been performance studied by the separate 'sse-test' code.
 * Doing 8 instructions within the loop, with postfix operators, seems
 * to be the most efficient way.     --AKa 24-Mar-2009
 *
 * 'a->data()' will return a 16-byte aligned pointer to raw data, or 0
 * (for 'SQD_Matrix' with version 7 alignment & data orientation that SSE
 * cannot handle).
 */

/*
 * c= a op b     'a' and 'b' both matrices
 *
 * Used by:  + (addps) for matrix + matrix
 *           - (subps) -''-
 *           * (mulps) -''-
 *           'div_by_nonzero' (divps) slot-wise division, divisor is non-zero
 *               Gives 'c' and 'a' the same (modifies in-place)
 *
 * Note: Using 'divps' with a zero divisor leads to a CPU level math interrupt.
 *       Don't even try.
 */
#define SSE_OP_mm(OP, C, A, B, I, N)                                           \
  {                                                                            \
    const float *a_data = (A)->getData();                                      \
    const float *b_data = (B)->getData();                                      \
    if (a_data && b_data) {                                                    \
      /*assert( ((size_t)a_data)%16==0 );*/                                    \
      /*assert( ((size_t)b_data)%16==0 );*/                                    \
                                                                               \
      float *c_data = (C).data;                                                \
      assert(c_data /* && ((size_t)c_data)%16==0 */);                          \
                                                                               \
      const __m128 *va = (const __m128 *)a_data;                               \
      const __m128 *vb = (const __m128 *)b_data;                               \
      __m128 *vc = (__m128 *)c_data;                                           \
                                                                               \
      while ((I) + (32 - 1) < (N)) { /* bulk handled here */                   \
        *(vc++) = OP(*va++, *vb++);                                            \
        *(vc++) = OP(*va++, *vb++);                                            \
        *(vc++) = OP(*va++, *vb++);                                            \
        *(vc++) = OP(*va++, *vb++);                                            \
        *(vc++) = OP(*va++, *vb++);                                            \
        *(vc++) = OP(*va++, *vb++);                                            \
        *(vc++) = OP(*va++, *vb++);                                            \
        *(vc++) = OP(*va++, *vb++);                                            \
        I += 32;                                                               \
      }                                                                        \
      while ((I) + (4 - 1) <                                                   \
             (N)) { /* last 4..31 values (just an optimization) */             \
        *(vc++) = OP(*va++, *vb++);                                            \
        I += 4;                                                                \
      }                                                                        \
    } /* last 0..3 values left for regular loop */                             \
  }

/*
 * c= a op scalar
 *
 * Used by:  + (addps) for matrix + scalar
 *           - (subps) -''-
 *           * (mulps) -''-
 *           / (divps) -''-              (divisor MUST be non-zero!)
 *           - unary minus (mulps with -1.0)
 */
#define SSE_OP_mf(OP, C, A, SCALAR, I, N)                                      \
  {                                                                            \
    const float *a_data = (A)->getData();                                      \
    if (a_data) {                                                              \
      /* assert( ((size_t)a_data)%16==0 ); */                                  \
                                                                               \
      float *c_data = (C).data;                                                \
      assert(c_data /* && ((size_t)c_data)%16==0 */);                          \
                                                                               \
      const __m128 *va = (const __m128 *)a_data;                               \
      const __m128 vv =                                                        \
          _mm_set1_ps(SCALAR); /* { SCALAR, SCALAR, SCALAR, SCALAR } */        \
      __m128 *vc = (__m128 *)c_data;                                           \
                                                                               \
      while ((I) + (32 - 1) < (N)) { /* bulk handled here */                   \
        *(vc++) = OP(*va++, vv);                                               \
        *(vc++) = OP(*va++, vv);                                               \
        *(vc++) = OP(*va++, vv);                                               \
        *(vc++) = OP(*va++, vv);                                               \
        *(vc++) = OP(*va++, vv);                                               \
        *(vc++) = OP(*va++, vv);                                               \
        *(vc++) = OP(*va++, vv);                                               \
        *(vc++) = OP(*va++, vv);                                               \
        I += 32;                                                               \
      }                                                                        \
      while ((I) + (4 - 1) <                                                   \
             (N)) { /* last 4..31 values (just an optimization) */             \
        *(vc++) = OP(*va++, vv);                                               \
        I += 4;                                                                \
      }                                                                        \
    } /* last 0..3 values left for regular loop */                             \
  }

/*
 * c= scalar op b
 *
 * Used by:  - (subps) for scalar - matrix
 */
#define SSE_OP_fm(OP, C, SCALAR, B, I, N)                                      \
  {                                                                            \
    const float *b_data = (B)->getData();                                      \
    if (b_data) {                                                              \
      /* assert( ((size_t)b_data)%16==0 ); */                                  \
                                                                               \
      float *c_data = (C).data;                                                \
      assert(c_data /* && ((size_t)c_data)%16==0 */);                          \
                                                                               \
      const __m128 *vb = (const __m128 *)b_data;                               \
      const __m128 vv =                                                        \
          _mm_set1_ps(SCALAR); /* { SCALAR, SCALAR, SCALAR, SCALAR } */        \
      __m128 *vc = (__m128 *)c_data;                                           \
                                                                               \
      while ((I) + (32 - 1) < (N)) { /* bulk handled here */                   \
        *(vc++) = OP(vv, *vb++);                                               \
        *(vc++) = OP(vv, *vb++);                                               \
        *(vc++) = OP(vv, *vb++);                                               \
        *(vc++) = OP(vv, *vb++);                                               \
        *(vc++) = OP(vv, *vb++);                                               \
        *(vc++) = OP(vv, *vb++);                                               \
        *(vc++) = OP(vv, *vb++);                                               \
        *(vc++) = OP(vv, *vb++);                                               \
        I += 32;                                                               \
      }                                                                        \
      while ((I) + (4 - 1) <                                                   \
             (N)) { /* last 4..31 values (just an optimization) */             \
        *(vc++) = OP(vv, *vb++);                                               \
        I += 4;                                                                \
      }                                                                        \
    } /* last 0..3 values left for regular loop */                             \
  }

/*
 * c= op a
 *
 * Used by:  'sqrt()' & '^0.5' (sqrtps) for matrix
 */
#define SSE_OP_m(OP, C, A, I, N)                                               \
  {                                                                            \
    const float *a_data = (A)->getData();                                      \
    if (a_data) {                                                              \
      /* assert( ((size_t)a_data)%16==0 ); */                                  \
                                                                               \
      float *c_data = (C).data;                                                \
      assert(c_data /* && assert( ((size_t)c_data)%16==0 */);                  \
                                                                               \
      const __m128 *va = (const __m128 *)a_data;                               \
      __m128 *vc = (__m128 *)c_data;                                           \
                                                                               \
      while ((I) + (32 - 1) < (N)) { /* bulk handled here */                   \
        *(vc++) = OP(*va++);                                                   \
        *(vc++) = OP(*va++);                                                   \
        *(vc++) = OP(*va++);                                                   \
        *(vc++) = OP(*va++);                                                   \
        *(vc++) = OP(*va++);                                                   \
        *(vc++) = OP(*va++);                                                   \
        *(vc++) = OP(*va++);                                                   \
        *(vc++) = OP(*va++);                                                   \
        I += 32;                                                               \
      }                                                                        \
      while ((I) + (4 - 1) <                                                   \
             (N)) { /* last 4..31 values (just an optimization) */             \
        *(vc++) = OP(*va++);                                                   \
        I += 4;                                                                \
      }                                                                        \
    } /* last 0..3 values left for regular loop */                             \
  }

/*
 * v= op a
 *
 * Min/max of matrix 'a' (as far as we can count)
 *
 * Used by: 'min()' (minps) for matrix -> scalar
 *          'max()' (maxps) -''-
 */
#define SSE_OP_minmax_1(OP, V, A, I, N, std_maxmin)                            \
  {                                                                            \
    const float *a_data = (A)->getData();                                      \
    if (a_data) {                                                              \
      /* assert( ((size_t)a_data)%16==0 ); */                                  \
      const __m128 *va = (const __m128 *)a_data;                               \
      __m128 vv = _mm_set1_ps(V); /* { [-]INF, [-]INF, [-]INF, [-]INF } */     \
                                                                               \
      /* Possible NAN values need to be in the LEFT SSE operand. */            \
      while ((I) + (16 - 1) < (N)) { /* bulk handled here */                   \
        vv = OP(*va++, vv);                                                    \
        vv = OP(*va++, vv);                                                    \
        vv = OP(*va++, vv);                                                    \
        vv = OP(*va++, vv);                                                    \
        I += 16;                                                               \
      }                                                                        \
      while ((I) + (4 - 1) <                                                   \
             (N)) { /* last 4..15 values (just an optimization) */             \
        vv = OP(*va++, vv);                                                    \
        I += 4;                                                                \
      }                                                                        \
      const float *vf = (const float *)&vv;                                    \
      V = std_maxmin(std_maxmin(vf[0], vf[1]), std_maxmin(vf[2], vf[3]));      \
    }                                                                          \
  } /* last 0..3 values left for regular loop */

/*
 * Detecting NANs using SSE.
 *
 * CMPORDPS operator works as follows:
 *
 *   res[0] = (op1[0] != NaN) && (op2[0] != NaN)
 *   res[1] = (op1[1] != NaN) && (op2[1] != NaN)
 *   res[2] = (op1[2] != NaN) && (op2[2] != NaN)
 *   res[3] = (op1[3] != NaN) && (op2[3] != NaN)
 *
 *   TRUE  = 0xFFFFFFFF
 *   FALSE = 0x00000000
 *
 *   0xFFFFFFFF0xFFFFFFFF0xFFFFFFFF0xFFFFFFFF    all eight floats without NANs.
 *
 *   '_mm_movemask_ps()' can be used to scale down the bits for comparison (it
 * takes the MSB of each four, resulting in 0..0x0f range).
 */
#define NO_NANS_8(vb)                                                          \
  (0x0f == /*0..0x0f*/ _mm_movemask_ps(_mm_cmpord_ps(*(vb), *((vb) + 1))))

/*
 * c= c op b    'c' and 'b' both matrices, op is '__builtin_ia32_minps' or
 *              '__builtin_ia32_maxps'. 'c' may have NANs, 'b' must be checked.
 *
 * NOTE: Order of assignment is NOT DEFINED in C++; we must not use a '++'
 *       on either side, if the same thing is also on the other (double role
 *       of 'vc' here).  --AKa 25-Mar-2009
 *
 * NOTE: If 'B' is to have NAN's ('_mm_cmpunord_ps' checks 2x4 floats at once
 *       and returns nonzero if any has a NAN), we cannot use SSE 'minps/maxps'
 *       reliably. For such pairs, the 'minmax_safe' (either 'max_safe' or
 * 'min_safe') function is used.
 *
 * Used by: 'min()' (minps) for two matrices
 *          'max()' (maxps) -''-
 */
#define SSE_OP_minmax(OP, C, B, I, N, minmax_safe)                             \
  {                                                                            \
    const float *b_data = (B)->getData();                                      \
    if (b_data) {                                                              \
      /* assert( ((size_t)b_data)%16==0 ); */                                  \
      float *c_data = (C).data;                                                \
      assert(c_data /* && ((size_t)c_data)%16==0 */);                          \
                                                                               \
      const __m128 *vb = (const __m128 *)b_data;                               \
      __m128 *vc = (__m128 *)c_data;                                           \
                                                                               \
      for (; (I) + (8 - 1) < (N); I += 8) { /* bulk handled here */            \
        if (NO_NANS_8(vb)) {                                                   \
          *vc = OP(*vc, *vb++);                                                \
          ++vc;                                                                \
          *vc = OP(*vc, *vb++);                                                \
          ++vc;                                                                \
        } else {                                                               \
          for (unsigned j = (I); j < (I) + 8; j++) {                           \
            C[j] = minmax_safe(C[j], (*B)[j]);                                 \
          }                                                                    \
          vb += 2;                                                             \
          vc += 2;                                                             \
        }                                                                      \
      }                                                                        \
    } /* last 0..7 values left for regular loop */                             \
  }

/*
 * c= c op scalar
 *
 * Note: The scalar value is guaranteed to be non-NAN and must be in the RIGHT
 *       parameter.
 *
 * Used in 'min()' or 'max()' between (one or more) matrices and (one or more)
 * scalars. If all the matrices had a NAN (a hole) that hole REMAINS through
 * this process (scalars only affect the non-NAN portion of the result).
 */
#define SSE_OP_minmax_scalar(OP, C, SCALAR, I, N, minmax_keep_nans)            \
  {                                                                            \
    float *c_data = (C).data;                                                  \
    assert(c_data /* && ((size_t)c_data)%16==0 */);                            \
                                                                               \
    const __m128 vv =                                                          \
        _mm_set1_ps(SCALAR); /* { SCALAR, SCALAR, SCALAR, SCALAR } */          \
    __m128 *vc = (__m128 *)c_data;                                             \
                                                                               \
    for (; (I) + (8 - 1) < (N); I += 8) { /* bulk handled here */              \
      if (NO_NANS_8(vc)) {                                                     \
        *vc = OP(*vc, vv);                                                     \
        ++vc;                                                                  \
        *vc = OP(*vc, vv);                                                     \
        ++vc;                                                                  \
      } else {                                                                 \
        for (unsigned j = (I); j < (I) + 8; j++) {                             \
          C[j] = minmax_keep_nans(C[j], SCALAR);                               \
        }                                                                      \
        vc += 2;                                                               \
      }                                                                        \
    }                                                                          \
  } /* last 0..7 values left for regular loop */

#endif
// SSE_H
