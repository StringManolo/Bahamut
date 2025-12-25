#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <string>
#include "../core/core.hpp"

namespace fs = std::filesystem;

class ProfileArgsTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::string timestamp = std::to_string(time(nullptr));
    test_dir = (fs::temp_directory_path() / ("bahamut_profile_test_" + timestamp)).string();
    
    original_cwd = fs::current_path();
    fs::create_directories(test_dir);
    fs::current_path(test_dir);
    
    fs::create_directories("profiles");
    fs::create_directories("modules");
  }

  void TearDown() override {
    fs::current_path(original_cwd);
    fs::remove_all(test_dir);
  }

  void createTestProfile(const std::string& name, const std::string& content) {
    std::string profilePath = "profiles/bahamut_" + name + ".txt";
    std::ofstream file(profilePath);
    file << content;
    file.close();
  }

  void createTestModule(const std::string& filename) {
    std::string modulePath = "modules/" + filename;
    std::ofstream file(modulePath);
    file << "#!/usr/bin/env bash\n";
    file << "echo 'test module'\n";
    file.close();
    fs::permissions(modulePath, fs::perms::owner_exec, fs::perm_options::add);
  }

  std::string test_dir;
  std::string original_cwd;
};

TEST_F(ProfileArgsTest, LoadProfileWithNoArguments) {
  std::string content = R"(
# Simple profile without arguments
module1.py
module2.js
module3.sh
)";
  
  createTestProfile("no_args", content);
  
  std::vector<ProfileModule> modules = loadProfile("no_args");
  
  ASSERT_EQ(modules.size(), 3);
  
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  EXPECT_EQ(modules[0].args.size(), 0);
  
  EXPECT_EQ(modules[1].moduleName, "module2.js");
  EXPECT_EQ(modules[1].args.size(), 0);
  
  EXPECT_EQ(modules[2].moduleName, "module3.sh");
  EXPECT_EQ(modules[2].args.size(), 0);
}

TEST_F(ProfileArgsTest, LoadProfileWithSimpleArguments) {
  std::string content = R"(
module1.py -v
module2.js --verbose
module3.sh -d --debug
)";
  
  createTestProfile("simple_args", content);
  
  std::vector<ProfileModule> modules = loadProfile("simple_args");
  
  ASSERT_EQ(modules.size(), 3);
  
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  ASSERT_EQ(modules[0].args.size(), 1);
  EXPECT_EQ(modules[0].args[0], "-v");
  
  EXPECT_EQ(modules[1].moduleName, "module2.js");
  ASSERT_EQ(modules[1].args.size(), 1);
  EXPECT_EQ(modules[1].args[0], "--verbose");
  
  EXPECT_EQ(modules[2].moduleName, "module3.sh");
  ASSERT_EQ(modules[2].args.size(), 2);
  EXPECT_EQ(modules[2].args[0], "-d");
  EXPECT_EQ(modules[2].args[1], "--debug");
}

TEST_F(ProfileArgsTest, LoadProfileWithArgumentsAndValues) {
  std::string content = R"(
scanner.py --url example.com --timeout 30
checker.js --port 443 --host localhost
)";
  
  createTestProfile("args_values", content);
  
  std::vector<ProfileModule> modules = loadProfile("args_values");
  
  ASSERT_EQ(modules.size(), 2);
  
  EXPECT_EQ(modules[0].moduleName, "scanner.py");
  ASSERT_EQ(modules[0].args.size(), 4);
  EXPECT_EQ(modules[0].args[0], "--url");
  EXPECT_EQ(modules[0].args[1], "example.com");
  EXPECT_EQ(modules[0].args[2], "--timeout");
  EXPECT_EQ(modules[0].args[3], "30");
  
  EXPECT_EQ(modules[1].moduleName, "checker.js");
  ASSERT_EQ(modules[1].args.size(), 4);
  EXPECT_EQ(modules[1].args[0], "--port");
  EXPECT_EQ(modules[1].args[1], "443");
  EXPECT_EQ(modules[1].args[2], "--host");
  EXPECT_EQ(modules[1].args[3], "localhost");
}

