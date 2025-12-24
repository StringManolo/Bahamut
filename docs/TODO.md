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
