/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2018 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdConvolution.h"
#include "Simd/SimdStore.h"
#include "Simd/SimdSynet.h"
#include "Simd/SimdAvx512f.h"

namespace Simd
{
#ifdef SIMD_AVX512F_ENABLE    
    namespace Avx512f
    {
        static void ConvolutionBiasAndActivation(const float * bias, size_t count, size_t size, ::SimdConvolutionActivationType activation, const float * params, ::SimdBool trans, float * dst)
        {
            size_t aligned = AlignLo(trans ? count : size, F);
            __mmask16 tail = __mmask16(-1) >> (F + aligned - (trans ? count : size));
            if (activation == ::SimdConvolutionActivationIdentity)
            {
                if (bias)
                    SynetAddBias(bias, count, size, dst, trans);
            }
            else if (activation == ::SimdConvolutionActivationRelu)
            {
                if (bias)
                {
                    __m512 _0 = _mm512_set1_ps(0.0f);
                    if (trans)
                    {
                        for (size_t j = 0; j < size; ++j)
                        {
                            size_t i = 0;
                            for (; i < aligned; i += F)
                            {
                                __m512 _dst = _mm512_loadu_ps(dst + i);
                                __m512 _bias = _mm512_loadu_ps(bias + i);
                                _mm512_storeu_ps(dst + i, _mm512_max_ps(_0, _mm512_add_ps(_dst, _bias)));
                            }
                            if (i < count)
                            {
                                __m512 _dst = _mm512_maskz_loadu_ps(tail, dst + i);
                                __m512 _bias = _mm512_maskz_loadu_ps(tail, bias + i);
                                _mm512_mask_storeu_ps(dst + i, tail, _mm512_max_ps(_0, _mm512_add_ps(_dst, _bias)));
                            }
                            dst += count;
                        }
                    }
                    else
                    {
                        for (size_t i = 0; i < count; ++i)
                        {
                            __m512 _bias = _mm512_set1_ps(bias[i]);
                            size_t j = 0;
                            for (; j < aligned; j += F)
                            {
                                __m512 _dst = _mm512_loadu_ps(dst + j);
                                _mm512_storeu_ps(dst + j, _mm512_max_ps(_0, _mm512_add_ps(_dst, _bias)));
                            }
                            if (j < size)
                            {
                                __m512 _dst = _mm512_maskz_loadu_ps(tail, dst + j);
                                _mm512_mask_storeu_ps(dst + j, tail, _mm512_max_ps(_0, _mm512_add_ps(_dst, _bias)));
                            }
                            dst += size;
                        }
                    }
                }
                else
                {
                    float slope = 0;
                    NeuralRelu(dst, size*count, &slope, dst);
                }
            }
            else if (activation == ::SimdConvolutionActivationLeakyRelu)
            {
                float slope = params[0];
                if (bias)
                {
                    __m512 _slope = _mm512_set1_ps(slope);
                    if (trans)
                    {
                        for (size_t j = 0; j < size; ++j)
                        {
                            size_t i = 0;
                            for (; i < aligned; i += F)
                            {
                                __m512 _dst = _mm512_loadu_ps(dst + i);
                                __m512 _bias = _mm512_loadu_ps(bias + i);
                                _mm512_storeu_ps(dst + i, SynetPreluLayerForward(_mm512_add_ps(_dst, _bias), _slope));
                            }
                            if (i < count)
                            {
                                __m512 _dst = _mm512_maskz_loadu_ps(tail, dst + i);
                                __m512 _bias = _mm512_maskz_loadu_ps(tail, bias + i);
                                _mm512_mask_storeu_ps(dst + i, tail, SynetPreluLayerForward(_mm512_add_ps(_dst, _bias), _slope));
                            }
                            dst += count;
                        }
                    }
                    else
                    {
                        for (size_t i = 0; i < count; ++i)
                        {
                            __m512 _bias = _mm512_set1_ps(bias[i]);
                            size_t j = 0;
                            for (; j < aligned; j += F)
                            {
                                __m512 value = _mm512_add_ps(_mm512_loadu_ps(dst + j), _bias);
                                _mm512_storeu_ps(dst + j, SynetPreluLayerForward(value, _slope));
                            }
                            if (j < size)
                            {
                                __m512 value = _mm512_add_ps(_mm512_maskz_loadu_ps(tail, dst + j), _bias);
                                _mm512_mask_storeu_ps(dst + j, tail, SynetPreluLayerForward(value, _slope));
                            }
                            dst += size;
                        }
                    }
                }
                else
                    NeuralRelu(dst, size*count, &slope, dst);
            }
            else if (activation == ::SimdConvolutionActivationRestrictRange)
            {
                float lower = params[0];
                float upper = params[1];
                if (bias)
                {
                    __m512 _lower = _mm512_set1_ps(lower);
                    __m512 _upper = _mm512_set1_ps(upper);
                    if (trans)
                    {
                        for (size_t j = 0; j < size; ++j)
                        {
                            size_t i = 0;
                            for (; i < aligned; i += F)
                            {
                                __m512 value = _mm512_add_ps(_mm512_loadu_ps(dst + i), _mm512_loadu_ps(bias + i));
                                _mm512_storeu_ps(dst + i, _mm512_min_ps(_mm512_max_ps(_lower, value), _upper));
                            }
                            if (i < count)
                            {
                                __m512 value = _mm512_add_ps(_mm512_maskz_loadu_ps(tail, dst + i), _mm512_maskz_loadu_ps(tail, bias + i));
                                _mm512_mask_storeu_ps(dst + i, tail, _mm512_min_ps(_mm512_max_ps(_lower, value), _upper));
                            }
                            dst += count;
                        }
                    }
                    else
                    {
                        for (size_t i = 0; i < count; ++i)
                        {
                            __m512 _bias = _mm512_set1_ps(bias[i]);
                            size_t j = 0;
                            for (; j < aligned; j += F)
                            {
                                __m512 value = _mm512_add_ps(_mm512_loadu_ps(dst + j), _bias);
                                _mm512_storeu_ps(dst + j, _mm512_min_ps(_mm512_max_ps(_lower, value), _upper));
                            }
                            if (j < size)
                            {
                                __m512 value = _mm512_add_ps(_mm512_maskz_loadu_ps(tail, dst + j), _bias);
                                _mm512_mask_storeu_ps(dst + j, tail, _mm512_min_ps(_mm512_max_ps(_lower, value), _upper));
                            }
                            dst += size;
                        }
                    }
                }
                else
                    SynetRestrictRange(dst, size*count, &lower, &upper, dst);
            }
            else if (activation == ::SimdConvolutionActivationPrelu)
            {
                if (bias)
                {
                    if (trans)
                    {
                        for (size_t j = 0; j < size; ++j)
                        {
                            size_t i = 0;
                            for (; i < aligned; i += F)
                            {
                                __m512 value = _mm512_add_ps(_mm512_loadu_ps(dst + i), _mm512_loadu_ps(bias + i));
                                _mm512_storeu_ps(dst + i, SynetPreluLayerForward(value, _mm512_loadu_ps(params + i)));
                            }
                            if (i < count)
                            {
                                __m512 value = _mm512_add_ps(_mm512_maskz_loadu_ps(tail, dst + i), _mm512_maskz_loadu_ps(tail, bias + i));
                                _mm512_mask_storeu_ps(dst + i, tail, SynetPreluLayerForward(value, _mm512_maskz_loadu_ps(tail, params + i)));
                            }
                            dst += count;
                        }
                    }
                    else
                    {
                        for (size_t i = 0; i < count; ++i)
                        {
                            __m512 _bias = _mm512_set1_ps(bias[i]);
                            __m512 _slope = _mm512_set1_ps(params[i]);
                            size_t j = 0;
                            for (; j < aligned; j += F)
                            {
                                __m512 value = _mm512_add_ps(_mm512_loadu_ps(dst + j), _bias);
                                _mm512_storeu_ps(dst + j, SynetPreluLayerForward(value, _slope));
                            }
                            if (j < size)
                            {
                                __m512 value = _mm512_add_ps(_mm512_maskz_loadu_ps(tail, dst + j), _bias);
                                _mm512_mask_storeu_ps(dst + j, tail, SynetPreluLayerForward(value, _slope));
                            }
                            dst += size;
                        }
                    }
                }
                else
                    Avx512f::SynetPreluLayerForward(dst, params, count, size, dst, trans);
            }
        }

        //---------------------------------------------------------------------

