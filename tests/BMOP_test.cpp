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
#include <algorithm>
#include "../core/core.hpp"
#include "../include/rapidjson/document.h"
#include "../include/rapidjson/error/en.h"

namespace fs = std::filesystem;

class BmopProtocolTest : public ::testing::Test {
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
      fs::path filepath(filename);
      fs::create_directories(filepath.parent_path());

      std::ofstream file(filename);
      file << content;
      file.close();

      fs::permissions(filepath, fs::perms::owner_exec, fs::perm_options::add);
    }

    std::string runModuleGetStdout(const std::string& moduleName) {
      std::string fullPath = findModulePath(moduleName);
      if (fullPath.empty()) return "";

      std::string runner;
      if (fullPath.ends_with(".js")) runner = "node";
      else if (fullPath.ends_with(".py")) runner = "python3 -u";  // -u para unbuffered
      else if (fullPath.ends_with(".sh")) runner = "bash";
      else return "";

      std::string cmd = runner + " " + fullPath + " 2>&1";  // Capturar stderr también
      std::array<char, 4096> buffer;
      std::string output;

      std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
      if (!pipe) return "";

      while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
      }

      return output;
    }

    std::string simulateParse(const std::string& input) {
      std::map<std::string, std::vector<DataItem>> storage;
      std::istringstream stream(input);
      std::string line;

      bool inBatch = false;
      std::string batchFormat;
      int batchItemsExpected = 0;
      int batchItemsRead = 0;

      while (std::getline(stream, line)) {
        line = trimString(line);
        if (line.empty()) continue;

        if (inBatch) {
          if (line[0] == '{') {
            parseBMOPLine(line, storage);

            if (line.find("batch_end") != std::string::npos) {
              inBatch = false;
              batchFormat.clear();
              batchItemsExpected = 0;
              batchItemsRead = 0;
            }
          } else {
            if (!batchFormat.empty() && batchItemsRead < batchItemsExpected) {
              DataItem item;
              item.format = batchFormat;
              item.value = line;
              storage[batchFormat].push_back(item);
              batchItemsRead++;
            }
          }
        } else {
          parseBMOPLine(line, storage);

          if (!storage["__batch_format__"].empty()) {
            inBatch = true;
            batchFormat = storage["__batch_format__"][0].format;

            rapidjson::Document doc;
            doc.Parse(line.c_str());

            if (doc.HasMember("c") && doc["c"].IsInt()) {
              batchItemsExpected = doc["c"].GetInt();
            }

            storage["__batch_format__"].clear();
            batchItemsRead = 0;
          }
        }
      }

      std::string result;
      for (const auto& [format, items] : storage) {
        if (format == "__batch_format__") continue;
        result += format + ": [";
        for (size_t i = 0; i < items.size(); ++i) {
          if (i > 0) result += ", ";
          result += items[i].value;
        }
        result += "]\n";
      }
      return result;
    }

};

TEST_F(BmopProtocolTest, ParseDataMessages) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"domain","v":"example.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"url","v":"https://example.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"ip","v":"192.168.1.1"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"test.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"email","v":"admin@example.com"})", storage);

  EXPECT_EQ(storage.size(), 4);
  EXPECT_EQ(storage["domain"].size(), 2);
  EXPECT_EQ(storage["url"].size(), 1);
  EXPECT_EQ(storage["ip"].size(), 1);
  EXPECT_EQ(storage["email"].size(), 1);

  EXPECT_EQ(storage["domain"][0].value, "example.com");
  EXPECT_EQ(storage["domain"][1].value, "test.com");
  EXPECT_EQ(storage["url"][0].value, "https://example.com");
  EXPECT_EQ(storage["ip"][0].value, "192.168.1.1");
  EXPECT_EQ(storage["email"][0].value, "admin@example.com");
}

TEST_F(BmopProtocolTest, IgnoreLogMessages) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"log","l":"info","m":"Starting module"})", storage);
  parseBMOPLine(R"({"t":"log","l":"debug","m":"Debug info"})", storage);
  parseBMOPLine(R"({"t":"log","l":"warn","m":"Warning"})", storage);
  parseBMOPLine(R"({"t":"log","l":"error","m":"Error occurred"})", storage);

  EXPECT_TRUE(storage.empty());
}

TEST_F(BmopProtocolTest, IgnoreProgressMessages) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"progress","c":10,"T":100})", storage);
  parseBMOPLine(R"({"t":"progress","c":50,"T":100,"m":"Halfway"})", storage);

  EXPECT_TRUE(storage.empty());
}

TEST_F(BmopProtocolTest, IgnoreResultMessages) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"result","ok":true,"count":42})", storage);
  parseBMOPLine(R"({"t":"result","ok":false,"error":"Failed"})", storage);

  EXPECT_TRUE(storage.empty());
}

TEST_F(BmopProtocolTest, IgnoreErrorMessages) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"error","code":"AUTH_FAILED","m":"Auth failed"})", storage);
  parseBMOPLine(R"({"t":"error","code":"NETWORK","m":"Timeout","fatal":true})", storage);

  EXPECT_TRUE(storage.empty());
}

TEST_F(BmopProtocolTest, ParseBatchControlMessages) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"batch","f":"domain","c":1000})", storage);
  EXPECT_EQ(storage.size(), 1);
  EXPECT_EQ(storage["__batch_format__"].size(), 1);
  EXPECT_EQ(storage["__batch_format__"][0].format, "domain");
  EXPECT_EQ(storage["__batch_format__"][0].value, "domain");

  storage.clear();
  parseBMOPLine(R"({"t":"batch_end"})", storage);
  EXPECT_TRUE(storage.empty());
}

TEST_F(BmopProtocolTest, ParseProtocolHeader) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"bmop":"1.0","module":"test","pid":12345})", storage);
  EXPECT_TRUE(storage.empty());

  parseBMOPLine(R"({"bmop":"1.0","module":"test"})", storage);
  EXPECT_TRUE(storage.empty());
}

