#!/bin/bash

pids=()

for i in 1 2 3 4; do
    for y in 1 2 3 4; do
        ./zappy_ai -p 4242 -n team$i -f 50 -v &
        pids+=($!)
    done
done

echo "Launched PIDs: ${pids[@]}"

# wait for Ctrl+C or manual kill
trap 'echo "Killing all zappy_ia..."; kill "${pids[@]}" 2>/dev/null' EXIT

wait
