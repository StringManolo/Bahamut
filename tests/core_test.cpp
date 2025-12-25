#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include "../core/core.hpp"

namespace fs = std::filesystem;

class BahamutTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::string timestamp = std::to_string(time(nullptr));
    test_dir = (fs::temp_directory_path() / ("bahamut_test_" + timestamp)).string();
    
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
    std::ofstream file(filename);
    file << content;
    file.close();
  }
  
  void createProfile(const std::string& name, const std::string& content) {
    std::ofstream file("profiles/bahamut_" + name + ".txt");
    file << content;
    file.close();
  }
};

TEST_F(BahamutTest, TrimStringBasic) {
  EXPECT_EQ(trimString(""), "");
  EXPECT_EQ(trimString("  hello  "), "hello");
  EXPECT_EQ(trimString("\t\n\r hello \t\n\r"), "hello");
  EXPECT_EQ(trimString("  hello world  "), "hello world");
}

TEST_F(BahamutTest, TrimStringNonBreakingSpaces) {
  std::string with_nbsp = "  \xc2\xa0hello\xc2\xa0  ";
  EXPECT_EQ(trimString(with_nbsp), "hello");
}

TEST_F(BahamutTest, FindModulePathSuccess) {
  createTestModule("modules/test.js", "console.log('test');");
  
  std::string path = findModulePath("test.js");
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(fs::exists(path));
}

TEST_F(BahamutTest, FindModulePathNotFound) {
  std::string path = findModulePath("nonexistent.js");
  EXPECT_TRUE(path.empty());
}

TEST_F(BahamutTest, FindModulePathRecursive) {
  createTestModule("modules/collectors/deep/test.js", "console.log('deep');");
  
  std::string path = findModulePath("test.js");
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(fs::exists(path));
}

TEST_F(BahamutTest, GetModulesEmpty) {
  auto modules = getModules();
  EXPECT_EQ(modules.size(), 0);
}

TEST_F(BahamutTest, GetModulesMultiple) {
  createTestModule("modules/test1.js", "console.log('1');");
  createTestModule("modules/collectors/test2.py", "print('2')");
  createTestModule("modules/processors/test3.sh", "echo '3'");
  
  auto modules = getModules();
  EXPECT_EQ(modules.size(), 3);
  
  std::set<std::string> module_set(modules.begin(), modules.end());
  EXPECT_TRUE(module_set.count("test1.js"));
  EXPECT_TRUE(module_set.count("test2.py"));
  EXPECT_TRUE(module_set.count("test3.sh"));
}

TEST_F(BahamutTest, GetModulesIgnoresDeps) {
  createTestModule("modules/test.js", "console.log('test');");
  createTestModule("modules/node_modules/ignore.js", "should be ignored");
  createTestModule("modules/shared_deps/ignore.py", "should be ignored");
  createTestModule("modules/python_libs/ignore.py", "should be ignored");
  
  auto modules = getModules();
  EXPECT_EQ(modules.size(), 1);
  EXPECT_EQ(modules[0], "test.js");
}

TEST_F(BahamutTest, ParseModuleMetadataFull) {
  std::string content = R"(#!/usr/bin/env node
// Name: Test Module
// Description: A test module
// Type: collector-domain
// Stage: 1
// Consumes: domain
// Provides: subdomain
// Install: npm install axios
// InstallScope: isolated
// Storage: replace
console.log('test');)";
  
  createTestModule("modules/test.js", content);
  
  ModuleMetadata meta = parseModuleMetadata("modules/test.js");
  EXPECT_EQ(meta.name, "Test Module");
  EXPECT_EQ(meta.description, "A test module");
  EXPECT_EQ(meta.type, "collector-domain");
  EXPECT_EQ(meta.stage, 1);
  EXPECT_EQ(meta.consumes, "domain");
  EXPECT_EQ(meta.provides, "subdomain");
  EXPECT_EQ(meta.installCmd, "npm install axios");
  EXPECT_EQ(meta.installScope, "isolated");
  EXPECT_EQ(meta.storageBehavior, "replace");
}

TEST_F(BahamutTest, ParseModuleMetadataPartial) {
  std::string content = R"(#!/usr/bin/env node
// Name: Partial Module
console.log('test');)";
  
  createTestModule("modules/test.js", content);
  
  ModuleMetadata meta = parseModuleMetadata("modules/test.js");
  EXPECT_EQ(meta.name, "Partial Module");
  EXPECT_EQ(meta.description, "");
  EXPECT_EQ(meta.type, "");
  EXPECT_EQ(meta.stage, 999);
  EXPECT_EQ(meta.consumes, "");
  EXPECT_EQ(meta.provides, "");
  EXPECT_EQ(meta.installCmd, "");
  EXPECT_EQ(meta.installScope, "shared");
  EXPECT_EQ(meta.storageBehavior, "add");
}

TEST_F(BahamutTest, ParseModuleMetadataStorageBehaviors) {
  std::string content_replace = R"(#!/usr/bin/env node
// Storage: replace)";
  
  std::string content_delete = R"(#!/usr/bin/env node
// Storage: delete)";
  
  std::string content_add = R"(#!/usr/bin/env node
// Storage: add)";
  
  std::string content_invalid = R"(#!/usr/bin/env node
// Storage: invalid)";
  
  createTestModule("modules/replace.js", content_replace);
  createTestModule("modules/delete.js", content_delete);
  createTestModule("modules/add.js", content_add);
  createTestModule("modules/invalid.js", content_invalid);
  
  EXPECT_EQ(parseModuleMetadata("modules/replace.js").storageBehavior, "replace");
  EXPECT_EQ(parseModuleMetadata("modules/delete.js").storageBehavior, "delete");
  EXPECT_EQ(parseModuleMetadata("modules/add.js").storageBehavior, "add");
  EXPECT_EQ(parseModuleMetadata("modules/invalid.js").storageBehavior, "add");
}

TEST_F(BahamutTest, GetPythonVersion) {
  std::string python39 = "#!/usr/bin/env python3.9";
  std::string python311 = "#!/usr/bin/env python3.11";
  std::string python3 = "#!/usr/bin/env python3";
  std::string python2 = "#!/usr/bin/env python2";
  std::string node = "#!/usr/bin/env node";
  
  createTestModule("modules/python39.py", python39);
  createTestModule("modules/python311.py", python311);
  createTestModule("modules/python3.py", python3);
  createTestModule("modules/python2.py", python2);
  createTestModule("modules/node.js", node);
  
  EXPECT_EQ(getPythonVersion("modules/python39.py"), "python3.9");
  EXPECT_EQ(getPythonVersion("modules/python311.py"), "python3.11");
  EXPECT_EQ(getPythonVersion("modules/python3.py"), "python3");
  EXPECT_EQ(getPythonVersion("modules/python2.py"), "python2");
  EXPECT_EQ(getPythonVersion("modules/node.js"), "python3");
}

