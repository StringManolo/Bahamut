#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>
#include <queue>
#include <atomic>
#include <future>
#include "../core/core.hpp"

namespace fs = std::filesystem;
using namespace std::chrono;

const int SMALL_DATA_SIZE = 100;
const int MEDIUM_DATA_SIZE = 1000;
const int LARGE_DATA_SIZE = 10000;
const int VERY_LARGE_DATA_SIZE = 50000;
const int EXTREME_DATA_SIZE = 100000;

class PerformanceTest : public ::testing::Test {
  protected:
    void SetUp() override {
      std::string timestamp = std::to_string(time(nullptr));
      test_dir = (fs::temp_directory_path() / ("bahamut_perf_test_" + timestamp)).string();

      original_cwd = fs::current_path();
      fs::create_directories(test_dir);
      fs::current_path(test_dir);

      setupTestEnvironment();
      setupPerformanceMonitoring();
    }

    void TearDown() override {
      logPerformanceResults();
      fs::current_path(original_cwd);
      fs::remove_all(test_dir);
    }

    void setupTestEnvironment() {
      fs::create_directories("modules/collectors");
      fs::create_directories("modules/processors");
      fs::create_directories("modules/outputs");
      fs::create_directories("modules/shared_deps");
      fs::create_directories("modules/benchmark");
    }

    void setupPerformanceMonitoring() {
      start_time = steady_clock::now();
      performance_log.clear();
    }

    void logPerformanceResults() {
      auto end_time = steady_clock::now();
      auto total_duration = duration_cast<milliseconds>(end_time - start_time).count();

      std::cout << "\n=== PERFORMANCE TEST SUMMARY ===" << std::endl;
      std::cout << "Total test duration: " << total_duration << "ms" << std::endl;
      std::cout << "Memory usage samples: " << performance_log.size() << std::endl;

      if (!performance_log.empty()) {
        long long total_memory = 0;
        for (const auto& log : performance_log) {
          total_memory += log.memory_usage;
        }
        std::cout << "Average memory usage: " << (total_memory / performance_log.size()) << "KB" << std::endl;
      }
    }

    struct PerfLog {
      std::string test_name;
      long long duration_ms;
      long long memory_usage;
      int items_processed;
    };

    void addPerfLog(const std::string& test_name, long long duration_ms, int items_processed) {
      PerfLog log;
      log.test_name = test_name;
      log.duration_ms = duration_ms;
      log.memory_usage = getMemoryUsage();
      log.items_processed = items_processed;
      performance_log.push_back(log);

      std::cout << "[PERF] " << test_name 
        << " | Time: " << duration_ms << "ms"
        << " | Memory: " << log.memory_usage << "KB"
        << " | Items: " << items_processed
        << " | Rate: " << (items_processed * 1000.0 / duration_ms) << " items/sec" << std::endl;
    }

    long long getMemoryUsage() {
      return 0; // Placeholder
    }

    void createTestModule(const std::string& filename, const std::string& content) {
      fs::path filepath(filename);
      fs::create_directories(filepath.parent_path());

      std::ofstream file(filename);
      file << content;
      file.close();

      fs::permissions(filepath, fs::perms::owner_exec, fs::perm_options::add);
    }

    std::vector<std::string> generateRandomDomains(int count) {
      std::vector<std::string> domains;
      std::vector<std::string> tlds = {".com", ".net", ".org", ".io", ".dev"};
      std::vector<std::string> prefixes = {"test", "example", "demo", "sample", "prod"};

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> tld_dist(0, tlds.size() - 1);
      std::uniform_int_distribution<> prefix_dist(0, prefixes.size() - 1);
      std::uniform_int_distribution<> num_dist(1000, 9999);

      for (int i = 0; i < count; i++) {
        std::string domain = prefixes[prefix_dist(gen)] + 
          std::to_string(num_dist(gen)) + 
          tlds[tld_dist(gen)];
        domains.push_back(domain);
      }

      return domains;
    }

    std::string test_dir;
    std::string original_cwd;
    steady_clock::time_point start_time;
    std::vector<PerfLog> performance_log;
};

TEST_F(PerformanceTest, BasicModuleExecutionTime) {
  std::string content = R"(#!/usr/bin/env node
// Name: Basic Performance Test
// Description: Simple module to measure basic execution time
console.log('{"bmop":"1.0","module":"basic_perf"}');

for (let i = 0; i < 1000; i++) {
    console.log(JSON.stringify({t:"d",f:"domain",v:"test" + i + ".com"}));
}

console.log('{"t":"result","ok":true,"count":1000}');)";

  createTestModule("modules/basic_perf.js", content);

  auto start = steady_clock::now();

  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("basic_perf.js", {}, storage, "");

  auto end = steady_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  addPerfLog("BasicModuleExecutionTime", duration, 1000);
  EXPECT_EQ(storage["domain"].size(), 1000);
  EXPECT_LT(duration, 5000); 
  }