TEST_F(BmopProtocolTest, InvalidJSONHandling) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine("", storage);
  parseBMOPLine("not json", storage);
  parseBMOPLine("{invalid}", storage);
  parseBMOPLine(R"({"t": "d", "f": "domain"})", storage);
  parseBMOPLine(R"({"t": "d", "v": "example.com"})", storage);

  EXPECT_TRUE(storage.empty());
}

TEST_F(BmopProtocolTest, MixedMessageTypes) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"bmop":"1.0","module":"mixed"})", storage);
  parseBMOPLine(R"({"t":"log","l":"info","m":"Start"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"example.com"})", storage);
  parseBMOPLine(R"({"t":"progress","c":1,"T":10})", storage);
  parseBMOPLine(R"({"t":"log","l":"debug","m":"Processing"})", storage);
  parseBMOPLine(R"({"t":"d","f":"url","v":"https://example.com"})", storage);
  parseBMOPLine(R"({"t":"error","code":"TEST","m":"Test"})", storage);
  parseBMOPLine(R"({"t":"d","f":"ip","v":"192.168.1.1"})", storage);
  parseBMOPLine(R"({"t":"result","ok":true,"count":3})", storage);

  EXPECT_EQ(storage.size(), 3);
  EXPECT_EQ(storage["domain"].size(), 1);
  EXPECT_EQ(storage["url"].size(), 1);
  EXPECT_EQ(storage["ip"].size(), 1);
}

TEST_F(BmopProtocolTest, TrimStringFunction) {
  EXPECT_EQ(trimString(""), "");
  EXPECT_EQ(trimString("  "), "");
  EXPECT_EQ(trimString("hello"), "hello");
  EXPECT_EQ(trimString("  hello  "), "hello");
  EXPECT_EQ(trimString("\t\nhello\r\n"), "hello");
  EXPECT_EQ(trimString("  hello world  "), "hello world");
  EXPECT_EQ(trimString("\xC2\xA0hello\xC2\xA0"), "hello");
}

TEST_F(BmopProtocolTest, ParseModuleMetadata) {
  std::string content = R"(#!/usr/bin/env node
// Name: Test Module
// Description: Test module description
// Type: collector-domain
// Stage: 1
// Consumes: domain
// Provides: subdomain
// Install: npm install test-package
// InstallScope: shared
// Storage: replace
// Args: --target <url> Target URL
// Args: --verbose Verbose output)";

  std::string filename = "modules/test_module.js";
  createTestModule(filename, content);

  ModuleMetadata meta = parseModuleMetadata(filename);

  EXPECT_EQ(meta.name, "Test Module");
  EXPECT_EQ(meta.description, "Test module description");
  EXPECT_EQ(meta.type, "collector-domain");
  EXPECT_EQ(meta.stage, 1);
  EXPECT_EQ(meta.consumes, "domain");
  EXPECT_EQ(meta.provides, "subdomain");
  EXPECT_EQ(meta.installCmd, "npm install test-package");
  EXPECT_EQ(meta.installScope, "shared");
  EXPECT_EQ(meta.storageBehavior, "replace");
  EXPECT_EQ(meta.argSpecs.size(), 2);
  EXPECT_EQ(meta.argSpecs[0], "--target <url> Target URL");
  EXPECT_EQ(meta.argSpecs[1], "--verbose Verbose output");
}

TEST_F(BmopProtocolTest, ParseModuleMetadataMinimal) {
  std::string content = R"(#!/usr/bin/env node
// Name: Minimal Module
// Description: Minimal module)";

  std::string filename = "modules/minimal.js";
  createTestModule(filename, content);

  ModuleMetadata meta = parseModuleMetadata(filename);

  EXPECT_EQ(meta.name, "Minimal Module");
  EXPECT_EQ(meta.description, "Minimal module");
  EXPECT_EQ(meta.type, "");
  EXPECT_EQ(meta.stage, 999);
  EXPECT_EQ(meta.consumes, "");
  EXPECT_EQ(meta.provides, "");
  EXPECT_EQ(meta.storageBehavior, "add");
  EXPECT_TRUE(meta.argSpecs.empty());
}

TEST_F(BmopProtocolTest, ParseModuleMetadataPython) {
  std::string content = R"(#!/usr/bin/env python3
# Name: Python Module
# Description: Python test module
# Type: processor
# Stage: 2
# Consumes: domain
# Provides: url
# Install: pip install requests
# InstallScope: isolated
# Storage: add)";

  std::string filename = "modules/python_module.py";
  createTestModule(filename, content);

  ModuleMetadata meta = parseModuleMetadata(filename);

  EXPECT_EQ(meta.name, "Python Module");
  EXPECT_EQ(meta.description, "Python test module");
  EXPECT_EQ(meta.type, "processor");
  EXPECT_EQ(meta.stage, 2);
  EXPECT_EQ(meta.consumes, "domain");
  EXPECT_EQ(meta.provides, "url");
  EXPECT_EQ(meta.installCmd, "pip install requests");
  EXPECT_EQ(meta.installScope, "isolated");
  EXPECT_EQ(meta.storageBehavior, "add");
}

TEST_F(BmopProtocolTest, ParseModuleMetadataBash) {
  std::string content = R"(#!/usr/bin/env bash
# Name: Bash Module
# Description: Bash test module
# Type: output
# Stage: 3
# Consumes: *
# Provides:
# Install: apt-get install jq
# InstallScope: global
# Storage: add)";

  std::string filename = "modules/bash_module.sh";
  createTestModule(filename, content);

  ModuleMetadata meta = parseModuleMetadata(filename);

  EXPECT_EQ(meta.name, "Bash Module");
  EXPECT_EQ(meta.description, "Bash test module");
  EXPECT_EQ(meta.type, "output");
  EXPECT_EQ(meta.stage, 3);
  EXPECT_EQ(meta.consumes, "*");
  EXPECT_EQ(meta.provides, "");
  EXPECT_EQ(meta.installCmd, "apt-get install jq");
  EXPECT_EQ(meta.installScope, "global");
  EXPECT_EQ(meta.storageBehavior, "add");
}

