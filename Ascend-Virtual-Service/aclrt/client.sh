export CRONUS_PORT=8625
export CRONUS_CHECKPOINT_REG_PORT=6625
export CCS_JOB_ID=0
export RANK_ID=0

export ASCEND_RT_VISIBLE_DEVICES=7

taskname=$1
case "$taskname" in 
    vllm)
        export LD_LIBRARY_PATH=./:$LD_LIBRARY_PATH
        vllm bench throughput --tensor-parallel-size 1 --gpu-memory-utilization=0.9 --model  /workplace/models/DeepSeek-R1-Distill-Llama-8B --num-prompts 2048 --max-num-seqs 64 --input-len 2560 --output-len 1 --seed 42 --max-model-len 5120
esac
