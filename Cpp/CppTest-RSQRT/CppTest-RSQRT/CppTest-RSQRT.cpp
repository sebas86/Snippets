// CppTest-RSQRT.cpp : Ten plik zawiera funkcję „main”. W nim rozpoczyna się i kończy wykonywanie programu.
//

#include <iostream>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>
#include <cinttypes>

#include <intrin.h>

using std::cin;
using std::cout;
using std::endl;
using std::flush;

using single_float_operation = float (*)(float);

struct FloatOperationBenchResult
{
    double duration;
    float result;
    const char* name;
};

/* See
https://randomascii.wordpress.com/2012/01/11/tricks-with-the-floating-point-format/
for the potential portability problems with the union and bit-fields below.
*/
union Float_t
{
    Float_t(float num = 0.0f) : f(num) {}
    int32_t RawSign() const { return (i >> 31) & 1; }
    int32_t RawMantissa() const { return i & ((1 << 23) - 1); }
    int32_t RawExponent() const { return (i >> 23) & 0xFF; }

    int32_t i;
    float f;
};

class Timer
{
private:
    std::chrono::steady_clock::time_point _startTime;
    double duration;
    bool started;

    static inline auto getCurrentTime()
    {
        return std::chrono::high_resolution_clock::now();
    }

public:
    Timer() : duration(0.0), started(false) {}

    void start()
    {
        _startTime = getCurrentTime();
        started = true;
    }

    void stop()
    {
        if (!started)
            return;
        constexpr double nanosecondsInSecond = (1000.0 * 1000.0 * 1000.0);
        duration = (getCurrentTime() - _startTime).count() / nanosecondsInSecond;
        started = false;
    }

    inline double getDuration() const
    {
        return duration;
    }
};


template<single_float_operation op>
FloatOperationBenchResult TestSum(size_t iterations, const char* name)
{
    Timer timer;
    timer.start();
    float sum = 0.0f;
    for (size_t i = 1; i <= iterations; ++i)
    {
        float test_sample = static_cast<float>(i) / static_cast<float>(iterations);
        sum += op(test_sample);
    }
    timer.stop();
    return { timer.getDuration(), sum, name };
}

void IterateAllPositiveFloats(void(*op)(void* userData, float value, int32_t index), void* userData)
{
    Float_t allFloats;
    allFloats.f = 0.0f;

    op(userData, allFloats.f, allFloats.i);

#if defined(_DEBUG)
    while (allFloats.RawExponent() < 2)
#else
    while (allFloats.RawExponent() < 255)
#endif
    {
        allFloats.i += 1;
        op(userData, allFloats.f, allFloats.i);
    }
}

float InvSqrtReference(float arg)
{
    return 1.0f / std::sqrt(arg);
}

float InvSqrtAccurate(float arg)
{
    const __m128 vec = _mm_load_ss(&arg);
    const __m128 sqrt = _mm_sqrt_ss(vec);
    const __m128 rsqrt = _mm_div_ss(_mm_set_ss(1.0f), sqrt);
    return _mm_cvtss_f32(rsqrt);
}

float InvSqrtAccurate2(float arg)
{
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(1.0f), _mm_sqrt_ss(_mm_load_ss(&arg))));
}

float InvSqrtFast(float arg)
{
    const __m128 vec = _mm_load_ss(&arg);
    const __m128 guess = _mm_rsqrt_ss(vec);
    return _mm_cvtss_f32(guess);
}

float InvSqrtFast2(float arg)
{
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_load_ss(&arg)));
}

float InvSqrtImprovedFast(float arg)
{
    const __m128 vec = _mm_load_ss(&arg);
    __m128 guess = _mm_rsqrt_ss(vec);
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(_mm_set_ss(-0.5f), _mm_mul_ss(vec, _mm_mul_ss(guess, guess)))));
    return _mm_cvtss_f32(guess);
}

float InvSqrtImprovedFast2(float arg)
{
    const __m128 vec = _mm_load_ss(&arg);
    __m128 guess = _mm_rsqrt_ss(vec);
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(_mm_set_ss(-0.5f), _mm_mul_ss(vec, _mm_mul_ss(guess, guess)))));
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(_mm_set_ss(-0.5f), _mm_mul_ss(vec, _mm_mul_ss(guess, guess)))));
    return _mm_cvtss_f32(guess);
}