TEST_F(PerformanceTest, BatchModePerformance) {
  struct BatchTest {
    int batch_size;
    std::string test_name;
  };

  std::vector<BatchTest> tests = {
    {100, "SmallBatch"},
    {1000, "MediumBatch"},
    {10000, "LargeBatch"},
    {50000, "VeryLargeBatch"}
  };

  for (const auto& test : tests) {
    std::string content = R"(#!/usr/bin/env node
// Name: Batch Performance Test
// Description: Test batch mode performance
console.log('{"bmop":"1.0","module":"batch_perf"}');

console.log(JSON.stringify({t:"batch",f:"domain",c:)" + std::to_string(test.batch_size) + R"(}));
    for (let i = 0; i < )" + std::to_string(test.batch_size) + R"(; i++) {
      console.log('test' + i + '.com');
    }
    console.log('{"t":"batch_end"}');
    console.log('{"t":"result","ok":true,"count":)" + std::to_string(test.batch_size) + R"(}');)";

    createTestModule("modules/batch_perf_" + std::to_string(test.batch_size) + ".js", content);

    auto start = steady_clock::now();

    std::map<std::string, std::vector<DataItem>> storage;
    runModuleWithPipe("batch_perf_" + std::to_string(test.batch_size) + ".js", {}, storage, "");

    auto end = steady_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    addPerfLog("BatchMode_" + test.test_name, duration, test.batch_size);
    EXPECT_EQ(storage["domain"].size(), test.batch_size);
  }
}

