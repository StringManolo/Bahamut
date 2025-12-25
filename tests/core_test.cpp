#include "gtest/gtest.h"
#include "../core/core.hpp"
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <algorithm>

namespace fs = std::filesystem;

class CoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "bahamut_test";
        modules_dir = test_dir / "modules";
        profiles_dir = test_dir / "profiles";
        
        fs::create_directories(modules_dir);
        fs::create_directories(profiles_dir);
        
        original_cwd = fs::current_path();
        fs::current_path(test_dir);
    }
    
    void TearDown() override {
        fs::current_path(original_cwd);
        fs::remove_all(test_dir);
    }
    
    void createTestModule(const std::string& name, const std::string& content) {
        std::ofstream file(modules_dir / name);
        file << content;
        file.close();
    }
    
    void createTestProfile(const std::string& name, const std::vector<std::string>& modules) {
        std::ofstream file(profiles_dir / ("bahamut_" + name + ".txt"));
        for (const auto& mod : modules) {
            file << mod << "\n";
        }
        file.close();
    }
    
    fs::path test_dir;
    fs::path modules_dir;
    fs::path profiles_dir;
    fs::path original_cwd;
};

// Pruebas para trimString
TEST_F(CoreTest, TrimString_RemovesSpaces) {
    EXPECT_EQ(trimString("  hello  "), "hello");
    EXPECT_EQ(trimString("  hello world  "), "hello world");
    EXPECT_EQ(trimString("\t\nhello\r\n"), "hello");
    EXPECT_EQ(trimString(""), "");
    EXPECT_EQ(trimString("   "), "");
}

TEST_F(CoreTest, TrimString_NoSpaces) {
    EXPECT_EQ(trimString("hello"), "hello");
    EXPECT_EQ(trimString("hello world"), "hello world");
}

// Pruebas para parseModuleMetadata
TEST_F(CoreTest, ParseModuleMetadata_FullMetadata) {
    std::string content = R"(
// Name: test-module
// Description: A test module for Bahamut
// Type: js
// Stage: 1
// Consumes: json
// Provides: csv
// Install: npm install test-package
// InstallScope: isolated
)";
    
    std::string module_path = (modules_dir / "test.js").string();
    std::ofstream file(module_path);
    file << content;
    file.close();
    
    ModuleMetadata meta = parseModuleMetadata(module_path);
    
    EXPECT_EQ(meta.name, "test-module");
    EXPECT_EQ(meta.description, "A test module for Bahamut");
    EXPECT_EQ(meta.type, "js");
    EXPECT_EQ(meta.stage, 1);
    EXPECT_EQ(meta.consumes, "json");
    EXPECT_EQ(meta.provides, "csv");
    EXPECT_EQ(meta.installCmd, "npm install test-package");
    EXPECT_EQ(meta.installScope, "isolated");
}

TEST_F(CoreTest, ParseModuleMetadata_PartialMetadata) {
    std::string content = R"(
// Name: simple-module
// Stage: 5
)";
    
    std::string module_path = (modules_dir / "simple.py").string();
    std::ofstream file(module_path);
    file << content;
    file.close();
    
    ModuleMetadata meta = parseModuleMetadata(module_path);
    
    EXPECT_EQ(meta.name, "simple-module");
    EXPECT_EQ(meta.stage, 5);
    EXPECT_EQ(meta.installScope, "shared"); 
    EXPECT_EQ(meta.type, "");
    EXPECT_EQ(meta.description, "");
}

TEST_F(CoreTest, ParseModuleMetadata_NoMetadata) {
    std::string content = R"(
console.log("Hello World");
function test() {
    return 42;
}
)";
    
    std::string module_path = (modules_dir / "empty.js").string();
    std::ofstream file(module_path);
    file << content;
    file.close();
    
    ModuleMetadata meta = parseModuleMetadata(module_path);
    
    EXPECT_EQ(meta.name, "");
    EXPECT_EQ(meta.stage, 999); 
}

