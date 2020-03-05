#! /bin/bash 

wasm3 --func fib fib32.wasm 30


while : ; do echo "${MESSAGE=Idling...}"; sleep ${INTERVAL=600}; done