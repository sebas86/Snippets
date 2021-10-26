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

#include <intrin.h>

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

    while (allFloats.RawExponent() < 255)
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
    float x2, y;

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
    float x2, y;

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
    const __m128 arg2 = _mm_mul_ss(_mm_set_ss(-0.5f), _mm_load_ss(&arg));
    guess = _mm_mul_ss(guess, _mm_add_ss(_mm_set_ss(1.5f), _mm_mul_ss(arg2, _mm_mul_ss(guess, guess))));
    return _mm_cvtss_f32(guess);
}

float InvSqrtSoftFastApproxImprovedSSE4(float arg)
{
    const __m128 number = _mm_load_ss(&arg);
    __m128 guess = _mm_castsi128_ps(_mm_sub_epi32(_mm_set1_epi32(0x5F1FFFF9), _mm_srai_epi32(_mm_castps_si128(number), 1)));
    guess = _mm_mul_ss(_mm_set_ss(0.703952253f), _mm_mul_ss(guess, _mm_sub_ss(_mm_set_ss(2.38924456f), _mm_mul_ss(_mm_load_ss(&arg), _mm_mul_ss(guess, guess)))));
    return _mm_cvtss_f32(guess);
}

void bench_rsqrt()
{
    constexpr size_t baseIterations = 1000 * 1000;
    constexpr size_t repeats = 10;
    constexpr size_t tests = 23;
    constexpr double singleTestDesiredDuration = 0.1;

    // Estimate iterations count to get around 0.1s of first test duration
    size_t iterations = baseIterations * 10;
    auto result = TestSum<InvSqrtReference>(iterations, "initial");
    iterations = std::max(static_cast<size_t>(1), static_cast<size_t>(iterations / baseIterations * singleTestDesiredDuration / result.duration)) * baseIterations;

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

        cout << "Test: " << firstBench.name << endl << "\t- result: " << firstBench.result << endl;
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
        const auto avg = std::accumulate(durations.begin(), durations.end(), 0.0f) / static_cast<double>(repeats);
        const auto median = durations[repeats / 2];
        if (test == 0)
        {
            referenceAvg = avg;
            referenceMedian = median;
        }
        
        cout << "\t- avg duration:      " << avg << endl;
        cout << "\t- median duration:   " << median << endl;
        cout << "\t- avg speed gain:    " << (referenceAvg / avg) << endl;
        cout << "\t- median speed gain: " << (referenceMedian / median) << endl;
    }
}

static float test_error_min;
static float test_error_max;
static float test_error_min_res_op1;
static float test_error_min_res_op2;
static float test_error_max_res_op1;
static float test_error_max_res_op2;

struct Error
{
    float errorValue;
    float inputValue;
    float outputValue1;
    float outputValue2;
};

struct ErrorTestData
{
    Error errorMin;
    Error errorMax;

    ErrorTestData()
    {
        memset(&errorMin, 0, sizeof(errorMin));
        memset(&errorMax, 0, sizeof(errorMax));

        errorMin.errorValue = FLT_MAX;
    }

    void update(float inputValue, float result1, float result2)
    {
        if (result1 > FLT_MAX || result1 < FLT_MIN || result2 > FLT_MAX || result2 < FLT_MIN)
            return;

        auto error = result2 - result1;
        if (result1 != 0)
            error /= result1;
        if (error < 0)
            error = -error;
        if (errorMin.errorValue > error)
        {
            errorMin.errorValue = error;
            errorMin.inputValue = inputValue;
            errorMin.outputValue1 = result1;
            errorMin.outputValue2 = result2;
        }
        if (test_error_max < error)
        {
            errorMax.errorValue = error;
            errorMax.inputValue = inputValue;
            errorMax.outputValue1 = result1;
            errorMax.outputValue2 = result2;
        }
    }
};
std::ostream& operator <<(std::ostream& os, const Error& error)
{
    return os << error.errorValue << " (input=" << error.inputValue << ", result_1=" << error.outputValue1 << ", result_2=" << error.outputValue2 << ")";
}
std::ostream& operator <<(std::ostream& os, const ErrorTestData& data)
{
    return os << "\t- min: " << data.errorMin << endl << "\t- max: " << data.errorMax << endl;
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
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxSSE>().execute("software fast (all on SSE, better constants)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxSSE2>().execute("software fast (all on SSE, better constants)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImproved>().execute("software fast + single Newton-Raphson iteration");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImproved2>().execute("software fast + single Newton-Raphson iteration (better constants)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImprovedSSE1>().execute("software fast + single Newton-Raphson iteration (integer on ALU, float on SSE)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImprovedSSE2>().execute("software fast + single Newton-Raphson iteration (integer on ALU, float on SSE, better constants)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImprovedSSE3>().execute("software fast + single Newton-Raphson iteration (all on SSE)");
    TestError<InvSqrtAccurate, InvSqrtSoftFastApproxImprovedSSE4>().execute("software fast + single Newton-Raphson iteration (all on SSE, better constants)");
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
            memset(&errorMin, 0, sizeof(errorMin));
            memset(&errorMax, 0, sizeof(errorMax));
            memset(buff, 0, buffSize * sizeof(buff[0]));

            errorMin.errorValue = FLT_MAX;
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

int main()
{
    bench_rsqrt();
    test_error_rsqrt();
    dump_rsqrt_data();
    compare_with_dump();
}