TEST_F(BahamutTest, EnsurePackageJson) {
  std::string test_dir = "test_package";
  fs::create_directories(test_dir);
  
  ensurePackageJson(test_dir);
  EXPECT_TRUE(fs::exists(test_dir + "/package.json"));
  
  std::ifstream file(test_dir + "/package.json");
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  EXPECT_TRUE(content.find("\"name\": \"bahamut-module\"") != std::string::npos);
  
  fs::remove_all(test_dir);
}

TEST_F(BahamutTest, LoadProfileSuccess) {
  createProfile("test", R"(# Test profile
module1.js
module2.py

# Another comment
module3.sh)");
  
  auto modules = loadProfile("test");
  EXPECT_EQ(modules.size(), 3);
  EXPECT_EQ(modules[0], "module1.js");
  EXPECT_EQ(modules[1], "module2.py");
  EXPECT_EQ(modules[2], "module3.sh");
}

TEST_F(BahamutTest, LoadProfileNotFound) {
  auto modules = loadProfile("nonexistent");
  EXPECT_EQ(modules.size(), 0);
}

TEST_F(BahamutTest, LoadProfileEmpty) {
  createProfile("empty", "");
  auto modules = loadProfile("empty");
  EXPECT_EQ(modules.size(), 0);
}

TEST_F(BahamutTest, ParseBMOPLineData) {
  std::map<std::string, std::vector<DataItem>> storage;
  
  parseBMOPLine(R"({"t":"d","f":"domain","v":"example.com"})", storage);
  EXPECT_EQ(storage["domain"].size(), 1);
  EXPECT_EQ(storage["domain"][0].format, "domain");
  EXPECT_EQ(storage["domain"][0].value, "example.com");
}

TEST_F(BahamutTest, ParseBMOPLineBatch) {
  std::map<std::string, std::vector<DataItem>> storage;
  
  parseBMOPLine(R"({"t":"batch","f":"domain","c":1000})", storage);
  EXPECT_EQ(storage["__batch_format__"].size(), 1);
  EXPECT_EQ(storage["__batch_format__"][0].format, "domain");
  EXPECT_EQ(storage["__batch_format__"][0].value, "domain");
}

TEST_F(BahamutTest, ParseBMOPLineInvalidJson) {
  std::map<std::string, std::vector<DataItem>> storage;
  
  parseBMOPLine("invalid json", storage);
  parseBMOPLine("", storage);
  parseBMOPLine("{}", storage);
  parseBMOPLine(R"({"t":"d"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain"})", storage);
  
  EXPECT_TRUE(storage.empty());
}

TEST_F(BahamutTest, ParseBMOPLineNonDataTypes) {
  std::map<std::string, std::vector<DataItem>> storage;
  
  parseBMOPLine(R"({"t":"log","l":"info","m":"test"})", storage);
  parseBMOPLine(R"({"t":"progress","c":1,"T":10})", storage);
  parseBMOPLine(R"({"t":"result","ok":true})", storage);
  parseBMOPLine(R"({"t":"error","code":"TEST"})", storage);
  parseBMOPLine(R"({"t":"batch_end"})", storage);
  
  EXPECT_TRUE(storage.empty() || storage.size() == 1);
}

TEST_F(BahamutTest, SetupNodeEnvironmentShared) {
  createTestModule("modules/test.js", "console.log('test');");
  fs::create_directories("modules/shared_deps/node_modules");
  
  std::string source = setupNodeEnvironment("modules/test.js", "shared", "modules");
  EXPECT_FALSE(source.empty());
  
  EXPECT_TRUE(fs::exists("modules/node_modules") || fs::is_symlink("modules/node_modules"));
  
  char* node_path = std::getenv("NODE_PATH");
  EXPECT_TRUE(node_path != nullptr);
}

TEST_F(BahamutTest, SetupNodeEnvironmentIsolated) {
  createTestModule("modules/isolated/test.js", "console.log('test');");
  fs::create_directories("modules/isolated/node_modules");
  
  std::string source = setupNodeEnvironment("modules/isolated/test.js", "isolated", "modules/isolated");
  EXPECT_EQ(source, "modules/isolated/node_modules");
}

TEST_F(BahamutTest, SetupPythonEnvironmentShared) {
  createTestModule("modules/test.py", "print('test')");
  fs::create_directories("modules/shared_deps/python_libs");
  
  std::string source = setupPythonEnvironment("modules/test.py", "shared", "modules");
  EXPECT_EQ(source, "modules/shared_deps/python_libs");
  
  char* python_path = std::getenv("PYTHONPATH");
  EXPECT_TRUE(python_path != nullptr);
}

TEST_F(BahamutTest, SetupPythonEnvironmentIsolated) {
  createTestModule("modules/isolated/test.py", "print('test')");
  fs::create_directories("modules/isolated/python_libs");
  
  std::string source = setupPythonEnvironment("modules/isolated/test.py", "isolated", "modules/isolated");
  EXPECT_EQ(source, "modules/isolated/python_libs");
}

TEST_F(BahamutTest, CreateSimpleNodeModule) {
  std::string content = R"(#!/usr/bin/env node
// Name: Test Collector
// Description: Test module
// Type: collector-domain
// Stage: 1
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"test"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"example.com"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"test.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:2}));)";
  
  createTestModule("modules/test.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("test.js", {}, storage, "");
  
  EXPECT_EQ(storage["domain"].size(), 2);
  EXPECT_EQ(storage["domain"][0].value, "example.com");
  EXPECT_EQ(storage["domain"][1].value, "test.com");
}

TEST_F(BahamutTest, CreateSimplePythonModule) {
  std::string content = R"(#!/usr/bin/env python3
# Name: Python Collector
# Description: Test Python module
# Type: collector-domain
# Stage: 1
# Provides: domain
import json
import sys
print(json.dumps({"bmop":"1.0","module":"python-test"}))
print(json.dumps({"t":"d","f":"domain","v":"python.com"}))
print(json.dumps({"t":"result","ok":True,"count":1})))";
  
  createTestModule("modules/test.py", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("test.py", {}, storage, "");
  
  EXPECT_EQ(storage["domain"].size(), 1);
  EXPECT_EQ(storage["domain"][0].value, "python.com");
}

TEST_F(BahamutTest, CreateSimpleBashModule) {
  std::string content = R"(#!/usr/bin/env bash
# Name: Bash Collector
# Description: Test Bash module
# Type: collector-domain
# Stage: 1
# Provides: domain
echo '{"bmop":"1.0","module":"bash-test"}'
echo '{"t":"d","f":"domain","v":"bash.com"}'
echo '{"t":"result","ok":true,"count":1}')";
  
  createTestModule("modules/test.sh", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("test.sh", {}, storage, "");
  
  EXPECT_EQ(storage["domain"].size(), 1);
  EXPECT_EQ(storage["domain"][0].value, "bash.com");
}

TEST_F(BahamutTest, ModuleWithBatchOutput) {
  std::string content = R"(#!/usr/bin/env node
// Name: Batch Collector
// Description: Test batch output
// Type: collector-domain
// Stage: 1
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"batch-test"}));
console.log(JSON.stringify({t:"batch",f:"domain",c:3}));
console.log("example1.com");
console.log("example2.com");
console.log("example3.com");
console.log(JSON.stringify({t:"batch_end"}));
console.log(JSON.stringify({t:"result",ok:true,count:3}));)";
  
  createTestModule("modules/batch.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("batch.js", {}, storage, "");
  
  EXPECT_EQ(storage["domain"].size(), 3);
  EXPECT_EQ(storage["domain"][0].value, "example1.com");
  EXPECT_EQ(storage["domain"][1].value, "example2.com");
  EXPECT_EQ(storage["domain"][2].value, "example3.com");
}

