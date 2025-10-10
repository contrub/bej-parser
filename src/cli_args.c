/**
* @file cli_args.c
 * @brief implementation of the command-line argument parser
 */

#include <string.h>
#include <stdio.h>

#include "cli_args.h"

// this function is documented in the header file (cli_args.h)
int parse_args(const int argc, char **argv, args_t *out) {
    // initialize the output structure to zero
    memset(out, 0, sizeof(*out));
    out->mode_encode = -1; // use -1 to indicate mode is not set

    // iterate over all command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            out->schema_path = argv[++i];
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            out->annot_path = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out->output_path = argv[++i];
        } else if (strcmp(argv[i], "encode") == 0) {
            out->mode_encode = 1;
        } else if (strcmp(argv[i], "decode") == 0) {
            out->mode_encode = 0;
        } else if (argv[i][0] != '-') {
            // assume any argument not starting with '-' is the input file
            if (!out->input_path) {
                out->input_path = argv[i];
            } else {
                fprintf(stderr, "error: multiple input files specified ('%s' and '%s')\n", out->input_path, argv[i]);
                return -1;
            }
        } else {
            fprintf(stderr, "error: unknown argument: %s\n", argv[i]);
            return -1;
        }
    }

    // validate that all required arguments were provided
    if (out->mode_encode == -1 || !out->input_path || !out->schema_path) {
        fprintf(stderr, "Usage:\n"
                        " - bej_parser encode <json-file> -s <schema> [-a <annotation>] [-o <output>]\n"
                        " - bej_parser decode <bej-file>  -s <schema> [-a <annotation>] [-o <output>]\n");
        return -1;
    }

    return 0;
}
