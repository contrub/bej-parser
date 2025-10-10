/**
* @file cli_encode.h
 * @brief command-line interface for encoding JSON to BEJ
 *
 * This file provides the function to run the JSON to BEJ encoding process
 * from command-line arguments
 */

#ifndef CLI_ENCODE_H
#define CLI_ENCODE_H

#include "cli_args.h"

/**
 * @brief runs the BEJ encoding process
 *
 * This function opens the input JSON file, parses it, loads the necessary
 * dictionaries, encodes the data into BEJ format, and writes the result
 * to the specified output file or to stdout
 *
 * @param args a pointer to a populated args_t structure with all required paths
 * @return 0 on success, or a non-zero value on failure
 */
int cli_run_encode(const args_t *args);

#endif // CLI_ENCODE_H
