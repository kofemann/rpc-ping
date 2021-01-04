# rpc-ping

A simple application to test SunRPC service response times

## How-to Compile

```
$ gcc -g -Wall -pedantic rpc-ping-mt.c -lpthread -I /usr/include/tirpc -ltirpc -o rpc-ping
```

## Simple test report

```
$ ./rpc-ping 127.0.0.1 1717 100017 1 16 10                                                                                          (git)-[master] 
Speed:  77369.44 rps in 20.68s (3.3088 ms per request), 1600000 in total
Speed:  85653.10 rps in 18.68s (2.9888 ms per request), 1600000 in total
Speed:  84700.90 rps in 18.89s (3.0224 ms per request), 1600000 in total
Speed:  79090.46 rps in 20.23s (3.2368 ms per request), 1600000 in total
Speed:  68201.19 rps in 23.46s (3.7536 ms per request), 1600000 in total
Speed:  72202.17 rps in 22.16s (3.5456 ms per request), 1600000 in total
Speed:  65173.12 rps in 24.55s (3.9280 ms per request), 1600000 in total
Speed:  65412.92 rps in 24.46s (3.9136 ms per request), 1600000 in total
Speed:  74661.69 rps in 21.43s (3.4288 ms per request), 1600000 in total
Speed:  70546.74 rps in 22.68s (3.6288 ms per request), 1600000 in total
```

## License

Public Domain.
