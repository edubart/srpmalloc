# srpmalloc - Small rpmalloc

This is a fork of [rpmalloc](https://github.com/mjansson/rpmalloc), with the intent to be
used in single threaded applications only, with old C99 compilers, and in old OS.

Summary of the changes:

* Works with single thread applications only
* Works with C99 compilers (C11 is not a required anymore)
* Remove thread safety
* Remove atomics usage
* Remove use of thread locals
* Remove statistics APIs
* Remove first class heaps APIs
* Remove APIs validations
* Remove huge page support
* Remove global cache support
* Remove malloc override support

By removing all this, it's much smaller, and works in old C compilers and Linux
distributions, it was confirmed to work for example in Debian 4 with GCC 4.1,
the original rpmalloc does not support many old Debian distributions due to use
of C11 atomics.