TEST_F(BmopProtocolTest, DataItemStorageAndRetrieval) {
  std::map<std::string, std::vector<DataItem>> storage;

  DataItem item1{"domain", "example.com"};
  DataItem item2{"domain", "test.com"};
  DataItem item3{"url", "https://example.com"};
  DataItem item4{"ip", "192.168.1.1"};
  DataItem item5{"domain", "another.com"};

  storage["domain"].push_back(item1);
  storage["domain"].push_back(item2);
  storage["domain"].push_back(item5);
  storage["url"].push_back(item3);
  storage["ip"].push_back(item4);

  EXPECT_EQ(storage.size(), 3);
  EXPECT_EQ(storage["domain"].size(), 3);
  EXPECT_EQ(storage["url"].size(), 1);
  EXPECT_EQ(storage["ip"].size(), 1);

  EXPECT_EQ(storage["domain"][0].value, "example.com");
  EXPECT_EQ(storage["domain"][1].value, "test.com");
  EXPECT_EQ(storage["domain"][2].value, "another.com");
  EXPECT_EQ(storage["url"][0].value, "https://example.com");
  EXPECT_EQ(storage["ip"][0].value, "192.168.1.1");
}

TEST_F(BmopProtocolTest, ParseComplexDataFormats) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"vulnerability","v":"{\"type\":\"XSS\",\"severity\":\"high\"}"})", storage);
  parseBMOPLine(R"({"t":"d","f":"certificate","v":"{\"cn\":\"example.com\",\"expires\":\"2025-12-31\"}"})", storage);
  parseBMOPLine(R"({"t":"d","f":"credential","v":"{\"user\":\"admin\",\"pass\":\"secret\"}"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"example.com","meta":{"source":"chaos","confidence":0.95}})", storage);

  EXPECT_EQ(storage.size(), 4);
  EXPECT_EQ(storage["vulnerability"][0].value, "{\"type\":\"XSS\",\"severity\":\"high\"}");
  EXPECT_EQ(storage["certificate"][0].value, "{\"cn\":\"example.com\",\"expires\":\"2025-12-31\"}");
  EXPECT_EQ(storage["credential"][0].value, "{\"user\":\"admin\",\"pass\":\"secret\"}");
  EXPECT_EQ(storage["domain"][0].value, "example.com");
}

TEST_F(BmopProtocolTest, ParseWithTrailingCommas) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"bmop":"1.0","module":"test",})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"example.com",})", storage);
  parseBMOPLine(R"({"t":"log","l":"info","m":"test",})", storage);

  EXPECT_EQ(storage.size(), 1);
  EXPECT_EQ(storage["domain"].size(), 1);
  EXPECT_EQ(storage["domain"][0].value, "example.com");
}

TEST_F(BmopProtocolTest, ParseWithComments) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"domain","v":"example.com" /* comment */})", storage);
  parseBMOPLine(R"({/* comment */ "t":"d","f":"domain","v":"test.com"})", storage);
  parseBMOPLine(R"({"t":"d",/* multi
line */ "f":"url","v":"https://test.com"})", storage);

  EXPECT_EQ(storage.size(), 2);
  EXPECT_EQ(storage["domain"].size(), 2);
  EXPECT_EQ(storage["url"].size(), 1);
}

TEST_F(BmopProtocolTest, LargeDatasetParsing) {
  std::map<std::string, std::vector<DataItem>> storage;

  for (int i = 0; i < 1000; ++i) {
    std::string json = R"({"t":"d","f":"domain","v":")" + 
      std::string("domain") + std::to_string(i) + R"(.com"})";
    parseBMOPLine(json, storage);
  }

  EXPECT_EQ(storage.size(), 1);
  EXPECT_EQ(storage["domain"].size(), 1000);

  for (int i = 0; i < 1000; ++i) {
    std::string expected = "domain" + std::to_string(i) + ".com";
    EXPECT_EQ(storage["domain"][i].value, expected);
  }
}

TEST_F(BmopProtocolTest, MultipleFormatsLarge) {
  std::map<std::string, std::vector<DataItem>> storage;

  std::vector<std::string> formats = {"domain", "url", "ip", "email", "subdomain"};

  for (int i = 0; i < 500; ++i) {
    for (const auto& format : formats) {
      std::string json = R"({"t":"d","f":")" + format + R"(","v":")" + 
        format + std::to_string(i) + R"("})";
      parseBMOPLine(json, storage);
    }
  }

  EXPECT_EQ(storage.size(), 5);
  for (const auto& format : formats) {
    EXPECT_EQ(storage[format].size(), 500);
  }
}

TEST_F(BmopProtocolTest, SimulateBatchProcessing) {
  std::string input = R"({"bmop":"1.0","module":"batch-test"})" "\n"
    R"({"t":"batch","f":"domain","c":5})" "\n"
    "example1.com\n"
    "example2.com\n"
    "example3.com\n"
    "example4.com\n"
    "example5.com\n"
    R"({"t":"batch_end"})" "\n"
    R"({"t":"result","ok":true,"count":5})";

  std::string result = simulateParse(input);
  std::string expected = "domain: [example1.com, example2.com, example3.com, example4.com, example5.com]\n";

  EXPECT_EQ(result, expected);
}

