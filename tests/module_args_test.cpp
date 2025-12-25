#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <cstring>
#include "../include/simpleargumentsparser.hpp"

void splitArguments(int argc, char* argv[], std::vector<char*>& bahamutArgs, std::vector<char*>& modArgs);

class ModuleArgsTest : public ::testing::Test {
protected:
  std::vector<char*> makeArgv(const std::vector<std::string>& args) {
    argv_storage.clear();
    for (const auto& arg : args) {
      argv_storage.push_back(new char[arg.size() + 1]);
      std::strcpy(argv_storage.back(), arg.c_str());
    }
    return argv_storage;
  }

  void TearDown() override {
    for (auto ptr : argv_storage) {
      delete[] ptr;
    }
    argv_storage.clear();
  }

  std::vector<char*> argv_storage;
};

TEST_F(ModuleArgsTest, SplitArgumentsNoSeparator) {
  std::vector<std::string> args = {"bahamut", "run", "module.py", "-v"};
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  EXPECT_EQ(bahamutArgs.size(), 4);
  EXPECT_EQ(modArgs.size(), 0);
  EXPECT_STREQ(bahamutArgs[0], "bahamut");
  EXPECT_STREQ(bahamutArgs[1], "run");
  EXPECT_STREQ(bahamutArgs[2], "module.py");
  EXPECT_STREQ(bahamutArgs[3], "-v");
}

TEST_F(ModuleArgsTest, SplitArgumentsWithSeparator) {
  std::vector<std::string> args = {"bahamut", "run", "module.py", "--", "-u", "test.com"};
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  EXPECT_EQ(bahamutArgs.size(), 3);
  EXPECT_EQ(modArgs.size(), 3);
  
  EXPECT_STREQ(bahamutArgs[0], "bahamut");
  EXPECT_STREQ(bahamutArgs[1], "run");
  EXPECT_STREQ(bahamutArgs[2], "module.py");
  
  EXPECT_STREQ(modArgs[0], "bahamut");
  EXPECT_STREQ(modArgs[1], "-u");
  EXPECT_STREQ(modArgs[2], "test.com");
}

TEST_F(ModuleArgsTest, SplitArgumentsMultipleModuleArgs) {
  std::vector<std::string> args = {
    "bahamut", "-v", "run", "test.py", "--", 
    "--url", "example.com", "-v", "--timeout", "10"
  };
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  EXPECT_EQ(bahamutArgs.size(), 4);
  EXPECT_EQ(modArgs.size(), 6);
  
  EXPECT_STREQ(bahamutArgs[1], "-v");
  EXPECT_STREQ(bahamutArgs[2], "run");
  EXPECT_STREQ(bahamutArgs[3], "test.py");
  
  EXPECT_STREQ(modArgs[1], "--url");
  EXPECT_STREQ(modArgs[2], "example.com");
  EXPECT_STREQ(modArgs[3], "-v");
  EXPECT_STREQ(modArgs[4], "--timeout");
  EXPECT_STREQ(modArgs[5], "10");
}

TEST_F(ModuleArgsTest, SplitArgumentsSeparatorAtEnd) {
  std::vector<std::string> args = {"bahamut", "run", "module.py", "--"};
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  EXPECT_EQ(bahamutArgs.size(), 3);
  EXPECT_EQ(modArgs.size(), 1);
  EXPECT_STREQ(modArgs[0], "bahamut");
}

TEST_F(ModuleArgsTest, SplitArgumentsProfileWithArgs) {
  std::vector<std::string> args = {
    "bahamut", "run", "--profile", "recon", "--", 
    "--depth", "3", "--verbose"
  };
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  EXPECT_EQ(bahamutArgs.size(), 4);
  EXPECT_EQ(modArgs.size(), 4);
  
  EXPECT_STREQ(bahamutArgs[1], "run");
  EXPECT_STREQ(bahamutArgs[2], "--profile");
  EXPECT_STREQ(bahamutArgs[3], "recon");
  
  EXPECT_STREQ(modArgs[1], "--depth");
  EXPECT_STREQ(modArgs[2], "3");
  EXPECT_STREQ(modArgs[3], "--verbose");
}

