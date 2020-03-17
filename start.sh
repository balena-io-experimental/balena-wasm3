#! /bin/bash 

wasm3 --func fib fib32.wasm 30

wasm3 --func add as_demo.wasm 20 30

# run standalone binary with value 33 and store location 9(arbitrary)
./increment 33 9 # expected 34

# call the start function -> idle
wasm3 as_demo.wasm



while : ; do echo "${MESSAGE=Idling...}"; sleep ${INTERVAL=600}; done