TEST_F(BmopProtocolTest, SimulateMixedBatchAndSingle) {
  std::string input = R"({"t":"d","f":"domain","v":"single1.com"})" "\n"
    R"({"t":"batch","f":"url","c":3})" "\n"
    "https://test1.com\n"
    "https://test2.com\n"
    "https://test3.com\n"
    R"({"t":"batch_end"})" "\n"
    R"({"t":"d","f":"domain","v":"single2.com"})" "\n"
    R"({"t":"batch","f":"ip","c":2})" "\n"
    "192.168.1.1\n"
    "10.0.0.1\n"
    R"({"t":"batch_end"})";

  std::string result = simulateParse(input);
  std::string expected = "domain: [single1.com, single2.com]\n"
    "ip: [192.168.1.1, 10.0.0.1]\n"
    "url: [https://test1.com, https://test2.com, https://test3.com]\n";

  EXPECT_EQ(result, expected);
}

TEST_F(BmopProtocolTest, EmptyValuesHandling) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"domain","v":""})", storage);
  parseBMOPLine(R"({"t":"d","f":"","v":"test"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"  "})", storage);
  parseBMOPLine(R"({"t":"d","f":"  ","v":"test"})", storage);

  EXPECT_EQ(storage.size(), 3);
  EXPECT_EQ(storage[""].size(), 1);
  EXPECT_EQ(storage["domain"].size(), 2);
}

TEST_F(BmopProtocolTest, SpecialCharactersInValues) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"url","v":"https://example.com/path?query=test&param=value"})", storage);
  parseBMOPLine(R"({"t":"d","f":"email","v":"test\"quotes\"@example.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"example\\test.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"path","v":"C:\\Users\\test\\file.txt"})", storage);
  parseBMOPLine(R"({"t":"d","f":"json","v":"{\"key\":\"value\",\"array\":[1,2,3]}"})", storage);

  EXPECT_EQ(storage.size(), 5);
  EXPECT_EQ(storage["url"][0].value, "https://example.com/path?query=test&param=value");
  EXPECT_EQ(storage["email"][0].value, "test\"quotes\"@example.com");
  EXPECT_EQ(storage["domain"][0].value, "example\\test.com");
  EXPECT_EQ(storage["path"][0].value, "C:\\Users\\test\\file.txt");
  EXPECT_EQ(storage["json"][0].value, "{\"key\":\"value\",\"array\":[1,2,3]}");
}

TEST_F(BmopProtocolTest, UnicodeCharacters) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"domain","v":"exämple.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"例子.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"🦄.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"café.fr"})", storage);

  EXPECT_EQ(storage.size(), 1);
  EXPECT_EQ(storage["domain"].size(), 4);
  EXPECT_EQ(storage["domain"][0].value, "exämple.com");
  EXPECT_EQ(storage["domain"][1].value, "例子.com");
  EXPECT_EQ(storage["domain"][2].value, "🦄.com");
  EXPECT_EQ(storage["domain"][3].value, "café.fr");
}

TEST_F(BmopProtocolTest, VeryLongValues) {
  std::map<std::string, std::vector<DataItem>> storage;

  std::string longValue(10000, 'a');
  std::string json = R"({"t":"d","f":"data","v":")" + longValue + R"("})";

  parseBMOPLine(json, storage);

  EXPECT_EQ(storage.size(), 1);
  EXPECT_EQ(storage["data"].size(), 1);
  EXPECT_EQ(storage["data"][0].value.size(), 10000);
  EXPECT_EQ(storage["data"][0].value, longValue);
}

TEST_F(BmopProtocolTest, MalformedJSONRecovery) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine("", storage);
  parseBMOPLine("{", storage);
  parseBMOPLine("}", storage);
  parseBMOPLine(R"({"t":})", storage);
  parseBMOPLine(R"({"t":"d" "f":"domain"})", storage);

  parseBMOPLine(R"({"t":"d","f":"domain","v":"good.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"another.com"})", storage);

  EXPECT_EQ(storage.size(), 1);
  EXPECT_EQ(storage["domain"].size(), 2);
}

TEST_F(BmopProtocolTest, NestedJSONInValues) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"complex","v":"{\"nested\":{\"level1\":{\"level2\":\"value\"}}}"})", storage);
  parseBMOPLine(R"({"t":"d","f":"array","v":"[1,2,3,4,5]"})", storage);
  parseBMOPLine(R"({"t":"d","f":"mixed","v":"{\"string\":\"text\",\"number\":123,\"bool\":true}"})", storage);

  EXPECT_EQ(storage.size(), 3);
  EXPECT_EQ(storage["complex"][0].value, R"({"nested":{"level1":{"level2":"value"}}})");
  EXPECT_EQ(storage["array"][0].value, "[1,2,3,4,5]");
  EXPECT_EQ(storage["mixed"][0].value, R"({"string":"text","number":123,"bool":true})");
}

TEST_F(BmopProtocolTest, FindModulePath) {
  createTestModule("modules/collectors/test1.js", "test");
  createTestModule("modules/processors/test2.py", "test");
  createTestModule("modules/outputs/test3.sh", "test");
  createTestModule("modules/deep/nested/test4.js", "test");

  EXPECT_TRUE(findModulePath("nonexistent.js").empty());
  EXPECT_FALSE(findModulePath("test1.js").empty());
  EXPECT_FALSE(findModulePath("test2.py").empty());
  EXPECT_FALSE(findModulePath("test3.sh").empty());
  EXPECT_FALSE(findModulePath("test4.js").empty());

  EXPECT_TRUE(fs::exists(findModulePath("test1.js")));
  EXPECT_TRUE(fs::exists(findModulePath("test2.py")));
  EXPECT_TRUE(fs::exists(findModulePath("test3.sh")));
  EXPECT_TRUE(fs::exists(findModulePath("test4.js")));
}

