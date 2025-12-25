#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <cstdio>
#include <memory>
#include <array>
#include "../core/core.hpp"

namespace fs = std::filesystem;

class BmopStderrTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::string timestamp = std::to_string(time(nullptr));
    test_dir = (fs::temp_directory_path() / ("bahamut_bmop_test_" + timestamp)).string();
    
    original_cwd = fs::current_path();
    fs::create_directories(test_dir);
    fs::current_path(test_dir);
    
    setupTestEnvironment();
  }
  
  void TearDown() override {
    fs::current_path(original_cwd);
    fs::remove_all(test_dir);
  }
  
  void setupTestEnvironment() {
    fs::create_directories("modules/collectors");
    fs::create_directories("modules/processors");
    fs::create_directories("modules/outputs");
    fs::create_directories("profiles");
    fs::create_directories("modules/shared_deps");
  }
  
  std::string test_dir;
  std::string original_cwd;
  
  void createTestModule(const std::string& filename, const std::string& content) {
    fs::path filepath(filename);
    fs::create_directories(filepath.parent_path());
    
    std::ofstream file(filename);
    file << content;
    file.close();
    
    fs::permissions(filepath, fs::perms::owner_exec, fs::perm_options::add);
  }
  
  std::pair<std::string, std::string> runModuleCaptureOutputs(const std::string& moduleName) {
    std::string fullPath = findModulePath(moduleName);
    if (fullPath.empty()) {
      return {"", "Module not found"};
    }
    
    ModuleMetadata meta = parseModuleMetadata(fullPath);
    std::string runner;
    
    if (fullPath.ends_with(".js")) {
      runner = "node";
    } else if (fullPath.ends_with(".py")) {
      runner = "python3";
    } else if (fullPath.ends_with(".sh")) {
      runner = "bash";
    } else {
      return {"", "Unknown interpreter"};
    }
    
    std::string cmd = runner + " " + fullPath;
    
    std::array<char, 128> buffer;
    std::string stdout_output;
    std::string stderr_output;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen((cmd + " 2>&1").c_str(), "r"), pclose);
    if (!pipe) {
      return {"", "Failed to open pipe"};
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      stdout_output += buffer.data();
    }
    
    return {stdout_output, ""};
  }
  
  std::tuple<std::string, std::string, int> runModuleSeparateStreams(const std::string& moduleName) {
    std::string fullPath = findModulePath(moduleName);
    if (fullPath.empty()) {
      return {"", "Module not found", -1};
    }
    
    std::string runner;
    if (fullPath.ends_with(".js")) {
      runner = "node";
    } else if (fullPath.ends_with(".py")) {
      runner = "python3";
    } else if (fullPath.ends_with(".sh")) {
      runner = "bash";
    }
    
    std::string cmd = runner + " " + fullPath;
    
    std::string stdout_cmd = cmd + " 2>/dev/null";
    std::string stderr_cmd = cmd + " 1>/dev/null";
    
    std::array<char, 4096> buffer;
    std::string stdout_output;
    std::string stderr_output;
    
    std::unique_ptr<FILE, decltype(&pclose)> stdout_pipe(popen(stdout_cmd.c_str(), "r"), pclose);
    if (stdout_pipe) {
      while (fgets(buffer.data(), buffer.size(), stdout_pipe.get()) != nullptr) {
        stdout_output += buffer.data();
      }
    }
    
    std::unique_ptr<FILE, decltype(&pclose)> stderr_pipe(popen(stderr_cmd.c_str(), "r"), pclose);
    if (stderr_pipe) {
      while (fgets(buffer.data(), buffer.size(), stderr_pipe.get()) != nullptr) {
        stderr_output += buffer.data();
      }
    }
    
    std::string full_cmd = cmd + " >/dev/null 2>&1";
    int exit_code = std::system(full_cmd.c_str());
    
    return {stdout_output, stderr_output, exit_code};
  }
};

