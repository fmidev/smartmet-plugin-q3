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
#include <cassert>
class Matrix;
class MemMatrix;

/*
 * These functions handle as much of the matrix they can (0..all), leaving the
 * rest to the caller's trivial loop (if 'i'<'n').
 */
void SSE_abs(MemMatrix &c, const Matrix &a, MatrixPos::offset_t &i,
             MatrixPos::offset_t n);

using reduce_sum_safe_f = void (*)(float v, float &sum, unsigned &used);
using reduce_count_safe_f = void (*)(float v, unsigned &used);

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
template <typename Op, typename IndexT, typename CountT>
inline void sse_op_mm(Op op, float *c_data, const float *a_data,
                      const float *b_data, IndexT &i, CountT n) {
  if (a_data && b_data) {
    /*assert( ((size_t)a_data)%16==0 );*/
    /*assert( ((size_t)b_data)%16==0 );*/

    assert(c_data /* && ((size_t)c_data)%16==0 */);

    const __m128 *va = (const __m128 *)a_data;
    const __m128 *vb = (const __m128 *)b_data;
    __m128 *vc = (__m128 *)c_data;

    while (i + (32 - 1) < n) { /* bulk handled here */
      *(vc++) = op(*va++, *vb++);
      *(vc++) = op(*va++, *vb++);
      *(vc++) = op(*va++, *vb++);
      *(vc++) = op(*va++, *vb++);
      *(vc++) = op(*va++, *vb++);
      *(vc++) = op(*va++, *vb++);
      *(vc++) = op(*va++, *vb++);
      *(vc++) = op(*va++, *vb++);
      i += 32;
    }
    while (i + (4 - 1) < n) { /* last 4..31 values (just an optimization) */
      *(vc++) = op(*va++, *vb++);
      i += 4;
    }
  } /* last 0..3 values left for regular loop */
}

#define SSE_OP_mm(OP, C, A, B, I, N)                                           \
  sse_op_mm([](__m128 x, __m128 y) { return OP(x, y); }, (C).data,            \
            (A)->getData(), (B)->getData(), I, N)

/*
 * c= a op scalar
 *
 * Used by:  + (addps) for matrix + scalar
 *           - (subps) -''-
 *           * (mulps) -''-
 *           / (divps) -''-              (divisor MUST be non-zero!)
 *           - unary minus (mulps with -1.0)
 */
template <typename Op, typename IndexT, typename CountT>
inline void sse_op_mf(Op op, float *c_data, const float *a_data, float scalar,
                      IndexT &i, CountT n) {
  if (a_data) {
    /* assert( ((size_t)a_data)%16==0 ); */

    assert(c_data /* && ((size_t)c_data)%16==0 */);

    const __m128 *va = (const __m128 *)a_data;
    const __m128 vv =
        _mm_set1_ps(scalar); /* { SCALAR, SCALAR, SCALAR, SCALAR } */
    __m128 *vc = (__m128 *)c_data;

    while (i + (32 - 1) < n) { /* bulk handled here */
      *(vc++) = op(*va++, vv);
      *(vc++) = op(*va++, vv);
      *(vc++) = op(*va++, vv);
      *(vc++) = op(*va++, vv);
      *(vc++) = op(*va++, vv);
      *(vc++) = op(*va++, vv);
      *(vc++) = op(*va++, vv);
      *(vc++) = op(*va++, vv);
      i += 32;
    }
    while (i + (4 - 1) < n) { /* last 4..31 values (just an optimization) */
      *(vc++) = op(*va++, vv);
      i += 4;
    }
  } /* last 0..3 values left for regular loop */
}

#define SSE_OP_mf(OP, C, A, SCALAR, I, N)                                      \
  sse_op_mf([](__m128 x, __m128 y) { return OP(x, y); }, (C).data,            \
            (A)->getData(), SCALAR, I, N)

/*
 * c= scalar op b
 *
 * Used by:  - (subps) for scalar - matrix
 */
template <typename Op, typename IndexT, typename CountT>
inline void sse_op_fm(Op op, float *c_data, float scalar, const float *b_data,
                      IndexT &i, CountT n) {
  if (b_data) {
    /* assert( ((size_t)b_data)%16==0 ); */

    assert(c_data /* && ((size_t)c_data)%16==0 */);

    const __m128 *vb = (const __m128 *)b_data;
    const __m128 vv =
        _mm_set1_ps(scalar); /* { SCALAR, SCALAR, SCALAR, SCALAR } */
    __m128 *vc = (__m128 *)c_data;

    while (i + (32 - 1) < n) { /* bulk handled here */
      *(vc++) = op(vv, *vb++);
      *(vc++) = op(vv, *vb++);
      *(vc++) = op(vv, *vb++);
      *(vc++) = op(vv, *vb++);
      *(vc++) = op(vv, *vb++);
      *(vc++) = op(vv, *vb++);
      *(vc++) = op(vv, *vb++);
      *(vc++) = op(vv, *vb++);
      i += 32;
    }
    while (i + (4 - 1) < n) { /* last 4..31 values (just an optimization) */
      *(vc++) = op(vv, *vb++);
      i += 4;
    }
  } /* last 0..3 values left for regular loop */
}

#define SSE_OP_fm(OP, C, SCALAR, B, I, N)                                      \
  sse_op_fm([](__m128 x, __m128 y) { return OP(x, y); }, (C).data, SCALAR,    \
            (B)->getData(), I, N)

/*
 * c= op a
 *
 * Used by:  'sqrt()' & '^0.5' (sqrtps) for matrix
 */