TEST_F(BmopProtocolTest, GetModulesList) {
  createTestModule("modules/collectors/mod1.js", "test");
  createTestModule("modules/processors/mod2.py", "test");
  createTestModule("modules/outputs/mod3.sh", "test");
  createTestModule("modules/mod4.js", "test");

  auto modules = getModules();

  EXPECT_GE(modules.size(), 4);
  EXPECT_NE(std::find(modules.begin(), modules.end(), "mod1.js"), modules.end());
  EXPECT_NE(std::find(modules.begin(), modules.end(), "mod2.py"), modules.end());
  EXPECT_NE(std::find(modules.begin(), modules.end(), "mod3.sh"), modules.end());
  EXPECT_NE(std::find(modules.begin(), modules.end(), "mod4.js"), modules.end());
}

TEST_F(BmopProtocolTest, SimulateFullWorkflow) {
  std::string input = R"({"bmop":"1.0","module":"workflow-test"})" "\n"
    R"({"t":"log","l":"info","m":"Starting workflow"})" "\n"
    R"({"t":"d","f":"domain","v":"start.com"})" "\n"
    R"({"t":"progress","c":1,"T":10})" "\n"
    R"({"t":"batch","f":"subdomain","c":3})" "\n"
    "www.start.com\n"
    "api.start.com\n"
    "mail.start.com\n"
    R"({"t":"batch_end"})" "\n"
    R"({"t":"d","f":"url","v":"https://start.com"})" "\n"
    R"({"t":"log","l":"warn","m":"Almost done"})" "\n"
    R"({"t":"progress","c":10,"T":10})" "\n"
    R"({"t":"d","f":"ip","v":"1.2.3.4"})" "\n"
    R"({"t":"result","ok":true,"count":6})";

  std::string result = simulateParse(input);
  std::string expected = "domain: [start.com]\n"
    "ip: [1.2.3.4]\n"
    "subdomain: [www.start.com, api.start.com, mail.start.com]\n"
    "url: [https://start.com]\n";

  EXPECT_EQ(result, expected);
}

TEST_F(BmopProtocolTest, StorageOverwriteBehavior) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"domain","v":"old1.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"old2.com"})", storage);
  EXPECT_EQ(storage["domain"].size(), 2);

  storage["domain"].clear();
  parseBMOPLine(R"({"t":"d","f":"domain","v":"new1.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"new2.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"new3.com"})", storage);
  EXPECT_EQ(storage["domain"].size(), 3);

  EXPECT_EQ(storage["domain"][0].value, "new1.com");
  EXPECT_EQ(storage["domain"][1].value, "new2.com");
  EXPECT_EQ(storage["domain"][2].value, "new3.com");
}

TEST_F(BmopProtocolTest, StorageDeleteBehavior) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"domain","v":"test1.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"url","v":"https://test1.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"test2.com"})", storage);
  EXPECT_EQ(storage.size(), 2);
  EXPECT_EQ(storage["domain"].size(), 2);

  storage.erase("domain");
  EXPECT_EQ(storage.size(), 1);
  EXPECT_TRUE(storage.find("domain") == storage.end());
  EXPECT_TRUE(storage.find("url") != storage.end());
}

TEST_F(BmopProtocolTest, StorageAddBehavior) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"domain","v":"existing1.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"existing2.com"})", storage);
  EXPECT_EQ(storage["domain"].size(), 2);

  parseBMOPLine(R"({"t":"d","f":"domain","v":"new1.com"})", storage);
  parseBMOPLine(R"({"t":"d","f":"domain","v":"new2.com"})", storage);
  EXPECT_EQ(storage["domain"].size(), 4);

  EXPECT_EQ(storage["domain"][0].value, "existing1.com");
  EXPECT_EQ(storage["domain"][1].value, "existing2.com");
  EXPECT_EQ(storage["domain"][2].value, "new1.com");
  EXPECT_EQ(storage["domain"][3].value, "new2.com");
}

TEST_F(BmopProtocolTest, RealModuleWithBMOP) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"test-module"}));
console.error(JSON.stringify({t:"log",l:"info",m:"Starting"}));
console.log(JSON.stringify({t:"d",f:"domain",v:"example.com"}));
console.error(JSON.stringify({t:"progress",c:1,T:5}));
console.log(JSON.stringify({t:"d",f:"url",v:"https://example.com"}));
console.error(JSON.stringify({t:"log",l:"warn",m:"Warning"}));
console.log(JSON.stringify({t:"d",f:"ip",v:"1.2.3.4"}));
console.error(JSON.stringify({t:"progress",c:5,T:5}));
console.log(JSON.stringify({t:"result",ok:true,count:3}));)";

  createTestModule("modules/test_real.js", content);
  std::string output = runModuleGetStdout("test_real.js");

  EXPECT_FALSE(output.empty());
  EXPECT_TRUE(output.find("\"bmop\":\"1.0\"") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"d\"") != std::string::npos);
  EXPECT_TRUE(output.find("example.com") != std::string::npos);
  EXPECT_TRUE(output.find("https://example.com") != std::string::npos);
  EXPECT_TRUE(output.find("1.2.3.4") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"result\"") != std::string::npos);
}

TEST_F(BmopProtocolTest, PythonModuleWithBMOP) {
  std::string content = R"(#!/usr/bin/env python3
import json
import sys
print(json.dumps({"bmop":"1.0","module":"python-test"}))
print(json.dumps({"t":"log","l":"info","m":"Python starting"}), file=sys.stderr)
print(json.dumps({"t":"d","f":"domain","v":"python.com"}))
print(json.dumps({"t":"d","f":"url","v":"https://python.org"}))
print(json.dumps({"t":"result","ok":True,"count":2}))
sys.stdout.flush())";

  createTestModule("modules/test_python.py", content);
  std::string output = runModuleGetStdout("test_python.py");

  EXPECT_FALSE(output.empty());
  
  std::string normalized_output = output;
  normalized_output.erase(std::remove(normalized_output.begin(), normalized_output.end(), ' '), normalized_output.end());
  normalized_output.erase(std::remove(normalized_output.begin(), normalized_output.end(), '\n'), normalized_output.end());
  
  // Buscar SIN espacios (porque los quitamos)
  EXPECT_TRUE(normalized_output.find("\"bmop\":\"1.0\"") != std::string::npos);
  EXPECT_TRUE(normalized_output.find("\"t\":\"d\"") != std::string::npos);
  EXPECT_TRUE(normalized_output.find("python.com") != std::string::npos);
  EXPECT_TRUE(normalized_output.find("https://python.org") != std::string::npos);
  EXPECT_TRUE(normalized_output.find("\"t\":\"result\"") != std::string::npos);
}

