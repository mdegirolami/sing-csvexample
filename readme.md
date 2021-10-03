A simple example of how to port a C++ library in sing
======================================================= 

NOTE: You should open with Visual Studio Code only one of the csvstream_1 or csvstream_2 folders, not the root !

The two projects adopt two different strategies:

csvstream_1 changes the interfaces of the library to make them sing-compliant. The result (in this case) is an header - only library

csvstream_2 wraps the library with a layer of sing-compliant functions. The result is a static library.

Both projects also include a simple application to test the result.

Here follows a description of how the projects have been built starting from a plain hello world project.


The csvstream library
---------------------
csvstream (the library to be ported) is a csv parser with mit license: https://github.com/awdeorio/csvstream
This example is only apparently simple. The interface has very few functions but the used language constructs have not an equivalent in sing (mostly for good reasons).

This is the public part of the library header:

~~~
class csvstream {
public:
  // Constructor from filename. Throws csvstream_exception if open fails.
  csvstream(const std::string &filename, char delimiter=',', bool strict=true);

  // Constructor from stream
  csvstream(std::istream &is, char delimiter=',', bool strict=true);

  // Destructor
  ~csvstream();

  // Return false if an error flag on underlying stream is set
  explicit operator bool() const;

  // Return header processed by constructor
  std::vector<std::string> getheader() const;

  // Stream extraction operator reads one row. Throws csvstream_exception if
  // the number of items in a row does not match the header.
  csvstream & operator>> (std::map<std::string, std::string>& row);

  // Stream extraction operator reads one row, keeping column order. Throws
  // csvstream_exception if the number of items in a row does not match the
  // header.
  csvstream & operator>> (std::vector<std::pair<std::string, std::string> >& row);
}
~~~

Note that sing doesn't support:
- constructors
- operator overloading
- returning heavyweight objects by value (like std::vector\<std::string\>)

Also, the library uses exceptions to signal errors (knee deep in c++) and std::istream, which we would need to import as well.

We must craft a brand new interface. Let's start writing it as we would like to have it. Here is sing_headers/csv.sing:

~~~
namespace csvstream;
requires "sio";

public class CsvStream {
public:
    // intit from filename. returns false on failure.
	fn mut initFromFilename(filename string, delimiter string = ",", strict bool = true) bool;

	fn mut initFromStream(stream sio.Stream, delimiter string = ",", strict bool = true) void;

	fn streamOperationFailed() bool;

    // return the names of the fields of a csv row.
	fn getHeader(out header [*]string) void;

    // get the values from the next row. returns false on error (if the number of fields don't match the header)
	fn mut getRow(values [*]string) bool;

    // if an error occurs, you can get a description from here
	fn getErrorString() string;

	fn finalize() void;
}
~~~

Now we need to match this to c++ types. We can do it manually or, if we want the confort of an authomatic conversion, we can use the -p option of the sing compiler. 

To do that I felt the need to put down the sing command into a ninja file to prevent the need to type by hands a command complete with very long paths.

I added to the build/build_aux.ninja file a few rules:

~~~
## Converting from sing headers to c++ headers
sing_headers = ../../sing_headers
cpp_headers = ../../inc

rule s2h
  command = $singcc -p -I $sing_headers -I $singhh -o $out $in

build $cpp_headers/csv.h : s2h $sing_headers/csv.sing

build all_headers: phony $cpp_headers/csv.h
~~~

The first build rule tells ninja how to build $cpp_headers/csv.h from $sing_headers/csv.sing with the s2h rule.

The second build rule is a dummy rule and just builds all its dependencies. It is there so you can add new headers to be converted and make them all with a single stroke. 

To push the automation further I added to .vscode/tasks.json a new command to make it available in the ctl-shift-B menu.
It is enough to just add this section to the tasks array:

~~~
        {
            "type": "shell",
            "label": "sing2header",

            "command": "ninja",
            "windows": {
                "args": ["-v", "-f", "../build_win_debug.ninja", "all_headers"],
            },
            "linux": {
                "args": ["-v", "-f", "../build_win_release.ninja", "all_headers"],
            },
            "presentation": {
                "clear": true
            },
            "options": {
                "cwd": "${workspaceFolder}/build/obj_d"
            },
            "problemMatcher": [
                "$gcc"
            ],
            "group": {
                "kind": "build",
                "isDefault": true
            }
        }