TEST_F(ModuleArgsTest, ParseModuleArgsShortFlags) {
  std::vector<std::string> args = {"program", "-v", "-d", "-u", "test.com"};
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_TRUE(cli.s["v"]);
  EXPECT_TRUE(cli.s["d"]);
  EXPECT_STREQ(cli.s["u"].toString().c_str(), "test.com");
}

TEST_F(ModuleArgsTest, ParseModuleArgsLongFlags) {
  std::vector<std::string> args = {
    "program", "--url", "example.com", "--verbose", "--timeout", "10"
  };
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_STREQ(cli.c["url"].toString().c_str(), "example.com");
  EXPECT_TRUE(cli.c["verbose"]);
  EXPECT_STREQ(cli.c["timeout"].toString().c_str(), "10");
}

TEST_F(ModuleArgsTest, ParseModuleArgsMixedFlags) {
  std::vector<std::string> args = {
    "program", "-v", "--url", "test.com", "-u", "user", "--port", "443"
  };
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_TRUE(cli.s["v"]);
  EXPECT_STREQ(cli.s["u"].toString().c_str(), "user");
  EXPECT_STREQ(cli.c["url"].toString().c_str(), "test.com");
  EXPECT_STREQ(cli.c["port"].toString().c_str(), "443");
}

TEST_F(ModuleArgsTest, ParseModuleArgsPositional) {
  std::vector<std::string> args = {"program", "input.txt", "output.txt"};
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_EQ(cli.o.size(), 2);
  EXPECT_STREQ(cli.o[0].first.c_str(), "input.txt");
  EXPECT_STREQ(cli.o[1].first.c_str(), "output.txt");
}

TEST_F(ModuleArgsTest, ParseModuleArgsBooleanFlags) {
  std::vector<std::string> args = {
    "program", "--verbose", "--debug", "--quiet"
  };
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_TRUE(cli.c["verbose"]);
  EXPECT_TRUE(cli.c["debug"]);
  EXPECT_TRUE(cli.c["quiet"]);
}

TEST_F(ModuleArgsTest, ParseModuleArgsHyphenatedLongFlags) {
  std::vector<std::string> args = {
    "program", "--output-dir", "/tmp/output", "--max-depth", "5"
  };
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_STREQ(cli.c["output-dir"].toString().c_str(), "/tmp/output");
  EXPECT_STREQ(cli.c["max-depth"].toString().c_str(), "5");
}

TEST_F(ModuleArgsTest, ParseModuleArgsCombinedShortFlags) {
  std::vector<std::string> args = {"program", "-vdu", "value"};
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_TRUE(cli.s["v"]);
  EXPECT_TRUE(cli.s["d"]);
  EXPECT_STREQ(cli.s["u"].toString().c_str(), "value");
}

TEST_F(ModuleArgsTest, SplitArgumentsBahamutVerboseAndModuleVerbose) {
  std::vector<std::string> args = {
    "bahamut", "-v", "run", "module.py", "--", "-v", "--url", "test.com"
  };
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  CLI bahamutCli = parseCLI(bahamutArgs.size(), bahamutArgs.data());
  CLI moduleCli = parseCLI(modArgs.size(), modArgs.data());
  
  EXPECT_TRUE(bahamutCli.s["v"]);
  EXPECT_TRUE(moduleCli.s["v"]);
  EXPECT_STREQ(moduleCli.c["url"].toString().c_str(), "test.com");
  
  EXPECT_FALSE(bahamutCli.c["url"].existsValue());
}

TEST_F(ModuleArgsTest, SplitArgumentsComplexRealWorldExample) {
  std::vector<std::string> args = {
    "bahamut", "-v", "-d", "run", "--profile", "recon", "--",
    "--url", "example.com", "-v", "--timeout", "30", "--threads", "10"
  };
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  CLI bahamutCli = parseCLI(bahamutArgs.size(), bahamutArgs.data());
  CLI moduleCli = parseCLI(modArgs.size(), modArgs.data());
  
  EXPECT_TRUE(bahamutCli.s["v"]);
  EXPECT_TRUE(bahamutCli.s["d"]);
  EXPECT_STREQ(bahamutCli.c["profile"].toString().c_str(), "recon");
  
  EXPECT_STREQ(moduleCli.c["url"].toString().c_str(), "example.com");
  EXPECT_TRUE(moduleCli.s["v"]);
  EXPECT_STREQ(moduleCli.c["timeout"].toString().c_str(), "30");
  EXPECT_STREQ(moduleCli.c["threads"].toString().c_str(), "10");
  
  EXPECT_FALSE(bahamutCli.c["url"].existsValue());
  EXPECT_FALSE(bahamutCli.c["timeout"].existsValue());
  EXPECT_FALSE(moduleCli.c["profile"].existsValue());
}