TEST_F(ProfileArgsTest, LoadProfileWithQuotedStrings) {
  std::string content = R"(
module1.py --message "Hello World"
module2.js --path '/tmp/test path'
module3.sh --name "Test Module" -v
)";
  
  createTestProfile("quoted", content);
  
  std::vector<ProfileModule> modules = loadProfile("quoted");
  
  ASSERT_EQ(modules.size(), 3);
  
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  ASSERT_EQ(modules[0].args.size(), 2);
  EXPECT_EQ(modules[0].args[0], "--message");
  EXPECT_EQ(modules[0].args[1], "\"Hello World\"");
  
  EXPECT_EQ(modules[1].moduleName, "module2.js");
  ASSERT_EQ(modules[1].args.size(), 2);
  EXPECT_EQ(modules[1].args[0], "--path");
  EXPECT_EQ(modules[1].args[1], "'/tmp/test path'");
  
  EXPECT_EQ(modules[2].moduleName, "module3.sh");
  ASSERT_EQ(modules[2].args.size(), 3);
  EXPECT_EQ(modules[2].args[0], "--name");
  EXPECT_EQ(modules[2].args[1], "\"Test Module\"");
  EXPECT_EQ(modules[2].args[2], "-v");
}

TEST_F(ProfileArgsTest, LoadProfileWithComments) {
  std::string content = R"(
# This is a comment
module1.py -v
# Another comment
module2.js --debug
# Final comment
)";
  
  createTestProfile("comments", content);
  
  std::vector<ProfileModule> modules = loadProfile("comments");
  
  ASSERT_EQ(modules.size(), 2);
  
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  EXPECT_EQ(modules[1].moduleName, "module2.js");
}

TEST_F(ProfileArgsTest, LoadProfileWithEmptyLines) {
  std::string content = R"(

module1.py -v

module2.js --debug

module3.sh

)";
  
  createTestProfile("empty_lines", content);
  
  std::vector<ProfileModule> modules = loadProfile("empty_lines");
  
  ASSERT_EQ(modules.size(), 3);
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  EXPECT_EQ(modules[1].moduleName, "module2.js");
  EXPECT_EQ(modules[2].moduleName, "module3.sh");
}

TEST_F(ProfileArgsTest, LoadProfileWithMixedContent) {
  std::string content = R"(
# Prerequisites
checktor.js --proxy socks5://127.0.0.1:9050

# Data collection
getdomains.py --url https://example.com/data.txt -v

# Processing
cleanwildcards.js

# Subdomain generation
createsubdomains.py -v --max-depth 3 --wordlist common.txt

# Export
exportcsv.sh --format json --output results.json
)";
  
  createTestProfile("mixed", content);
  
  std::vector<ProfileModule> modules = loadProfile("mixed");
  
  ASSERT_EQ(modules.size(), 5);
  
  EXPECT_EQ(modules[0].moduleName, "checktor.js");
  ASSERT_EQ(modules[0].args.size(), 2);
  EXPECT_EQ(modules[0].args[0], "--proxy");
  EXPECT_EQ(modules[0].args[1], "socks5://127.0.0.1:9050");
  
  EXPECT_EQ(modules[1].moduleName, "getdomains.py");
  ASSERT_EQ(modules[1].args.size(), 3);
  EXPECT_EQ(modules[1].args[0], "--url");
  EXPECT_EQ(modules[1].args[1], "https://example.com/data.txt");
  EXPECT_EQ(modules[1].args[2], "-v");
  
  EXPECT_EQ(modules[2].moduleName, "cleanwildcards.js");
  EXPECT_EQ(modules[2].args.size(), 0);
  
  EXPECT_EQ(modules[3].moduleName, "createsubdomains.py");
  ASSERT_EQ(modules[3].args.size(), 5);
  EXPECT_EQ(modules[3].args[0], "-v");
  EXPECT_EQ(modules[3].args[1], "--max-depth");
  EXPECT_EQ(modules[3].args[2], "3");
  EXPECT_EQ(modules[3].args[3], "--wordlist");
  EXPECT_EQ(modules[3].args[4], "common.txt");
  
  EXPECT_EQ(modules[4].moduleName, "exportcsv.sh");
  ASSERT_EQ(modules[4].args.size(), 4);
  EXPECT_EQ(modules[4].args[0], "--format");
  EXPECT_EQ(modules[4].args[1], "json");
  EXPECT_EQ(modules[4].args[2], "--output");
  EXPECT_EQ(modules[4].args[3], "results.json");
}

