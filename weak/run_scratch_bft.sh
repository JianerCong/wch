w=./build-weak/wch

$w --port 7777 --mock-exe --consensus Rbft --Bft.node-list localhost:7777
# Start as the single primary in bft cluster