~~~

I placed csvstream.sing in root/sing_headers (out of the sing directory) to prevent the building system from trying to compile it and I created the output directory 'inc' to hold the output. 

Then I wrote app.sing to test the library interface and changed the name of the $bin_target in app.exe
(just replaced csvstream_x to app wherever in the build files).

At this point there are two ways we can go:
1) to patch the original library to comply with the new header
2) to build a wrapper around the original library

The first option generally gives a best performing result, but requires effort to align future versions of the library and some insight in the original library code.

We can explore both:

Patching the library (csvstream_1 project)
------------------------------------------
We create a new src/csv.h library file starting from the original original/csvstream.h and inserting the lines from inc/csv.h where appropriate:

~~~
#pragma once            // relaces the original guard.
#include <sing.h>       // added to the include list
#include "sio.h" 
namespace csvstream {   // soon after the includes
class CsvStream ...     
}                       // at the end of the file
~~~

Add to CsvStream the private part from the original csvstream class and an extra member to store the error message:
~~~
    // Error description
    std::string error;
~~~

Now we can delete the csvstream_exception and 'class csvstream' declarations.

csvstream type is renamed CsvStream wherever in the file.

Public functions signatures are changed according to the new declarations.

Constructors are replaced by the init() function and initialization lists are replaced by assignments.

Wherever the code throws an exception we save the error message in the 'error' member and return failure.  
This also brings us to change the return value of read_header() because it must be able to return failure.

Opening the CsvStream with a sing stream is not supported. This would imply transforming a sio.Stream into a std::istream, which is now impossible. (we may add functions to sio.Stream to make it possible in the future).
As a result it is possible to cut the 'std::ifstream fin' variable turning directly the member variable 'is' into a stream (not a stream reference).

I made an effort to keep read_csv_line() untouched. That is the function where the value is. (the parser state machine where the programmer thinking and probably debugging effort was put).

Note how the public functions are heavily impacted but the private implementation is barely touched. This allows you to assess the cost of importing a library:

Libraries with a thin interface and a lot of code under the hood are best candidates for porting, while all-interface libraries are better rewritten in sing.

Building a Wrapper  (csvstream_2 project)
-----------------------------------------
We split the src directory into src/app and src/lib. In the latter we want to place the code of the wrapper: csv.cpp. In src/app we will place a simple test app.

csv.cpp needs an implementation header. For that we copy inc/csv.h to cpp_headers/csv.h and add a couple of private members for the implementation. 

To prevent client applications from including the csvstream.h implementation (in this case the library header is lightweight but in other cases it could be huge) I used the 'pointer is implementation' pattern. 

At this point We need to modify the build files to create first a library and then link it to the demo app.

This is done adding the variable lib_target to all the build_platform_configuration.ninja files and adding to build_aux.ninja:

~~~
#library 
build $lib_target : aa csv.o
~~~

Note that the rule to build csv.o can be generated automatically by the sing updater. Also, you must be sure that $lib_target is included in the target application instead of csv.o.

~~~
#link
build $bin_target: ln app.o main.o $lib_target
~~~

At last please note that I made a small modification to the original code to return an empty vector when the end of the file is reached.


Using the Library
-----------------
Before trying to build (or even to make the following modifications) be sure to run the sing updater so that all the build rules for obj files are in place.

We must add to the app project:

- The sing_headers path in the sing search paths.

For the compiler, adding "-I ../../sing_headers" to the sc rule command line, in build/build_aux.ninja.

For the intellisense system, adding the path in .vscode/sing_sense.txt. NOTE: this takes effect only the next time you open the folder in visual studio code. (so you may need to close and open it again).

- The csv.h include directory in the gcc path. 

This is done in csvstream_2 (in csvstream_1 the src directory is included yet) adding "-I ../../cpp_headers" to the cc_flags variable.

- The library file (if any) to the link rule of the application

You can add the library name ($lib_target for csvstream_2) to the ln_libs variable or to the dependencies list in the app link rule. If you choose the latter (in this case you should), build updates/creates the library before building the app. 

Here we are: compile and run the projects to see a perfectly working csv parser.
