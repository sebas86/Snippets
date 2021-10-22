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

void bench_rsqrt()
{
    constexpr size_t baseIterations = 1000 * 1000;
    constexpr size_t repeats = 10;
    constexpr size_t tests = 10;
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
        benchmarks[i][test++] = TestSum<InvSqrtFastMasked>(iterations, "Hardware fast limited to 11bit preccission");
        benchmarks[i][test++] = TestSum<InvSqrtImprovedFastMasked>(iterations, "Hardware fast limited to 11bit preccission + single Newton-Raphson iteration");
        benchmarks[i][test++] = TestSum<InvSqrtImprovedFastMasked2>(iterations, "Hardware fast limited to 11bit preccission + two Newton-Raphson iterationsa");
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
}

void compare_with_dump()
{
    CompareWithDump<InvSqrtAccurate>().execute("rsqrt_accurate.dat");
    CompareWithDump<InvSqrtFast>().execute("rsqrt_fast.dat");
    CompareWithDump<InvSqrtImprovedFast>().execute("rsqrt_fast_newton_raphson.dat");
    CompareWithDump<InvSqrtFastMasked>().execute("rsqrt_fast_masked.dat");
    CompareWithDump<InvSqrtImprovedFastMasked>().execute("rsqrt_fast_masked_newton_raphson.dat");}

int main()
{
    bench_rsqrt();
    test_error_rsqrt();
    //dump_rsqrt_data();
    compare_with_dump();
}