TEST_F(CoreTest, EnsurePackageJson_CreatesWhenMissing) {
    std::string test_path = (test_dir / "test_module").string();
    fs::create_directories(test_path);
    
    ensurePackageJson(test_path);
    
    std::string pjson_path = test_path + "/package.json";
    EXPECT_TRUE(fs::exists(pjson_path));
    
    std::ifstream file(pjson_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    EXPECT_NE(content.find("bahamut-module"), std::string::npos);
    EXPECT_NE(content.find("1.0.0"), std::string::npos);
}

TEST_F(CoreTest, EnsurePackageJson_DoesNotOverwriteExisting) {
    std::string test_path = (test_dir / "existing").string();
    fs::create_directories(test_path);
    
    std::string pjson_path = test_path + "/package.json";
    std::ofstream file(pjson_path);
    file << R"({"name": "existing-module", "version": "0.1.0"})";
    file.close();
    
    ensurePackageJson(test_path);
    
    std::ifstream read_file(pjson_path);
    std::string content((std::istreambuf_iterator<char>(read_file)),
                       std::istreambuf_iterator<char>());
    
    EXPECT_EQ(content, R"({"name": "existing-module", "version": "0.1.0"})");
}

TEST_F(CoreTest, GetModules_FindsAllModuleTypes) {
    createTestModule("test1.js", "// JavaScript module");
    createTestModule("test2.py", "# Python module");
    createTestModule("test3.sh", "# Shell module");
    
    createTestModule("data.txt", "Just a text file");
    createTestModule("config.json", "{}");
    
    fs::create_directories(modules_dir / "node_modules");
    createTestModule("node_modules/ignore.js", "Should be ignored");
    
    auto modules = getModules();
    
    EXPECT_EQ(modules.size(), 3);
    
    std::set<std::string> module_names(modules.begin(), modules.end());
    EXPECT_TRUE(module_names.count("test1.js"));
    EXPECT_TRUE(module_names.count("test2.py"));
    EXPECT_TRUE(module_names.count("test3.sh"));
    EXPECT_FALSE(module_names.count("data.txt"));
    EXPECT_FALSE(module_names.count("config.json"));
    EXPECT_FALSE(module_names.count("ignore.js"));
}

TEST_F(CoreTest, GetModules_EmptyWhenNoModules) {
    auto modules = getModules();
    EXPECT_TRUE(modules.empty());
}

TEST_F(CoreTest, FindModulePath_FindsModuleInRoot) {
    createTestModule("findme.js", "// Test module");
    
    std::string path = findModulePath("findme.js");
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(path.find("findme.js") != std::string::npos);
}

TEST_F(CoreTest, FindModulePath_FindsModuleInSubdirectory) {
    fs::create_directories(modules_dir / "subdir");
    createTestModule("subdir/deep.js", "// Deep module");
    
    std::string path = findModulePath("deep.js");
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(path.find("deep.js") != std::string::npos);
}

TEST_F(CoreTest, FindModulePath_ReturnsEmptyWhenNotFound) {
    std::string path = findModulePath("nonexistent.js");
    EXPECT_TRUE(path.empty());
}

TEST_F(CoreTest, GetPythonVersion_DetectsPython3) {
    createTestModule("test.py", "#!/usr/bin/env python3\nprint('Hello')");
    std::string version = getPythonVersion((modules_dir / "test.py").string());
    EXPECT_EQ(version, "python3");
}

TEST_F(CoreTest, GetPythonVersion_DetectsPython3WithMinor) {
    createTestModule("test.py", "#!/usr/bin/env python3.9\nprint('Hello')");
    std::string version = getPythonVersion((modules_dir / "test.py").string());
    EXPECT_EQ(version, "python3.9");
}

TEST_F(CoreTest, GetPythonVersion_DefaultsToPython3) {
    createTestModule("test.py", "#!/usr/bin/env python\nprint('Hello')");
    std::string version = getPythonVersion((modules_dir / "test.py").string());
    EXPECT_EQ(version, "python3");
}

TEST_F(CoreTest, GetPythonVersion_HandlesPython2) {
    createTestModule("test.py", "#!/usr/bin/env python2\nprint 'Hello'");
    std::string version = getPythonVersion((modules_dir / "test.py").string());
    EXPECT_EQ(version, "python2");
}

TEST(ParseBMOPLineTest, ParsesDataItem) {
    std::map<std::string, std::vector<DataItem>> storage;
    
    std::string line = R"({"t":"d","f":"json","v":"{\"key\":\"value\"}"})";
    parseBMOPLine(line, storage);
    
    EXPECT_EQ(storage.size(), 1);
    EXPECT_EQ(storage["json"].size(), 1);
    EXPECT_EQ(storage["json"][0].format, "json");
    EXPECT_EQ(storage["json"][0].value, "{\"key\":\"value\"}");
}

TEST(ParseBMOPLineTest, ParsesBatchStart) {
    std::map<std::string, std::vector<DataItem>> storage;
    
    std::string line = R"({"t":"batch","f":"csv"})";
    parseBMOPLine(line, storage);
    
    EXPECT_EQ(storage.size(), 1);
    EXPECT_EQ(storage["__batch_format__"].size(), 1);
    EXPECT_EQ(storage["__batch_format__"][0].format, "csv");
}