TEST_F(BmopProtocolTest, BashModuleWithBMOP) {
  std::string content = R"(#!/usr/bin/env bash
echo '{"bmop":"1.0","module":"bash-test"}'
echo '{"t":"d","f":"domain","v":"bash.com"}'
echo '{"t":"d","f":"ip","v":"127.0.0.1"}'
echo '{"t":"result","ok":true,"count":2}')";

  createTestModule("modules/test_bash.sh", content);
  std::string output = runModuleGetStdout("test_bash.sh");

  EXPECT_FALSE(output.empty());
  EXPECT_TRUE(output.find("\"bmop\":\"1.0\"") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"d\"") != std::string::npos);
  EXPECT_TRUE(output.find("bash.com") != std::string::npos);
  EXPECT_TRUE(output.find("127.0.0.1") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"result\"") != std::string::npos);
}

TEST_F(BmopProtocolTest, BatchModuleWithBMOP) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"batch-module"}));
console.log(JSON.stringify({t:"batch",f:"domain",c:5}));
console.log("batch1.com");
console.log("batch2.com");
console.log("batch3.com");
console.log("batch4.com");
console.log("batch5.com");
console.log(JSON.stringify({t:"batch_end"}));
console.log(JSON.stringify({t:"result",ok:true,count:5}));)";

  createTestModule("modules/test_batch.js", content);
  std::string output = runModuleGetStdout("test_batch.js");

  EXPECT_FALSE(output.empty());
  EXPECT_TRUE(output.find("\"bmop\":\"1.0\"") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"batch\"") != std::string::npos);
  EXPECT_TRUE(output.find("batch1.com") != std::string::npos);
  EXPECT_TRUE(output.find("batch5.com") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"batch_end\"") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"result\"") != std::string::npos);
}

TEST_F(BmopProtocolTest, ErrorHandlingModule) {
  std::string content = R"(#!/usr/bin/env node
console.log(JSON.stringify({bmop:"1.0",module:"error-module"}));
console.error(JSON.stringify({t:"error","code":"TEST","m":"Test error","fatal":true}));
console.log(JSON.stringify({t:"result","ok":false,"error":"Test failed"}));)";

  createTestModule("modules/test_error.js", content);
  std::string output = runModuleGetStdout("test_error.js");

  EXPECT_FALSE(output.empty());
  EXPECT_TRUE(output.find("\"bmop\":\"1.0\"") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"result\"") != std::string::npos);
  EXPECT_TRUE(output.find("\"ok\":false") != std::string::npos);
}

TEST_F(BmopProtocolTest, ModuleWithMetadataAndBMOP) {
  std::string content = R"(#!/usr/bin/env node
// Name: Complete Test Module
// Description: Tests all BMOP features
// Type: collector-domain
// Stage: 1
// Consumes:
// Provides: domain, url, ip
// Install: npm install test
// InstallScope: shared
// Storage: add
// Args: --target <url> Target URL

console.log(JSON.stringify({bmop:"1.0","module":"complete-test"}));
console.error(JSON.stringify({t:"log","l":"info","m":"Module with metadata"}));
console.log(JSON.stringify({t:"d","f":"domain","v":"meta.com"}));
console.error(JSON.stringify({t:"progress","c":1,"T":3}));
console.log(JSON.stringify({t:"d","f":"url","v":"https://meta.com"}));
console.error(JSON.stringify({t:"progress","c":2,"T":3}));
console.log(JSON.stringify({t:"d","f":"ip","v":"8.8.8.8"}));
console.error(JSON.stringify({t:"progress","c":3,"T":3}));
console.log(JSON.stringify({t:"result","ok":true,"count":3}));)";

  std::string filename = "modules/complete_test.js";
  createTestModule(filename, content);

  ModuleMetadata meta = parseModuleMetadata(filename);
  EXPECT_EQ(meta.name, "Complete Test Module");
  EXPECT_EQ(meta.type, "collector-domain");
  EXPECT_EQ(meta.stage, 1);
  EXPECT_EQ(meta.provides, "domain, url, ip");
  EXPECT_EQ(meta.storageBehavior, "add");

  std::string output = runModuleGetStdout("complete_test.js");
  EXPECT_TRUE(output.find("\"bmop\":\"1.0\"") != std::string::npos);
  EXPECT_TRUE(output.find("meta.com") != std::string::npos);
  EXPECT_TRUE(output.find("https://meta.com") != std::string::npos);
  EXPECT_TRUE(output.find("8.8.8.8") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"result\"") != std::string::npos);
}

TEST_F(BmopProtocolTest, PerformanceTestLargeDataset) {
  std::map<std::string, std::vector<DataItem>> storage;

  const int ITEM_COUNT = 5000;

  for (int i = 0; i < ITEM_COUNT; ++i) {
    std::string json = R"({"t":"d","f":"test","v":")" + 
      std::to_string(i) + R"("})";
    parseBMOPLine(json, storage);
  }

  EXPECT_EQ(storage.size(), 1);
  EXPECT_EQ(storage["test"].size(), ITEM_COUNT);

  for (int i = 0; i < ITEM_COUNT; ++i) {
    EXPECT_EQ(storage["test"][i].value, std::to_string(i));
  }
}