TEST_F(BmopStderrTest, NodeModuleWithLogsAndData) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"test"}));
console.error(JSON.stringify({t:"log",l:"info",m:"Starting module"}));
console.error(JSON.stringify({t:"log",l:"debug",m:"Debug message"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"example.com"}));
console.error(JSON.stringify({t:"log",l:"warn",m:"Warning message"}));
console.error(JSON.stringify({t:"progress",c:1,T:10}));
console.log(JSON.stringify({t:"d",f:"domain",v:"test.com"}));
console.error(JSON.stringify({t:"log",l:"error",m:"Error occurred"}));
console.error(JSON.stringify({t:"progress",c:10,T:10}));
console.log(JSON.stringify({t:"result",ok:true,count:2}));)";
  
  createTestModule("modules/log_test.js", content);
  
  auto [stdout_output, stderr_output, exit_code] = runModuleSeparateStreams("log_test.js");
  
  EXPECT_EQ(exit_code, 0);
  
  EXPECT_TRUE(stdout_output.find("example.com") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("test.com") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("\"t\":\"d\"") != std::string::npos);
  
  EXPECT_TRUE(stderr_output.find("Starting module") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("Warning message") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("Error occurred") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"t\":\"progress\"") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"t\":\"log\"") != std::string::npos);
  
  EXPECT_TRUE(stdout_output.find("Starting module") == std::string::npos);
  EXPECT_TRUE(stdout_output.find("Warning message") == std::string::npos);
}

TEST_F(BmopStderrTest, PythonModuleWithMixedOutput) {
  std::string content = R"(#!/usr/bin/env python3
import json
import sys
print(json.dumps({"bmop":"1.0","module":"test"}))
print(json.dumps({"t":"log","l":"info","m":"Python starting"}), file=sys.stderr)
print(json.dumps({"t":"d","f":"url","v":"https://example.com"}))
print(json.dumps({"t":"progress","c":50,"T":100}), file=sys.stderr)
print(json.dumps({"t":"d","f":"url","v":"https://test.com"}))
print(json.dumps({"t":"log","l":"debug","m":"Processing complete"}), file=sys.stderr)
print(json.dumps({"t":"result","ok":True,"count":2})))";
  
  createTestModule("modules/py_log_test.py", content);
  
  auto [stdout_output, stderr_output, exit_code] = runModuleSeparateStreams("py_log_test.py");
  
  EXPECT_EQ(exit_code, 0);
  
  EXPECT_TRUE(stdout_output.find("https://example.com") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("https://test.com") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("\"t\":\"d\"") != std::string::npos);
  
  EXPECT_TRUE(stderr_output.find("Python starting") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("Processing complete") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"t\":\"progress\"") != std::string::npos);
  
  EXPECT_TRUE(stdout_output.find("Python starting") == std::string::npos);
  EXPECT_TRUE(stdout_output.find("\"t\":\"progress\"") == std::string::npos);
}

TEST_F(BmopStderrTest, BashModuleWithStderrRedirect) {
  std::string content = R"(#!/usr/bin/env bash
echo '{"bmop":"1.0","module":"bash-test"}'
echo '{"t":"log","l":"info","m":"Bash module starting"}' >&2
echo '{"t":"d","f":"ip","v":"192.168.1.1"}'
echo '{"t":"progress","c":1,"T":5}' >&2
echo '{"t":"d","f":"ip","v":"10.0.0.1"}'
echo '{"t":"log","l":"warn","m":"Warning from bash"}' >&2
echo '{"t":"result","ok":true,"count":2}')";
  
  createTestModule("modules/bash_log_test.sh", content);
  
  auto [stdout_output, stderr_output, exit_code] = runModuleSeparateStreams("bash_log_test.sh");
  
  EXPECT_EQ(exit_code, 0);
  
  EXPECT_TRUE(stdout_output.find("192.168.1.1") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("10.0.0.1") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("\"t\":\"d\"") != std::string::npos);
  
  EXPECT_TRUE(stderr_output.find("Bash module starting") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("Warning from bash") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"t\":\"progress\"") != std::string::npos);
}