template <typename Op, typename IndexT, typename CountT>
inline void sse_op_m(Op op, float *c_data, const float *a_data, IndexT &i,
                     CountT n) {
  if (a_data) {
    /* assert( ((size_t)a_data)%16==0 ); */

    assert(c_data /* && assert( ((size_t)c_data)%16==0 */);

    const __m128 *va = (const __m128 *)a_data;
    __m128 *vc = (__m128 *)c_data;

    while (i + (32 - 1) < n) { /* bulk handled here */
      *(vc++) = op(*va++);
      *(vc++) = op(*va++);
      *(vc++) = op(*va++);
      *(vc++) = op(*va++);
      *(vc++) = op(*va++);
      *(vc++) = op(*va++);
      *(vc++) = op(*va++);
      *(vc++) = op(*va++);
      i += 32;
    }
    while (i + (4 - 1) < n) { /* last 4..31 values (just an optimization) */
      *(vc++) = op(*va++);
      i += 4;
    }
  } /* last 0..3 values left for regular loop */
}

#define SSE_OP_m(OP, C, A, I, N)                                               \
  sse_op_m([](__m128 x) { return OP(x); }, (C).data, (A)->getData(), I, N)

/*
 * v= op a
 *
 * Min/max of matrix 'a' (as far as we can count)
 *
 * Used by: 'min()' (minps) for matrix -> scalar
 *          'max()' (maxps) -''-
 */
template <typename Op, typename MaxMin, typename IndexT, typename CountT>
inline void sse_op_minmax_1(Op op, float &v, const float *a_data, IndexT &i,
                            CountT n, MaxMin std_maxmin) {
  if (a_data) {
    /* assert( ((size_t)a_data)%16==0 ); */
    const __m128 *va = (const __m128 *)a_data;
    __m128 vv = _mm_set1_ps(v); /* { [-]INF, [-]INF, [-]INF, [-]INF } */

    /* Possible NAN values need to be in the LEFT SSE operand. */
    while (i + (16 - 1) < n) { /* bulk handled here */
      vv = op(*va++, vv);
      vv = op(*va++, vv);
      vv = op(*va++, vv);
      vv = op(*va++, vv);
      i += 16;
    }
    while (i + (4 - 1) < n) { /* last 4..15 values (just an optimization) */
      vv = op(*va++, vv);
      i += 4;
    }
    const float *vf = (const float *)&vv;
    v = std_maxmin(std_maxmin(vf[0], vf[1]), std_maxmin(vf[2], vf[3]));
  }
} /* last 0..3 values left for regular loop */

#define SSE_OP_minmax_1(OP, V, A, I, N, std_maxmin)                            \
  sse_op_minmax_1([](__m128 x, __m128 y) { return OP(x, y); }, V,             \
                  (A)->getData(), I, N,                                        \
                  [](float x, float y) { return std_maxmin(x, y); })

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
 * c= c op b    'c' and 'b' both matrices, op is '_mm_min_ps' or
 *              '_mm_max_ps'. 'c' may have NANs, 'b' must be checked.
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
template <typename Op, typename Safe, typename CM, typename BM,
          typename IndexT, typename CountT>
inline void sse_op_minmax(Op op, float *c_data, CM &c, const BM *b,
                          const float *b_data, IndexT &i, CountT n,
                          Safe minmax_safe) {
  if (b_data) {
    /* assert( ((size_t)b_data)%16==0 ); */
    assert(c_data /* && ((size_t)c_data)%16==0 */);

    const __m128 *vb = (const __m128 *)b_data;
    __m128 *vc = (__m128 *)c_data;

    for (; i + (8 - 1) < n; i += 8) { /* bulk handled here */
      if (NO_NANS_8(vb)) {
        *vc = op(*vc, *vb++);
        ++vc;
        *vc = op(*vc, *vb++);
        ++vc;
      } else {
        for (unsigned j = i; j < i + 8; j++) {
          c[j] = minmax_safe(c[j], (*b)[j]);
        }
        vb += 2;
        vc += 2;
      }
    }
  } /* last 0..7 values left for regular loop */
}

#define SSE_OP_minmax(OP, C, B, I, N, minmax_safe)                             \
  sse_op_minmax([](__m128 x, __m128 y) { return OP(x, y); }, (C).data, C, B,  \
                (B)->getData(), I, N,                                          \
                [](float x, float y) { return minmax_safe(x, y); })

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
template <typename Op, typename KeepNans, typename CM, typename IndexT,
          typename CountT>
inline void sse_op_minmax_scalar(Op op, float *c_data, CM &c, float scalar,
                                 IndexT &i, CountT n,
                                 KeepNans minmax_keep_nans) {
  assert(c_data /* && ((size_t)c_data)%16==0 */);

  const __m128 vv =
      _mm_set1_ps(scalar); /* { SCALAR, SCALAR, SCALAR, SCALAR } */
  __m128 *vc = (__m128 *)c_data;

  for (; i + (8 - 1) < n; i += 8) { /* bulk handled here */
    if (NO_NANS_8(vc)) {
      *vc = op(*vc, vv);
      ++vc;
      *vc = op(*vc, vv);
      ++vc;
    } else {
      for (unsigned j = i; j < i + 8; j++) {
        c[j] = minmax_keep_nans(c[j], scalar);
      }
      vc += 2;
    }
  }
} /* last 0..7 values left for regular loop */

#define SSE_OP_minmax_scalar(OP, C, SCALAR, I, N, minmax_keep_nans)            \
  sse_op_minmax_scalar([](__m128 x, __m128 y) { return OP(x, y); }, (C).data,  \
                       C, SCALAR, I, N,                                        \
                       [](float x, float y) { return minmax_keep_nans(x, y); })

#endif
// SSE_H