TEST_F(BahamutTest, ModuleConsumesData) {
  std::string collector = R"(#!/usr/bin/env node
// Name: Data Collector
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"collector"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"input1.com"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"input2.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:2}));)";
  
  std::string processor = R"(#!/usr/bin/env node
// Name: Data Processor
// Consumes: domain
// Provides: subdomain
console.log(JSON.stringify({bmop:"1.0",module:"processor"}));
let count = 0;
process.stdin.on('data', chunk => {
  const lines = chunk.toString().split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    try {
      const msg = JSON.parse(line);
      if (msg.t === 'd' && msg.f === 'domain') {
        console.log(JSON.stringify({t:"d",f:"subdomain",v:"www." + msg.v}));
        count++;
      }
    } catch(e) {}
  }
});
process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";
  
  createTestModule("modules/collector.js", collector);
  createTestModule("modules/processor.js", processor);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  runModuleWithPipe("collector.js", {}, storage, "");
  EXPECT_EQ(storage["domain"].size(), 2);
  
  runModuleWithPipe("processor.js", {}, storage, "domain");
  EXPECT_EQ(storage["subdomain"].size(), 2);
  EXPECT_EQ(storage["subdomain"][0].value, "www.input1.com");
  EXPECT_EQ(storage["subdomain"][1].value, "www.input2.com");
}

TEST_F(BahamutTest, ModuleConsumesAllFormats) {
  std::string collector1 = R"(#!/usr/bin/env node
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"collector1"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"example.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  std::string collector2 = R"(#!/usr/bin/env node
// Provides: ip
console.log(JSON.stringify({bmop:"1.0",module:"collector2"}));
console.log(JSON.stringify({t:"d",f:"ip",v:"192.168.1.1"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  std::string processor = R"(#!/usr/bin/env node
// Consumes: *
// Provides: url
console.log(JSON.stringify({bmop:"1.0",module:"processor"}));
let count = 0;
process.stdin.on('data', chunk => {
  const lines = chunk.toString().split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    try {
      const msg = JSON.parse(line);
      if (msg.t === 'd') {
        if (msg.f === 'domain') {
          console.log(JSON.stringify({t:"d",f:"url",v:"https://" + msg.v}));
          count++;
        } else if (msg.f === 'ip') {
          console.log(JSON.stringify({t:"d",f:"url",v:"http://" + msg.v}));
          count++;
        }
      }
    } catch(e) {}
  }
});
process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";
  
  createTestModule("modules/collector1.js", collector1);
  createTestModule("modules/collector2.js", collector2);
  createTestModule("modules/processor.js", processor);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  runModuleWithPipe("collector1.js", {}, storage, "");
  runModuleWithPipe("collector2.js", {}, storage, "");
  
  EXPECT_EQ(storage["domain"].size(), 1);
  EXPECT_EQ(storage["ip"].size(), 1);
  
  runModuleWithPipe("processor.js", {}, storage, "*");
  
  EXPECT_EQ(storage["url"].size(), 2);
}

TEST_F(BahamutTest, StorageBehaviorReplace) {
  std::string processor = R"(#!/usr/bin/env node
// Consumes: domain
// Provides: domain
// Storage: replace
console.log(JSON.stringify({bmop:"1.0",module:"cleaner"}));
let cleaned = [];
process.stdin.on('data', chunk => {
  const lines = chunk.toString().split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    try {
      const msg = JSON.parse(line);
      if (msg.t === 'd' && msg.f === 'domain') {
        const clean = msg.v.toLowerCase();
        if (!cleaned.includes(clean)) {
          cleaned.push(clean);
        }
      }
    } catch(e) {}
  }
});
process.stdin.on('end', () => {
  cleaned.forEach(d => {
    console.log(JSON.stringify({t:"d",f:"domain",v:d}));
  });
  console.log(JSON.stringify({t:"result",ok:true,count:cleaned.length}));
});)";
  
  createTestModule("modules/cleaner.js", processor);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  storage["domain"].push_back({"domain", "EXAMPLE.COM"});
  storage["domain"].push_back({"domain", "Example.Com"});
  storage["domain"].push_back({"domain", "test.com"});
  
  EXPECT_EQ(storage["domain"].size(), 3);
  
  runModuleWithPipe("cleaner.js", {}, storage, "domain");
  
  EXPECT_EQ(storage["domain"].size(), 2);
}

TEST_F(BahamutTest, StorageBehaviorDelete) {
  std::string filter = R"(#!/usr/bin/env node
// Consumes: domain
// Provides: domain
// Storage: delete
console.log(JSON.stringify({bmop:"1.0",module:"filter"}));
process.stdin.on('data', chunk => {
  // Read but don't output anything - effectively deleting all domains
});
process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"result",ok:true,count:0}));
});)";
  
  createTestModule("modules/filter.js", filter);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  storage["domain"].push_back({"domain", "bad.com"});
  storage["domain"].push_back({"domain", "evil.com"});
  storage["other"].push_back({"other", "should remain"});
  
  EXPECT_EQ(storage["domain"].size(), 2);
  EXPECT_EQ(storage["other"].size(), 1);
  
  runModuleWithPipe("filter.js", {}, storage, "domain");
  
  EXPECT_EQ(storage.count("domain"), 0);
  EXPECT_EQ(storage["other"].size(), 1);
}

TEST_F(BahamutTest, StorageBehaviorAddDefault) {
  std::string collector1 = R"(#!/usr/bin/env node
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"collector1"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"first.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  std::string collector2 = R"(#!/usr/bin/env node
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"collector2"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"second.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  createTestModule("modules/collector1.js", collector1);
  createTestModule("modules/collector2.js", collector2);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  runModuleWithPipe("collector1.js", {}, storage, "");
  EXPECT_EQ(storage["domain"].size(), 1);
  
  runModuleWithPipe("collector2.js", {}, storage, "");
  EXPECT_EQ(storage["domain"].size(), 2);
}

TEST_F(BahamutTest, ProfileExecutionOrder) {
  std::string module1 = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"module1"}));
