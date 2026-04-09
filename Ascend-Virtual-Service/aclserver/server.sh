NR_SERVERS=$1
export CRONUS_BIND_DEVICE=0
export CRONUS_PORT_BASE=8625
export CRONUS_DEVPTR_MEMORY_SIZE=30G
export CRONUS_RANK_ID=0

export ASCEND_RT_VISIBLE_DEVICES=7

for ((i=0;i<$NR_SERVERS;i++)); do
    echo "create server_$i"
    CRONUS_PORT=$(($CRONUS_PORT_BASE+$i)) ./aclserver_standalone $i > log_$i 2>&1 &
done

wait