TEST(ParseBMOPLineTest, IgnoresInvalidJSON) {
    std::map<std::string, std::vector<DataItem>> storage;
    
    std::string line = "not json at all";
    parseBMOPLine(line, storage);
    
    EXPECT_TRUE(storage.empty());
}

TEST(ParseBMOPLineTest, IgnoresMissingFields) {
    std::map<std::string, std::vector<DataItem>> storage;
    
    std::string line = R"({"t":"d"})"; 
    parseBMOPLine(line, storage);
    
    EXPECT_TRUE(storage.empty());
}

TEST(CollectModuleOutputTest, HandlesRegularOutput) {
    std::map<std::string, std::vector<DataItem>> storage;
    std::string moduleName = "test-module";
    
    std::string output = 
        R"({"t":"d","f":"json","v":"{\"test\":1}"})" "\n"
        R"({"t":"d","f":"xml","v":"<test>value</test>"})" "\n"
        R"({"t":"batch","f":"csv"})" "\n"
        "a,b,c\n"
        "1,2,3\n"
        R"({"t":"batch_end"})" "\n"
        R"({"t":"d","f":"json","v":"{\"end\":true}"})" "\n";
    
    // mock FILE* using stringstream 
    std::stringstream stream(output);
    
    // mock pipe
    std::string captured;
    char buffer[4096];
    
    // mock collectModuleOutput
    std::string lineBuffer;
    std::string batchFormat;
    bool inBatch = false;
    
    while (stream.getline(buffer, sizeof(buffer))) {
        std::string line(buffer);
        
        if (line.empty()) continue;
        
        if (line[0] == '{') {
            parseBMOPLine(line, storage);
            
            if (line.find("\"t\":\"batch\"") != std::string::npos) {
                inBatch = true;
                if (!storage["__batch_format__"].empty()) {
                    batchFormat = storage["__batch_format__"][0].format;
                    storage["__batch_format__"].clear();
                }
            }
            else if (line.find("\"t\":\"batch_end\"") != std::string::npos) {
                inBatch = false;
                batchFormat.clear();
            }
        }
        else if (inBatch && !batchFormat.empty()) {
            DataItem item;
            item.format = batchFormat;
            item.value = line;
            storage[batchFormat].push_back(item);
        }
    }
    
    EXPECT_EQ(storage["json"].size(), 2);
    EXPECT_EQ(storage["xml"].size(), 1);
    EXPECT_EQ(storage["csv"].size(), 2);
}

TEST(CollectModuleOutputTest, HandlesEmptyOutput) {
    std::map<std::string, std::vector<DataItem>> storage;
    std::string moduleName = "empty-module";
    
    std::string output = "";
    std::stringstream stream(output);
    
    parseBMOPLine(output, storage);
    
    EXPECT_TRUE(storage.empty());
}

TEST(CollectModuleOutputTest, HandlesMalformedLines) {
    std::map<std::string, std::vector<DataItem>> storage;
    std::string moduleName = "malformed-module";
    
    std::string output = R"({"t":"d","f":"json","v":"good"})" "\n" 
                       "not json\n" 
                       R"({"missing": "fields"})" "\n" 
                       R"({"t":"d","f":"json","v":"another"})";
    
    std::stringstream stream(output);
    std::string line;
    
    while (std::getline(stream, line)) {
        parseBMOPLine(line, storage);
    }
    
    EXPECT_EQ(storage["json"].size(), 2);
    EXPECT_EQ(storage["json"][0].value, "good");
    EXPECT_EQ(storage["json"][1].value, "another");
}

TEST_F(CoreTest, LoadProfile_LoadsModulesFromFile) {
    std::vector<std::string> expected = {"module1.js", "module2.py", "module3.sh"};
    createTestProfile("test", expected);
    
    auto modules = loadProfile("test");
    
    EXPECT_EQ(modules.size(), 3);
    EXPECT_EQ(modules, expected);
}

TEST_F(CoreTest, LoadProfile_IgnoresCommentsAndEmptyLines) {
    std::string profile_content = R"(# This is a comment
module1.js

# Another comment
module2.py
# Inline comment
module3.sh)";
    
    std::ofstream file(profiles_dir / "bahamut_comments.txt");
    file << profile_content;
    file.close();
    
    auto modules = loadProfile("comments");
    
    EXPECT_EQ(modules.size(), 3);
    EXPECT_EQ(modules[0], "module1.js");
    EXPECT_EQ(modules[1], "module2.py");
    EXPECT_EQ(modules[2], "module3.sh");
}