console.log(JSON.stringify({t:"d",f:"order",v:"1"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  std::string module2 = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"module2"}));
console.log(JSON.stringify({t:"d",f:"order",v:"2"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  std::string module3 = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"module3"}));
console.log(JSON.stringify({t:"d",f:"order",v:"3"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  createTestModule("modules/module1.js", module1);
  createTestModule("modules/module2.js", module2);
  createTestModule("modules/module3.js", module3);
  
  createProfile("test_order", R"(module2.js
module1.js
module3.js)");
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  auto modules = loadProfile("test_order");
  for (const auto& module : modules) {
    runModuleWithPipe(module, {}, storage, "");
  }
  
  EXPECT_EQ(storage["order"].size(), 3);
  EXPECT_EQ(storage["order"][0].value, "2");
  EXPECT_EQ(storage["order"][1].value, "1");
  EXPECT_EQ(storage["order"][2].value, "3");
}

TEST_F(BahamutTest, RunModulesByStage) {
  std::string stage1 = R"(#!/usr/bin/env node
// Stage: 1
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"stage1"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"stage1.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  std::string stage2 = R"(#!/usr/bin/env node
// Stage: 2
// Consumes: domain
// Provides: subdomain
console.log(JSON.stringify({bmop:"1.0",module:"stage2"}));
let count = 0;
process.stdin.on('data', chunk => {
  const lines = chunk.toString().split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    try {
      const msg = JSON.parse(line);
      if (msg.t === 'd' && msg.f === 'domain') {
        console.log(JSON.stringify({t:"d",f:"subdomain",v:"www." + msg.v}));
        count++;
      }
    } catch(e) {}
  }
});
process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";
  
  std::string stage99 = R"(#!/usr/bin/env node
// Stage: 99
// Consumes: *
console.log(JSON.stringify({bmop:"1.0",module:"stage99"}));
let total = 0;
process.stdin.on('data', chunk => {
  const lines = chunk.toString().split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    total++;
  }
});
process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"log",l:"info",m:"Total items: " + total}));
  console.log(JSON.stringify({t:"result",ok:true,count:total}));
});)";
  
  createTestModule("modules/stage1.js", stage1);
  createTestModule("modules/stage2.js", stage2);
  createTestModule("modules/stage99.js", stage99);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  auto modules = getModules();
  std::map<int, std::vector<std::pair<std::string, ModuleMetadata>>> stageModules;
  
  for (const auto& moduleName : modules) {
    std::string path = findModulePath(moduleName);
    ModuleMetadata meta = parseModuleMetadata(path);
    stageModules[meta.stage].push_back({moduleName, meta});
  }
  
  for (auto& [stage, moduleList] : stageModules) {
    for (const auto& [moduleName, meta] : moduleList) {
      runModuleWithPipe(moduleName, {}, storage, meta.consumes);
    }
  }
  
  EXPECT_EQ(storage["domain"].size(), 1);
  EXPECT_EQ(storage["subdomain"].size(), 1);
  EXPECT_EQ(storage["subdomain"][0].value, "www.stage1.com");
}

TEST_F(BahamutTest, ModuleWithLogsAndProgress) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"logger"}));
console.error(JSON.stringify({t:"log",l:"info",m:"Starting"}));
console.error(JSON.stringify({t:"progress",c:1,T:10}));
console.error(JSON.stringify({t:"log",l:"debug",m:"Processing"}));
console.log(JSON.stringify({t:"d",f:"test",v:"value"}));
console.error(JSON.stringify({t:"progress",c:10,T:10}));
console.error(JSON.stringify({t:"log",l:"info",m:"Done"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  createTestModule("modules/logger.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("logger.js", {}, storage, "");
  
  EXPECT_EQ(storage["test"].size(), 1);
  EXPECT_EQ(storage["test"][0].value, "value");
}

TEST_F(BahamutTest, ModuleWithError) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"error"}));
console.error(JSON.stringify({t:"error",code:"TEST",m:"Test error",fatal:true}));
console.log(JSON.stringify({t:"result",ok:false,error":"Test error"}));
process.exit(1);)";
  
  createTestModule("modules/error.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("error.js", {}, storage, "");
  
  EXPECT_TRUE(storage.empty());
}

TEST_F(BahamutTest, ModuleWithMixedBatchAndIndividual) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"mixed"}));
console.log(JSON.stringify({t:"batch",f:"domain",c:2}));
console.log("batch1.com");
console.log("batch2.com");
console.log(JSON.stringify({t:"batch_end"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"individual.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:3}));)";
  
  createTestModule("modules/mixed.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("mixed.js", {}, storage, "");
  
  EXPECT_EQ(storage["domain"].size(), 3);
  EXPECT_EQ(storage["domain"][0].value, "batch1.com");
  EXPECT_EQ(storage["domain"][1].value, "batch2.com");
  EXPECT_EQ(storage["domain"][2].value, "individual.com");
}

TEST_F(BahamutTest, ModuleInstallationMetadata) {
  std::string node_shared = R"(#!/usr/bin/env node
// Install: npm install axios
// InstallScope: shared)";
  
  std::string node_isolated = R"(#!/usr/bin/env node
// Install: npm install lodash
// InstallScope: isolated)";
  
  std::string node_global = R"(#!/usr/bin/env node
// Install: npm install -g typescript
// InstallScope: global)";
  
  std::string python_shared = R"(#!/usr/bin/env python3
# Install: pip install requests
# InstallScope: shared)";
  
  std::string python_isolated = R"(#!/usr/bin/env python3
# Install: pip install beautifulsoup4
# InstallScope: isolated)";
  
  std::string bash_none = R"(#!/usr/bin/env bash
# Install: apt-get install curl
# InstallScope: global)";
  
  createTestModule("modules/node_shared.js", node_shared);
  createTestModule("modules/node_isolated.js", node_isolated);
  createTestModule("modules/node_global.js", node_global);
  createTestModule("modules/python_shared.py", python_shared);
  createTestModule("modules/python_isolated.py", python_isolated);
  createTestModule("modules/bash_none.sh", bash_none);
  
  EXPECT_EQ(parseModuleMetadata("modules/node_shared.js").installScope, "shared");
  EXPECT_EQ(parseModuleMetadata("modules/node_isolated.js").installScope, "isolated");
  EXPECT_EQ(parseModuleMetadata("modules/node_global.js").installScope, "global");
  EXPECT_EQ(parseModuleMetadata("modules/python_shared.py").installScope, "shared");
  EXPECT_EQ(parseModuleMetadata("modules/python_isolated.py").installScope, "isolated");
  EXPECT_EQ(parseModuleMetadata("modules/bash_none.sh").installScope, "global");
}

TEST_F(BahamutTest, ModuleTypeAndStageClassification) {
  std::string collector = R"(#!/usr/bin/env node
// Type: collector-domain
// Stage: 1
// Provides: domain)";
  
  std::string processor = R"(#!/usr/bin/env node
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: subdomain)";
  
  std::string output = R"(#!/usr/bin/env node
// Type: output
// Stage: 3
// Consumes: *)";
  
  std::string unspecified = R"(#!/usr/bin/env node
// Just a module)";
  
  createTestModule("modules/collector.js", collector);
  createTestModule("modules/processor.js", processor);
  createTestModule("modules/output.js", output);
  createTestModule("modules/unspecified.js", unspecified);
  
  EXPECT_EQ(parseModuleMetadata("modules/collector.js").type, "collector-domain");
  EXPECT_EQ(parseModuleMetadata("modules/collector.js").stage, 1);
  EXPECT_EQ(parseModuleMetadata("modules/processor.js").type, "processor");
  EXPECT_EQ(parseModuleMetadata("modules/processor.js").stage, 2);
  EXPECT_EQ(parseModuleMetadata("modules/output.js").type, "output");
  EXPECT_EQ(parseModuleMetadata("modules/output.js").stage, 3);
  EXPECT_EQ(parseModuleMetadata("modules/unspecified.js").type, "");
  EXPECT_EQ(parseModuleMetadata("modules/unspecified.js").stage, 999);
}

TEST_F(BahamutTest, ProfileWithCommentsAndEmptyLines) {
  createProfile("comments", R"(# This is a comment

# Another comment
module1.js

module2.py
# Inline comment

# Final comment
module3.sh)");
  
  auto modules = loadProfile("comments");
  EXPECT_EQ(modules.size(), 3);
  EXPECT_EQ(modules[0], "module1.js");
  EXPECT_EQ(modules[1], "module2.py");
  EXPECT_EQ(modules[2], "module3.sh");
}

TEST_F(BahamutTest, ModuleWithCustomDataFormats) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"custom"}));
console.log(JSON.stringify({t:"d",f:"custom-format",v:"custom-value"}));
console.log(JSON.stringify({t:"d",f:"vulnerability",v:"{\"type\":\"XSS\",\"severity\":\"high\"}"}));
console.log(JSON.stringify({t:"d",f:"credential",v:"{\"user\":\"admin\",\"pass\":\"secret\"}"}));
console.log(JSON.stringify({t:"result",ok:true,count:3}));)";
  
  createTestModule("modules/custom.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("custom.js", {}, storage, "");
  
  EXPECT_EQ(storage["custom-format"].size(), 1);
  EXPECT_EQ(storage["vulnerability"].size(), 1);
  EXPECT_EQ(storage["credential"].size(), 1);
  
  EXPECT_EQ(storage["custom-format"][0].value, "custom-value");
  EXPECT_EQ(storage["vulnerability"][0].value, "{\"type\":\"XSS\",\"severity\":\"high\"}");
  EXPECT_EQ(storage["credential"][0].value, "{\"user\":\"admin\",\"pass\":\"secret\"}");
}