TEST_F(BmopStderrTest, ErrorMessagesToStderr) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"error-test"}));
console.error(JSON.stringify({t:"error",code:"AUTH_FAILED",m:"Authentication failed"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"error.com"}));
console.error(JSON.stringify({t:"error",code:"NETWORK",m:"Connection timeout",fatal:true}));
console.log(JSON.stringify({t:"result",ok:false,error:"Multiple errors"}));)";
  
  createTestModule("modules/error_test.js", content);
  
  auto [stdout_output, stderr_output, exit_code] = runModuleSeparateStreams("error_test.js");
  
  EXPECT_TRUE(stderr_output.find("Authentication failed") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("Connection timeout") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"t\":\"error\"") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("AUTH_FAILED") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("NETWORK") != std::string::npos);
  
  EXPECT_TRUE(stdout_output.find("error.com") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("\"t\":\"d\"") != std::string::npos);
  
  EXPECT_TRUE(stdout_output.find("Authentication failed") == std::string::npos);
}

TEST_F(BmopStderrTest, ProgressUpdatesOnlyOnStderr) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"progress-test"}));
for (let i = 0; i <= 10; i++) {
  console.error(JSON.stringify({t:"progress",c:i,T:10}));
}
console.log(JSON.stringify({t:"d",f:"number",v:"100"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  createTestModule("modules/progress_test.js", content);
  
  auto [stdout_output, stderr_output, exit_code] = runModuleSeparateStreams("progress_test.js");
  
  EXPECT_EQ(exit_code, 0);
  
  size_t progress_count = 0;
  size_t pos = 0;
  while ((pos = stderr_output.find("\"t\":\"progress\"", pos)) != std::string::npos) {
    progress_count++;
    pos += 15;
  }
  
  EXPECT_GE(progress_count, 10);
  
  EXPECT_TRUE(stdout_output.find("100") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("\"t\":\"progress\"") == std::string::npos);
}

TEST_F(BmopStderrTest, BatchModeWithLogs) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"test"}));
console.error(JSON.stringify({t:"log",l:"info",m:"Starting batch"}));
console.log(JSON.stringify({t:"batch",f:"domain",c:3}));
console.log("batch1.com");
console.log("batch2.com");
console.log("batch3.com");
console.log(JSON.stringify({t:"batch_end"}));
console.error(JSON.stringify({t:"log",l:"info",m:"Batch complete"}));
console.log(JSON.stringify({t:"result",ok:true,count:3}));)";

  createTestModule("modules/batch_log_test.js", content);

  auto [stdout_output, stderr_output, exit_code] = runModuleSeparateStreams("batch_log_test.js");

  EXPECT_EQ(exit_code, 0);

  EXPECT_TRUE(stdout_output.find("batch1.com") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("batch2.com") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("batch3.com") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("\"t\":\"batch\"") != std::string::npos);
  EXPECT_TRUE(stdout_output.find("\"t\":\"batch_end\"") != std::string::npos);

  EXPECT_TRUE(stderr_output.find("Starting batch") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("Batch complete") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"t\":\"log\"") != std::string::npos);
}

