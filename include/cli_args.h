/**
* @file cli_args.h
 * @brief command-line argument parsing
 *
 * This header defines the structures and functions for parsing command-line
 * arguments for the BEJ encoder and decoder
 */

#ifndef CLI_ARGS_H
#define CLI_ARGS_H

#include <stdbool.h>

/**
 * @brief structure holding parsed command-line arguments
 */
typedef struct {
    const char *input_path;    /**< path to input file (JSON or BEJ) */
    const char *schema_path;   /**< path to schema dictionary (required) */
    const char *annot_path;    /**< path to annotation dictionary (optional) */
    const char *output_path;   /**< path to output file (or NULL for stdout) */
    int mode_encode;           /**< operation mode: 1 = encode, 0 = decode */
} args_t;

/**
 * @brief parses command-line arguments into an args_t structure
 *
 * This function handles the following arguments:
 * - `encode` or `decode` to set the operation mode
 * - `<input-file>` for the source file (JSON or BEJ)
 * - `-s <schema>` for the required schema dictionary
 * - `-a <annotation>` for the optional annotation dictionary
 * - `-o <output>` for the output file
 *
 * @param argc the number of command-line arguments
 * @param argv the array of command-line arguments
 * @param out a pointer to the args_t struct to be filled
 * @return 0 on success, -1 on error
 */
int parse_args(int argc, char **argv, args_t *out);

#endif // CLI_ARGS_H