float InvSqrtImprovedFast3(float arg)
{
    const __m128 vec = _mm_load_ss(&arg);
    const __m128 vec2 = _mm_mul_ss(_mm_set_ss(-0.5f), vec);
    __m128 guess = _mm_rsqrt_ss(vec);
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(vec2, _mm_mul_ss(guess, guess))));
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(vec2, _mm_mul_ss(guess, guess))));
    return _mm_cvtss_f32(guess);
}

constexpr int32_t least_significant_mantisa_mask = 0b11111111111111111110000000000000;

float InvSqrtFastMasked(float arg)
{
    const __m128 vec = _mm_load_ss(&arg);
    const __m128 mantisa_mask = _mm_castsi128_ps(_mm_set1_epi32(least_significant_mantisa_mask));
    __m128 guess = _mm_rsqrt_ss(vec);
    guess = _mm_and_ps(mantisa_mask, guess);
    return _mm_cvtss_f32(guess);
}

float InvSqrtImprovedFastMasked(float arg)
{
    const __m128 vec = _mm_load_ss(&arg);
    const __m128 mantisa_mask = _mm_castsi128_ps(_mm_set1_epi32(least_significant_mantisa_mask));
    __m128 guess = _mm_rsqrt_ss(vec);
    guess = _mm_and_ps(mantisa_mask, guess);
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(_mm_set_ss(-0.5f), _mm_mul_ss(vec, _mm_mul_ss(guess, guess)))));
    return _mm_cvtss_f32(guess);
}

float InvSqrtImprovedFastMasked2(float arg)
{
    const __m128 vec = _mm_load_ss(&arg);
    const __m128 mantisa_mask = _mm_castsi128_ps(_mm_set1_epi32(least_significant_mantisa_mask));
    __m128 guess = _mm_rsqrt_ss(vec);
    guess = _mm_and_ps(mantisa_mask, guess);
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(_mm_set_ss(-0.5f), _mm_mul_ss(vec, _mm_mul_ss(guess, guess)))));
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(_mm_set_ss(-0.5f), _mm_mul_ss(vec, _mm_mul_ss(guess, guess)))));
    return _mm_cvtss_f32(guess);
}

float InvSqrtSoftFastApprox(float arg)
{
    const int32_t guessInt = 0x5f3759df - ((*reinterpret_cast<uint32_t*>(&arg)) >> 1);
    return *reinterpret_cast<const float*>(&guessInt);
}

float InvSqrtSoftFastApprox2(float arg)
{
    const int32_t guessInt = 0x5F1FFFF9 - ((*reinterpret_cast<uint32_t*>(&arg)) >> 1);
    return *reinterpret_cast<const float*>(&guessInt);
}

float InvSqrtSoftFastApproxSSE(float arg)
{
    const __m128 number = _mm_load_ss(&arg);
    __m128 guess = _mm_castsi128_ps(_mm_sub_epi32(_mm_set1_epi32(0x5f3759df), _mm_srai_epi32(_mm_castps_si128(number), 1)));
    return _mm_cvtss_f32(guess);
}

float InvSqrtSoftFastApproxSSE2(float arg)
{
    const __m128 number = _mm_load_ss(&arg);
    __m128 guess = _mm_castsi128_ps(_mm_sub_epi32(_mm_set1_epi32(0x5F1FFFF9), _mm_srai_epi32(_mm_castps_si128(number), 1)));
    return _mm_cvtss_f32(guess);
}

float InvSqrtSoftFastApproxImproved(float arg)
{
    uint32_t i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = arg * 0.5F;
    y = arg;
    //memcpy(&i, &y, sizeof(y));
    i = *(uint32_t*)&y;
    i = 0x5f3759df - (i >> 1);
    //memcpy(&y, &i, sizeof(y));
    y = *(float*)&i;
    y = y * (threehalfs - (x2 * y * y));   // 1st iteration
//	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed
    return y;
}

float InvSqrtSoftFastApproxImproved2(float arg)
{
    uint32_t i;
    float y;

    y = arg;
    i = *(uint32_t*)&y;
    i = 0x5F1FFFF9 - (i >> 1);
    y = *(float*)&i;
    y =  y * (0.703952253f * (2.38924456f - (arg * y * y)));
    return y;
}

