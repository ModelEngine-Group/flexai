import logging
import os
import sys


current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)

from workload_sender import TRACE_30, run_trace


logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")


def main():
    cluster_trace = list(TRACE_30)
    cluster_trace[22] = (1019, 2, 0, "vgg16", 0, 32, 0, 0.05, 1)
    run_trace(cluster_trace, "/home/djh/dataset/ImageNet-1K")


if __name__ == "__main__":
    main()