TEST_F(BahamutTest, ComplexPipelineIntegration) {
  std::string collector = R"(#!/usr/bin/env node
// Type: collector-domain
// Stage: 1
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"collector"}));
console.log(JSON.stringify({t:"batch",f:"domain",c:3}));
console.log("EXAMPLE.COM");
console.log("TEST.COM");
console.log("WILD.*.COM");
console.log(JSON.stringify({t:"batch_end"}));
console.log(JSON.stringify({t:"result",ok:true,count:3}));)";
  
  std::string cleaner = R"(#!/usr/bin/env node
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: domain
// Storage: replace
console.log(JSON.stringify({bmop:"1.0",module:"cleaner"}));
let cleaned = new Set();
process.stdin.on('data', chunk => {
  const lines = chunk.toString().split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    try {
      const msg = JSON.parse(line);
      if (msg.t === 'd' && msg.f === 'domain') {
        let domain = msg.v.toLowerCase();
        if (domain.startsWith('*.')) domain = domain.substring(2);
        if (domain.startsWith('wild.')) domain = domain.substring(5);
        cleaned.add(domain);
      }
    } catch(e) {}
  }
});
process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"batch",f:"domain",c:cleaned.size}));
  for (const domain of cleaned) {
    console.log(domain);
  }
  console.log(JSON.stringify({t:"batch_end"}));
  console.log(JSON.stringify({t:"result",ok:true,count:cleaned.size}));
});)";
  
  std::string subdomain_gen = R"(#!/usr/bin/env node
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: subdomain
console.log(JSON.stringify({bmop:"1.0",module:"subdomain-gen"}));
let count = 0;
process.stdin.on('data', chunk => {
  const lines = chunk.toString().split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    try {
      const msg = JSON.parse(line);
      if (msg.t === 'd' && msg.f === 'domain') {
        const prefixes = ['www', 'api', 'mail'];
        for (const prefix of prefixes) {
          console.log(JSON.stringify({t:"d",f:"subdomain",v:prefix + '.' + msg.v}));
          count++;
        }
      }
    } catch(e) {}
  }
});
process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";
  
  createTestModule("modules/collector.js", collector);
  createTestModule("modules/cleaner.js", cleaner);
  createTestModule("modules/subdomain_gen.js", subdomain_gen);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  runModuleWithPipe("collector.js", {}, storage, "");
  EXPECT_EQ(storage["domain"].size(), 3);
  
  runModuleWithPipe("cleaner.js", {}, storage, "domain");
  EXPECT_EQ(storage["domain"].size(), 2);
  
  runModuleWithPipe("subdomain_gen.js", {}, storage, "domain");
  EXPECT_EQ(storage["subdomain"].size(), 6);
  
  EXPECT_EQ(storage["domain"][0].value, "example.com");
  EXPECT_EQ(storage["domain"][1].value, "test.com");
  
  bool found_www_example = false;
  bool found_api_test = false;
  for (const auto& item : storage["subdomain"]) {
    if (item.value == "www.example.com") found_www_example = true;
    if (item.value == "api.test.com") found_api_test = true;
  }
  EXPECT_TRUE(found_www_example);
  EXPECT_TRUE(found_api_test);
}

TEST_F(BahamutTest, ModuleWithArguments) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"args"}));
const args = process.argv.slice(2);
args.forEach((arg, i) => {
  console.log(JSON.stringify({t:"d",f:"arg",v:`${i}:${arg}`}));
});
console.log(JSON.stringify({t:"result",ok:true,count:args.length}));)";
  
  createTestModule("modules/args.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  std::vector<std::string> args = {"--target", "example.com", "--verbose"};
  
  runModuleWithPipe("args.js", args, storage, "");
  
  EXPECT_EQ(storage["arg"].size(), 3);
  EXPECT_EQ(storage["arg"][0].value, "0:--target");
  EXPECT_EQ(storage["arg"][1].value, "1:example.com");
  EXPECT_EQ(storage["arg"][2].value, "2:--verbose");
}