TEST_F(ProfileArgsTest, LoadProfileWithExtraSpaces) {
  std::string content = R"(
module1.py    -v     --debug
module2.js  --url    example.com     --timeout   30
)";
  
  createTestProfile("extra_spaces", content);
  
  std::vector<ProfileModule> modules = loadProfile("extra_spaces");
  
  ASSERT_EQ(modules.size(), 2);
  
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  ASSERT_EQ(modules[0].args.size(), 2);
  EXPECT_EQ(modules[0].args[0], "-v");
  EXPECT_EQ(modules[0].args[1], "--debug");
  
  EXPECT_EQ(modules[1].moduleName, "module2.js");
  ASSERT_EQ(modules[1].args.size(), 4);
  EXPECT_EQ(modules[1].args[0], "--url");
  EXPECT_EQ(modules[1].args[1], "example.com");
  EXPECT_EQ(modules[1].args[2], "--timeout");
  EXPECT_EQ(modules[1].args[3], "30");
}

TEST_F(ProfileArgsTest, LoadProfileWithURLsAndPaths) {
  std::string content = R"(
fetcher.py --url https://raw.githubusercontent.com/user/repo/main/data.txt
scanner.py --path /var/log/app.log --output /tmp/results.json
)";
  
  createTestProfile("urls_paths", content);
  
  std::vector<ProfileModule> modules = loadProfile("urls_paths");
  
  ASSERT_EQ(modules.size(), 2);
  
  EXPECT_EQ(modules[0].moduleName, "fetcher.py");
  ASSERT_EQ(modules[0].args.size(), 2);
  EXPECT_EQ(modules[0].args[0], "--url");
  EXPECT_EQ(modules[0].args[1], "https://raw.githubusercontent.com/user/repo/main/data.txt");
  
  EXPECT_EQ(modules[1].moduleName, "scanner.py");
  ASSERT_EQ(modules[1].args.size(), 4);
  EXPECT_EQ(modules[1].args[0], "--path");
  EXPECT_EQ(modules[1].args[1], "/var/log/app.log");
  EXPECT_EQ(modules[1].args[2], "--output");
  EXPECT_EQ(modules[1].args[3], "/tmp/results.json");
}

TEST_F(ProfileArgsTest, LoadProfileWithNumericArguments) {
  std::string content = R"(
scanner.py --threads 50 --timeout 30 --port 443
checker.js --max-depth 5 --retry 3
)";
  
  createTestProfile("numeric", content);
  
  std::vector<ProfileModule> modules = loadProfile("numeric");
  
  ASSERT_EQ(modules.size(), 2);
  
  EXPECT_EQ(modules[0].moduleName, "scanner.py");
  ASSERT_EQ(modules[0].args.size(), 6);
  EXPECT_EQ(modules[0].args[0], "--threads");
  EXPECT_EQ(modules[0].args[1], "50");
  EXPECT_EQ(modules[0].args[2], "--timeout");
  EXPECT_EQ(modules[0].args[3], "30");
  EXPECT_EQ(modules[0].args[4], "--port");
  EXPECT_EQ(modules[0].args[5], "443");
  
  EXPECT_EQ(modules[1].moduleName, "checker.js");
  ASSERT_EQ(modules[1].args.size(), 4);
  EXPECT_EQ(modules[1].args[0], "--max-depth");
  EXPECT_EQ(modules[1].args[1], "5");
  EXPECT_EQ(modules[1].args[2], "--retry");
  EXPECT_EQ(modules[1].args[3], "3");
}

