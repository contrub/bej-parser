/**
* @file bej_encode.h
 * @brief API for the BEJ (Binary Encoded JSON) encoder
 *
 * This file declares the public function for encoding JSON values into the BEJ
 * binary format using a schema dictionary
 */

#ifndef BEJ_PARSER_BEJ_ENCODE_H
#define BEJ_PARSER_BEJ_ENCODE_H

#include <stdio.h>
#include "bej_dictionary.h"
#include "json.h"

/**
 * @brief encodes a JSON value tree into the BEJ binary format and writes it to a stream
 *
 * @param output_stream the output file stream to write BEJ data to
 * @param json_data     the root JSON value to be encoded
 * @param schema_dict   the schema dictionary used for encoding
 * @param annot_dict    the annotation dictionary used for encoding metadata
 * @return 1 on success, 0 on failure
 */
int bej_encode_stream(FILE* output_stream, const json_value_t* json_data,
                      const bej_dictionary_t* schema_dict,
                      const bej_dictionary_t* annot_dict);

#endif //BEJ_PARSER_BEJ_ENCODE_H
