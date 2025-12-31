curl -X POST http://127.0.0.1:8080/device/register \
     -H "Content-Type: application/json" \
     -d '{
           "flexnpu_id": "0",
           "entity": "xxx",
           "npu": 7,
           "chip": 0,
           "aicore_alloc": 2,
           "memory_alloc": 16384
         }'

curl -X POST http://127.0.0.1:8080/device/register \
     -H "Content-Type: application/json" \
     -d '{
           "flexnpu_id": "1",
           "entity": "xxx",
           "npu": 7,
           "chip": 0,
           "aicore_alloc": 2,
           "memory_alloc": 16384
         }'
curl -X POST http://127.0.0.1:8080/device/register \
     -H "Content-Type: application/json" \
     -d '{
           "flexnpu_id": "2",
           "entity": "xxx",
           "npu": 7,
           "chip": 0,
           "aicore_alloc": 8,
           "memory_alloc": 16384
         }'

curl -X POST http://127.0.0.1:8080/device/register \
     -H "Content-Type: application/json" \
     -d '{
           "flexnpu_id": "3",
           "entity": "xxx",
           "npu": 7,
           "chip": 0,
           "aicore_alloc": 8,
           "memory_alloc": 16384
         }'