TEST_F(ProfileArgsTest, LoadProfileWithCombinedShortFlags) {
  std::string content = R"(
module1.py -vd
module2.js -abc --long-flag
module3.sh -xyz --test value
)";
  
  createTestProfile("combined_flags", content);
  
  std::vector<ProfileModule> modules = loadProfile("combined_flags");
  
  ASSERT_EQ(modules.size(), 3);
  
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  ASSERT_EQ(modules[0].args.size(), 1);
  EXPECT_EQ(modules[0].args[0], "-vd");
  
  EXPECT_EQ(modules[1].moduleName, "module2.js");
  ASSERT_EQ(modules[1].args.size(), 2);
  EXPECT_EQ(modules[1].args[0], "-abc");
  EXPECT_EQ(modules[1].args[1], "--long-flag");
  
  EXPECT_EQ(modules[2].moduleName, "module3.sh");
  ASSERT_EQ(modules[2].args.size(), 3);
  EXPECT_EQ(modules[2].args[0], "-xyz");
  EXPECT_EQ(modules[2].args[1], "--test");
  EXPECT_EQ(modules[2].args[2], "value");
}

TEST_F(ProfileArgsTest, LoadProfileWithHyphenatedArguments) {
  std::string content = R"(
module1.py --output-dir /tmp/output --max-threads 10
module2.js --enable-cache --disable-logging
)";
  
  createTestProfile("hyphenated", content);
  
  std::vector<ProfileModule> modules = loadProfile("hyphenated");
  
  ASSERT_EQ(modules.size(), 2);
  
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  ASSERT_EQ(modules[0].args.size(), 4);
  EXPECT_EQ(modules[0].args[0], "--output-dir");
  EXPECT_EQ(modules[0].args[1], "/tmp/output");
  EXPECT_EQ(modules[0].args[2], "--max-threads");
  EXPECT_EQ(modules[0].args[3], "10");
  
  EXPECT_EQ(modules[1].moduleName, "module2.js");
  ASSERT_EQ(modules[1].args.size(), 2);
  EXPECT_EQ(modules[1].args[0], "--enable-cache");
  EXPECT_EQ(modules[1].args[1], "--disable-logging");
}

TEST_F(ProfileArgsTest, LoadProfileNonExistent) {
  std::vector<ProfileModule> modules = loadProfile("nonexistent");
  
  EXPECT_EQ(modules.size(), 0);
}

TEST_F(ProfileArgsTest, LoadProfileEmpty) {
  std::string content = R"(
# Only comments
# Nothing else
)";
  
  createTestProfile("empty", content);
  
  std::vector<ProfileModule> modules = loadProfile("empty");
  
  EXPECT_EQ(modules.size(), 0);
}

TEST_F(ProfileArgsTest, LoadProfileWithSpecialCharacters) {
  std::string content = R"(
module1.py --regex "[a-z]+" --pattern "test.*"
module2.js --user admin@example.com --password "P@ssw0rd!"
)";
  
  createTestProfile("special_chars", content);
  
  std::vector<ProfileModule> modules = loadProfile("special_chars");
  
  ASSERT_EQ(modules.size(), 2);
  
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  ASSERT_EQ(modules[0].args.size(), 4);
  EXPECT_EQ(modules[0].args[0], "--regex");
  EXPECT_EQ(modules[0].args[1], "\"[a-z]+\"");
  EXPECT_EQ(modules[0].args[2], "--pattern");
  EXPECT_EQ(modules[0].args[3], "\"test.*\"");
  
  EXPECT_EQ(modules[1].moduleName, "module2.js");
  ASSERT_EQ(modules[1].args.size(), 4);
  EXPECT_EQ(modules[1].args[0], "--user");
  EXPECT_EQ(modules[1].args[1], "admin@example.com");
  EXPECT_EQ(modules[1].args[2], "--password");
  EXPECT_EQ(modules[1].args[3], "\"P@ssw0rd!\"");
}

