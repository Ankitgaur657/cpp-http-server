#!/bin/bash


START=$(date +%s%N)


SUCCESS=0


for i in {1..100}
do

curl -s localhost:8080/index.html > /dev/null &

done


wait


END=$(date +%s%N)



TIME=$(( (END-START)/1000000 ))



echo "Load test completed"

echo "Total requests: 100"

echo "Time taken: ${TIME} ms"