float InvSqrtSoftFastApproxImproved3(float arg)
{
    uint32_t i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = arg * 0.5F;
    y = arg;
    memcpy(&i, &y, sizeof(y));
    i = 0x5f3759df - (i >> 1);
    memcpy(&y, &i, sizeof(y));
    y = y * (threehalfs - (x2 * y * y));
    return y;
}

float InvSqrtSoftFastApproxImproved4(float arg)
{
    uint32_t i;
    float y;

    y = arg;
    memcpy(&i, &y, sizeof(y));
    i = 0x5F1FFFF9 - (i >> 1);
    memcpy(&y, &i, sizeof(y));
    y = y * (0.703952253f * (2.38924456f - (arg * y * y)));
    return y;
}

float InvSqrtSoftFastApproxImprovedSSE1(float arg)
{
    const int32_t guessInt = 0x5f3759df - ((*reinterpret_cast<uint32_t*>(&arg)) >> 1);
    __m128 guess = _mm_castsi128_ps(_mm_set1_epi32(guessInt));
    const __m128 arg2 = _mm_mul_ss(_mm_set_ss(-0.5f), _mm_load_ss(&arg));
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(arg2, _mm_mul_ss(guess, guess))));
    return _mm_cvtss_f32(guess);
}

float InvSqrtSoftFastApproxImprovedSSE2(float arg)
{
    const int32_t guessInt = 0x5F1FFFF9 - ((*reinterpret_cast<uint32_t*>(&arg)) >> 1);
    __m128 guess = _mm_castsi128_ps(_mm_set1_epi32(guessInt));
    guess = _mm_mul_ss(_mm_set_ss(0.703952253f), _mm_mul_ss(guess, _mm_sub_ss(_mm_set_ss(2.38924456f), _mm_mul_ss(_mm_load_ss(&arg), _mm_mul_ss(guess, guess)))));
    return _mm_cvtss_f32(guess);
}

float InvSqrtSoftFastApproxImprovedSSE3(float arg)
{
    const __m128 number = _mm_load_ss(&arg);
    __m128 guess = _mm_castsi128_ps(_mm_sub_epi32(_mm_set1_epi32(0x5f3759df), _mm_srai_epi32(_mm_castps_si128(number), 1)));
    const __m128 arg2 = _mm_mul_ss(_mm_set_ss(-0.5f), number);
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(arg2, _mm_mul_ss(guess, guess))));
    return _mm_cvtss_f32(guess);
}

float InvSqrtSoftFastApproxImprovedSSE4(float arg)
{
    const __m128 number = _mm_load_ss(&arg);
    __m128 guess = _mm_castsi128_ps(_mm_sub_epi32(_mm_set1_epi32(0x5F1FFFF9), _mm_srai_epi32(_mm_castps_si128(number), 1)));
    guess = _mm_mul_ss(_mm_set_ss(0.703952253f), _mm_mul_ss(guess, _mm_sub_ss(_mm_set_ss(2.38924456f), _mm_mul_ss(number, _mm_mul_ss(guess, guess)))));
    return _mm_cvtss_f32(guess);
}