TEST_F(ProfileArgsTest, LoadProfileWithSingleQuotes) {
  std::string content = R"(
module1.py --path '/tmp/test file'
module2.js --name 'Test Module' -v
)";
  
  createTestProfile("single_quotes", content);
  
  std::vector<ProfileModule> modules = loadProfile("single_quotes");
  
  ASSERT_EQ(modules.size(), 2);
  
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  ASSERT_EQ(modules[0].args.size(), 2);
  EXPECT_EQ(modules[0].args[0], "--path");
  EXPECT_EQ(modules[0].args[1], "'/tmp/test file'");
  
  EXPECT_EQ(modules[1].moduleName, "module2.js");
  ASSERT_EQ(modules[1].args.size(), 3);
  EXPECT_EQ(modules[1].args[0], "--name");
  EXPECT_EQ(modules[1].args[1], "'Test Module'");
  EXPECT_EQ(modules[1].args[2], "-v");
}

TEST_F(ProfileArgsTest, LoadProfileRealWorldExample) {
  std::string content = R"(
# Bahamut Reconnaissance Profile
# Author: Bahamut Team
# Purpose: Full domain reconnaissance workflow

# Prerequisites check
checktor.js --proxy socks5://127.0.0.1:9050

# Data collection
getbugbountydomains.py --url https://raw.githubusercontent.com/arkadiyt/bounty-targets-data/main/data/domains.txt

# Data processing
cleanwildcards.js

# Subdomain generation
createsubdomains.py -v

# Export results
exportcsv.sh --format json
)";
  
  createTestProfile("recon", content);
  
  std::vector<ProfileModule> modules = loadProfile("recon");
  
  ASSERT_EQ(modules.size(), 5);
  
  EXPECT_EQ(modules[0].moduleName, "checktor.js");
  EXPECT_EQ(modules[1].moduleName, "getbugbountydomains.py");
  EXPECT_EQ(modules[2].moduleName, "cleanwildcards.js");
  EXPECT_EQ(modules[3].moduleName, "createsubdomains.py");
  EXPECT_EQ(modules[4].moduleName, "exportcsv.sh");
}

TEST_F(ProfileArgsTest, LoadProfileWithEqualsSign) {
  std::string content = R"(
module1.py --key=value --setting=true
module2.js --config=/path/to/file
)";
  
  createTestProfile("equals", content);
  
  std::vector<ProfileModule> modules = loadProfile("equals");
  
  ASSERT_EQ(modules.size(), 2);
  
  EXPECT_EQ(modules[0].moduleName, "module1.py");
  ASSERT_EQ(modules[0].args.size(), 2);
  EXPECT_EQ(modules[0].args[0], "--key=value");
  EXPECT_EQ(modules[0].args[1], "--setting=true");
  
  EXPECT_EQ(modules[1].moduleName, "module2.js");
  ASSERT_EQ(modules[1].args.size(), 1);
  EXPECT_EQ(modules[1].args[0], "--config=/path/to/file");
}

TEST_F(ProfileArgsTest, BackwardCompatibilityNoArgs) {
  std::string content = R"(
checktor.js
getbugbountydomains.py
cleanwildcards.js
createsubdomains.py
exportcsv.sh
)";
  
  createTestProfile("legacy", content);
  
  std::vector<ProfileModule> modules = loadProfile("legacy");
  
  ASSERT_EQ(modules.size(), 5);
  
  for (const auto& module : modules) {
    EXPECT_EQ(module.args.size(), 0);
  }
  
  EXPECT_EQ(modules[0].moduleName, "checktor.js");
  EXPECT_EQ(modules[1].moduleName, "getbugbountydomains.py");
  EXPECT_EQ(modules[2].moduleName, "cleanwildcards.js");
  EXPECT_EQ(modules[3].moduleName, "createsubdomains.py");
  EXPECT_EQ(modules[4].moduleName, "exportcsv.sh");
}