TEST_F(ModuleArgsTest, ParseModuleArgsEmpty) {
  std::vector<std::string> args = {"program"};
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_TRUE(cli.noArgs);
  EXPECT_EQ(cli.argc, 0);
  EXPECT_TRUE(cli.s.empty());
  EXPECT_TRUE(cli.c.empty());
  EXPECT_TRUE(cli.o.empty());
}

TEST_F(ModuleArgsTest, SplitArgumentsOnlyBahamut) {
  std::vector<std::string> args = {"bahamut", "-v", "-d", "list"};
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  EXPECT_EQ(bahamutArgs.size(), 4);
  EXPECT_EQ(modArgs.size(), 0);
}

TEST_F(ModuleArgsTest, ParseModuleArgsVersionFlag) {
  std::vector<std::string> args = {"program", "--version"};
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_TRUE(cli.c["version"]);
}

TEST_F(ModuleArgsTest, ParseModuleArgsHelpFlag) {
  std::vector<std::string> args = {"program", "-h", "--help"};
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_TRUE(cli.s["h"]);
  EXPECT_TRUE(cli.c["help"]);
}

TEST_F(ModuleArgsTest, SplitArgumentsMultipleSeparators) {
  std::vector<std::string> args = {
    "bahamut", "run", "module.py", "--", "-v", "--", "extra"
  };
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  EXPECT_EQ(bahamutArgs.size(), 3);
  EXPECT_GE(modArgs.size(), 2);
  
  EXPECT_STREQ(modArgs[1], "-v");
}

TEST_F(ModuleArgsTest, ParseModuleArgsWithEquals) {
  std::vector<std::string> args = {"program", "--url=example.com", "--port=443"};
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_TRUE(cli.o.size() >= 2 || cli.c.size() >= 2);
}

TEST_F(ModuleArgsTest, SplitArgumentsRunAll) {
  std::vector<std::string> args = {
    "bahamut", "run", "all", "--", "--timeout", "5", "--verbose"
  };
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  EXPECT_EQ(bahamutArgs.size(), 3);
  EXPECT_EQ(modArgs.size(), 4);
  
  CLI moduleCli = parseCLI(modArgs.size(), modArgs.data());
  
  EXPECT_STREQ(moduleCli.c["timeout"].toString().c_str(), "5");
  EXPECT_TRUE(moduleCli.c["verbose"]);
}

TEST_F(ModuleArgsTest, ParseModuleArgsSpecialCharacters) {
  std::vector<std::string> args = {
    "program", "--url", "https://example.com/path?query=value"
  };
  auto argv = makeArgv(args);
  
  CLI cli = parseCLI(argv.size(), argv.data());
  
  EXPECT_STREQ(cli.c["url"].toString().c_str(), "https://example.com/path?query=value");
}

TEST_F(ModuleArgsTest, SplitArgumentsDebugModuleArgs) {
  std::vector<std::string> args = {
    "bahamut", "--debug-module-args", "run", "test.py", "--", "-v", "--url", "test.com"
  };
  auto argv = makeArgv(args);
  
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argv.size(), argv.data(), bahamutArgs, modArgs);
  
  CLI bahamutCli = parseCLI(bahamutArgs.size(), bahamutArgs.data());
  CLI moduleCli = parseCLI(modArgs.size(), modArgs.data());
  
  EXPECT_TRUE(bahamutCli.c["debug-module-args"]);
  EXPECT_TRUE(moduleCli.s["v"]);
  EXPECT_STREQ(moduleCli.c["url"].toString().c_str(), "test.com");
}