void bench_rsqrt()
{
    constexpr size_t baseIterations = 1000 * 1000;
    constexpr size_t repeats = 20;
    constexpr size_t tests = 23;
    constexpr double singleTestDesiredDuration = 0.1;

    size_t iterations = baseIterations * 10;

#if 0
    // Estimate iterations count to get around 0.1s of first test duration
    auto result = TestSum<InvSqrtReference>(iterations, "initial");
    iterations = std::max(static_cast<size_t>(1), static_cast<size_t>(iterations / baseIterations * singleTestDesiredDuration / result.duration)) * baseIterations;
#endif

    FloatOperationBenchResult benchmarks[repeats][tests];
    for (size_t i = 0; i < repeats; ++i)
    {
        size_t test = 0;
        benchmarks[i][test++] = TestSum<InvSqrtReference>(iterations, "Reference");
        benchmarks[i][test++] = TestSum<InvSqrtAccurate>(iterations, "Hardware accurate");
        benchmarks[i][test++] = TestSum<InvSqrtAccurate2>(iterations, "Hardware accurate 2");
        benchmarks[i][test++] = TestSum<InvSqrtFast>(iterations, "Hardware fast");
        benchmarks[i][test++] = TestSum<InvSqrtFast2>(iterations, "Hardware fast 2");
        benchmarks[i][test++] = TestSum<InvSqrtImprovedFast>(iterations, "Hardware fast + single Newton-Raphson iteration");
        benchmarks[i][test++] = TestSum<InvSqrtImprovedFast2>(iterations, "Hardware fast + two Newton-Raphson iterations");
        benchmarks[i][test++] = TestSum<InvSqrtImprovedFast3>(iterations, "Hardware fast + two Newton-Raphson iterations (+ optimization)");
        benchmarks[i][test++] = TestSum<InvSqrtFastMasked>(iterations, "Hardware fast limited to 11bit preccission");
        benchmarks[i][test++] = TestSum<InvSqrtImprovedFastMasked>(iterations, "Hardware fast limited to 11bit preccission + single Newton-Raphson iteration");
        benchmarks[i][test++] = TestSum<InvSqrtImprovedFastMasked2>(iterations, "Hardware fast limited to 11bit preccission + two Newton-Raphson iterationsa");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApprox>(iterations, "Software fast approx");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApprox2>(iterations, "Software fast approx (better constant)");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApproxSSE>(iterations, "Software fast approx (SSE)");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApproxSSE2>(iterations, "Software fast approx (SSE, better constant)");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApproxImproved>(iterations, "Software fast approx + single Newton-Raphson iteration (unsafe cast)");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApproxImproved2>(iterations, "Software fast approx + single Newton-Raphson iteration (unsafe cast, better constants)");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApproxImproved3>(iterations, "Software fast approx + single Newton-Raphson iteration (memcopy instead unsafe cast)");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApproxImproved4>(iterations, "Software fast approx + single Newton-Raphson iteration (memcopy instead unsafe cast, better constants)");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApproxImprovedSSE1>(iterations, "Software fast approx + single Newton-Raphson iteration (integer on ALU, float on SSE)");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApproxImprovedSSE2>(iterations, "Software fast approx + single Newton-Raphson iteration (integer on ALU, float on SSE, better constants)");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApproxImprovedSSE3>(iterations, "Software fast approx + single Newton-Raphson iteration (all on SSE)");
        benchmarks[i][test++] = TestSum<InvSqrtSoftFastApproxImprovedSSE4>(iterations, "Software fast approx + single Newton-Raphson iteration (all on SSE, better constants)");
        assert(test == tests);
    }

    double referenceAvg = 0.0f;
    double referenceMedian = 0.0f;

    cout << "Iterations: " << iterations << ". Repeats: " << repeats << "." << endl;
    for (size_t test = 0; test < tests; ++test)
    {
        const auto& firstBench = benchmarks[0][test];

        std::vector<double> durations;
        for (size_t i = 0; i < repeats; ++i)
        {
            const auto bench = benchmarks[i][test];
            for (size_t j = i + 1; j < repeats; ++j)
            {
                const auto otherBench = benchmarks[j][test];
                assert(bench.result == otherBench.result && "corrupted data");
            }
            durations.push_back(bench.duration);
        }
        std::sort(durations.begin(), durations.end());
        const auto avg = std::accumulate(durations.begin(), durations.end(), 0.0) / static_cast<double>(repeats);
        const auto median = durations[repeats / 2];
        if (test == 0)
        {
            referenceAvg = avg;
            referenceMedian = median;
        }
        
        cout << "Test: " << firstBench.name << endl;
        cout << "\t- result:            " << firstBench.result << endl;
        cout << "\t- avg duration:      " << avg << endl;
        cout << "\t- median duration:   " << median << endl;
        cout << "\t- avg speed gain:    " << (referenceAvg / avg) << endl;
        cout << "\t- median speed gain: " << (referenceMedian / median) << endl;
    }
}

struct Error
{
    float errorValue;
    float inputValue;
    float outputValue1;
    float outputValue2;

    void setAll(float aErrorValue, float aInputValue, float aOutputValue1, float aOutputValue2)
    {
        errorValue = aErrorValue;
        inputValue = aInputValue;
        outputValue1 = aOutputValue1;
        outputValue2 = aOutputValue2;
    }
};

