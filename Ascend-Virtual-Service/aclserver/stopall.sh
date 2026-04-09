pkill -9 vllm
pkill -9 aclserver

ls /dev/shm/ | grep -E 'slot-shm-buffer|cronus|sem\.mp' | xargs -I {} rm -f /dev/shm/{}
rm -rf log_*