TEST_F(BahamutTest, LargeBatchPerformance) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"large-batch"}));
const count = 1000;
console.log(JSON.stringify({t:"batch",f:"number",c:count}));
for (let i = 0; i < count; i++) {
  console.log(i.toString());
}
console.log(JSON.stringify({t:"batch_end"}));
console.log(JSON.stringify({t:"result",ok:true,count:count}));)";
  
  createTestModule("modules/large.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("large.js", {}, storage, "");
  
  EXPECT_EQ(storage["number"].size(), 1000);
  for (int i = 0; i < 1000; i++) {
    EXPECT_EQ(storage["number"][i].value, std::to_string(i));
  }
}

TEST_F(BahamutTest, ModuleWithMalformedBMOP) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"malformed"}));
console.log("Not JSON");
console.log("{invalid json");
console.log("{\"t\":\"d\"}");  // Missing f and v
console.log("{\"t\":\"d\",\"f\":\"test\"}");  // Missing v
console.log("{\"t\":\"d\",\"v\":\"value\"}");  // Missing f
console.log(JSON.stringify({t:"d",f:"good",v:"value"}));  // Good one
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  createTestModule("modules/malformed.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("malformed.js", {}, storage, "");
  
  EXPECT_EQ(storage["good"].size(), 1);
  EXPECT_EQ(storage["good"][0].value, "value");
}

TEST_F(BahamutTest, ChainedProcessorsWithStorageBehaviors) {
  std::string source1 = R"(#!/usr/bin/env node
// Provides: data
console.log(JSON.stringify({bmop:"1.0",module:"source1"}));
console.log(JSON.stringify({t:"d",f:"data",v:"A"}));
console.log(JSON.stringify({t:"d",f:"data",v:"B"}));
console.log(JSON.stringify({t:"d",f:"data",v:"C"}));
console.log(JSON.stringify({t:"result",ok:true,count:3}));)";
  
  std::string filter = R"(#!/usr/bin/env node
// Consumes: data
// Provides: data
// Storage: delete
console.log(JSON.stringify({bmop:"1.0",module:"filter"}));
process.stdin.on('data', chunk => {
  // Delete everything
});
process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"result",ok:true,count:0}));
});)";
  
  std::string source2 = R"(#!/usr/bin/env node
// Provides: data
console.log(JSON.stringify({bmop:"1.0",module:"source2"}));
console.log(JSON.stringify({t:"d",f:"data",v:"D"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  createTestModule("modules/source1.js", source1);
  createTestModule("modules/filter.js", filter);
  createTestModule("modules/source2.js", source2);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  runModuleWithPipe("source1.js", {}, storage, "");
  EXPECT_EQ(storage["data"].size(), 3);
  
  runModuleWithPipe("filter.js", {}, storage, "data");
  EXPECT_EQ(storage.count("data"), 0);
  
  runModuleWithPipe("source2.js", {}, storage, "");
  EXPECT_EQ(storage["data"].size(), 1);
  EXPECT_EQ(storage["data"][0].value, "D");
}

TEST_F(BahamutTest, EnsureDirectoriesExist) {
  fs::remove_all("modules");
  fs::remove_all("profiles");
  
  std::string module = "modules/collectors/test.js";
  std::string profile = "profiles/bahamut_test.txt";
  
  EXPECT_FALSE(fs::exists("modules"));
  EXPECT_FALSE(fs::exists("profiles"));
  
  std::vector<std::string> modules = getModules();
  EXPECT_EQ(modules.size(), 0);
  
  createTestModule(module, "console.log('test');");
  EXPECT_TRUE(fs::exists(module));
  
  createProfile("test", "test.js");
  EXPECT_TRUE(fs::exists(profile));
}

TEST_F(BahamutTest, ModulePathResolution) {
  fs::create_directories("modules/deep/nested/structure");
  createTestModule("modules/deep/nested/structure/hidden.js", "console.log('hidden');");
  createTestModule("modules/shallow.js", "console.log('shallow');");
  createTestModule("modules/deep/top.js", "console.log('top');");
  
  EXPECT_FALSE(findModulePath("nonexistent.js").empty());
  EXPECT_FALSE(findModulePath("hidden.js").empty());
  EXPECT_FALSE(findModulePath("shallow.js").empty());
  EXPECT_FALSE(findModulePath("top.js").empty());
  
  auto modules = getModules();
  EXPECT_EQ(modules.size(), 3);
}

TEST_F(BahamutTest, ProfileExecutionIntegration) {
  std::string module1 = R"(#!/usr/bin/env node
// Name: First
console.log(JSON.stringify({bmop:"1.0",module:"first"}));
console.log(JSON.stringify({t:"d",f:"output",v:"first"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  std::string module2 = R"(#!/usr/bin/env node
// Name: Second
// Consumes: output
console.log(JSON.stringify({bmop:"1.0",module:"second"}));
let received = [];
process.stdin.on('data', chunk => {
  const lines = chunk.toString().split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    try {
      const msg = JSON.parse(line);
      if (msg.t === 'd' && msg.f === 'output') {
        received.push(msg.v);
      }
    } catch(e) {}
  }
});
process.stdin.on('end', () => {
  received.forEach(val => {
    console.log(JSON.stringify({t:"d",f:"processed",v:val.toUpperCase()}));
  });
  console.log(JSON.stringify({t:"result",ok:true,count:received.length}));
});)";
  
  createTestModule("modules/first.js", module1);
  createTestModule("modules/second.js", module2);
  
  createProfile("integration", R"(first.js
second.js)");
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  auto modules = loadProfile("integration");
  for (const auto& module : modules) {
    std::string path = findModulePath(module);
    ModuleMetadata meta = parseModuleMetadata(path);
    runModuleWithPipe(module, {}, storage, meta.consumes);
  }
  
  EXPECT_EQ(storage["output"].size(), 1);
  EXPECT_EQ(storage["processed"].size(), 1);
  EXPECT_EQ(storage["output"][0].value, "first");
  EXPECT_EQ(storage["processed"][0].value, "FIRST");
}

TEST_F(BahamutTest, MixedLanguagePipeline) {
  std::string node_collector = R"(#!/usr/bin/env node
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"node-collector"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"node.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  std::string python_processor = R"(#!/usr/bin/env python3
# Consumes: domain
# Provides: url
import json
import sys
print(json.dumps({"bmop":"1.0","module":"python-processor"}))
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        msg = json.loads(line)
        if msg.get('t') == 'd' and msg.get('f') == 'domain':
            domain = msg.get('v')
            print(json.dumps({"t":"d","f":"url","v":f"https://{domain}"}))
    except:
        pass
print(json.dumps({"t":"result","ok":True,"count":1})))";
  
  std::string bash_output = R"(#!/usr/bin/env bash
# Consumes: *
echo '{"bmop":"1.0","module":"bash-output"}'
count=0
while IFS= read -r line; do
    [[ -z "$line" ]] && continue
    if echo "$line" | grep -q '"t":"d"'; then
        ((count++))
    fi
done
echo "{\"t\":\"log\",\"l\":\"info\",\"m\":\"Processed $count items\"}" >&2
echo "{\"t\":\"result\",\"ok\":true,\"count\":$count}")";
  
  createTestModule("modules/node.js", node_collector);
  createTestModule("modules/python.py", python_processor);
  createTestModule("modules/bash.sh", bash_output);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  runModuleWithPipe("node.js", {}, storage, "");
  EXPECT_EQ(storage["domain"].size(), 1);
  
  runModuleWithPipe("python.py", {}, storage, "domain");
  EXPECT_EQ(storage["url"].size(), 1);
  EXPECT_EQ(storage["url"][0].value, "https://node.com");
  
  runModuleWithPipe("bash.sh", {}, storage, "*");
  EXPECT_EQ(storage["url"].size(), 1);
}

TEST_F(BahamutTest, ModuleWithTrailingCommasInJSON) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"trailing",}));
console.log(JSON.stringify({t:"d",f:"test",v:"value",}));
console.log(JSON.stringify({t:"result",ok:true,count:1,}));)";
  
  createTestModule("modules/trailing.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("trailing.js", {}, storage, "");
  
  EXPECT_EQ(storage["test"].size(), 1);
  EXPECT_EQ(storage["test"][0].value, "value");
}