struct ErrorTestData
{
    Error errorMin;
    Error errorMax;
    float inputValueMin;
    float inputValueMax;
    float outputForInputValueMin;
    float outputForInputValueMax;
    double errorAvg;
    uint32_t samples;
    bool hasResultNaN;

    ErrorTestData()
    {
        errorMin.setAll(std::numeric_limits<float>::infinity(), 0, 0, 0);
        errorMax.setAll(0, 0, 0, 0);
        inputValueMin = std::numeric_limits<float>::infinity();
        inputValueMax = 0;
        outputForInputValueMin = 0;
        outputForInputValueMax = 0;
        errorAvg = 0;
        samples = 0;
        hasResultNaN = false;
    }

    void update(float inputValue, float result1, float result2)
    {
        if (std::isnan(inputValue))
            return;

        if (std::isnan(result2))
            hasResultNaN = true;

        if (inputValueMin > inputValue)
        {
            inputValueMin = inputValue;
            outputForInputValueMin = result2;
        }
        if (inputValueMax < inputValue)
        {
            inputValueMax = inputValue;
            outputForInputValueMax = result2;
        }

        if (std::isnan(result1) || std::isnan(result2))
            return;

        // ignore inf values
        if (result1 < 0)
            result1 = -result1;
        if (result2 < 0)
            result2 = -result2;
        if (result1 > std::numeric_limits<float>::max() || result2 > std::numeric_limits<float>::max())
            return;

        double errorHighPrecision = (double)result2 - (double)result1;
        if (result1 > 0)
            errorHighPrecision /= (double)result1;
        if (errorHighPrecision < 0)
            errorHighPrecision = -errorHighPrecision;

        const float error = (float)errorHighPrecision;

        if (errorMin.errorValue > error)
            errorMin.setAll(error, inputValue, result1, result2);
        if (errorMax.errorValue < error)
            errorMax.setAll(error, inputValue, result1, result2);

        ++samples;
        errorAvg = ((samples - 1) * errorAvg + errorHighPrecision) / (double)samples;
    }
};
std::ostream& operator <<(std::ostream& os, const Error& error)
{
    return os << error.errorValue << " (input=" << error.inputValue << ", result_1=" << error.outputValue1 << ", result_2=" << error.outputValue2 << ")";
}
std::ostream& operator <<(std::ostream& os, const ErrorTestData& data)
{
    if (data.hasResultNaN)
        cout << "\t- has not a number result!" << endl;
    return os << "\t- min: " << data.errorMin << endl << "\t- max: " << data.errorMax << endl << "\t- avg: " << data.errorAvg << endl;
}

template<single_float_operation op1, single_float_operation op2>
class TestError
{
private:
    static void testInternal(void* userData, float inputValue, int32_t index)
    {
        auto result1 = op1(inputValue);
        auto result2 = op2(inputValue);

        ErrorTestData& testData = *reinterpret_cast<ErrorTestData*>(userData);
        testData.update(inputValue, result1, result2);
    }

public:
    void execute(const char* testName)
    {
        ErrorTestData testData;

        Timer timer;
        timer.start();
        IterateAllPositiveFloats(testInternal, &testData);
        timer.stop();

        cout << "Error test: " << testName << ". Duration: " << timer.getDuration() << endl;
        cout << testData;
    }
};

