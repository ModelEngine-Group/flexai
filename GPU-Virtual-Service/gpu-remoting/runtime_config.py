import json
from functools import lru_cache
from pathlib import Path

import redis


GPU_REMOTING_ROOT = Path(__file__).resolve().parent
CONFIG_PATH = GPU_REMOTING_ROOT / "config.json"
SCHEDULER_DIR = GPU_REMOTING_ROOT / "scheduler"
JOB_INFO_PATH = SCHEDULER_DIR / "job_info.csv"


@lru_cache(maxsize=1)
def load_runtime_config():
    with CONFIG_PATH.open("r", encoding="utf-8") as config_file:
        return json.load(config_file)


def get_job_info_path():
    return JOB_INFO_PATH


def create_redis_connection(redis_config=None, db_key="db"):
    config = redis_config or load_runtime_config()["RedisConfig"]
    db = config.get(db_key, config["db"])
    return redis.Redis(
        host=config["host"],
        port=config["port"],
        db=db,
        password=config["password"],
    )
