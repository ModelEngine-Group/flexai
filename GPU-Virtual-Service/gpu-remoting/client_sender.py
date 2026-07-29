import logging

from workload_sender import TRACE_30, run_trace


logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")


def main():
    run_trace(TRACE_30, "/mnt/nvme0/FlexGV_Test/ImageNet-1K")


if __name__ == "__main__":
    main()
