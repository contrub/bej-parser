/**
* @file cli_decode.h
 * @brief command-line interface for decoding BEJ to JSON
 *
 * This file provides the function to run the BEJ decoding process
 * from command-line arguments
 */

#ifndef CLI_DECODE_H
#define CLI_DECODE_H

#include "cli_args.h"

/**
 * @brief runs the BEJ decoding process
 * * This function opens the input BEJ file, loads the necessary dictionaries,
 * decodes the data into a JSON tree, and writes the result to the
 * specified output file or to stdout
 *
 * @param args a pointer to a populated args_t structure with all required paths
 * @return 0 on success, or a non-zero value on failure
 */
int cli_run_decode(const args_t *args);

#endif // CLI_DECODE_H
