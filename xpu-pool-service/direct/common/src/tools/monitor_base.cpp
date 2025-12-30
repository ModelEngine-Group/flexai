/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <iostream>
#include <getopt.h>
#include "log.h"
#include "tools/monitor_base.h"

using namespace std;

namespace xpu {

static void Usage()
{
    cerr << "Usage: xpu-monitor [option [value]]\n";
    cerr << "\n";
    cerr << "Valid options:\n";
    cerr << "  -p,--period    The time period in seconds used to calculate computing power\n";
    cerr << "                 range 1 ~ 86400, default 60 (1 minute)\n";
    cerr << "  -o,--output    Output format, one of: json|table\n";
    cerr << "  -h,--help      Print usage information\n";
    cerr << endl;
}

int ParseArgs(Args &args, int argc, char *const argv[])
{
    int nextOption;
    int tmpValue;
    const char *const shortOptions = "o:p:h";
    const struct option longOptions[] = {
        {"output", 1, nullptr, 'o'},
        {"period", 1, nullptr, 'p'},
        {"help", 0, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };

    while ((nextOption = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) != -1) {
        switch (nextOption) {
            case 'o': 
                if (optarg == string_view("json")) {
                    args.format = OutputFormat::JSON;
                } else if (optarg == string_view("table")) {
                    args.format = OutputFormat::TABLE;
                } else {
                    cerr << "format value is invalid: " << optarg << ", the value must be json or table\n";
                    Usage();
                    return RET_FAIL;
                }
                break;
            case 'p': 
                tmpValue = atoi(optarg);
                if (tmpValue >= PERIOD_MIN && tmpValue <= PERIOD_MAX) {
                    args.period = tmpValue;
                } else {
                    cerr << "option value is invalid: " << optarg << ", the value must range in 1 ~ 86400\n";
                    Usage();
                    return RET_FAIL;
                }
                break;
            case 'h':
            default:
                Usage();
                return RET_FAIL;
        }
    }
    return RET_SUCC;
}
}
