#include <benchmark/benchmark.h>
#include "crypto_utils.h"
#include <string>
#include <vector>

// SHA256性能测试
static void BM_SHA256_SmallData(benchmark::State& state) {
    std::string data = "Hello, World!";
    for (auto _ : state) {
        auto result = CryptoUtils::sha256(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_SHA256_SmallData);

static void BM_SHA256_LargeData(benchmark::State& state) {
    std::string data(state.range(0), 'A');
    for (auto _ : state) {
        auto result = CryptoUtils::sha256(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_SHA256_LargeData)->Range(1024, 1024*1024);

// AES加密性能测试
static void BM_AES_Encrypt(benchmark::State& state) {
    std::string data(state.range(0), 'A');
    auto key = CryptoUtils::generateRandomBytes(32);
    auto iv = CryptoUtils::generateRandomBytes(16);
    
    for (auto _ : state) {
        auto result = CryptoUtils::aes256Encrypt(data, key, iv);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_AES_Encrypt)->Range(1024, 1024*1024);

// AES解密性能测试
static void BM_AES_Decrypt(benchmark::State& state) {
    std::string data(state.range(0), 'A');
    auto key = CryptoUtils::generateRandomBytes(32);
    auto iv = CryptoUtils::generateRandomBytes(16);
    auto encrypted = CryptoUtils::aes256Encrypt(data, key, iv);
    
    for (auto _ : state) {
        auto result = CryptoUtils::aes256Decrypt(encrypted, key, iv);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_AES_Decrypt)->Range(1024, 1024*1024);

// 随机数生成性能测试
static void BM_RandomBytes(benchmark::State& state) {
    for (auto _ : state) {
        auto result = CryptoUtils::generateRandomBytes(state.range(0));
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_RandomBytes)->Range(16, 1024);

// 完整加密解密流程性能测试
static void BM_FullEncryptDecryptCycle(benchmark::State& state) {
    std::string data(state.range(0), 'A');
    
    for (auto _ : state) {
        auto key = CryptoUtils::generateRandomBytes(32);
        auto iv = CryptoUtils::generateRandomBytes(16);
        auto encrypted = CryptoUtils::aes256Encrypt(data, key, iv);
        auto decrypted = CryptoUtils::aes256Decrypt(encrypted, key, iv);
        benchmark::DoNotOptimize(decrypted);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_FullEncryptDecryptCycle)->Range(1024, 64*1024);

BENCHMARK_MAIN();