TEST_F(BmopStderrTest, MixedBMOPMessagesCorrectSeparation) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"mixed-test"}));
console.error(JSON.stringify({t:"log",l:"info",m:"Step 1"}));
console.log(JSON.stringify({t:"d",f:"test",v:"data1"}));
console.error(JSON.stringify({t:"progress",c:1,T:3}));
console.error(JSON.stringify({t:"log",l:"debug",m:"Debug info"}));
console.log(JSON.stringify({t:"d",f:"test",v:"data2"}));
console.error(JSON.stringify({t:"progress",c:2,T:3}));
console.error(JSON.stringify({t:"error",code:"TEST",m:"Test error"}));
console.log(JSON.stringify({t:"d",f:"test",v:"data3"}));
console.error(JSON.stringify({t:"progress",c:3,T:3}));
console.error(JSON.stringify({t:"log",l:"info",m:"Step 2"}));
console.log(JSON.stringify({t:"result",ok:true,count:3}));)";
  
  createTestModule("modules/mixed_test.js", content);
  
  auto [stdout_output, stderr_output, exit_code] = runModuleSeparateStreams("mixed_test.js");
  
  EXPECT_EQ(exit_code, 0);
  
  std::vector<std::string> stdout_lines;
  std::istringstream stdout_stream(stdout_output);
  std::string line;
  while (std::getline(stdout_stream, line)) {
    if (!line.empty()) {
      stdout_lines.push_back(line);
    }
  }
  
  EXPECT_GE(stdout_lines.size(), 6);
  
  bool has_bmop = false;
  bool has_data1 = false;
  bool has_data2 = false;
  bool has_data3 = false;
  bool has_result = false;
  
  for (const auto& line : stdout_lines) {
    if (line.find("\"bmop\":\"1.0\"") != std::string::npos) has_bmop = true;
    if (line.find("data1") != std::string::npos) has_data1 = true;
    if (line.find("data2") != std::string::npos) has_data2 = true;
    if (line.find("data3") != std::string::npos) has_data3 = true;
    if (line.find("\"t\":\"result\"") != std::string::npos) has_result = true;
  }
  
  EXPECT_TRUE(has_bmop);
  EXPECT_TRUE(has_data1);
  EXPECT_TRUE(has_data2);
  EXPECT_TRUE(has_data3);
  EXPECT_TRUE(has_result);
  
  std::vector<std::string> stderr_lines;
  std::istringstream stderr_stream(stderr_output);
  while (std::getline(stderr_stream, line)) {
    if (!line.empty()) {
      stderr_lines.push_back(line);
    }
  }
  
  EXPECT_GE(stderr_lines.size(), 7);
  
  bool has_log_step1 = false;
  bool has_log_step2 = false;
  bool has_log_debug = false;
  bool has_error = false;
  bool has_progress1 = false;
  bool has_progress2 = false;
  bool has_progress3 = false;
  
  for (const auto& line : stderr_lines) {
    if (line.find("Step 1") != std::string::npos) has_log_step1 = true;
    if (line.find("Step 2") != std::string::npos) has_log_step2 = true;
    if (line.find("Debug info") != std::string::npos) has_log_debug = true;
    if (line.find("Test error") != std::string::npos) has_error = true;
    if (line.find("\"c\":1") != std::string::npos) has_progress1 = true;
    if (line.find("\"c\":2") != std::string::npos) has_progress2 = true;
    if (line.find("\"c\":3") != std::string::npos) has_progress3 = true;
  }
  
  EXPECT_TRUE(has_log_step1);
  EXPECT_TRUE(has_log_step2);
  EXPECT_TRUE(has_log_debug);
  EXPECT_TRUE(has_error);
  EXPECT_TRUE(has_progress1 || has_progress2 || has_progress3);
}

TEST_F(BmopStderrTest, ModuleWithFatalError) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"fatal-test"}));
console.error(JSON.stringify({t:"log",l:"info",m:"Starting"}));
console.error(JSON.stringify({t:"error",code:"FATAL",m:"Critical failure",fatal:true}));
console.log(JSON.stringify({t:"d",f:"data",v:"should not appear"}));
console.log(JSON.stringify({t:"result",ok:false,error:"Fatal error"}));)";
  
  createTestModule("modules/fatal_test.js", content);
  
  auto [stdout_output, stderr_output, exit_code] = runModuleSeparateStreams("fatal_test.js");
  
  EXPECT_TRUE(stderr_output.find("Critical failure") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"fatal\":true") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("FATAL") != std::string::npos);
  
  EXPECT_TRUE(stdout_output.find("should not appear") == std::string::npos);
}

