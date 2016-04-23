##### FIFO brokerless log-like data structure library inspired by: 

* Apache Kafka
* Aeron
* LMAX Disruptor
* Write ahead logs


To compile:

`make`

To run tests (after having compiled):

```
cd test
make run
```

To generate test code coverage (requires `lcov` package)

```
make clean
make gcov
make lcov
```
Now open lcov-html/index.html with your favourite browser