TEST_F(BmopProtocolTest, MultipleBatchProcessing) {
  std::string input = R"({"bmop":"1.0","module":"multi-batch"})" "\n"
    R"({"t":"batch","f":"domains","c":3})" "\n"
    "domain1.com\n"
    "domain2.com\n"
    "domain3.com\n"
    R"({"t":"batch_end"})" "\n"
    R"({"t":"batch","f":"urls","c":2})" "\n"
    "https://test1.com\n"
    "https://test2.com\n"
    R"({"t":"batch_end"})" "\n"
    R"({"t":"batch","f":"ips","c":4})" "\n"
    "1.1.1.1\n"
    "2.2.2.2\n"
    "3.3.3.3\n"
    "4.4.4.4\n"
    R"({"t":"batch_end"})" "\n"
    R"({"t":"result","ok":true,"count":9})";

  std::string result = simulateParse(input);

  EXPECT_TRUE(result.find("domains: [domain1.com, domain2.com, domain3.com]") != std::string::npos);
  EXPECT_TRUE(result.find("urls: [https://test1.com, https://test2.com]") != std::string::npos);
  EXPECT_TRUE(result.find("ips: [1.1.1.1, 2.2.2.2, 3.3.3.3, 4.4.4.4]") != std::string::npos);
}

TEST_F(BmopProtocolTest, MixedSingleAndBatch) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"t":"d","f":"type","v":"single"})", storage);
  parseBMOPLine(R"({"t":"batch","f":"items","c":2})", storage);

  storage.erase("__batch_format__");

  DataItem item1{"items", "batch1"};
  DataItem item2{"items", "batch2"};
  storage["items"].push_back(item1);
  storage["items"].push_back(item2);

  parseBMOPLine(R"({"t":"batch_end"})", storage);
  parseBMOPLine(R"({"t":"d","f":"type","v":"another"})", storage);

  EXPECT_EQ(storage.size(), 2);
  EXPECT_EQ(storage["type"].size(), 2);
  EXPECT_EQ(storage["items"].size(), 2);
  EXPECT_EQ(storage["type"][0].value, "single");
  EXPECT_EQ(storage["type"][1].value, "another");
  EXPECT_EQ(storage["items"][0].value, "batch1");
  EXPECT_EQ(storage["items"][1].value, "batch2");
}

TEST_F(BmopProtocolTest, EdgeCaseEmptyBatch) {
  std::string input = R"({"bmop":"1.0","module":"empty-batch"})" "\n"
    R"({"t":"batch","f":"empty","c":0})" "\n"
    R"({"t":"batch_end"})" "\n"
    R"({"t":"result","ok":true,"count":0})";

  std::string result = simulateParse(input);
  std::string expected = ""; 

  EXPECT_EQ(result, expected);
}

TEST_F(BmopProtocolTest, ProtocolVersionVariations) {
  std::map<std::string, std::vector<DataItem>> storage;

  parseBMOPLine(R"({"bmop":"1.0","module":"test"})", storage);
  parseBMOPLine(R"({"bmop":"1.1","module":"test"})", storage);
  parseBMOPLine(R"({"bmop":"2.0","module":"test"})", storage);
  parseBMOPLine(R"({"module":"test"})", storage);
  parseBMOPLine(R"({"bmop":"1.0"})", storage);

  EXPECT_TRUE(storage.empty());
}

TEST_F(BmopProtocolTest, RealWorldSimulation) {
  std::stringstream input;
  
  input << R"({"bmop":"1.0","module":"recon-workflow"})" << "\n";
  
  for (int i = 1; i <= 10; i++) {
    input << R"({"t":"d","f":"domain","v":"target)" << i << R"(.com"})" << "\n";
  }
  
  input << R"({"t":"batch","f":"subdomain","c":30})" << "\n";
  
  for (int i = 1; i <= 10; i++) {
    input << "www.target" << i << ".com\n";
    input << "api.target" << i << ".com\n";
    input << "mail.target" << i << ".com\n";
  }
  
  input << R"({"t":"batch_end"})" << "\n";
  
  for (int i = 1; i <= 5; i++) {
    input << R"({"t":"d","f":"port","v":")" << (i + 1000) << R"("})" << "\n";
  }
  
  input << R"({"t":"result","ok":true,"count":45})" << "\n";
  
  std::map<std::string, std::vector<DataItem>> storage;
  std::string line;
  
  while (std::getline(input, line)) {
    line = trimString(line);
    if (!line.empty()) {
      parseBMOPLine(line, storage);
    }
  }
  
  EXPECT_EQ(storage["domain"].size(), 10);
  EXPECT_GE(storage.size(), 2); 
}


TEST_F(BmopProtocolTest, StressTestParsing) {
  const int TOTAL_MESSAGES = 10000;
  std::map<std::string, std::vector<DataItem>> storage;

  std::vector<std::string> formats = {"A", "B", "C", "D", "E"};

  for (int i = 0; i < TOTAL_MESSAGES; ++i) {
    std::string format = formats[i % formats.size()];
    std::string json = R"({"t":"d","f":")" + format + R"(","v":"value)" + 
      std::to_string(i) + R"("})";

    parseBMOPLine(json, storage);

    if (i % 1000 == 0) {
      std::string progress = R"({"t":"progress","c":)" + 
        std::to_string(i) + R"(,"T":)" + 
        std::to_string(TOTAL_MESSAGES) + R"(})";
      parseBMOPLine(progress, storage);
    }
  }

  EXPECT_EQ(storage.size(), 5);

  int total_items = 0;
  for (const auto& [format, items] : storage) {
    total_items += items.size();
  }

  EXPECT_EQ(total_items, TOTAL_MESSAGES);
}

