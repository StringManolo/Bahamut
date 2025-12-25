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
- ADD SESSIONS TO CONTINUE MODULE EXECUTION

### To docu:
- Storage: add | replace | delete
- Fix: CleanWildcards example module
- Ignore "log", "error", "result". TODO: Proper output
- Check if modules scope are respected
- Add insane amount of tests
- Check tests:
```
[----------] Global test environment tear-down
[==========] 109 tests from 4 test suites ran. (484140 ms total)
[  PASSED  ] 87 tests.
[  FAILED  ] 22 tests, listed below:
[  FAILED  ] BmopStderrTest.NodeModuleWithLogsAndData
[  FAILED  ] BmopStderrTest.PythonModuleWithMixedOutput
[  FAILED  ] BmopStderrTest.BashModuleWithStderrRedirect
[  FAILED  ] BmopStderrTest.ErrorMessagesToStderr
[  FAILED  ] BmopStderrTest.ProgressUpdatesOnlyOnStderr
[  FAILED  ] BmopStderrTest.BatchModeWithLogs
[  FAILED  ] BmopStderrTest.MixedBMOPMessagesCorrectSeparation
[  FAILED  ] BmopStderrTest.ModuleWithFatalError
[  FAILED  ] BmopStderrTest.RealTimeProgressMonitoring
[  FAILED  ] BmopStderrTest.LogLevelsSeparation
[  FAILED  ] BahamutTest.FindModulePathRecursive
[  FAILED  ] BahamutTest.SetupPythonEnvironmentShared
[  FAILED  ] BahamutTest.ComplexPipelineIntegration
[  FAILED  ] BahamutTest.EnsureDirectoriesExist
[  FAILED  ] BahamutTest.ModulePathResolution
[  FAILED  ] BahamutTest.ProfileModuleNotFound
[  FAILED  ] BahamutTest.CompleteSystemTest
[  FAILED  ] PerformanceTest.CrossLanguageLargeDataProcessing
[  FAILED  ] PerformanceTest.ProcessingPipelinePerformance
[  FAILED  ] PerformanceTest.CrossLanguageComparison
[  FAILED  ] PerformanceTest.StorageBehaviorPerformance
[  FAILED  ] PerformanceTest.ComplexTransformationPipeline

22 FAILED TESTS
```
- Add more test
