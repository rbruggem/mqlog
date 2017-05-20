## mqlog is a library that provides an embeddable durable queue implementend as an immutable log.

[![Build Status](https://travis-ci.org/rbruggem/mqlog.svg?branch=master)](https://travis-ci.org/rbruggem/mqlog)  [![Coverage Status](https://coveralls.io/repos/github/rbruggem/mqlog/badge.svg)](https://coveralls.io/github/rbruggem/mqlog)

#### This library is NOT production ready

To compile:

`make`

To run tests:

`make test`

To generate test code coverage (requires `lcov` package)

```
make clean
make gcov
make test
make lcov
```
Now open lcov-html/index.html with your favourite browser