        ConvolutionGemmNN::ConvolutionGemmNN(const ConvParam & p)
            : Avx2::ConvolutionGemmNN(p)
        {
            _index.Resize(F);
            for (size_t i = 0; i < F; ++i)
                _index[i] = int(i * p.strideX);
            _nose.Resize(p.kernelX);
            _tail.Resize(p.kernelX);
            ptrdiff_t aligned = AlignHi(p.dstW, F) - F;
            for (size_t kx = 0; kx < p.kernelX; ++kx)
            {
                _nose[kx] = 0;
                _tail[kx] = 0;
                ptrdiff_t sx = kx * p.dilationX - p.padX;
                for (size_t dx = 0; dx < p.dstW; ++dx)
                {
                    if (sx >= 0 && sx < ptrdiff_t(p.srcW) && dx < F)
                        _nose[kx] |= 1 << dx;
                    if (sx < ptrdiff_t(p.srcW) && ptrdiff_t(dx) >= aligned)
                        _tail[kx] |= 1 << (dx - aligned);
                    sx += p.strideX;
                }
            }
        }

        void ConvolutionGemmNN::GemmAndBias(const float * src, float * dst)
        {
            const ConvParam & p = _param;
            for (size_t g = 0; g < p.group; ++g)
            {
                if (p.srcT)
                    Avx512f::Gemm32fNN(_M, _N, _K, &_1, src + _grS * g, _ldS, _weight + _grW * g, _ldW, &_0, dst + _grD * g, _ldD);
                else
                    Avx512f::Gemm32fNN(_M, _N, _K, &_1, _weight + _grW * g, _ldW, src + _grS * g, _ldS, &_0, dst + _grD * g, _ldD);
            }
            Avx512f::ConvolutionBiasAndActivation(_bias, p.dstC, p.dstH*p.dstW, p.activation, _params, p.dstT, dst);
        }

        void ConvolutionGemmNN::ImgToCol(const float * src, float * dst)
        {
            const ConvParam & p = _param;
            size_t srcSize = p.srcW * p.srcH;
            if (p.dilationX == 1 && p.dilationY == 1 && p.strideX == 2 && p.strideY == 2 && p.padX == 0 && p.padY == 0 && p.padW == 0 && p.padH == 0 && p.kernelX == 1 && p.kernelY == 1)
            {
                for (size_t c = 0; c < p.srcC; ++c)
                {
                    for (size_t dy = 0; dy < p.dstH; ++dy)
                    {
                        const float * psrc = src + 2 * dy*p.srcW;
                        for (size_t dx = 0, sx = 0; dx < p.dstW; ++dx, sx += 2)
                            *(dst++) = psrc[sx];
                    }
                    src += srcSize;
                }
            }
            else if (p.dilationX*p.dilationY*p.strideX*p.strideY != 1)
            {
                __m512 _0 = _mm512_setzero_ps();
                __m512i index = _mm512_loadu_si512(_index.data);
                size_t aligned = AlignHi(p.dstW, F) - F;
                __mmask16 storeTail = TailMask16(p.dstW - aligned);
                __mmask16 storeNose = aligned ? __mmask16(-1) : storeTail;
                for (size_t c = 0; c < p.srcC; ++c)
                {
                    for (size_t ky = 0; ky < p.kernelY; ky++)
                    {
                        for (size_t kx = 0; kx < p.kernelX; kx++)
                        {
                            __mmask16 nose = _nose[kx];
                            __mmask16 tail = _tail[kx];
                            size_t sx0 = kx * p.dilationX - p.padX;
                            size_t sy = ky * p.dilationY - p.padY;
                            for (size_t dy = 0; dy < p.dstH; ++dy)
                            {
                                if (sy < p.srcH)
                                {
                                    size_t dx = 0, sx = sx0 + sy * p.srcW;
                                    _mm512_mask_storeu_ps(dst + dx, storeNose, _mm512_mask_i32gather_ps(_0, nose, index, (src + sx), 4));
                                    dx += F, sx += p.strideX*F;
                                    //if (p.strideX == 3)
                                    //{
                                    //    for (; dx < aligned; dx += F, sx += p.strideX*F)
                                    //        _mm512_storeu_ps(dst + dx, Avx512f::Gather<3>(src + sx));
                                    //}
                                    //else
                                    //{
                                        for (; dx < aligned; dx += F, sx += p.strideX*F)
                                            _mm512_storeu_ps(dst + dx, _mm512_i32gather_ps(index, (src + sx), 4));
                                    //}
                                    if (aligned)
                                        _mm512_mask_storeu_ps(dst + dx, storeTail, _mm512_mask_i32gather_ps(_0, tail, index, (src + sx), 4));
                                }
                                else
                                {
                                    memset(dst, 0, p.dstW * sizeof(float));
                                }
                                dst += p.dstW;
                                sy += p.strideY;
                            }
                        }
                    }
                    src += srcSize;
                }
            }
            else
            {
                Base::ConvolutionGemmNN::ImgToCol(src, dst);
            }
        }

        //---------------------------------------------------------------------

        ConvolutionGemmNT::ConvolutionGemmNT(const ConvParam & p)
            : Avx2::ConvolutionGemmNT(p)
        {
        }

        void ConvolutionGemmNT::GemmAndBias(const float * src, float * dst)
        {
            const ConvParam & p = _param;
            for (size_t g = 0; g < p.group; ++g)
                Avx512f::Gemm32fNT(_M, _N, _K, &_1, _weight + _weightStep * g, _K, src + _srcStep * g, _K, &_0, dst + _dstStep * g, _N);
            Avx512f::ConvolutionBiasAndActivation(_bias, p.dstC, p.dstH*p.dstW, _param.activation, _params, ::SimdFalse, dst);
        }

        //---------------------------------------------------------------------

        ConvolutionWinograd2x3p::ConvolutionWinograd2x3p(const ConvParam & p)
            : Avx2::ConvolutionWinograd2x3p(p)
        {
            _setFilter = Avx512f::Winograd2x3SetFilter;
        }

        void ConvolutionWinograd2x3p::Forward(const float * src, float * buf, float * dst)
        {
            const ConvParam & p = _param;
            float * bufS = Buffer(buf);
            float * bufD = bufS + _strideS * _count;
            Avx512f::Winograd2x3pSetInput(src, p.srcC, p.srcH, p.srcW, buf, _pad);
            for (size_t i = 0; i < _count; ++i)
                Avx512f::Gemm32fNN(_M, _N, _K, &_1, _weight.data + i * _strideW, _K, bufS + i * _strideS, _N, &_0, bufD + i * _strideD, _N);
            Avx512f::Winograd2x3pSetOutput(bufD, dst, p.dstC, p.dstH, p.dstW);
            Avx512f::ConvolutionBiasAndActivation(_bias, p.dstC, p.dstH*p.dstW, _param.activation, _params, ::SimdFalse, dst);
        }

        //---------------------------------------------------------------------

        ConvolutionDirectChw::ConvolutionDirectChw(const ConvParam & p)
            : Avx2::ConvolutionDirectChw(p)
        {
            _convolutionBiasActivation = SetConvolutionBiasActivation();
        }

        template <size_t size> SIMD_INLINE void LoadWeight(const float * src, __m512 * dst)
        {
            for (size_t i = 0; i < size; ++i)
                dst[i] = _mm512_set1_ps(src[i]);
        }

        template<int kernel, int stride> struct Kernel
        {
            static __m512 Convolution(const float * src, size_t step, const __m512  * weight);
        };

        template<> struct Kernel<1, 1>
        {
            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                return _mm512_mul_ps(_mm512_loadu_ps(src), weight[0]);
            }
        };