TEST_F(BahamutTest, ModuleWithCommentsInJSON) {
  std::string content = R"(#!/usr/bin/env node
console.log('{"bmop":"1.0","module":"comments"} // comment');
console.log('{"t":"d","f":"test","v":"value"} /* comment */');
console.log('{"t":"result","ok":true,"count":1}');)";
  
  createTestModule("modules/comments.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("comments.js", {}, storage, "");
  
  EXPECT_EQ(storage["test"].size(), 1);
  EXPECT_EQ(storage["test"][0].value, "value");
}

TEST_F(BahamutTest, EdgeCaseValues) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"edge"}));
console.log(JSON.stringify({t:"d",f:"empty",v:""}));
console.log(JSON.stringify({t:"d",f:"special",v:"line\nbreak"}));
console.log(JSON.stringify({t:"d",f:"special",v:"tab\there"}));
console.log(JSON.stringify({t:"d",f:"special",v:"quote\"quote"}));
console.log(JSON.stringify({t:"d",f:"special",v:"backslash\\here"}));
console.log(JSON.stringify({t:"result",ok:true,count:5}));)";
  
  createTestModule("modules/edge.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("edge.js", {}, storage, "");
  
  EXPECT_EQ(storage["empty"].size(), 1);
  EXPECT_EQ(storage["empty"][0].value, "");
  EXPECT_EQ(storage["special"].size(), 4);
}

TEST_F(BahamutTest, MultipleFormatsInOneModule) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"multi"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"example.com"}));
console.log(JSON.stringify({t:"d",f:"ip",v:"1.2.3.4"}));
console.log(JSON.stringify({t:"d",f:"url",v:"https://example.com"}));
console.log(JSON.stringify({t:"d",f:"email",v:"admin@example.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:4}));)";
  
  createTestModule("modules/multi.js", content);
  
  std::map<std::string, std::vector<DataItem>> storage;
  runModuleWithPipe("multi.js", {}, storage, "");
  
  EXPECT_EQ(storage["domain"].size(), 1);
  EXPECT_EQ(storage["ip"].size(), 1);
  EXPECT_EQ(storage["url"].size(), 1);
  EXPECT_EQ(storage["email"].size(), 1);
  
  EXPECT_EQ(storage["domain"][0].value, "example.com");
  EXPECT_EQ(storage["ip"][0].value, "1.2.3.4");
  EXPECT_EQ(storage["url"][0].value, "https://example.com");
  EXPECT_EQ(storage["email"][0].value, "admin@example.com");
}

TEST_F(BahamutTest, ModuleNotFoundError) {
  std::map<std::string, std::vector<DataItem>> storage;
  
  testing::internal::CaptureStdout();
  runModuleWithPipe("nonexistent.js", {}, storage, "");
  std::string output = testing::internal::GetCapturedStdout();
  
  EXPECT_TRUE(output.find("not found") != std::string::npos);
}

TEST_F(BahamutTest, ProfileModuleNotFound) {
  createProfile("missing", R"(existing.js
nonexistent.js)");
  
  std::string module = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"existing"}));
console.log(JSON.stringify({t:"result",ok:true,count:0}));)";
  
  createTestModule("modules/existing.js", module);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  testing::internal::CaptureStdout();
  auto modules = loadProfile("missing");
  for (const auto& moduleName : modules) {
    std::string path = findModulePath(moduleName);
    if (!path.empty()) {
      ModuleMetadata meta = parseModuleMetadata(path);
      runModuleWithPipe(moduleName, {}, storage, meta.consumes);
    }
  }
  std::string output = testing::internal::GetCapturedStdout();
  
  EXPECT_TRUE(output.find("not found") != std::string::npos);
  EXPECT_EQ(storage.size(), 0);
}

TEST_F(BahamutTest, InvalidShebangHandling) {
  std::string content = "Invalid shebang\n// Name: Test";
  createTestModule("modules/invalid.js", content);
  
  ModuleMetadata meta = parseModuleMetadata("modules/invalid.js");
  EXPECT_EQ(meta.name, "Test");
}

TEST_F(BahamutTest, EmptyModuleFile) {
  createTestModule("modules/empty.js", "");
  
  ModuleMetadata meta = parseModuleMetadata("modules/empty.js");
  EXPECT_EQ(meta.name, "");
  EXPECT_EQ(meta.stage, 999);
}

TEST_F(BahamutTest, ModuleWithOnlyShebang) {
  std::string content = "#!/usr/bin/env node";
  createTestModule("modules/shebang.js", content);
  
  ModuleMetadata meta = parseModuleMetadata("modules/shebang.js");
  EXPECT_EQ(meta.name, "");
  EXPECT_EQ(meta.stage, 999);
}

