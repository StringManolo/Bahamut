# TODO

- Encapsulate modules in each folder
- Normalice input and output from modules (add a 4 modules in different languages to test)
- Create reports module
- Make cli (help, etc)
- Make a binary output format (like json but shorter and faster)
- Make cli ln global runnable
- Check tools like nodejs and npm are installed when needed by a module, if not, autoinstall..
- Make c++ functions default behaviour to return std::String or std::Vector<std::String> instead of writting to stdout directly. Call them from CLI to parse and color the output.
- Expose / export internal core functions to user created ./modules to be able to make middlewares without breaking core encapsulation. For example to debug, log, test, mdofiy functions output format, etc from user careated modules.
- Check make warnings
- Track per module dependencies to make a selective purge in shared modules


### To docu:
- Storage: add | replace | delete
- Fix: CleanWildcards example module
- Ignore "log", "error", "result". TODO: Proper output
- Check if modules scope are respected
- Add insane amount of tests
- Check tests:
```
77 tests from 2 test suites ran. (301964 ms total)
[  PASSED  ] 70 tests.
[  FAILED  ] 7 tests, listed below:
[  FAILED  ] BahamutTest.FindModulePathRecursive
[  FAILED  ] BahamutTest.SetupPythonEnvironmentShared
[  FAILED  ] BahamutTest.ComplexPipelineIntegration
[  FAILED  ] BahamutTest.EnsureDirectoriesExist
[  FAILED  ] BahamutTest.ModulePathResolution
[  FAILED  ] BahamutTest.ProfileModuleNotFound
[  FAILED  ] BahamutTest.CompleteSystemTest
```
- Add more test