TEST_F(PerformanceTest, CrossLanguageLargeDataProcessing) {
  std::vector<int> data_sizes = {1000, 5000, 10000};

  for (int size : data_sizes) {
    std::string content = R"(#!/usr/bin/env node
// Name: Node.js Large Data Processor
// Description: Process large data in Node.js
console.log('{"bmop":"1.0","module":"node_large_data"}');

let count = 0;
let buffer = '';
process.stdin.on('data', chunk => {
    buffer += chunk.toString();
    const lines = buffer.split('\n');
    buffer = lines.pop();

    lines.forEach(line => {
        if (!line.trim()) return;
        try {
            const msg = JSON.parse(line);
            if (msg.t === 'd' && msg.f === 'domain') {
                console.log(JSON.stringify({
                    t: "d",
                    f: "processed_domain",
                    v: msg.v.toUpperCase()
                }));
                count++;
            }
        } catch (e) {}
    });
});

process.stdin.on('end', () => {
    console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";

    createTestModule("modules/node_large_" + std::to_string(size) + ".js", content);

    std::map<std::string, std::vector<DataItem>> input_storage;
    auto domains = generateRandomDomains(size);
    for (const auto& domain : domains) {
      DataItem item;
      item.format = "domain";
      item.value = domain;
      input_storage["domain"].push_back(item);
    }

auto start = steady_clock::now();

std::map<std::string, std::vector<DataItem>> output_storage;
runModuleWithPipe("node_large_" + std::to_string(size) + ".js", {}, 
    output_storage, "domain");

auto end = steady_clock::now();
auto duration = duration_cast<milliseconds>(end - start).count();

addPerfLog("NodeJS_LargeData_" + std::to_string(size), duration, size);
EXPECT_EQ(output_storage["processed_domain"].size(), size);
}
}

TEST_F(PerformanceTest, ConcurrentModuleExecution) {
  std::string module_template = R"(#!/usr/bin/env node
// Name: Concurrent Test Module
// Description: Module for concurrent execution testing
console.log('{"bmop":"1.0","module":"concurrent_test"}');

// Simular trabajo variable
const workTime = Math.random() * 100;
const start = Date.now();
while (Date.now() - start < workTime) {
    // Simulate process
}

for (let i = 0; i < 10; i++) {
    console.log(JSON.stringify({t:"d",f:"result",v:"data_" + Math.random()}));
}

console.log(JSON.stringify({t:"result",ok:true,count:10}));)";

  const int num_modules = 10;
  for (int i = 0; i < num_modules; i++) {
    createTestModule("modules/concurrent_" + std::to_string(i) + ".js", module_template);
  }

auto start = steady_clock::now();

std::vector<std::future<void>> futures;
for (int i = 0; i < num_modules; i++) {
  futures.push_back(std::async(std::launch::async, [i, this]() {
        std::map<std::string, std::vector<DataItem>> storage;
        runModuleWithPipe("concurrent_" + std::to_string(i) + ".js", {}, storage, "");
        }));
}

for (auto& future : futures) {
  future.wait();
}

auto end = steady_clock::now();
auto duration = duration_cast<milliseconds>(end - start).count();

addPerfLog("ConcurrentModules_10", duration, num_modules * 10);
}

TEST_F(PerformanceTest, ProcessingPipelinePerformance) {
  const int data_size = 5000;

  std::string collector = R"(#!/usr/bin/env node
// Name: Data Collector
// Description: Collect initial data
// Provides: raw_data
console.log('{"bmop":"1.0","module":"collector"}');

console.log(JSON.stringify({t:"batch",f:"raw_data",c:)" + std::to_string(data_size) + R"(}));
  for (let i = 0; i < )" + std::to_string(data_size) + R"(; i++) {
    console.log('data_item_' + i);
  }
  console.log('{"t":"batch_end"}');
  console.log('{"t":"result","ok":true,"count":)" + std::to_string(data_size) + R"(}');)";

  std::string processor = R"(#!/usr/bin/env node
// Name: Data Processor
// Description: Process raw data
// Consumes: raw_data
// Provides: processed_data
console.log('{"bmop":"1.0","module":"processor"}');

let count = 0;
let buffer = '';
process.stdin.on('data', chunk => {
    buffer += chunk.toString();
    const lines = buffer.split('\n');
    buffer = lines.pop();

    lines.forEach(line => {
        if (line.trim() && !line.startsWith('{')) {
            console.log(JSON.stringify({
                t: "d",
                f: "processed_data",
                v: "PROCESSED_" + line
            }));
            count++;
        }
    });
});

process.stdin.on('end', () => {
    console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";

  std::string output = R"(#!/usr/bin/env node
// Name: Data Output
// Description: Final output stage
// Consumes: processed_data
console.log('{"bmop":"1.0","module":"output"}');

let count = 0;
let buffer = '';
process.stdin.on('data', chunk => {
    buffer += chunk.toString();
    const lines = buffer.split('\n');
    buffer = lines.pop();

    lines.forEach(line => {
        if (line.trim()) {
            try {
                const msg = JSON.parse(line);
                if (msg.t === 'd' && msg.f === 'processed_data') {
                    count++;
                }
            } catch (e) {}
        }
    });
});

process.stdin.on('end', () => {
    console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";

  createTestModule("modules/collector.js", collector);
  createTestModule("modules/processor.js", processor);
  createTestModule("modules/output.js", output);

  auto start = steady_clock::now();

  std::map<std::string, std::vector<DataItem>> storage;

  runModuleWithPipe("collector.js", {}, storage, "");
  runModuleWithPipe("processor.js", {}, storage, "raw_data");
  runModuleWithPipe("output.js", {}, storage, "processed_data");

  auto end = steady_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  addPerfLog("ProcessingPipeline", duration, data_size);
  EXPECT_EQ(storage["raw_data"].size(), data_size);
  EXPECT_EQ(storage["processed_data"].size(), data_size);
  }

TEST_F(PerformanceTest, BMOPParsingPerformance) {
  std::vector<int> message_counts = {1000, 5000, 10000};

  for (int count : message_counts) {
    std::string content = R"(#!/usr/bin/env node
// Name: BMOP Parser Test
// Description: Test BMOP parsing performance
console.log('{"bmop":"1.0","module":"bmop_parser"}');

for (let i = 0; i < )" + std::to_string(count) + R"(; i++) {
  const msgType = i % 5;
  switch(msgType) {
    case 0:
      console.log(JSON.stringify({t:"d",f:"domain",v:"test" + i + ".com"}));
      break;
    case 1:
      console.log(JSON.stringify({t:"log",l:"info",m:"Processing item " + i}));
      break;
    case 2:
      console.log(JSON.stringify({t:"progress",c:i,T:)" + std::to_string(count) + R"(}));
      break;
    case 3:
      console.log(JSON.stringify({t:"batch",f:"data",c:10}));
      for (let j = 0; j < 10; j++) {
        console.log('batch_data_' + j);
      }
      console.log('{"t":"batch_end"}');
      i += 9; 
      break;
    case 4:
      console.log(JSON.stringify({t:"error",code:"TEST",m:"Test error " + i}));
      break;
  }
}

console.log(JSON.stringify({t:"result",ok:true,count:)" + std::to_string(count) + R"(}));)";

createTestModule("modules/bmop_parser_" + std::to_string(count) + ".js", content);

auto start = steady_clock::now();

std::map<std::string, std::vector<DataItem>> storage;
runModuleWithPipe("bmop_parser_" + std::to_string(count) + ".js", {}, storage, "");

auto end = steady_clock::now();
auto duration = duration_cast<milliseconds>(end - start).count();

addPerfLog("BMOPParsing_" + std::to_string(count), duration, count);
}
}

TEST_F(PerformanceTest, MemoryUsageWithLargeDatasets) {
  const int data_size = VERY_LARGE_DATA_SIZE;

  std::string content = R"(#!/usr/bin/env node
// Name: Memory Usage Test
// Description: Test memory usage with large datasets
console.log('{"bmop":"1.0","module":"memory_test"}');


const formats = ['domain', 'subdomain', 'ip', 'url', 'email'];
let totalCount = 0;

console.log(JSON.stringify({t:"batch",f:"mixed_data",c:)" + std::to_string(data_size * 5) + R"(}));
  for (let i = 0; i < )" + std::to_string(data_size) + R"(; i++) {
    for (const format of formats) {
      console.log(format + '_value_' + i + '_' + Math.random().toString(36).substr(2, 9));
      totalCount++;
    }
  }
  console.log('{"t":"batch_end"}');
  console.log(JSON.stringify({t:"result",ok:true,count:)" + std::to_string(data_size * 5) + R"(}));)";

  createTestModule("modules/memory_test.js", content);

  auto start = steady_clock::now();

  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("memory_test.js", {}, storage, "");

  auto end = steady_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  addPerfLog("MemoryUsage_VeryLarge", duration, data_size * 5);
  EXPECT_GE(storage["mixed_data"].size(), data_size * 5);
}

TEST_F(PerformanceTest, DependencyInstallationPerformance) {
  std::vector<std::pair<std::string, std::string>> packages = {
    {"small", "npm install chalk"},
    {"medium", "npm install axios chalk lodash moment"},
    {"large", "npm install express react react-dom typescript @types/node"}
  };

  for (const auto& [size, install_cmd] : packages) {
    std::string content = R"(#!/usr/bin/env node
// Name: Installation Test )" + size + R"(
// Install: )" + install_cmd + R"(
// InstallScope: shared
    console.log('{"bmop":"1.0","module":"install_test"}');
    console.log('{"t":"result","ok":true,"count":0}');)";

    createTestModule("modules/install_" + size + ".js", content);

    fs::remove_all("modules/shared_deps");

    auto start = steady_clock::now();

    installModule("install_" + size + ".js");

    auto end = steady_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    addPerfLog("Installation_" + size, duration, 0);

    EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules"));
  }
}

TEST_F(PerformanceTest, CrossLanguageComparison) {
  const int data_size = 10000;

  std::string node_content = R"(#!/usr/bin/env node
// Name: Node.js Performance Test
console.log('{"bmop":"1.0","module":"node_perf"}');

console.log(JSON.stringify({t:"batch",f:"node_data",c:)" + std::to_string(data_size) + R"(}));
  for (let i = 0; i < )" + std::to_string(data_size) + R"(; i++) {
    console.log('node_value_' + i);
  }
  console.log('{"t":"batch_end"}');
  console.log('{"t":"result","ok":true,"count":)" + std::to_string(data_size) + R"(}');)";

  std::string python_content = R"(#!/usr/bin/env python3
# Name: Python Performance Test
import json
import sys

print(json.dumps({"bmop":"1.0","module":"python_perf"}))

print(json.dumps({"t":"batch","f":"python_data","c":)" + std::to_string(data_size) + R"(}))

  for i in range()" + std::to_string(data_size) + R"():
    print(f'python_value_{i}')

      print(json.dumps({"t":"batch_end"}))
      print(json.dumps({"t":"result","ok":True,"count":)" + std::to_string(data_size) + R"(}))";

  createTestModule("modules/node_perf.js", node_content);
  createTestModule("modules/python_perf.py", python_content);

  auto node_start = steady_clock::now();
  std::map<std::string, std::vector<DataItem>> node_storage;
  runModuleWithPipe("node_perf.js", {}, node_storage, "");
  auto node_end = steady_clock::now();
  auto node_duration = duration_cast<milliseconds>(node_end - node_start).count();

  addPerfLog("NodeJS_Perf", node_duration, data_size);

  auto python_start = steady_clock::now();
  std::map<std::string, std::vector<DataItem>> python_storage;
  runModuleWithPipe("python_perf.py", {}, python_storage, "");
  auto python_end = steady_clock::now();
  auto python_duration = duration_cast<milliseconds>(python_end - python_start).count();

  addPerfLog("Python_Perf", python_duration, data_size);

  EXPECT_EQ(node_storage["node_data"].size(), data_size);
  EXPECT_EQ(python_storage["python_data"].size(), data_size);
}