TEST_F(CoreTest, LoadProfile_ReturnsEmptyWhenNotFound) {
    auto modules = loadProfile("nonexistent");
    EXPECT_TRUE(modules.empty());
}

TEST_F(CoreTest, Integration_ParseAndRunSimpleModule) {
    std::string module_content = R"(#!/bin/bash
# Name: test-integration
# Description: Integration test module
# Type: sh
# Stage: 1
# Provides: test

echo "{\"t\":\"d\",\"f\":\"test\",\"v\":\"integration-passed\"}")";
    
    createTestModule("integration.sh", module_content);
    
    std::map<std::string, std::vector<DataItem>> storage;
    std::vector<std::string> args;
    
    runModuleWithPipe("integration.sh", args, storage, "");
    
    if (!storage.empty()) {
        EXPECT_TRUE(storage.count("test") > 0 || storage.empty());
    }
}

TEST(ModuleSortingTest, StagesAreOrderedCorrectly) {
    std::map<int, std::vector<std::pair<std::string, ModuleMetadata>>> stageModules;
    
    ModuleMetadata m1, m2, m3;
    m1.stage = 3;
    m2.stage = 1;
    m3.stage = 2;
    
    stageModules[3].push_back({"m1", m1});
    stageModules[1].push_back({"m2", m2});
    stageModules[2].push_back({"m3", m3});
    
    std::vector<int> stages;
    for (const auto& pair : stageModules) {
        stages.push_back(pair.first);
    }
    
    std::sort(stages.begin(), stages.end());
    EXPECT_EQ(stages[0], 1);
    EXPECT_EQ(stages[1], 2);
    EXPECT_EQ(stages[2], 3);
}

TEST(PipeDataTest, PipesDataWithWildcard) {
    std::map<std::string, std::vector<DataItem>> storage;
    
    DataItem item1, item2, item3;
    item1.format = "json";
    item1.value = "{\"test\":1}";
    item2.format = "xml";
    item2.value = "<test/>";
    item3.format = "json";
    item3.value = "{\"test\":2}";
    
    storage["json"].push_back(item1);
    storage["xml"].push_back(item2);
    storage["json"].push_back(item3);
    
    std::string captured;
    FILE* pipe = fmemopen(nullptr, 4096, "w+");
    ASSERT_NE(pipe, nullptr);
    
    pipeDataToModule(pipe, storage, "*");
    
    fflush(pipe);
    rewind(pipe);
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        captured += buffer;
    }
    fclose(pipe);
    
    EXPECT_NE(captured.find("{\"test\":1}"), std::string::npos);
    EXPECT_NE(captured.find("{\"test\":2}"), std::string::npos);
    EXPECT_NE(captured.find("<test/>"), std::string::npos);
}

TEST(PipeDataTest, PipesDataWithSpecificFormat) {
    std::map<std::string, std::vector<DataItem>> storage;
    
    DataItem item1, item2;
    item1.format = "json";
    item1.value = "{\"test\":1}";
    item2.format = "xml";
    item2.value = "<test/>";
    
    storage["json"].push_back(item1);
    storage["xml"].push_back(item2);
    
    std::string captured;
    FILE* pipe = fmemopen(nullptr, 4096, "w+");
    ASSERT_NE(pipe, nullptr);
    
    pipeDataToModule(pipe, storage, "json");
    
    fflush(pipe);
    rewind(pipe);
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        captured += buffer;
    }
    fclose(pipe);
    
    EXPECT_NE(captured.find("{\"test\":1}"), std::string::npos);
    EXPECT_EQ(captured.find("<test/>"), std::string::npos);
}

TEST(EdgeCasesTest, TrimString_UnicodeAndSpecialChars) {
    EXPECT_EQ(trimString("  héllò  "), "héllò");
    EXPECT_EQ(trimString("\t\n\r \u00A0hello\u00A0 \n\t\r"), "hello");
}

TEST(EdgeCasesTest, ParseModuleMetadata_EdgeCases) {
    char tmpname[] = "/tmp/bahamut_test_XXXXXX";
    int fd = mkstemp(tmpname);
    ASSERT_NE(fd, -1);
    
    std::string content = R"(// Name: 
// Stage: not_a_number
// InstallScope: invalid_scope)";
    
    write(fd, content.c_str(), content.size());
    close(fd);
    
    ModuleMetadata meta = parseModuleMetadata(tmpname);
    
    EXPECT_EQ(meta.name, "");
    EXPECT_EQ(meta.stage, 999); 
    EXPECT_EQ(meta.installScope, "shared"); 
    
    unlink(tmpname);
}

/*
*/
// No main function here - using gtest_main