TEST_F(BmopProtocolTest, CompleteEndToEndTest) {
  std::string moduleContent = R"(#!/usr/bin/env node
// Name: End-to-End Test
// Description: Complete BMOP protocol test
// Type: collector
// Stage: 1
// Provides: data
// InstallScope: global

console.log(JSON.stringify({bmop:"1.0","module":"e2e-test","pid":process.pid}));

const data = [
  "first", "second", "third", "fourth", "fifth",
  "sixth", "seventh", "eighth", "ninth", "tenth"
];

console.error(JSON.stringify({t:"log","l":"info","m":"Starting data generation"}));

data.forEach((item, index) => {
  console.log(JSON.stringify({t:"d","f":"data","v":item}));

  if ((index + 1) % 3 === 0) {
    console.error(JSON.stringify({t:"progress","c":index + 1,"T":data.length}));
  }
});

console.error(JSON.stringify({t:"log","l":"info","m":"Data generation complete"}));
console.log(JSON.stringify({t:"result","ok":true,"count":data.length}));)";

  createTestModule("modules/e2e_test.js", moduleContent);

  ModuleMetadata meta = parseModuleMetadata("modules/e2e_test.js");
  EXPECT_EQ(meta.name, "End-to-End Test");
  EXPECT_EQ(meta.type, "collector");
  EXPECT_EQ(meta.stage, 1);
  EXPECT_EQ(meta.provides, "data");

  std::string output = runModuleGetStdout("e2e_test.js");

  EXPECT_FALSE(output.empty());

  EXPECT_TRUE(output.find("\"bmop\":\"1.0\"") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"d\"") != std::string::npos);
  EXPECT_TRUE(output.find("first") != std::string::npos);
  EXPECT_TRUE(output.find("tenth") != std::string::npos);
  EXPECT_TRUE(output.find("\"t\":\"result\"") != std::string::npos);

  std::map<std::string, std::vector<DataItem>> storage;
  std::istringstream stream(output);
  std::string line;

  while (std::getline(stream, line)) {
    line = trimString(line);
    if (!line.empty()) {
      parseBMOPLine(line, storage);
    }
  }

EXPECT_EQ(storage.size(), 1);
EXPECT_EQ(storage["data"].size(), 10);

std::vector<std::string> expected = {
  "first", "second", "third", "fourth", "fifth",
  "sixth", "seventh", "eighth", "ninth", "tenth"
};

for (size_t i = 0; i < expected.size(); ++i) {
  EXPECT_EQ(storage["data"][i].value, expected[i]);
}
}

TEST_F(BmopProtocolTest, ComprehensiveProtocolCoverage) {
  std::vector<std::string> testCases = {
    R"({"bmop":"1.0","module":"test"})",
    R"({"t":"log","l":"info","m":"test"})",
    R"({"t":"log","l":"debug","m":"test"})",
    R"({"t":"log","l":"warn","m":"test"})",
    R"({"t":"log","l":"error","m":"test"})",
    R"({"t":"progress","c":1,"T":10})",
    R"({"t":"progress","c":5,"T":10,"m":"halfway"})",
    R"({"t":"d","f":"domain","v":"test.com"})",
    R"({"t":"d","f":"url","v":"https://test.com"})",
    R"({"t":"d","f":"ip","v":"1.2.3.4"})",
    R"({"t":"d","f":"email","v":"test@test.com"})",
    R"({"t":"d","f":"subdomain","v":"www.test.com"})",
    R"({"t":"d","f":"hash","v":"abc123"})",
    R"({"t":"d","f":"vulnerability","v":"XSS"})",
    R"({"t":"d","f":"credential","v":"admin:password"})",
    R"({"t":"d","f":"certificate","v":"test"})",
    R"({"t":"batch","f":"domain","c":100})",
    R"({"t":"batch_end"})",
    R"({"t":"result","ok":true,"count":42})",
    R"({"t":"result","ok":false,"error":"failed"})",
    R"({"t":"error","code":"TEST","m":"test error"})",
    R"({"t":"error","code":"FATAL","m":"fatal","fatal":true})"
  };

  std::map<std::string, std::vector<DataItem>> storage;
  int dataCount = 0;

  for (const auto& testCase : testCases) {
    parseBMOPLine(testCase, storage);

    if (testCase.find("\"t\":\"d\"") != std::string::npos) {
      dataCount++;
    }
  }

  EXPECT_EQ(storage.size(), 10);
  EXPECT_EQ(dataCount, 9);

  EXPECT_EQ(storage["domain"].size(), 1);
  EXPECT_EQ(storage["url"].size(), 1);
  EXPECT_EQ(storage["ip"].size(), 1);
  EXPECT_EQ(storage["email"].size(), 1);
  EXPECT_EQ(storage["subdomain"].size(), 1);
  EXPECT_EQ(storage["hash"].size(), 1);
  EXPECT_EQ(storage["vulnerability"].size(), 1);
  EXPECT_EQ(storage["credential"].size(), 1);
  EXPECT_EQ(storage["certificate"].size(), 1);
  EXPECT_EQ(storage["__batch_format__"].size(), 1);

  EXPECT_EQ(storage["domain"][0].value, "test.com");
  EXPECT_EQ(storage["url"][0].value, "https://test.com");
  EXPECT_EQ(storage["ip"][0].value, "1.2.3.4");
  EXPECT_EQ(storage["email"][0].value, "test@test.com");
  EXPECT_EQ(storage["subdomain"][0].value, "www.test.com");
  EXPECT_EQ(storage["hash"][0].value, "abc123");
  EXPECT_EQ(storage["vulnerability"][0].value, "XSS");
  EXPECT_EQ(storage["credential"][0].value, "admin:password");
  EXPECT_EQ(storage["certificate"][0].value, "test");
  EXPECT_EQ(storage["__batch_format__"][0].format, "domain");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