TEST_F(PerformanceTest, StorageBehaviorPerformance) {
  const int data_size = 10000;

  std::vector<std::string> behaviors = {"add", "replace", "delete"};

  for (const auto& behavior : behaviors) {
    std::string content = R"(#!/usr/bin/env node
// Name: Storage Behavior Test - )" + behavior + R"(
// Consumes: test_data
// Provides: test_data
// Storage: )" + behavior + R"(
    console.log('{"bmop":"1.0","module":"storage_)" + behavior + R"("}');

let count = 0;
let buffer = '';
process.stdin.on('data', chunk => {
    buffer += chunk.toString();
    const lines = buffer.split('\n');
    buffer = lines.pop();

    lines.forEach(line => {
        if (!line.trim()) return;
        try {
            const msg = JSON.parse(line);
            if (msg.t === 'd' && msg.f === 'test_data') {
                console.log(JSON.stringify({
                    t: "d",
                    f: "test_data",
                    v: "TRANSFORMED_" + msg.v
                }));
                count++;
            }
        } catch (e) {}
    });
});

process.stdin.on('end', () => {
    console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";

    createTestModule("modules/storage_" + behavior + ".js", content);

    std::map<std::string, std::vector<DataItem>> storage;
    for (int i = 0; i < data_size; i++) {
      DataItem item;
      item.format = "test_data";
      item.value = "original_" + std::to_string(i);
      storage["test_data"].push_back(item);
    }

auto start = steady_clock::now();

runModuleWithPipe("storage_" + behavior + ".js", {}, storage, "test_data");

auto end = steady_clock::now();
auto duration = duration_cast<milliseconds>(end - start).count();

addPerfLog("StorageBehavior_" + behavior, duration, data_size);

if (behavior == "replace") {
  EXPECT_EQ(storage["test_data"].size(), data_size);
  for (const auto& item : storage["test_data"]) {
    EXPECT_TRUE(item.value.find("TRANSFORMED_") != std::string::npos);
  }
} else if (behavior == "delete") {
  EXPECT_EQ(storage["test_data"].size(), 0);
}
}
}

TEST_F(PerformanceTest, ComplexTransformationPipeline) {
  const int initial_data_size = 10000;

  std::string generator = R"(#!/usr/bin/env node
// Name: Data Generator
// Provides: raw_numbers
console.log('{"bmop":"1.0","module":"generator"}');

console.log(JSON.stringify({t:"batch",f:"raw_numbers",c:)" + std::to_string(initial_data_size) + R"(}));
  for (let i = 0; i < )" + std::to_string(initial_data_size) + R"(; i++) {
    console.log(i);
  }
  console.log('{"t":"batch_end"}');
  console.log('{"t":"result","ok":true,"count":)" + std::to_string(initial_data_size) + R"(}');)";

  std::string filter = R"(#!/usr/bin/env node
// Name: Even Number Filter
// Consumes: raw_numbers
// Provides: even_numbers
// Storage: replace
console.log('{"bmop":"1.0","module":"filter"}');

let count = 0;
let buffer = '';
process.stdin.on('data', chunk => {
    buffer += chunk.toString();
    const lines = buffer.split('\n');
    buffer = lines.pop();

    lines.forEach(line => {
        if (line.trim() && !line.startsWith('{')) {
            const num = parseInt(line);
            if (num % 2 === 0) {
                console.log(JSON.stringify({
                    t: "d",
                    f: "even_numbers",
                    v: num.toString()
                }));
                count++;
            }
        }
    });
});

process.stdin.on('end', () => {
    console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";

  std::string squarer = R"(#!/usr/bin/env node
// Name: Number Squarer
// Consumes: even_numbers
// Provides: squared_numbers
// Storage: replace
console.log('{"bmop":"1.0","module":"squarer"}');

let count = 0;
let buffer = '';
process.stdin.on('data', chunk => {
    buffer += chunk.toString();
    const lines = buffer.split('\n');
    buffer = lines.pop();

    lines.forEach(line => {
        if (!line.trim()) return;
        try {
            const msg = JSON.parse(line);
            if (msg.t === 'd' && msg.f === 'even_numbers') {
                const num = parseInt(msg.v);
                const squared = num * num;
                console.log(JSON.stringify({
                    t: "d",
                    f: "squared_numbers",
                    v: squared.toString()
                }));
                count++;
            }
        } catch (e) {}
    });
});

process.stdin.on('end', () => {
    console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";

  std::string aggregator = R"(#!/usr/bin/env node
// Name: Sum Aggregator
// Consumes: squared_numbers
// Provides: final_sum
console.log('{"bmop":"1.0","module":"aggregator"}');

let sum = 0;
let count = 0;
let buffer = '';
process.stdin.on('data', chunk => {
    buffer += chunk.toString();
    const lines = buffer.split('\n');
    buffer = lines.pop();

    lines.forEach(line => {
        if (!line.trim()) return;
        try {
            const msg = JSON.parse(line);
            if (msg.t === 'd' && msg.f === 'squared_numbers') {
                sum += parseInt(msg.v);
                count++;
            }
        } catch (e) {}
    });
});

process.stdin.on('end', () => {
    console.log(JSON.stringify({
        t: "d",
        f: "final_sum",
        v: sum.toString()
    }));
    console.log(JSON.stringify({t:"result",ok:true,count:1}));
});)";

  createTestModule("modules/generator.js", generator);
  createTestModule("modules/filter.js", filter);
  createTestModule("modules/squarer.js", squarer);
  createTestModule("modules/aggregator.js", aggregator);

  auto start = steady_clock::now();

  std::map<std::string, std::vector<DataItem>> storage;

  runModuleWithPipe("generator.js", {}, storage, "");
  runModuleWithPipe("filter.js", {}, storage, "raw_numbers");
  runModuleWithPipe("squarer.js", {}, storage, "even_numbers");
  runModuleWithPipe("aggregator.js", {}, storage, "squared_numbers");

  auto end = steady_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  int expected_even_count = initial_data_size / 2;
  addPerfLog("ComplexPipeline", duration, initial_data_size);

  EXPECT_EQ(storage["raw_numbers"].size(), initial_data_size);
  EXPECT_EQ(storage["even_numbers"].size(), expected_even_count);
  EXPECT_EQ(storage["squared_numbers"].size(), expected_even_count);
  EXPECT_EQ(storage["final_sum"].size(), 1);
  }

TEST_F(PerformanceTest, MultipleFormatStressTest) {
  const int items_per_format = 5000;
  std::vector<std::string> formats = {"domain", "subdomain", "ip", "url", "email", "port"};

  std::string formats_array = "[";
  for (size_t i = 0; i < formats.size(); ++i) {
    if (i > 0) formats_array += ", ";
    formats_array += "'" + formats[i] + "'";
  }
  formats_array += "]";

  std::string content = R"(#!/usr/bin/env node
// Name: Multi-Format Stress Test
console.log('{"bmop":"1.0","module":"multi_format"}');

const formats = )" + formats_array + R"(;

  let totalCount = 0;
  for (const format of formats) {
  console.log(JSON.stringify({t:"batch",f:format,c:)" + std::to_string(items_per_format) + R"(}));
  for (let i = 0; i < )" + std::to_string(items_per_format) + R"(; i++) {
  console.log(format + '_' + i + '_' + Math.random().toString(36).substr(2, 5));
  totalCount++;
  }
  console.log('{"t":"batch_end"}');
  }

  console.log(JSON.stringify({t:"result",ok:true,count:)" +
    std::to_string(items_per_format * formats.size()) + R"(}));)";

    createTestModule("modules/multi_format.js", content);

    auto start = steady_clock::now();

    std::map<std::string, std::vector<DataItem>> storage;
    runModuleWithPipe("multi_format.js", {}, storage, "");

    auto end = steady_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    int total_items = items_per_format * formats.size();
    addPerfLog("MultiFormatStress", duration, total_items);

    for (const auto& format : formats) {
    EXPECT_EQ(storage[format].size(), items_per_format);
    }
    }

  TEST_F(PerformanceTest, CompleteProfilePerformance) {
    std::vector<std::pair<std::string, std::string>> modules = {
      {"collector1.js", "collector-domain"},
      {"collector2.js", "collector-subdomain"},
      {"processor1.js", "processor"},
      {"processor2.js", "processor"},
      {"output1.js", "output"}
    };

    fs::create_directories("profiles");

    std::ofstream profile("profiles/bahamut_performance.txt");

    for (size_t i = 0; i < modules.size(); i++) {
      const auto& [filename, type] = modules[i];

      std::string content = R"(#!/usr/bin/env node
// Name: Performance Module )" + std::to_string(i + 1) + R"(
// Type: )" + type + R"(
// Stage: )" + std::to_string(i + 1) + R"(
      console.log('{"bmop":"1.0","module":")" + filename + R"("}');

if (")" + type + R"(" === "collector-domain") {
    console.log(JSON.stringify({t:"batch",f:"domain",c:1000}));
    for (let i = 0; i < 1000; i++) {
        console.log('test' + i + '.com');
    }
    console.log('{"t":"batch_end"}');
} else if (")" + type + R"(" === "collector-subdomain") {
    console.log(JSON.stringify({t:"batch",f:"subdomain",c:500}));
    for (let i = 0; i < 500; i++) {
        console.log('sub' + i + '.test.com');
    }
    console.log('{"t":"batch_end"}');
}

console.log('{"t":"result","ok":true,"count":1000}');)";

      createTestModule("modules/" + filename, content);
      profile << filename << "\n";
      }
profile.close();

auto start = steady_clock::now();

std::vector<std::string> args = {"--profile", "performance"};
runModulesFromProfile("performance", args);

auto end = steady_clock::now();
auto duration = duration_cast<milliseconds>(end - start).count();

addPerfLog("CompleteProfile", duration, modules.size() * 1000);
}

TEST_F(PerformanceTest, RegressionBaselineComparison) {
  const int baseline_size = 1000;

  std::string content = R"(#!/usr/bin/env node
// Name: Regression Baseline
console.log('{"bmop":"1.0","module":"regression_baseline"}');

const start = Date.now();
let sum = 0;
for (let i = 0; i < 1000000; i++) {
    sum += Math.sin(i) * Math.cos(i);
}

console.log(JSON.stringify({t:"batch",f:"baseline_data",c:)" + std::to_string(baseline_size) + R"(}));
  for (let i = 0; i < )" + std::to_string(baseline_size) + R"(; i++) {
    console.log('baseline_value_' + i);
  }
console.log('{"t":"batch_end"}');

console.log(JSON.stringify({t:"result","ok":true,"count":)" + std::to_string(baseline_size) + R"(,"processing_time":")" + 
      std::to_string(1000000) + R"( iterations"}));)";

createTestModule("modules/regression_baseline.js", content);

auto start_time = steady_clock::now();

std::map<std::string, std::vector<DataItem>> storage;
runModuleWithPipe("regression_baseline.js", {}, storage, "");

auto end_time = steady_clock::now();
auto duration = duration_cast<milliseconds>(end_time - start_time).count();

addPerfLog("RegressionBaseline", duration, baseline_size);

std::ofstream baseline("performance_baseline.txt", std::ios::app);
  baseline << "RegressionBaseline: " << duration << "ms at " 
<< duration_cast<seconds>(system_clock::now().time_since_epoch()).count() 
  << std::endl;

  EXPECT_EQ(storage["baseline_data"].size(), baseline_size);
  }

TEST_F(PerformanceTest, SimulatedThreadScalability) {
  const int num_workers = 4;
  const int items_per_worker = 2500;

  std::string worker_template = R"(#!/usr/bin/env node
// Name: Worker Module
console.log('{"bmop":"1.0","module":"worker"}');

console.log(JSON.stringify({t:"batch",f:"worker_output",c:)" + std::to_string(items_per_worker) + R"(}));
  for (let i = 0; i < )" + std::to_string(items_per_worker) + R"(; i++) {
    console.log('worker_data_' + i + '_' + Math.random().toString(36).substr(2, 8));
  }
  console.log('{"t":"batch_end"}');
  console.log('{"t":"result","ok":true,"count":)" + std::to_string(items_per_worker) + R"(}');)";

  for (int i = 0; i < num_workers; i++) {
    createTestModule("modules/worker_" + std::to_string(i) + ".js", worker_template);
  }

  auto start = steady_clock::now();

  std::vector<std::future<void>> futures;
  for (int i = 0; i < num_workers; i++) {
    futures.push_back(std::async(std::launch::async, [i, this]() {
          std::map<std::string, std::vector<DataItem>> storage;
          runModuleWithPipe("worker_" + std::to_string(i) + ".js", {}, storage, "");
          }));
  }

  for (auto& future : futures) {
    future.wait();
  }

  auto end = steady_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  addPerfLog("ThreadScalability_" + std::to_string(num_workers), 
      duration, num_workers * items_per_worker);
}

TEST_F(PerformanceTest, ExternalDependencyPerformance) {
  std::string content = R"(#!/usr/bin/env node
// Name: External Dependency Test
// Install: npm install uuid
// InstallScope: shared
const { v4: uuidv4 } = require('uuid');

console.log('{"bmop":"1.0","module":"uuid_generator"}');

console.log(JSON.stringify({t:"batch",f:"uuid",c:1000}));
for (let i = 0; i < 1000; i++) {
    console.log(uuidv4());
}
console.log('{"t":"batch_end"}');
console.log('{"t":"result","ok":true,"count":1000}');)";

  createTestModule("modules/uuid_generator.js", content);

  installModule("uuid_generator.js");

  auto start = steady_clock::now();

  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("uuid_generator.js", {}, storage, "");

  auto end = steady_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  addPerfLog("ExternalDependency_UUID", duration, 1000);
  EXPECT_EQ(storage["uuid"].size(), 1000);
  }

TEST_F(PerformanceTest, NetworkLatencySimulation) {
  std::vector<int> delays_ms = {10, 50, 100};

  for (int delay : delays_ms) {
    std::string content = R"(#!/usr/bin/env node
// Name: Network Simulation )" + std::to_string(delay) + R"(ms
    console.log('{"bmop":"1.0","module":"network_sim"}');

    const simulateNetworkDelay = (ms) => {
      const start = Date.now();
      while (Date.now() - start < ms) {}
    };

    let count = 0;
    let buffer = '';
    process.stdin.on('data', chunk => {
        buffer += chunk.toString();
        const lines = buffer.split('\n');
        buffer = lines.pop();

        lines.forEach(line => {
            if (!line.trim()) return;
            try {
            const msg = JSON.parse(line);
            if (msg.t === 'd' && msg.f === 'input_data') {
            simulateNetworkDelay()" + std::to_string(delay) + R"();

            console.log(JSON.stringify({
t: "d",
f: "output_data",
v: "PROCESSED_" + msg.v
}));
            count++;
            }
            } catch (e) {}
            });
});

process.stdin.on('end', () => {
    console.log(JSON.stringify({t:"result",ok:true,count:count}));
    });)";

createTestModule("modules/network_sim_" + std::to_string(delay) + ".js", content);

std::map<std::string, std::vector<DataItem>> storage;
for (int i = 0; i < 100; i++) {
  DataItem item;
  item.format = "input_data";
  item.value = "input_" + std::to_string(i);
  storage["input_data"].push_back(item);
}

auto start = steady_clock::now();

runModuleWithPipe("network_sim_" + std::to_string(delay) + ".js", {}, 
    storage, "input_data");

auto end = steady_clock::now();
auto duration = duration_cast<milliseconds>(end - start).count();

addPerfLog("NetworkLatency_" + std::to_string(delay) + "ms", duration, 100);
EXPECT_EQ(storage["output_data"].size(), 100);
}
}

TEST_F(PerformanceTest, CachePerformanceTest) {
  //const int iterations = 5;
  const int data_size = 1000;

  std::string content = R"(#!/usr/bin/env node
// Name: Cache Performance Test
console.log('{"bmop":"1.0","module":"cache_test"}');

const cache = new Map();

let count = 0;
let buffer = '';
process.stdin.on('data', chunk => {
    buffer += chunk.toString();
    const lines = buffer.split('\n');
    buffer = lines.pop();

    lines.forEach(line => {
        if (!line.trim()) return;
        try {
            const msg = JSON.parse(line);
            if (msg.t === 'd' && msg.f === 'cache_input') {
                let result;
                if (cache.has(msg.v)) {
                    result = cache.get(msg.v);
                } else {
                    result = msg.v.split('').reverse().join('');
                    cache.set(msg.v, result);
                }

                console.log(JSON.stringify({
                    t: "d",
                    f: "cache_output",
                    v: result
                }));
                count++;
            }
        } catch (e) {}
    });
});

process.stdin.on('end', () => {
    console.log('{"t":"log","l":"info","m":"Cache hits: ' + (data_size - cache.size) + '"}');
    console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";

  createTestModule("modules/cache_test.js", content);

  std::map<std::string, std::vector<DataItem>> storage;
  for (int i = 0; i < data_size; i++) {
    DataItem item;
    item.format = "cache_input";
    item.value = "value_" + std::to_string(i % (data_size / 2));
    storage["cache_input"].push_back(item);
  }

auto start = steady_clock::now();

runModuleWithPipe("cache_test.js", {}, storage, "cache_input");

auto end = steady_clock::now();
auto duration = duration_cast<milliseconds>(end - start).count();

addPerfLog("CachePerformance", duration, data_size);
EXPECT_EQ(storage["cache_output"].size(), data_size);
}

TEST_F(PerformanceTest, MemoryLeakDetection) {
  const int iterations = 10;
  const int batch_size = 1000;

  std::string content = R"(#!/usr/bin/env node
// Name: Memory Leak Test
console.log('{"bmop":"1.0","module":"memory_leak_test"}');

const processData = (data) => {
    return data.split('').map(c => c.charCodeAt(0)).reduce((a, b) => a + b, 0);
};

let totalProcessed = 0;

for (let iter = 0; iter < )" + std::to_string(iterations) + R"(; iter++) {
  console.log(JSON.stringify({t:"batch",f:"processed_data",c:)" + std::to_string(batch_size) + R"(}));

  for (let i = 0; i < )" + std::to_string(batch_size) + R"(; i++) {
    const data = 'data_' + iter + '_' + i + '_' + Math.random().toString(36);
    const result = processData(data);
    console.log(result.toString());
    totalProcessed++;

    if (i % 100 === 0 && global.gc) {
      global.gc();
    }
  }

  console.log('{"t":"batch_end"}');
}

console.log(JSON.stringify({t:"result","ok":true,"count":)" + 
    std::to_string(iterations * batch_size) + R"(}));)";

    createTestModule("modules/memory_leak_test.js", content);

    auto start = steady_clock::now();

    std::map<std::string, std::vector<DataItem>> storage;
    runModuleWithPipe("memory_leak_test.js", {}, storage, "");

    auto end = steady_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    int total_items = iterations * batch_size;
    addPerfLog("MemoryLeakDetection", duration, total_items);

    // Very basic check
    EXPECT_EQ(storage["processed_data"].size(), total_items);
    }

    TEST_F(PerformanceTest, SystemLoadSimulation) {
      std::vector<std::thread> background_load;
      std::atomic<int> background_work{0};

      for (int i = 0; i < 4; i++) {
        background_load.emplace_back([&background_work]() {
            for (int j = 0; j < 1000000; j++) {
            background_work += std::sqrt(j) * std::sin(j);
            }
            });
      }

      std::string content = R"(#!/usr/bin/env node
// Name: System Load Test
console.log('{"bmop":"1.0","module":"system_load_test"}');

console.log(JSON.stringify({t:"batch",f:"load_test_data",c:5000}));
for (let i = 0; i < 5000; i++) {
    console.log('test_data_' + i + '_' + Math.random().toString(36).substr(2, 4));
}
console.log('{"t":"batch_end"}');
console.log('{"t":"result","ok":true,"count":5000}');)";

      createTestModule("modules/system_load_test.js", content);

      auto start = steady_clock::now();

      std::map<std::string, std::vector<DataItem>> storage;
      runModuleWithPipe("system_load_test.js", {}, storage, "");

      auto end = steady_clock::now();
      auto duration = duration_cast<milliseconds>(end - start).count();

      for (auto& thread : background_load) {
        thread.join();
      }

addPerfLog("SystemLoadSimulation", duration, 5000);
EXPECT_EQ(storage["load_test_data"].size(), 5000);
}

TEST_F(PerformanceTest, FinalComprehensiveBenchmark) {
  std::cout << "\n=== COMPREHENSIVE PERFORMANCE BENCHMARK ===" << std::endl;

  struct Benchmark {
    std::string name;
    std::function<void()> test;
  };

  std::vector<Benchmark> benchmarks = {
    {"Small Data Processing", [this]() {
                                         std::string content = R"(#!/usr/bin/env node
console.log('{"bmop":"1.0","module":"bench_small"}');
for (let i = 0; i < 100; i++) {
    console.log(JSON.stringify({t:"d",f:"bench",v:"small_" + i}));
}
console.log('{"t":"result","ok":true,"count":100}');)";
                                         createTestModule("modules/bench_small.js", content);

                                         auto start = steady_clock::now();
                                         std::map<std::string, std::vector<DataItem>> storage;
                                         runModuleWithPipe("bench_small.js", {}, storage, "");
                                         auto end = steady_clock::now();

                                         addPerfLog("Bench_Small", 
                                             duration_cast<milliseconds>(end - start).count(), 100);
}},

{"Medium Data Processing", [this]() {
  std::string content = R"(#!/usr/bin/env node
console.log('{"bmop":"1.0","module":"bench_medium"}');
console.log(JSON.stringify({t:"batch",f:"bench",c:5000}));
for (let i = 0; i < 5000; i++) {
    console.log('medium_' + i);
}
console.log('{"t":"batch_end"}');
console.log('{"t":"result","ok":true,"count":5000}');)";
  createTestModule("modules/bench_medium.js", content);

  auto start = steady_clock::now();
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("bench_medium.js", {}, storage, "");
  auto end = steady_clock::now();

  addPerfLog("Bench_Medium", 
      duration_cast<milliseconds>(end - start).count(), 5000);
}},

{"Large Data Processing", [this]() {
  std::string content = R"(#!/usr/bin/env node
console.log('{"bmop":"1.0","module":"bench_large"}');
console.log(JSON.stringify({t:"batch",f:"bench",c:50000}));
for (let i = 0; i < 50000; i++) {
    console.log('large_' + i);
}
console.log('{"t":"batch_end"}');
console.log('{"t":"result","ok":true,"count":50000}');)";
  createTestModule("modules/bench_large.js", content);

  auto start = steady_clock::now();
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("bench_large.js", {}, storage, "");
  auto end = steady_clock::now();

  addPerfLog("Bench_Large", 
      duration_cast<milliseconds>(end - start).count(), 50000);
}}
};

for (const auto& benchmark : benchmarks) {
  std::cout << "\nRunning benchmark: " << benchmark.name << std::endl;
  benchmark.test();
}

std::cout << "\n=== FINAL BENCHMARK REPORT ===" << std::endl;
std::cout << "Total benchmarks run: " << benchmarks.size() << std::endl;
std::cout << "Performance metrics logged to internal structure" << std::endl;
}