TEST_F(BmopStderrTest, RealTimeProgressMonitoring) {
  std::string content = R"(#!/usr/bin/env python3
import json
import sys
import time
print(json.dumps({"bmop":"1.0","module":"realtime-progress"}))
for i in range(5):
    print(json.dumps({"t":"progress","c":i,"T":5}), file=sys.stderr)
    print(json.dumps({"t":"log","l":"info","m":f"Processed {i}/5"}), file=sys.stderr)
    time.sleep(0.01)
    print(json.dumps({"t":"d","f":"item","v":f"item_{i}"}))
print(json.dumps({"t":"result","ok":True,"count":5})))";

  createTestModule("modules/realtime_test.py", content);

  auto [stdout_output, stderr_output, exit_code] = runModuleSeparateStreams("realtime_test.py");

  EXPECT_EQ(exit_code, 0);

  for (int i = 0; i < 5; i++) {
    std::string processed_str = "Processed " + std::to_string(i) + "/5";
    std::string progress_c_str = "\"c\":" + std::to_string(i);
    std::string item_str = "item_" + std::to_string(i);

    EXPECT_TRUE(stderr_output.find(processed_str) != std::string::npos ||
                stderr_output.find(progress_c_str) != std::string::npos);
    EXPECT_TRUE(stdout_output.find(item_str) != std::string::npos);
  }

  EXPECT_TRUE(stdout_output.find("Processed 0/5") == std::string::npos);
  EXPECT_TRUE(stdout_output.find("\"t\":\"progress\"") == std::string::npos);
}

TEST_F(BmopStderrTest, LogLevelsSeparation) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"log-levels"}));
console.error(JSON.stringify({t:"log",l:"debug",m:"Debug message"}));
console.error(JSON.stringify({t:"log",l:"info",m:"Info message"}));
console.error(JSON.stringify({t:"log",l:"warn",m:"Warning message"}));
console.error(JSON.stringify({t:"log",l:"error",m:"Error message"}));
console.log(JSON.stringify({t:"d",f:"status",v:"ok"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  createTestModule("modules/levels_test.js", content);
  
  auto [stdout_output, stderr_output, exit_code] = runModuleSeparateStreams("levels_test.js");
  
  EXPECT_EQ(exit_code, 0);
  
  EXPECT_TRUE(stderr_output.find("Debug message") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("Info message") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("Warning message") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("Error message") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"l\":\"debug\"") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"l\":\"info\"") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"l\":\"warn\"") != std::string::npos);
  EXPECT_TRUE(stderr_output.find("\"l\":\"error\"") != std::string::npos);
  
  EXPECT_TRUE(stdout_output.find("Debug message") == std::string::npos);
  EXPECT_TRUE(stdout_output.find("Error message") == std::string::npos);
}

TEST_F(BmopStderrTest, CoreParsingIgnoresNonDataMessages) {
  std::map<std::string, std::vector<DataItem>> storage;
  
  parseBMOPLine(R"({"t":"log","l":"info","m":"Test log"})", storage);
  parseBMOPLine(R"({"t":"progress","c":1,"T":10})", storage);
  parseBMOPLine(R"({"t":"error","code":"TEST","m":"Error"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"example.com"})", storage);
  parseBMOPLine(R"({"t":"result","ok":true,"count":1})", storage);
  parseBMOPLine(R"({"t":"batch","f":"domain","c":2})", storage);
  parseBMOPLine(R"({"t":"batch_end"})", storage);
  
  EXPECT_EQ(storage.size(), 2);
  EXPECT_EQ(storage["domain"].size(), 1);
  EXPECT_EQ(storage["__batch_format__"].size(), 1);
  EXPECT_EQ(storage["domain"][0].value, "example.com");
  EXPECT_EQ(storage["__batch_format__"][0].format, "domain");
}