TEST_F(BahamutTest, StoragePersistsBetweenModules) {
  std::string collector1 = R"(#!/usr/bin/env node
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"c1"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"one.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  std::string collector2 = R"(#!/usr/bin/env node
// Provides: domain
console.log(JSON.stringify({bmop:"1.0",module:"c2"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"two.com"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  std::string processor = R"(#!/usr/bin/env node
// Consumes: *
console.log(JSON.stringify({bmop:"1.0",module:"p"}));
let count = 0;
process.stdin.on('data', chunk => {
  const lines = chunk.toString().split('\n');
  count += lines.filter(l => l.trim()).length;
});
process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"result",ok:true,count:count}));
});)";
  
  createTestModule("modules/c1.js", collector1);
  createTestModule("modules/c2.js", collector2);
  createTestModule("modules/p.js", processor);
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  runModuleWithPipe("c1.js", {}, storage, "");
  EXPECT_EQ(storage["domain"].size(), 1);
  
  runModuleWithPipe("c2.js", {}, storage, "");
  EXPECT_EQ(storage["domain"].size(), 2);
  
  runModuleWithPipe("p.js", {}, storage, "*");
  EXPECT_EQ(storage["domain"].size(), 2);
}

TEST_F(BahamutTest, CompleteSystemTest) {
  fs::create_directories("modules/collectors");
  fs::create_directories("modules/processors");
  fs::create_directories("modules/outputs");
  fs::create_directories("profiles");
  
  std::string collector = R"(#!/usr/bin/env node
// Name: Domain Collector
// Description: Collects test domains
// Type: collector-domain
// Stage: 1
// Provides: domain
// Install: npm install fake-package
// InstallScope: shared
console.log(JSON.stringify({bmop:"1.0",module:"collector"}));
console.error(JSON.stringify({t:"log",l:"info",m:"Starting collection"}));
const domains = ["alpha.com", "BETA.COM", "gamma.com", "*.wildcard.com"];
console.error(JSON.stringify({t:"progress",c:0,T:domains.length}));
console.log(JSON.stringify({t:"batch",f:"domain",c:domains.length}));
domains.forEach(d => console.log(d));
console.log(JSON.stringify({t:"batch_end"}));
console.error(JSON.stringify({t:"progress",c:domains.length,T:domains.length}));
console.log(JSON.stringify({t:"result",ok:true,count:domains.length}));)";
  
  std::string cleaner = R"(#!/usr/bin/env node
// Name: Domain Cleaner
// Description: Cleans and normalizes domains
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: domain
// Storage: replace
// Install:
// InstallScope: global
console.log(JSON.stringify({bmop:"1.0",module:"cleaner"}));
let cleaned = new Set();
process.stdin.on('data', chunk => {
  const lines = chunk.toString().split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    try {
      const msg = JSON.parse(line);
      if (msg.t === 'd' && msg.f === 'domain') {
        let domain = msg.v.toLowerCase();
        if (domain.startsWith('*.')) domain = domain.substring(2);
        if (domain && !cleaned.has(domain)) {
          cleaned.add(domain);
        }
      }
    } catch(e) {}
  }
});
process.stdin.on('end', () => {
  console.error(JSON.stringify({t:"log",l:"info",m:"Cleaned " + cleaned.size + " domains"}));
  const sorted = Array.from(cleaned).sort();
  console.log(JSON.stringify({t:"batch",f:"domain",c:sorted.length}));
  sorted.forEach(d => console.log(d));
  console.log(JSON.stringify({t:"batch_end"}));
  console.log(JSON.stringify({t:"result",ok:true,count:sorted.length}));
});)";
  
  std::string subdomain_gen = R"(#!/usr/bin/env python3
# Name: Subdomain Generator
# Description: Generates common subdomains
# Type: processor
# Stage: 2
# Consumes: domain
# Provides: subdomain
# Install: pip install fake-package
# InstallScope: isolated
import json
import sys
print(json.dumps({"bmop":"1.0","module":"subdomain-gen"}))
prefixes = ['www', 'api', 'mail', 'admin']
count = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        msg = json.loads(line)
        if msg.get('t') == 'd' and msg.get('f') == 'domain':
            domain = msg.get('v')
            for prefix in prefixes:
                subdomain = f"{prefix}.{domain}"
                print(json.dumps({"t":"d","f":"subdomain","v":subdomain}))
                count += 1
                if count % 10 == 0:
                    print(json.dumps({"t":"progress","c":count,"T":0}), file=sys.stderr)
    except:
        pass
print(json.dumps({"t":"result","ok":True,"count":count})))";
  


    std::string output = R"(#!/usr/bin/env bash
# Name: CSV Exporter
# Description: Exports data to CSV
# Type: output
# Stage: 3
# Consumes: *
# Install:
# InstallScope: global
echo '{"bmop":"1.0","module":"csv-exporter"}'
domain_count=0
subdomain_count=0
while IFS= read -r line; do
    if [[ $line == *'"t":"d"'* ]]; then
        if [[ $line == *'"f":"domain"'* ]]; then
            [[ $line =~ '"v":"'([^\"]*)'"' ]] && echo "${BASH_REMATCH[1]}" >> domains.csv
            ((domain_count++))
        elif [[ $line == *'"f":"subdomain"'* ]]; then
            [[ $line =~ '"v":"'([^\"]*)'"' ]] && echo "${BASH_REMATCH[1]}" >> subdomains.csv
            ((subdomain_count++))
        fi
    fi
done
echo "{\"t\":\"log\",\"l\":\"info\",\"m\":\"Exported $domain_count domains\"}" >&2
echo "{\"t\":\"log\",\"l\":\"info\",\"m\":\"Exported $subdomain_count subdomains\"}" >&2
total=$((domain_count + subdomain_count))
echo "{\"t\":\"result\",\"ok\":true,\"count\":$total}")";

  
  createTestModule("modules/collectors/collector.js", collector);
  createTestModule("modules/processors/cleaner.js", cleaner);
  createTestModule("modules/processors/subdomain_gen.py", subdomain_gen);
  createTestModule("modules/outputs/csv.sh", output);
  
  createProfile("full_workflow", R"(# Full workflow test
collector.js
cleaner.js
subdomain_gen.py
csv.sh)");
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  auto modules = loadProfile("full_workflow");
  for (const auto& module : modules) {
    std::string path = findModulePath(module);
    ModuleMetadata meta = parseModuleMetadata(path);
    runModuleWithPipe(module, {}, storage, meta.consumes);
  }
  
  EXPECT_TRUE(fs::exists("domains.csv"));
  EXPECT_TRUE(fs::exists("subdomains.csv"));
  
  EXPECT_EQ(storage["domain"].size(), 3);
  EXPECT_EQ(storage["subdomain"].size(), 12);
  
  bool found_alpha = false;
  bool found_beta = false;
  bool found_gamma = false;
  bool found_wildcard = false;
  
  for (const auto& item : storage["domain"]) {
    if (item.value == "alpha.com") found_alpha = true;
    if (item.value == "beta.com") found_beta = true;
    if (item.value == "gamma.com") found_gamma = true;
    if (item.value == "wildcard.com") found_wildcard = true;
  }
  
  EXPECT_TRUE(found_alpha);
  EXPECT_TRUE(found_beta);
  EXPECT_TRUE(found_gamma);
  EXPECT_FALSE(found_wildcard);
  
  EXPECT_EQ(storage["subdomain"].size(), 12);
}

