/**
* @file bej_decode.h
 * @brief API for the BEJ (Binary Encoded JSON) decoder
 *
 * This file declares the public functions for decoding a BEJ binary stream
 * or buffer into a JSON representation
 */

#ifndef BEJ_PARSER_BEJ_DECODE_H
#define BEJ_PARSER_BEJ_DECODE_H

#include <stdint.h>
#include <stdio.h> // For FILE*
#include "bej_dictionary.h"
#include "json.h"

/**
 * @brief decodes a BEJ stream into a textual JSON stream
 * @param output_stream the output stream to write JSON text to
 * @param input_stream the input stream containing BEJ data
 * @param schema_dict the main schema dictionary
 * @param annot_dict the annotation dictionary
 * @return 1 on success, 0 on failure
 */
int bej_decode_stream(FILE* output_stream, FILE* input_stream,
                      const bej_dictionary_t* schema_dict,
                      const bej_dictionary_t* annot_dict);

/**
 * @brief decodes a BEJ buffer in memory into a parsed JSON object tree
 * @param data pointer to the buffer with BEJ data
 * @param size the size of the buffer
 * @param schema_dict the main schema dictionary
 * @param annot_dict the annotation dictionary
 * @return a pointer to a `json_value_t` on success, or NULL on failure
 */
json_value_t* bej_decode_buffer(const uint8_t* data, size_t size,
                                const bej_dictionary_t* schema_dict,
                                const bej_dictionary_t* annot_dict);

#endif // BEJ_PARSER_BEJ_DECODE_H
