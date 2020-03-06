#! /bin/bash 

wasm3 --func fib fib32.wasm 30

wasm3 --func add as_demo.wasm 20 30


while : ; do echo "${MESSAGE=Idling...}"; sleep ${INTERVAL=600}; done