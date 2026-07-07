/* Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <math.h>

////////////////////////////////////////////////////////////////////////////////
// Polynomial approximation of cumulative normal distribution function
////////////////////////////////////////////////////////////////////////////////
static double CND(double d) {
  const double A1 = 0.31938153;
  const double A2 = -0.356563782;
  const double A3 = 1.781477937;
  const double A4 = -1.821255978;
  const double A5 = 1.330274429;
  const double RSQRT2PI = 0.39894228040143267793994605993438;

  double K = 1.0 / (1.0 + 0.2316419 * fabs(d));

  double cnd = RSQRT2PI * exp(-0.5 * d * d) *
               (K * (A1 + K * (A2 + K * (A3 + K * (A4 + K * A5)))));

  if (d > 0) cnd = 1.0 - cnd;

  return cnd;
}

////////////////////////////////////////////////////////////////////////////////
// Black-Scholes formula for both call and put
////////////////////////////////////////////////////////////////////////////////
static void BlackScholesBodyCPU(float &callResult, float &putResult,
                                float Sf,  // Stock price
                                float Xf,  // Option strike
                                float Tf,  // Option years
                                float Rf,  // Riskless rate
                                float Vf  // Volatility rate
                                ) {
  double S = Sf, X = Xf, T = Tf, R = Rf, V = Vf;

  double sqrtT = sqrt(T);
  double d1 = (log(S / X) + (R + 0.5 * V * V) * T) / (V * sqrtT);
  double d2 = d1 - V * sqrtT;
  double CNDD1 = CND(d1);
  double CNDD2 = CND(d2);

  // Calculate Call and Put simultaneously
  double expRT = exp(-R * T);
  callResult = (float)(S * CNDD1 - X * expRT * CNDD2);
  putResult = (float)(X * expRT * (1.0 - CNDD2) - S * (1.0 - CNDD1));
}

////////////////////////////////////////////////////////////////////////////////
// Process an array of optN options
////////////////////////////////////////////////////////////////////////////////
extern "C" void BlackScholesCPU(float *h_CallResult, float *h_PutResult,
                                float *h_StockPrice, float *h_OptionStrike,
                                float *h_OptionYears, float Riskfree,
                                float Volatility, int optN) {
  for (int opt = 0; opt < optN; opt++)
    BlackScholesBodyCPU(h_CallResult[opt], h_PutResult[opt], h_StockPrice[opt],
                        h_OptionStrike[opt], h_OptionYears[opt], Riskfree,
                        Volatility);
}


////////////////////////////////////////////////////////////////////////////////
// Polynomial approximation of cumulative normal distribution function
////////////////////////////////////////////////////////////////////////////////
__device__ inline float cndGPU(float d) {
  const float A1 = 0.31938153f;
  const float A2 = -0.356563782f;
  const float A3 = 1.781477937f;
  const float A4 = -1.821255978f;
  const float A5 = 1.330274429f;
  const float RSQRT2PI = 0.39894228040143267793994605993438f;

  float K = __fdividef(1.0f, (1.0f + 0.2316419f * fabsf(d)));

  float cnd = RSQRT2PI * __expf(-0.5f * d * d) *
              (K * (A1 + K * (A2 + K * (A3 + K * (A4 + K * A5)))));

  if (d > 0) cnd = 1.0f - cnd;

  return cnd;
}

////////////////////////////////////////////////////////////////////////////////
// Black-Scholes formula for both call and put
////////////////////////////////////////////////////////////////////////////////
__device__ inline void BlackScholesBodyGPU(float &CallResult, float &PutResult,
                                           float S,  // Stock price
                                           float X,  // Option strike
                                           float T,  // Option years
                                           float R,  // Riskless rate
                                           float V  // Volatility rate
                                           ) {
  float sqrtT, expRT;
  float d1, d2, CNDD1, CNDD2;

  sqrtT = __fdividef(1.0F, rsqrtf(T));
  d1 = __fdividef(__logf(S / X) + (R + 0.5f * V * V) * T, V * sqrtT);
  d2 = d1 - V * sqrtT;

  CNDD1 = cndGPU(d1);
  CNDD2 = cndGPU(d2);

  // Calculate Call and Put simultaneously
  expRT = __expf(-R * T);
  CallResult = S * CNDD1 - X * expRT * CNDD2;
  PutResult = X * expRT * (1.0f - CNDD2) - S * (1.0f - CNDD1);
}

////////////////////////////////////////////////////////////////////////////////
// Process an array of optN options on GPU
////////////////////////////////////////////////////////////////////////////////
__launch_bounds__(128) __global__
    void BlackScholesGPU(float2 *__restrict d_CallResult,
                         float2 *__restrict d_PutResult,
                         float2 *__restrict d_StockPrice,
                         float2 *__restrict d_OptionStrike,
                         float2 *__restrict d_OptionYears, float Riskfree,
                         float Volatility, int optN) {
  ////Thread index
  // const int      tid = blockDim.x * blockIdx.x + threadIdx.x;
  ////Total number of threads in execution grid
  // const int THREAD_N = blockDim.x * gridDim.x;

  const int opt = blockDim.x * blockIdx.x + threadIdx.x;

  // Calculating 2 options per thread to increase ILP (instruction level
  // parallelism)
  if (opt < (optN / 2)) {
    float callResult1, callResult2;
    float putResult1, putResult2;
    BlackScholesBodyGPU(callResult1, putResult1, d_StockPrice[opt].x,
                        d_OptionStrike[opt].x, d_OptionYears[opt].x, Riskfree,
                        Volatility);
    BlackScholesBodyGPU(callResult2, putResult2, d_StockPrice[opt].y,
                        d_OptionStrike[opt].y, d_OptionYears[opt].y, Riskfree,
                        Volatility);
    d_CallResult[opt] = make_float2(callResult1, callResult2);
    d_PutResult[opt] = make_float2(putResult1, putResult2);
  }
}
