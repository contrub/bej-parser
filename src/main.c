#include "cli_encode.h"
#include "cli_decode.h"
#include "cli_args.h"

int main(int argc, char **argv) {
    args_t args;

    if (parse_args(argc, argv, &args) != 0)
        return 1;

    if (args.mode_encode)
        return cli_run_encode(&args);

    return cli_run_decode(&args);
}