void test_error_rsqrt()
{
    TestError<InvSqrtAccurate, InvSqrtReference>().execute("reference");
    TestError<InvSqrtAccurate, InvSqrtFast>().execute("hardware fast");
    TestError<InvSqrtAccurate, InvSqrtImprovedFast>().execute("hardware fast + single Newton-Raphson iteration");
    TestError<InvSqrtAccurate, InvSqrtImprovedFastMasked>().execute("hardware fast + masked + single Newton-Raphson iteration");
    TestError<InvSqrtImprovedFast, InvSqrtImprovedFastMasked>().execute("fast vs fast masked (both with single Newton-Raphson iteration)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApprox>().execute("software fast");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApprox2>().execute("software fast (better constants)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxSSE>().execute("software fast (all on SSE)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxSSE2>().execute("software fast (all on SSE, better constants)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImproved>().execute("software fast + single Newton-Raphson iteration");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImproved2>().execute("software fast + single Newton-Raphson iteration (better constants)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImprovedSSE1>().execute("software fast + single Newton-Raphson iteration (integer on ALU, float on SSE)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImprovedSSE2>().execute("software fast + single Newton-Raphson iteration (integer on ALU, float on SSE, better constants)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImprovedSSE3>().execute("software fast + single Newton-Raphson iteration (all on SSE)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImprovedSSE4>().execute("software fast + single Newton-Raphson iteration (all on SSE, better constants)");
}

template<single_float_operation op1, single_float_operation op2>
class TestErrorCluster
{
private:
#if defined(_DEBUG)
    static constexpr size_t clusters = 3;
#else
    static constexpr size_t clusters = 256;
#endif

    static void testInternal(void* userData, float inputValue, int32_t index)
    {
        const size_t clusterIndex = Float_t(inputValue).RawExponent();
        assert(clusterIndex >= 0 && clusterIndex < clusters);

        const auto result1 = op1(inputValue);
        const auto result2 = op2(inputValue);

        ErrorTestData& testData = reinterpret_cast<ErrorTestData*>(userData)[clusterIndex];
        testData.update(inputValue, result1, result2);
    }

public:
    void execute(const char* testName)
    {
        ErrorTestData testData[clusters];

        Timer timer;
        timer.start();
        IterateAllPositiveFloats(testInternal, testData);
        timer.stop();

        cout << "Error test: " << testName << ". Duration: " << timer.getDuration() << endl;
        printf("Index, %12s, %12s, %12s, %12s, %12s, %12s, %12s\n", "input min", "input max", "out for min", "out for max", "error min", "error max", "error avg");
        for (size_t i = 0; i < clusters; ++i)
        {
            const auto& test = testData[i];
            printf("%5" PRIiPTR ", %12e, %12e, %12e, %12e, %12e, %12e, %12e\n", i, test.inputValueMin, test.inputValueMax,
                test.outputForInputValueMin, test.outputForInputValueMax,
                test.errorMin.errorValue, test.errorMax.errorValue, test.errorAvg);
            //cout << "Cluster " << i << "<" << test.inputValueMin << ", " << test.inputValueMax << ">:" << endl;
            //cout << test;
        }
    }
};

void test_error_cluster_rsqrt()
{
    TestErrorCluster<InvSqrtAccurate, InvSqrtFast>().execute("hardware fast");
    TestErrorCluster<InvSqrtAccurate, InvSqrtImprovedFast>().execute("hardware fast + single Newton-Raphson iteration");
    TestErrorCluster<InvSqrtAccurate, InvSqrtImprovedFastMasked>().execute("fast masked + single Newton-Raphson iteration");
    TestErrorCluster<InvSqrtAccurate, InvSqrtSoftFastApproxImprovedSSE3>().execute("software fast + single Newton-Raphson iteration (all on SSE)");
    TestErrorCluster<InvSqrtAccurate, InvSqrtSoftFastApproxImprovedSSE4>().execute("software fast + single Newton-Raphson iteration (all on SSE, better constants)");
}

template<single_float_operation op>
class DumpFloats
{
private:
    struct TestData
    {
        static constexpr size_t buffSize = 10 * 1024;
        std::ofstream outputData;
        float buff[buffSize];
        size_t usedBuff = 0;

        void writeData()
        {
            outputData.write(reinterpret_cast<const char*>(buff), buffSize * sizeof(buff[0]));
            usedBuff = 0;
        }
    };

    static void internalOp(void* userData, float value, int32_t index)
    {
        TestData& testData = *reinterpret_cast<TestData*>(userData);
        testData.buff[testData.usedBuff++] = op(value);

        if (testData.usedBuff >= TestData::buffSize)
            testData.writeData();
    }

public:
    void execute(const char* fileName)
    {
        cout << "Creating dump to: " << fileName << "..." << flush;

        Timer timer;
        timer.start();
        TestData testData;
        testData.outputData.open(fileName, std::ofstream::out | std::ofstream::binary);
        IterateAllPositiveFloats(internalOp, &testData);
        if (testData.usedBuff > 0)
            testData.writeData();
        testData.outputData.close();
        timer.stop();

        cout << " done! Duration: " << timer.getDuration() << "." << endl;
    }
};

template<single_float_operation op>
class CompareWithDump
{
private:
    struct TestData : public ErrorTestData
    {
        static constexpr size_t buffSize = 10 * 1024;
        std::ifstream stream;
        float buff[buffSize];
        size_t usedBuff = 0;
        size_t consumedBuff = 0;

        TestData()
        {
            errorMin.setAll(std::numeric_limits<float>::infinity(), 0, 0, 0);
            errorMax.setAll(0, 0, 0, 0);
            memset(buff, 0, buffSize * sizeof(buff[0]));
        }

        void readData()
        {
            const auto beginPos = stream.tellg();
            stream.read(reinterpret_cast<char*>(buff), buffSize * sizeof(buff[0]));
            const auto endPos = stream.tellg();
            usedBuff = (endPos - beginPos) / sizeof(buff[0]);
            consumedBuff = 0;
        }
    };

    static void internalOp(void* userData, float value, int32_t index)
    {
        TestData& testData = *reinterpret_cast<TestData*>(userData);

        if (testData.consumedBuff >= testData.usedBuff)
        {
            testData.readData();
            if (testData.consumedBuff >= testData.usedBuff)
                return;
        }

        const float referenceValue = testData.buff[testData.consumedBuff++];
        const float testedValue = op(value);

        testData.update(value, referenceValue, testedValue);
    }

public:
    void execute(const char* fileName)
    {
        cout << "Compare result with reference from file " << fileName << "... ";

        TestData testData;

        Timer timer;
        timer.start();
        testData.stream.open(fileName, std::ofstream::in | std::ofstream::binary);
        IterateAllPositiveFloats(internalOp, &testData);
        testData.stream.close();
        timer.stop();

        cout << "done. Duration: " << timer.getDuration() << endl;
        cout << testData;
    }
};

void dump_rsqrt_data()
{
    DumpFloats<InvSqrtAccurate>().execute("rsqrt_accurate.dat");
    DumpFloats<InvSqrtFast>().execute("rsqrt_fast.dat");
    DumpFloats<InvSqrtImprovedFast>().execute("rsqrt_fast_newton_raphson.dat");
    DumpFloats<InvSqrtFastMasked>().execute("rsqrt_fast_masked.dat");
    DumpFloats<InvSqrtImprovedFastMasked>().execute("rsqrt_fast_masked_newton_raphson.dat");
    DumpFloats<InvSqrtSoftFastApproxImprovedSSE4>().execute("rsqrt_fast_soft_newton_raphson_sse.dat");
}

void compare_with_dump()
{
    CompareWithDump<InvSqrtAccurate>().execute("rsqrt_accurate.dat");
    CompareWithDump<InvSqrtFast>().execute("rsqrt_fast.dat");
    CompareWithDump<InvSqrtImprovedFast>().execute("rsqrt_fast_newton_raphson.dat");
    CompareWithDump<InvSqrtFastMasked>().execute("rsqrt_fast_masked.dat");
    CompareWithDump<InvSqrtImprovedFastMasked>().execute("rsqrt_fast_masked_newton_raphson.dat");
    CompareWithDump<InvSqrtSoftFastApproxImprovedSSE4>().execute("rsqrt_fast_soft_newton_raphson_sse.dat");
}

bool askQuestionYesNoQuit(const char* description)
{
    bool result;
    cout << description << " (0 - no, 1 - yes, other - quit)" << endl;
    cin.clear();
    cin >> result;
    if (cin.fail())
        exit(-1);
    return result;
}

int main()
{
    const bool performBench = askQuestionYesNoQuit("Perform benchmarks?");
    const bool testErrorMinMaxAvg = askQuestionYesNoQuit("Test min/max/avg errors?");
    const bool testErrorMinMaxAvgPerCluster = askQuestionYesNoQuit("Test min/max/avg errors per cluster?");
    const bool createDataDump = askQuestionYesNoQuit("Create data dump?");
    const bool compareDataDump = askQuestionYesNoQuit("Compare test resulst with data dump?");

    if (performBench)
        bench_rsqrt();
    if (testErrorMinMaxAvg)
        test_error_rsqrt();
    if (testErrorMinMaxAvgPerCluster)
        test_error_cluster_rsqrt();
    if (createDataDump)
        dump_rsqrt_data();
    if (compareDataDump)
        compare_with_dump();

    return 0;
}