        template<> struct Kernel<1, 2>
        {
            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                __m512 s0 = _mm512_loadu_ps(src + 0);
                __m512 s1 = _mm512_loadu_ps(src + F);
                return _mm512_permutexvar_ps(K32_PERMUTE_FOR_PACK, _mm512_mul_ps(_mm512_shuffle_ps(s0, s1, 0x88), weight[0]));
            }
        };

        template<> struct Kernel<2, 1>
        {
            static SIMD_INLINE __m512 RowConv(const float * src, const __m512  * weight)
            {
                return _mm512_fmadd_ps(_mm512_loadu_ps(src), weight[0],
                    _mm512_mul_ps(_mm512_loadu_ps(src + 1), weight[1]));
            }

            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                return _mm512_add_ps(RowConv(src, weight), RowConv(src + step, weight + 2));
            }
        };

        template<> struct Kernel<2, 2>
        {
            static SIMD_INLINE __m512 RowConv(const float * src, const __m512  * weight)
            {
                __m512 s0 = _mm512_loadu_ps(src + 0);
                __m512 s1 = _mm512_loadu_ps(src + F);
                return _mm512_fmadd_ps(_mm512_shuffle_ps(s0, s1, 0x88), weight[0],
                    _mm512_mul_ps(_mm512_shuffle_ps(s0, s1, 0xDD), weight[1]));
            }

            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                return _mm512_permutexvar_ps(K32_PERMUTE_FOR_PACK, _mm512_add_ps(RowConv(src, weight), RowConv(src + step, weight + 2)));
            }
        };

        template<> struct Kernel<3, 1>
        {
            static SIMD_INLINE __m512 RowConv(const float * src, const __m512  * weight)
            {
                return _mm512_fmadd_ps(_mm512_loadu_ps(src), weight[0],
                    _mm512_fmadd_ps(_mm512_loadu_ps(src + 1), weight[1],
                        _mm512_mul_ps(_mm512_loadu_ps(src + 2), weight[2])));
            }

            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                return _mm512_add_ps(RowConv(src, weight),
                    _mm512_add_ps(RowConv(src + step, weight + 3),
                        RowConv(src + 2 * step, weight + 6)));
            }
        };

        template<> struct Kernel<3, 2>
        {
            static SIMD_INLINE __m512 RowConv(const float * src, const __m512  * weight)
            {
                __m512 s00 = _mm512_loadu_ps(src);
                __m512 s10 = _mm512_loadu_ps(src + F);
                __m512 s02 = _mm512_loadu_ps(src + 2);
                __m512 s12 = _mm512_loadu_ps(src + 2 + F);
                return _mm512_fmadd_ps(_mm512_shuffle_ps(s00, s10, 0x88), weight[0],
                    _mm512_fmadd_ps(_mm512_shuffle_ps(s00, s10, 0xDD), weight[1],
                        _mm512_mul_ps(_mm512_shuffle_ps(s02, s12, 0x88), weight[2])));
            }

            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                return _mm512_permutexvar_ps(K32_PERMUTE_FOR_PACK, _mm512_add_ps(RowConv(src, weight),
                    _mm512_add_ps(RowConv(src + step, weight + 3), RowConv(src + 2 * step, weight + 6))));
            }
        };

        const __m512i K32_IDX_3_0A = SIMD_MM512_SETR_EPI32(0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 0, 0, 0, 0, 0);
        const __m512i K32_IDX_3_0B = SIMD_MM512_SETR_EPI32(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 7, 10, 13);
        const __m512i K32_IDX_3_1A = SIMD_MM512_SETR_EPI32(1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46);
        const __m512i K32_IDX_3_1B = SIMD_MM512_SETR_EPI32(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 8, 11, 14);
        const __m512i K32_IDX_3_2A = SIMD_MM512_SETR_EPI32(2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 35, 38, 41, 44, 47);
        const __m512i K32_IDX_3_2B = SIMD_MM512_SETR_EPI32(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 6, 9, 12, 15);

        template<> struct Kernel<3, 3>
        {

            static SIMD_INLINE __m512 RowConv(const float * src, const __m512  * weight)
            {
                __m512 src0 = _mm512_loadu_ps(src + 0 * F);
                __m512 src1 = _mm512_loadu_ps(src + 1 * F);
                __m512 src2 = _mm512_loadu_ps(src + 2 * F);
                __m512 s0 = _mm512_mask_permutexvar_ps(_mm512_maskz_permutex2var_ps(0xFFFF, src0, K32_IDX_3_0A, src1), 0xF800, K32_IDX_3_0B, src2);
                __m512 s1 = _mm512_mask_permutexvar_ps(_mm512_maskz_permutex2var_ps(0xFFFF, src0, K32_IDX_3_1A, src1), 0xF800, K32_IDX_3_1B, src2);
                __m512 s2 = _mm512_mask_permutexvar_ps(_mm512_maskz_permutex2var_ps(0xFFFF, src0, K32_IDX_3_2A, src1), 0xFC00, K32_IDX_3_2B, src2);
                return _mm512_fmadd_ps(s0, weight[0], _mm512_fmadd_ps(s1, weight[1], _mm512_mul_ps(s2, weight[2])));
            }

            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                return _mm512_add_ps(RowConv(src, weight), _mm512_add_ps(RowConv(src + step, weight + 3), RowConv(src + 2 * step, weight + 6)));
            }
        };

        template<> struct Kernel<4, 1>
        {
            static SIMD_INLINE __m512 RowConv(const float * src, const __m512  * weight)
            {
                return _mm512_fmadd_ps(_mm512_loadu_ps(src), weight[0], _mm512_fmadd_ps(_mm512_loadu_ps(src + 1), weight[1],
                        _mm512_fmadd_ps(_mm512_loadu_ps(src + 2), weight[2], _mm512_mul_ps(_mm512_loadu_ps(src + 3), weight[3]))));
            }

            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                return _mm512_add_ps(RowConv(src, weight), _mm512_add_ps(RowConv(src + step, weight + 4),
                    _mm512_add_ps(RowConv(src + 2 * step, weight + 8), RowConv(src + 3 * step, weight + 12))));
            }
        };

        template<> struct Kernel<4, 2>
        {
            static SIMD_INLINE __m512 RowConv(const float * src, const __m512  * weight)
            {
                __m512 s00 = _mm512_loadu_ps(src);
                __m512 s10 = _mm512_loadu_ps(src + F);
                __m512 s02 = _mm512_loadu_ps(src + 2);
                __m512 s12 = _mm512_loadu_ps(src + 2 + F);
                return _mm512_fmadd_ps(_mm512_shuffle_ps(s00, s10, 0x88), weight[0], _mm512_fmadd_ps(_mm512_shuffle_ps(s00, s10, 0xDD), weight[1],
                    _mm512_fmadd_ps(_mm512_shuffle_ps(s02, s12, 0x88), weight[2], _mm512_mul_ps(_mm512_shuffle_ps(s02, s12, 0xDD), weight[3]))));
            }

            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                return _mm512_permutexvar_ps(K32_PERMUTE_FOR_PACK, _mm512_add_ps(RowConv(src, weight),
                    _mm512_add_ps(RowConv(src + step, weight + 4), _mm512_add_ps(RowConv(src + 2 * step, weight + 8), RowConv(src + 3 * step, weight + 12)))));
            }
        };

        template<> struct Kernel<5, 1>
        {
            static SIMD_INLINE __m512 RowConv(const float * src, const __m512  * weight)
            {
                return _mm512_fmadd_ps(_mm512_loadu_ps(src), weight[0], _mm512_fmadd_ps(_mm512_loadu_ps(src + 1), weight[1],
                    _mm512_fmadd_ps(_mm512_loadu_ps(src + 2), weight[2], _mm512_fmadd_ps(_mm512_loadu_ps(src + 3), weight[3],
                        _mm512_mul_ps(_mm512_loadu_ps(src + 4), weight[4])))));
            }

            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                return _mm512_add_ps(RowConv(src, weight), _mm512_add_ps(RowConv(src + step, weight + 5),
                    _mm512_add_ps(RowConv(src + 2 * step, weight + 10), _mm512_add_ps(RowConv(src + 3 * step, weight + 15), 
                        RowConv(src + 4 * step, weight + 20)))));
            }
        };

        template<> struct Kernel<5, 2>
        {
            static SIMD_INLINE __m512 RowConv(const float * src, const __m512  * weight)
            {
                __m512 s00 = _mm512_loadu_ps(src);
                __m512 s10 = _mm512_loadu_ps(src + F);
                __m512 s02 = _mm512_loadu_ps(src + 2);
                __m512 s12 = _mm512_loadu_ps(src + 2 + F);
                __m512 s04 = _mm512_loadu_ps(src + 4);
                __m512 s14 = _mm512_loadu_ps(src + 4 + F);
                return _mm512_fmadd_ps(_mm512_shuffle_ps(s00, s10, 0x88), weight[0], _mm512_fmadd_ps(_mm512_shuffle_ps(s00, s10, 0xDD), weight[1],
                    _mm512_fmadd_ps(_mm512_shuffle_ps(s02, s12, 0x88), weight[2], _mm512_fmadd_ps(_mm512_shuffle_ps(s02, s12, 0xDD), weight[3],
                        _mm512_mul_ps(_mm512_shuffle_ps(s04, s14, 0x88), weight[4])))));
            }

            static SIMD_INLINE __m512 Convolution(const float * src, size_t step, const __m512  * weight)
            {
                return _mm512_permutexvar_ps(K32_PERMUTE_FOR_PACK, _mm512_add_ps(RowConv(src, weight), _mm512_add_ps(RowConv(src + step, weight + 5), 
                    _mm512_add_ps(RowConv(src + 2 * step, weight + 10), _mm512_add_ps(RowConv(src + 3 * step, weight + 15), RowConv(src + 4 * step, weight + 20))))));
            }
        };

        template<::SimdConvolutionActivationType type> SIMD_INLINE __m512 Activate(__m512 value, const __m512 * params);

        template<> SIMD_INLINE __m512 Activate<::SimdConvolutionActivationIdentity>(__m512 value, const __m512 * params)
        {
            return value;
        }

        template<> SIMD_INLINE __m512 Activate<::SimdConvolutionActivationRelu>(__m512 value, const __m512 * params)
        {
            return _mm512_max_ps(_mm512_setzero_ps(), value);
        }

        template<> SIMD_INLINE __m512 Activate<::SimdConvolutionActivationLeakyRelu>(__m512 value, const __m512 * params)
        {
            return _mm512_add_ps(_mm512_max_ps(_mm512_setzero_ps(), value), _mm512_mul_ps(params[0], _mm512_min_ps(_mm512_setzero_ps(), value)));
        }

        template<> SIMD_INLINE __m512 Activate<::SimdConvolutionActivationRestrictRange>(__m512 value, const __m512 * params)
        {
            return _mm512_min_ps(_mm512_max_ps(params[0], value), params[1]);
        }

        template<> SIMD_INLINE __m512 Activate<::SimdConvolutionActivationPrelu>(__m512 value, const __m512 * params)
        {
            return _mm512_add_ps(_mm512_max_ps(_mm512_setzero_ps(), value), _mm512_mul_ps(params[0], _mm512_min_ps(_mm512_setzero_ps(), value)));
        }

        template<int kernel, int stride, ::SimdConvolutionActivationType type> 
        void ConvolutionBiasActivation(const float * src, size_t srcC, size_t srcH, size_t srcW, const float * weight, 
            const float * bias, const float * params, float * dst, size_t dstC, size_t dstH, size_t dstW)
        {
            __m512 _weight[kernel*kernel];
            __m512 _params[2];
            _params[0] = _mm512_set1_ps(params[0]);
            if (type == ::SimdConvolutionActivationRestrictRange)
                _params[1] = _mm512_set1_ps(params[1]);
            size_t dstWF = Simd::AlignLo(dstW, F);
            __mmask16 tail = TailMask16(dstW - dstWF);
            for (size_t dc = 0; dc < dstC; ++dc)
            {
                if (type == ::SimdConvolutionActivationPrelu)
                    _params[0] = _mm512_set1_ps(params[dc]);
                if (srcC == 1)
                {
                    const float * ps = src;
                    float * pd = dst;
                    LoadWeight<kernel*kernel>(weight, _weight);
                    __m512 _bias = bias ? _mm512_set1_ps(bias[dc]) : _mm512_setzero_ps();
                    for (size_t y = 0; y < dstH; ++y)
                    {
                        size_t x = 0;
                        for (; x < dstWF; x += F)
                        {
                            __m512 conv = Kernel<kernel, stride>::Convolution(ps + x * stride, srcW, _weight);
                            _mm512_storeu_ps(pd + x, Activate<type>(_mm512_add_ps(_bias, conv), _params));
                        }
                        if (x < dstW)
                        {
                            __m512 conv = Kernel<kernel, stride>::Convolution(ps + x * stride, srcW, _weight);
                            _mm512_mask_storeu_ps(pd + x, tail, Activate<type>(_mm512_add_ps(_bias, conv), _params));
                        }
                        ps += srcW * stride;
                        pd += dstW;
                    }
                    weight += kernel * kernel;
                }
                else
                {
                    size_t sc = 0;
                    for (; sc < 1; ++sc)
                    {
                        const float * ps = src;
                        float * pd = dst;
                        LoadWeight<kernel*kernel>(weight, _weight);
                        __m512 _bias = bias ? _mm512_set1_ps(bias[dc]) : _mm512_setzero_ps();
                        for (size_t y = 0; y < dstH; ++y)
                        {
                            size_t x = 0;
                            for (; x < dstWF; x += F)
                            {
                                __m512 conv = Kernel<kernel, stride>::Convolution(ps + x * stride, srcW, _weight);
                                _mm512_storeu_ps(pd + x, _mm512_add_ps(_bias, conv));
                            }
                            if (x < dstW)
                            {
                                __m512 conv = Kernel<kernel, stride>::Convolution(ps + x * stride, srcW, _weight);
                                _mm512_mask_storeu_ps(pd + x, tail, _mm512_add_ps(_bias, conv));
                            }
                            ps += srcW * stride;
                            pd += dstW;
                        }
                        weight += kernel * kernel;
                    }
                    for (; sc < srcC - 1; ++sc)
                    {
                        const float * ps = src + sc * srcW * srcH;
                        float * pd = dst;
                        LoadWeight<kernel*kernel>(weight, _weight);
                        for (size_t y = 0; y < dstH; ++y)
                        {
                            size_t x = 0;
                            for (; x < dstWF; x += F)
                            {
                                __m512 _dst = _mm512_loadu_ps(pd + x);
                                __m512 conv = Kernel<kernel, stride>::Convolution(ps + x * stride, srcW, _weight);
                                _mm512_storeu_ps(pd + x, _mm512_add_ps(_dst, conv));
                            }
                            if (x < dstW)
                            {
                                __m512 _dst = _mm512_maskz_loadu_ps(tail, pd + x);
                                __m512 conv = Kernel<kernel, stride>::Convolution(ps + x * stride, srcW, _weight);
                                _mm512_mask_storeu_ps(pd + x, tail, _mm512_add_ps(_dst, conv));
                            }
                            ps += srcW * stride;
                            pd += dstW;
                        }
                        weight += kernel * kernel;
                    }
                    for (; sc < srcC; ++sc)
                    {
                        const float * ps = src + sc * srcW * srcH;
                        float * pd = dst;
                        LoadWeight<kernel*kernel>(weight, _weight);
                        for (size_t y = 0; y < dstH; ++y)
                        {
                            size_t x = 0;
                            for (; x < dstWF; x += F)
                            {
                                __m512 _dst = _mm512_loadu_ps(pd + x);
                                __m512 conv = Kernel<kernel, stride>::Convolution(ps + x * stride, srcW, _weight);
                                _mm512_storeu_ps(pd + x, Activate<type>(_mm512_add_ps(_dst, conv), _params));
                            }
                            if (x < dstW)
                            {
                                __m512 _dst = _mm512_maskz_loadu_ps(tail, pd + x);
                                __m512 conv = Kernel<kernel, stride>::Convolution(ps + x * stride, srcW, _weight);
                                _mm512_mask_storeu_ps(pd + x, tail, Activate<type>(_mm512_add_ps(_dst, conv), _params));
                            }
                            ps += srcW * stride;
                            pd += dstW;
                        }
                        weight += kernel * kernel;
                    }
                }
                dst += dstH * dstW;
            }
        }

         bool ConvolutionDirectChw::Preferable(const ConvParam & p)
        {
            if (!p.IsDilation(1))
                return false;
            if (!(p.IsStride(1) || p.IsStride(2) || p.IsStride(3)))
                return false;
            double k = double(p.srcC) / p.group * p.strideX * p.strideX * p.strideY / p.kernelX / p.kernelY;
            return k < 2.0 && ((p.IsStride(1) && p.IsKernel(1)) || p.IsKernel(2) || p.IsKernel(3)
#if SIMD_ZMM_COUNT == 32
                || p.IsKernel(4) || p.IsKernel(5)
#endif
                ) && p.IsChw();
        }

        template <int kernel, int stride> ConvolutionDirectChw::ConvolutionBiasActivationPtr SetConvolutionBiasActivation(::SimdConvolutionActivationType type)
        {
            switch (type)
            {
            case ::SimdConvolutionActivationIdentity: return ConvolutionBiasActivation<kernel, stride, ::SimdConvolutionActivationIdentity>;
            case ::SimdConvolutionActivationRelu: return ConvolutionBiasActivation<kernel, stride, ::SimdConvolutionActivationRelu>;
            case ::SimdConvolutionActivationLeakyRelu: return ConvolutionBiasActivation<kernel, stride, ::SimdConvolutionActivationLeakyRelu>;
            case ::SimdConvolutionActivationRestrictRange: return ConvolutionBiasActivation<kernel, stride, ::SimdConvolutionActivationRestrictRange>;
            case ::SimdConvolutionActivationPrelu: return ConvolutionBiasActivation<kernel, stride, ::SimdConvolutionActivationPrelu>;
            default:
                assert(0);
                return NULL;
            }
        }

        ConvolutionDirectChw::ConvolutionBiasActivationPtr ConvolutionDirectChw::SetConvolutionBiasActivation()
        {
            const ConvParam & p = _param;
            if (p.dstW <= HF)
                return Avx2::ConvolutionDirectChw::SetConvolutionBiasActivation();
            switch (p.strideX)
            {
            case 1:
                if (p.kernelX == 1)
                    return Avx512f::SetConvolutionBiasActivation<1, 1>(p.activation);
                if (p.kernelX == 2)
                    return Avx512f::SetConvolutionBiasActivation<2, 1>(p.activation);
                if (p.kernelX == 3)
                    return Avx512f::SetConvolutionBiasActivation<3, 1>(p.activation);
                if (p.kernelX == 4)
                    return Avx512f::SetConvolutionBiasActivation<4, 1>(p.activation);
                if (p.kernelX == 5)
                    return Avx512f::SetConvolutionBiasActivation<5, 1>(p.activation);
                break;
            case 2:
                if (p.kernelX == 2)
                    return Avx512f::SetConvolutionBiasActivation<2, 2>(p.activation);
                if (p.kernelX == 3)
                    return Avx512f::SetConvolutionBiasActivation<3, 2>(p.activation);
                if (p.kernelX == 4)
                    return Avx512f::SetConvolutionBiasActivation<4, 2>(p.activation);
                if (p.kernelX == 5)
                    return Avx512f::SetConvolutionBiasActivation<5, 2>(p.activation);
                break;
            case 3:
                if (p.kernelX == 3)
                    return Avx512f::SetConvolutionBiasActivation<3, 3>(p.activation);
                break;
            }
            return Avx2::ConvolutionDirectChw::SetConvolutionBiasActivation();
        }

        //---------------------------------------------------------------------

        ConvolutionDirectHwc::ConvolutionDirectHwc(const ConvParam & p)
            : Avx2::ConvolutionDirectHwc(p)
        {
            _convolutionBiasActivation = SetConvolutionBiasActivation();
        }

        template<::SimdConvolutionActivationType type> SIMD_INLINE __m512 Activate(__m512 value, const float * params, size_t offset, __mmask16 tail = -1);

        template<> SIMD_INLINE __m512 Activate<::SimdConvolutionActivationIdentity>(__m512 value, const float * params, size_t offset, __mmask16 tail)
        {
            return value;
        }

        template<> SIMD_INLINE __m512 Activate<::SimdConvolutionActivationRelu>(__m512 value, const float * params, size_t offset, __mmask16 tail)
        {
            return _mm512_max_ps(_mm512_setzero_ps(), value);
        }

        template<> SIMD_INLINE __m512 Activate<::SimdConvolutionActivationLeakyRelu>(__m512 value, const float * params, size_t offset, __mmask16 tail)
        {
            return _mm512_add_ps(_mm512_max_ps(_mm512_setzero_ps(), value), _mm512_mul_ps(_mm512_set1_ps(params[0]), _mm512_min_ps(_mm512_setzero_ps(), value)));
        }

        template<> SIMD_INLINE __m512 Activate<::SimdConvolutionActivationRestrictRange>(__m512 value, const float * params, size_t offset, __mmask16 tail)
        {
            return _mm512_min_ps(_mm512_max_ps(_mm512_set1_ps(params[0]), value), _mm512_set1_ps(params[1]));
        }

        template<> SIMD_INLINE __m512 Activate<::SimdConvolutionActivationPrelu>(__m512 value, const float * params, size_t offset, __mmask16 tail)
        {
            return _mm512_add_ps(_mm512_max_ps(_mm512_setzero_ps(), value), _mm512_mul_ps(_mm512_maskz_loadu_ps(tail, params + offset), _mm512_min_ps(_mm512_setzero_ps(), value)));
        }

        SIMD_INLINE void KernelHwcDefaultEdge(const float * src, const ConvParam & p, size_t kH, size_t kW, const float * weight, __m512 & sum, __mmask16 tail = -1)
        {
            size_t size = kW * p.srcC, rest = (p.kernelX - kW)*p.srcC*p.dstC, dstC = p.dstC, stride = p.srcW * p.srcC;
            for (size_t ky = 0; ky < kH; ++ky)
            {
                for (size_t i = 0; i < size; ++i, weight += dstC)
                    sum = _mm512_fmadd_ps(_mm512_set1_ps(src[i]), _mm512_maskz_loadu_ps(tail, weight), sum);
                weight += rest;
                src += stride;
            }
        }

        template<::SimdConvolutionActivationType type>
        SIMD_INLINE void KernelHwcDefaultEdge(const float * src, const ConvParam & p, size_t kH, size_t kW, const float * weight, const float * bias, const float * params, float * dst)
        {
            size_t dstC = p.dstC;
            size_t dstCF = AlignLo(dstC, F);

            size_t dc = 0;
            for (; dc < dstCF; dc += F)
            {
                __m512 conv = bias ? _mm512_loadu_ps(bias + dc) : _mm512_setzero_ps();
                KernelHwcDefaultEdge(src, p, kH, kW, weight + dc, conv);
                _mm512_storeu_ps(dst + dc, Activate<type>(conv, params, dc));
            }
            if (dc < dstC)
            {
                __mmask16 tail = TailMask16(dstC - dstCF); 
                dc = dstC - F;
                __m512 conv = bias ? _mm512_maskz_loadu_ps(tail, bias + dc) : _mm512_setzero_ps();
                KernelHwcDefaultEdge(src, p, kH, kW, weight + dc, conv, tail);
                _mm512_mask_storeu_ps(dst + dc, tail, Activate<type>(conv, params, dc, tail));
            }
        }

        SIMD_INLINE void KernelHwcDefaultBody2x2(const float * src, const ConvParam & p, const float * weight, __m512 sums[2][2])
        {
            size_t size = p.kernelX * p.srcC, dstC = p.dstC, stride = p.srcW * p.srcC, step = p.srcC * p.strideX;
            const float * src0 = src + 0 * step;
            const float * src1 = src + 1 * step;
            __m512 w0, w1, s0;
            for (size_t ky = 0; ky < p.kernelY; ++ky)
            {
                size_t offset = ky * stride;
                for (size_t end = offset + size; offset < end; ++offset)
                {
                    w0 = _mm512_loadu_ps(weight + 0 * F);
                    w1 = _mm512_loadu_ps(weight + 1 * F);
                    s0 = _mm512_set1_ps(src0[offset]);
                    sums[0][0] = _mm512_fmadd_ps(s0, w0, sums[0][0]);
                    sums[0][1] = _mm512_fmadd_ps(s0, w1, sums[0][1]);
                    s0 = _mm512_set1_ps(src1[offset]);
                    sums[1][0] = _mm512_fmadd_ps(s0, w0, sums[1][0]);
                    sums[1][1] = _mm512_fmadd_ps(s0, w1, sums[1][1]);
                    weight += dstC;
                }
            }
        }

        SIMD_INLINE void KernelHwcDefaultBody2x1(const float * src, const ConvParam & p, const float * weight, __m512 sums[2][1], __mmask16 tail = -1)
        {
            size_t size = p.kernelX * p.srcC, dstC = p.dstC, stride = p.srcW * p.srcC, step = p.srcC * p.strideX;
            const float * src0 = src + 0 * step;
            const float * src1 = src + 1 * step;
            __m512 w0, s0;
            for (size_t ky = 0; ky < p.kernelY; ++ky)
            {
                size_t offset = ky * stride;
                for (size_t end = offset + size; offset < end; ++offset)
                {
                    w0 = _mm512_maskz_loadu_ps(tail, weight + 0 * F);
                    s0 = _mm512_set1_ps(src0[offset]);
                    sums[0][0] = _mm512_fmadd_ps(s0, w0, sums[0][0]);
                    s0 = _mm512_set1_ps(src1[offset]);
                    sums[1][0] = _mm512_fmadd_ps(s0, w0, sums[1][0]);
                    weight += dstC;
                }
            }
        }

        template<::SimdConvolutionActivationType type>
        SIMD_INLINE void KernelHwcDefaultBody2(const float * src, const ConvParam & p, const float * weight, const float * bias, const float * params, float * dst)
        {
            size_t dstC = p.dstC;
            size_t dstCF1 = AlignLo(dstC, 1 * F);
            size_t dstCF2 = AlignLo(dstC, 2 * F);
            size_t dc = 0;
            for (; dc < dstCF2; dc += 2 * F)
            {
                __m512 sums[2][2];
                __m512 bias0 = bias ? _mm512_loadu_ps(bias + dc + 0 * F) : _mm512_setzero_ps();
                __m512 bias1 = bias ? _mm512_loadu_ps(bias + dc + 1 * F) : _mm512_setzero_ps();
                sums[0][0] = bias0;
                sums[0][1] = bias1;
                sums[1][0] = bias0;
                sums[1][1] = bias1;
                KernelHwcDefaultBody2x2(src, p, weight + dc, sums);
                _mm512_storeu_ps(dst + dc + 0 * dstC + 0 * F, Activate<type>(sums[0][0], params, dc + 0 * F));
                _mm512_storeu_ps(dst + dc + 0 * dstC + 1 * F, Activate<type>(sums[0][1], params, dc + 1 * F));
                _mm512_storeu_ps(dst + dc + 1 * dstC + 0 * F, Activate<type>(sums[1][0], params, dc + 0 * F));
                _mm512_storeu_ps(dst + dc + 1 * dstC + 1 * F, Activate<type>(sums[1][1], params, dc + 1 * F));
            }
            for (; dc < dstCF1; dc += 1 * F)
            {
                __m512 sums[2][1];
                __m512 bias0 = bias ? _mm512_loadu_ps(bias + dc) : _mm512_setzero_ps();
                sums[0][0] = bias0;
                sums[1][0] = bias0;
                KernelHwcDefaultBody2x1(src, p, weight + dc, sums);
                _mm512_storeu_ps(dst + dc + 0 * dstC, Activate<type>(sums[0][0], params, dc));
                _mm512_storeu_ps(dst + dc + 1 * dstC, Activate<type>(sums[1][0], params, dc));
            }
            if (dc < dstC)
            {
                __mmask16 tail = TailMask16(dstC - dstCF1);
                dc = dstC - F;
                __m512 sums[2][1];
                __m512 bias0 = bias ? _mm512_maskz_loadu_ps(tail, bias + dc) : _mm512_setzero_ps();
                sums[0][0] = bias0;
                sums[1][0] = bias0;
                KernelHwcDefaultBody2x1(src, p, weight + dc, sums);
                _mm512_mask_storeu_ps(dst + dc + 0 * dstC, tail, Activate<type>(sums[0][0], params, dc, tail));
                _mm512_mask_storeu_ps(dst + dc + 1 * dstC, tail, Activate<type>(sums[1][0], params, dc, tail));
            }
        }

        SIMD_INLINE void KernelHwcDefaultBody6x2(const float * src, const ConvParam & p, const float * weight, __m512 sums[6][2])
        {
            size_t size = p.kernelX * p.srcC, dstC = p.dstC, stride = p.srcW * p.srcC, step = p.srcC * p.strideX;
            const float * src0 = src + 0 * step;
            const float * src1 = src + 1 * step;
            const float * src2 = src + 2 * step;
            const float * src3 = src + 3 * step;
            const float * src4 = src + 4 * step;
            const float * src5 = src + 5 * step;
            __m512 w0, w1, s0;
            for (size_t ky = 0; ky < p.kernelY; ++ky)
            {
                size_t offset = ky * stride;
                for (size_t end = offset + size; offset < end; ++offset)
                {
                    w0 = _mm512_loadu_ps(weight + 0 * F);
                    w1 = _mm512_loadu_ps(weight + 1 * F);
                    s0 = _mm512_set1_ps(src0[offset]);
                    sums[0][0] = _mm512_fmadd_ps(s0, w0, sums[0][0]);
                    sums[0][1] = _mm512_fmadd_ps(s0, w1, sums[0][1]);
                    s0 = _mm512_set1_ps(src1[offset]);
                    sums[1][0] = _mm512_fmadd_ps(s0, w0, sums[1][0]);
                    sums[1][1] = _mm512_fmadd_ps(s0, w1, sums[1][1]);
                    s0 = _mm512_set1_ps(src2[offset]);
                    sums[2][0] = _mm512_fmadd_ps(s0, w0, sums[2][0]);
                    sums[2][1] = _mm512_fmadd_ps(s0, w1, sums[2][1]);
                    s0 = _mm512_set1_ps(src3[offset]);
                    sums[3][0] = _mm512_fmadd_ps(s0, w0, sums[3][0]);
                    sums[3][1] = _mm512_fmadd_ps(s0, w1, sums[3][1]);
                    s0 = _mm512_set1_ps(src4[offset]);
                    sums[4][0] = _mm512_fmadd_ps(s0, w0, sums[4][0]);
                    sums[4][1] = _mm512_fmadd_ps(s0, w1, sums[4][1]);
                    s0 = _mm512_set1_ps(src5[offset]);
                    sums[5][0] = _mm512_fmadd_ps(s0, w0, sums[5][0]);
                    sums[5][1] = _mm512_fmadd_ps(s0, w1, sums[5][1]);
                    weight += dstC;
                }
            }
        }

        SIMD_INLINE void KernelHwcDefaultBody6x1(const float * src, const ConvParam & p, const float * weight, __m512 sums[6][1], __mmask16 tail = -1)
        {
            size_t size = p.kernelX * p.srcC, dstC = p.dstC, stride = p.srcW * p.srcC, step = p.srcC * p.strideX;
            const float * src0 = src + 0 * step;
            const float * src1 = src + 1 * step;
            const float * src2 = src + 2 * step;
            const float * src3 = src + 3 * step;
            const float * src4 = src + 4 * step;
            const float * src5 = src + 5 * step;
            __m512 w0, s0;
            for (size_t ky = 0; ky < p.kernelY; ++ky)
            {
                size_t offset = ky * stride;
                for (size_t end = offset + size; offset < end; ++offset)
                {
                    w0 = _mm512_maskz_loadu_ps(tail, weight + 0 * F);
                    s0 = _mm512_set1_ps(src0[offset]);
                    sums[0][0] = _mm512_fmadd_ps(s0, w0, sums[0][0]);
                    s0 = _mm512_set1_ps(src1[offset]);
                    sums[1][0] = _mm512_fmadd_ps(s0, w0, sums[1][0]);
                    s0 = _mm512_set1_ps(src2[offset]);
                    sums[2][0] = _mm512_fmadd_ps(s0, w0, sums[2][0]);
                    s0 = _mm512_set1_ps(src3[offset]);
                    sums[3][0] = _mm512_fmadd_ps(s0, w0, sums[3][0]);
                    s0 = _mm512_set1_ps(src4[offset]);
                    sums[4][0] = _mm512_fmadd_ps(s0, w0, sums[4][0]);
                    s0 = _mm512_set1_ps(src5[offset]);
                    sums[5][0] = _mm512_fmadd_ps(s0, w0, sums[5][0]);
                    weight += dstC;
                }
            }
        }

        template<::SimdConvolutionActivationType type>
        SIMD_INLINE void KernelHwcDefaultBody6(const float * src, const ConvParam & p, const float * weight, const float * bias, const float * params, float * dst)
        {
            size_t dstC = p.dstC;
            size_t dstCF1 = AlignLo(dstC, 1 * F);
            size_t dstCF2 = AlignLo(dstC, 2 * F);
            size_t dc = 0;
            for (; dc < dstCF2; dc += 2 * F)
            {
                __m512 sums[6][2];
                __m512 bias0 = bias ? _mm512_loadu_ps(bias + dc + 0 * F) : _mm512_setzero_ps();
                __m512 bias1 = bias ? _mm512_loadu_ps(bias + dc + 1 * F) : _mm512_setzero_ps();
                sums[0][0] = bias0;
                sums[0][1] = bias1;
                sums[1][0] = bias0;
                sums[1][1] = bias1;
                sums[2][0] = bias0;
                sums[2][1] = bias1;
                sums[3][0] = bias0;
                sums[3][1] = bias1;
                sums[4][0] = bias0;
                sums[4][1] = bias1;
                sums[5][0] = bias0;
                sums[5][1] = bias1;
                KernelHwcDefaultBody6x2(src, p, weight + dc, sums);
                _mm512_storeu_ps(dst + dc + 0 * dstC + 0 * F, Activate<type>(sums[0][0], params, dc + 0 * F));
                _mm512_storeu_ps(dst + dc + 0 * dstC + 1 * F, Activate<type>(sums[0][1], params, dc + 1 * F));
                _mm512_storeu_ps(dst + dc + 1 * dstC + 0 * F, Activate<type>(sums[1][0], params, dc + 0 * F));
                _mm512_storeu_ps(dst + dc + 1 * dstC + 1 * F, Activate<type>(sums[1][1], params, dc + 1 * F));
                _mm512_storeu_ps(dst + dc + 2 * dstC + 0 * F, Activate<type>(sums[2][0], params, dc + 0 * F));
                _mm512_storeu_ps(dst + dc + 2 * dstC + 1 * F, Activate<type>(sums[2][1], params, dc + 1 * F));
                _mm512_storeu_ps(dst + dc + 3 * dstC + 0 * F, Activate<type>(sums[3][0], params, dc + 0 * F));
                _mm512_storeu_ps(dst + dc + 3 * dstC + 1 * F, Activate<type>(sums[3][1], params, dc + 1 * F));
                _mm512_storeu_ps(dst + dc + 4 * dstC + 0 * F, Activate<type>(sums[4][0], params, dc + 0 * F));
                _mm512_storeu_ps(dst + dc + 4 * dstC + 1 * F, Activate<type>(sums[4][1], params, dc + 1 * F));
                _mm512_storeu_ps(dst + dc + 5 * dstC + 0 * F, Activate<type>(sums[5][0], params, dc + 0 * F));
                _mm512_storeu_ps(dst + dc + 5 * dstC + 1 * F, Activate<type>(sums[5][1], params, dc + 1 * F));
            }
            for (; dc < dstCF1; dc += 1 * F)
            {
                __m512 sums[6][1];
                __m512 bias0 = bias ? _mm512_loadu_ps(bias + dc) : _mm512_setzero_ps();
                sums[0][0] = bias0;
                sums[1][0] = bias0;
                sums[2][0] = bias0;
                sums[3][0] = bias0;
                sums[4][0] = bias0;
                sums[5][0] = bias0;
                KernelHwcDefaultBody6x1(src, p, weight + dc, sums);
                _mm512_storeu_ps(dst + dc + 0 * dstC, Activate<type>(sums[0][0], params, dc));
                _mm512_storeu_ps(dst + dc + 1 * dstC, Activate<type>(sums[1][0], params, dc));
                _mm512_storeu_ps(dst + dc + 2 * dstC, Activate<type>(sums[2][0], params, dc));
                _mm512_storeu_ps(dst + dc + 3 * dstC, Activate<type>(sums[3][0], params, dc));
                _mm512_storeu_ps(dst + dc + 4 * dstC, Activate<type>(sums[4][0], params, dc));
                _mm512_storeu_ps(dst + dc + 5 * dstC, Activate<type>(sums[5][0], params, dc));
            }
            if (dc < dstC)
            {
                __mmask16 tail = TailMask16(dstC - dstCF1);
                __m512 sums[6][1];
                __m512 bias0 = bias ? _mm512_maskz_loadu_ps(tail, bias + dc) : _mm512_setzero_ps();
                sums[0][0] = bias0;
                sums[1][0] = bias0;
                sums[2][0] = bias0;
                sums[3][0] = bias0;
                sums[4][0] = bias0;
                sums[5][0] = bias0;
                KernelHwcDefaultBody6x1(src, p, weight + dc, sums, tail);
                _mm512_mask_storeu_ps(dst + dc + 0 * dstC, tail, Activate<type>(sums[0][0], params, dc, tail));
                _mm512_mask_storeu_ps(dst + dc + 1 * dstC, tail, Activate<type>(sums[1][0], params, dc, tail));
                _mm512_mask_storeu_ps(dst + dc + 2 * dstC, tail, Activate<type>(sums[2][0], params, dc, tail));
                _mm512_mask_storeu_ps(dst + dc + 3 * dstC, tail, Activate<type>(sums[3][0], params, dc, tail));
                _mm512_mask_storeu_ps(dst + dc + 4 * dstC, tail, Activate<type>(sums[4][0], params, dc, tail));
                _mm512_mask_storeu_ps(dst + dc + 5 * dstC, tail, Activate<type>(sums[5][0], params, dc, tail));
            }
        }

        template<::SimdConvolutionActivationType type> void ConvolutionDirectHwcConvolutionBiasActivationDefault(const float * src, const ConvParam & p, const float * weight, const float * bias, const float * params, float * dst)
        {
            size_t noseH = p.padY, noseW = p.padX;
            size_t bodyH = p.srcH - p.kernelY + 1 + noseH, bodyW = p.srcW - p.kernelX + 1 + noseW;
            size_t tailH = bodyH + p.padH, tailW = bodyW + p.padW;
            size_t bodyW2 = AlignLoAny(bodyW - noseW, 2 * p.strideX) + noseW;
            size_t bodyW6 = AlignLoAny(bodyW - noseW, 6 * p.strideX) + noseW;
            size_t wS = p.srcC*p.dstC;
            size_t kY = p.kernelY - noseH, kX = p.kernelX - noseW, kH = bodyH + p.kernelY - 1, kW = bodyW + p.kernelX - 1;
            size_t sy = 0;
            for (; sy < noseH; sy += p.strideY)
            {
                size_t sx = 0;
                const float * w = weight + (noseH - sy) * p.kernelY * wS;
                for (; sx < noseW; sx += p.strideX, dst += p.dstC)
                    KernelHwcDefaultEdge<type>(src, p, kY + sy, kX + sx, w + (noseW - sx)*wS, bias, params, dst);
                for (; sx < bodyW; sx += p.strideX, dst += p.dstC)
                    KernelHwcDefaultEdge<type>(src + (sx - noseW) * p.srcC, p, kY + sy, p.kernelX, w, bias, params, dst);
                for (; sx < tailW; sx += p.strideX, dst += p.dstC)
                    KernelHwcDefaultEdge<type>(src + (sx - noseW) * p.srcC, p, kY + sy, kW - sx, w, bias, params, dst);
            }
            src += (sy - noseH)*p.srcW*p.srcC;
            for (; sy < bodyH; sy += p.strideY)
            {
                size_t sx = 0;
                for (; sx < noseW; sx += p.strideX, dst += p.dstC)
                    KernelHwcDefaultEdge<type>(src, p, p.kernelY, kX + sx, weight + (noseW - sx)*wS, bias, params, dst);
                for (; sx < bodyW6; sx += 6 * p.strideX, dst += 6 * p.dstC)
                    KernelHwcDefaultBody6<type>(src + (sx - noseW) * p.srcC, p, weight, bias, params, dst);
                for (; sx < bodyW2; sx += 2 * p.strideX, dst += 2 * p.dstC)
                    KernelHwcDefaultBody2<type>(src + (sx - noseW) * p.srcC, p, weight, bias, params, dst);
                for (; sx < bodyW; sx += p.strideX, dst += p.dstC)
                    KernelHwcDefaultEdge<type>(src + (sx - noseW) * p.srcC, p, p.kernelY, p.kernelX, weight, bias, params, dst);
                for (; sx < tailW; sx += p.strideX, dst += p.dstC)
                    KernelHwcDefaultEdge<type>(src + (sx - noseW) * p.srcC, p, p.kernelY, kW - sx, weight, bias, params, dst);
                src += p.strideY*p.srcW*p.srcC;
            }
            for (; sy < tailH; sy += p.strideY)
            {
                size_t sx = 0;
                for (; sx < noseW; sx += p.strideX, dst += p.dstC)
                    KernelHwcDefaultEdge<type>(src, p, kH - sy, kX + sx, weight + (noseW - sx)*wS, bias, params, dst);
                for (; sx < bodyW; sx += p.strideX, dst += p.dstC)
                    KernelHwcDefaultEdge<type>(src + (sx - noseW) * p.srcC, p, kH - sy, p.kernelX, weight, bias, params, dst);
                for (; sx < tailW; sx += p.strideX, dst += p.dstC)
                    KernelHwcDefaultEdge<type>(src + (sx - noseW) * p.srcC, p, kH - sy, kW - sx, weight, bias, params, dst);
                src += p.strideY*p.srcW*p.srcC;
            }
        }

        template<::SimdConvolutionActivationType type> void ConvolutionDirectHwcConvolutionBiasActivationDepthwise(const float * src, const ConvParam & p, const float * weight, const float * bias, const float * params, float * dst)
        {
            size_t size = p.group;
            size_t sizeF = AlignLo(size, F);
            size_t size2F = AlignLo(size, 2 * F);
            size_t size4F = AlignLo(size, 4 * F);
            size_t size8F = AlignLo(size, 8 * F);
            for (size_t dy = 0; dy < p.dstH; ++dy)
            {
                for (size_t dx = 0; dx < p.dstW; ++dx)
                {
                    size_t i = 0;
                    for (; i < size8F; i += 8 * F)
                    {
                        __m512 sums[8];
                        if (bias)
                        {
                            sums[0] = _mm512_loadu_ps(bias + i + 0 * F);
                            sums[1] = _mm512_loadu_ps(bias + i + 1 * F);
                            sums[2] = _mm512_loadu_ps(bias + i + 2 * F);
                            sums[3] = _mm512_loadu_ps(bias + i + 3 * F);
                            sums[4] = _mm512_loadu_ps(bias + i + 4 * F);
                            sums[5] = _mm512_loadu_ps(bias + i + 5 * F);
                            sums[6] = _mm512_loadu_ps(bias + i + 6 * F);
                            sums[7] = _mm512_loadu_ps(bias + i + 7 * F);
                        }
                        else
                        {
                            sums[0] = _mm512_setzero_ps();
                            sums[1] = _mm512_setzero_ps();
                            sums[2] = _mm512_setzero_ps();
                            sums[3] = _mm512_setzero_ps();
                            sums[4] = _mm512_setzero_ps();
                            sums[5] = _mm512_setzero_ps();
                            sums[6] = _mm512_setzero_ps();
                            sums[7] = _mm512_setzero_ps();
                        }
                        for (size_t ky = 0; ky < p.kernelY; ++ky)
                        {
                            size_t sy = dy * p.strideY + ky * p.dilationY - p.padY;
                            if (sy < p.srcH)
                            {
                                for (size_t kx = 0; kx < p.kernelX; ++kx)
                                {
                                    size_t sx = dx * p.strideX + kx * p.dilationX - p.padX;
                                    if (sx < p.srcW)
                                    {
                                        const float * pw = weight + (ky*p.kernelX + kx)*size + i;
                                        const float * ps = src + (sy*p.srcW + sx)*size + i;
                                        sums[0] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 0 * F), _mm512_loadu_ps(pw + 0 * F), sums[0]);
                                        sums[1] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 1 * F), _mm512_loadu_ps(pw + 1 * F), sums[1]);
                                        sums[2] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 2 * F), _mm512_loadu_ps(pw + 2 * F), sums[2]);
                                        sums[3] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 3 * F), _mm512_loadu_ps(pw + 3 * F), sums[3]);
                                        sums[4] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 4 * F), _mm512_loadu_ps(pw + 4 * F), sums[4]);
                                        sums[5] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 5 * F), _mm512_loadu_ps(pw + 5 * F), sums[5]);
                                        sums[6] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 6 * F), _mm512_loadu_ps(pw + 6 * F), sums[6]);
                                        sums[7] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 7 * F), _mm512_loadu_ps(pw + 7 * F), sums[7]);
                                    }
                                }
                            }
                        }
                        _mm512_storeu_ps(dst + i + 0 * F, Activate<type>(sums[0], params, i + 0 * F));
                        _mm512_storeu_ps(dst + i + 1 * F, Activate<type>(sums[1], params, i + 1 * F));
                        _mm512_storeu_ps(dst + i + 2 * F, Activate<type>(sums[2], params, i + 2 * F));
                        _mm512_storeu_ps(dst + i + 3 * F, Activate<type>(sums[3], params, i + 3 * F));
                        _mm512_storeu_ps(dst + i + 4 * F, Activate<type>(sums[4], params, i + 4 * F));
                        _mm512_storeu_ps(dst + i + 5 * F, Activate<type>(sums[5], params, i + 5 * F));
                        _mm512_storeu_ps(dst + i + 6 * F, Activate<type>(sums[6], params, i + 6 * F));
                        _mm512_storeu_ps(dst + i + 7 * F, Activate<type>(sums[7], params, i + 7 * F));
                    }
                    for (; i < size4F; i += 4 * F)
                    {
                        __m512 sums[4];
                        if (bias)
                        {
                            sums[0] = _mm512_loadu_ps(bias + i + 0 * F);
                            sums[1] = _mm512_loadu_ps(bias + i + 1 * F);
                            sums[2] = _mm512_loadu_ps(bias + i + 2 * F);
                            sums[3] = _mm512_loadu_ps(bias + i + 3 * F);
                        }
                        else
                        {
                            sums[0] = _mm512_setzero_ps();
                            sums[1] = _mm512_setzero_ps();
                            sums[2] = _mm512_setzero_ps();
                            sums[3] = _mm512_setzero_ps();
                        }
                        for (size_t ky = 0; ky < p.kernelY; ++ky)
                        {
                            size_t sy = dy * p.strideY + ky * p.dilationY - p.padY;
                            if (sy < p.srcH)
                            {
                                for (size_t kx = 0; kx < p.kernelX; ++kx)
                                {
                                    size_t sx = dx * p.strideX + kx * p.dilationX - p.padX;
                                    if (sx < p.srcW)
                                    {
                                        const float * pw = weight + (ky*p.kernelX + kx)*size + i;
                                        const float * ps = src + (sy*p.srcW + sx)*size + i;
                                        sums[0] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 0 * F), _mm512_loadu_ps(pw + 0 * F), sums[0]);
                                        sums[1] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 1 * F), _mm512_loadu_ps(pw + 1 * F), sums[1]);
                                        sums[2] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 2 * F), _mm512_loadu_ps(pw + 2 * F), sums[2]);
                                        sums[3] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 3 * F), _mm512_loadu_ps(pw + 3 * F), sums[3]);
                                    }
                                }
                            }
                        }
                        _mm512_storeu_ps(dst + i + 0 * F, Activate<type>(sums[0], params, i + 0 * F));
                        _mm512_storeu_ps(dst + i + 1 * F, Activate<type>(sums[1], params, i + 1 * F));
                        _mm512_storeu_ps(dst + i + 2 * F, Activate<type>(sums[2], params, i + 2 * F));
                        _mm512_storeu_ps(dst + i + 3 * F, Activate<type>(sums[3], params, i + 3 * F));
                    }
                    for (; i < size2F; i += 2 * F)
                    {
                        __m512 sums[2];
                        if (bias)
                        {
                            sums[0] = _mm512_loadu_ps(bias + i + 0 * F);
                            sums[1] = _mm512_loadu_ps(bias + i + 1 * F);
                        }
                        else
                        {
                            sums[0] = _mm512_setzero_ps();
                            sums[1] = _mm512_setzero_ps();
                        }
                        for (size_t ky = 0; ky < p.kernelY; ++ky)
                        {
                            size_t sy = dy * p.strideY + ky * p.dilationY - p.padY;
                            if (sy < p.srcH)
                            {
                                for (size_t kx = 0; kx < p.kernelX; ++kx)
                                {
                                    size_t sx = dx * p.strideX + kx * p.dilationX - p.padX;
                                    if (sx < p.srcW)
                                    {
                                        const float * pw = weight + (ky*p.kernelX + kx)*size + i;
                                        const float * ps = src + (sy*p.srcW + sx)*size + i;
                                        sums[0] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 0 * F), _mm512_loadu_ps(pw + 0 * F), sums[0]);
                                        sums[1] = _mm512_fmadd_ps(_mm512_loadu_ps(ps + 1 * F), _mm512_loadu_ps(pw + 1 * F), sums[1]);
                                    }
                                }
                            }
                        }
                        _mm512_storeu_ps(dst + i + 0 * F, Activate<type>(sums[0], params, i + 0 * F));
                        _mm512_storeu_ps(dst + i + 1 * F, Activate<type>(sums[1], params, i + 1 * F));
                    }
                    for (; i < size; i += F)
                    {
                        __mmask16 tail = i < sizeF ? __mmask16(-1) : TailMask16(size - i);
                        __m512 sum = bias ? _mm512_maskz_loadu_ps(tail, bias + i) : _mm512_setzero_ps();
                        for (size_t ky = 0; ky < p.kernelY; ++ky)
                        {
                            size_t sy = dy * p.strideY + ky * p.dilationY - p.padY;
                            if (sy < p.srcH)
                            {
                                for (size_t kx = 0; kx < p.kernelX; ++kx)
                                {
                                    size_t sx = dx * p.strideX + kx * p.dilationX - p.padX;
                                    if (sx < p.srcW)
                                    {
                                        const float * pw = weight + (ky*p.kernelX + kx)*size + i;
                                        const float * ps = src + (sy*p.srcW + sx)*size + i;
                                        sum = _mm512_fmadd_ps(_mm512_maskz_loadu_ps(tail, ps), _mm512_maskz_loadu_ps(tail, pw), sum);
                                    }
                                }
                            }
                        }
                        _mm512_mask_storeu_ps(dst + i, tail, Activate<type>(sum, params, i, tail));
                    }
                    dst += p.dstC;
                }
            }
        }

        template <::SimdConvolutionActivationType type> ConvolutionDirectHwc::ConvolutionBiasActivationPtr GetConvolutionBiasActivation(const ConvParam & p)
        {
            if (p.group == 1)
                return ConvolutionDirectHwcConvolutionBiasActivationDefault<type>;
            else if (p.IsDepthwise())
                return ConvolutionDirectHwcConvolutionBiasActivationDepthwise<type>;
            return NULL;
        }

        ConvolutionDirectHwc::ConvolutionBiasActivationPtr ConvolutionDirectHwc::SetConvolutionBiasActivation()
        {
            const ConvParam & p = _param;
            ConvolutionDirectHwc::ConvolutionBiasActivationPtr func = NULL;
            if (p.dstC > HF && p.dstH >= p.padY + p.padH && p.dstW >= p.padX + p.padW)
            {
                switch (p.activation)
                {
                case ::SimdConvolutionActivationIdentity: func = GetConvolutionBiasActivation<::SimdConvolutionActivationIdentity>(p); break;
                case ::SimdConvolutionActivationRelu: func = GetConvolutionBiasActivation<::SimdConvolutionActivationRelu>(p); break;
                case ::SimdConvolutionActivationLeakyRelu: func = GetConvolutionBiasActivation<::SimdConvolutionActivationLeakyRelu>(p); break;
                case ::SimdConvolutionActivationRestrictRange: func = GetConvolutionBiasActivation<::SimdConvolutionActivationRestrictRange>(p); break;
                case ::SimdConvolutionActivationPrelu: func = GetConvolutionBiasActivation<::SimdConvolutionActivationPrelu>(p); break;
                }
            }
            return func ? func : Avx2::ConvolutionDirectHwc::SetConvolutionBiasActivation();
        };

        //---------------------------------------------------------------------

        void * ConvolutionInit(size_t srcC, size_t srcH, size_t srcW, SimdBool srcT, size_t dstC, SimdBool dstT,
            size_t kernelY, size_t kernelX, size_t dilationY, size_t dilationX, size_t strideY, size_t strideX,
            size_t padY, size_t padX, size_t padH, size_t padW, size_t group, SimdConvolutionActivationType activation)
        {
            ConvParam param(srcC, srcH, srcW, srcT, dstC, dstT, kernelY, kernelX, dilationY, dilationX, strideY, strideX, padY, padX, padH, padW, group, activation);
            if (!param.Valid())
                return NULL;
            else if (Avx::ConvolutionDepthwiseDotProduct::Preferable(param))
                return new Avx::ConvolutionDepthwiseDotProduct(param);
            else if (ConvolutionWinograd2x3p::Preferable(param))
                return new ConvolutionWinograd2x3p(param);
            else if (ConvolutionGemmNT::Preferable(param))
                return new ConvolutionGemmNT(param);
            else if (ConvolutionDirectChw::Preferable(param))
                return new Avx512f::ConvolutionDirectChw(param);
            else if (ConvolutionDirectHwc::Preferable(param))
                return new ConvolutionDirectHwc(param);
            else
                return new ConvolutionGemmNN(param);
        }
    }
#endif//SIMD_AVX512F_ENABLE
}
