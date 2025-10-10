/**
 * @file bej_decode.c
 * @brief implementation of the BEJ (Binary Encoded JSON) decoder
 *
 * This file contains the internal (static) functions to parse a BEJ binary stream
 * into a JSON text stream, using a schema dictionary to interpret the data
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "bej_decode.h"
#include "bej_dictionary.h"
#include "json.h"

// Prototypes for static functions
static int decode_value(FILE* out, FILE* in,
                        const bej_dictionary_t* schema_dict,
                        const bej_dictionary_t* annot_dict,
                        const bej_dict_entry_t* entry,
                        uint64_t length);
static int bej_decode_stream_internal(FILE* output_stream, FILE* input_stream,
                                      const bej_dictionary_t* schema_dict,
                                      const bej_dictionary_t* annot_dict,
                                      const bej_dictionary_t* current_dict,
                                      uint16_t child_ptr, uint16_t child_count,
                                      uint64_t prop_count,
                                      int add_name);

/**
 * @brief unpacks a non-negative integer (nnint) from the stream
 * @param stream the input stream to read from
 * @param value pointer to store the unpacked 64-bit value
 * @return 1 on success, 0 on failure
 */
static int unpack_nnint(FILE* stream, uint64_t* value) {
    uint8_t num_bytes;
    if (fread(&num_bytes, 1, 1, stream) != 1 || num_bytes > 8) return 0;
    uint8_t bytes[8] = {0};
    if (num_bytes > 0 && fread(bytes, 1, num_bytes, stream) != num_bytes) return 0;
    *value = 0;
    for (int i = 0; i < num_bytes; i++) {
        *value |= ((uint64_t)bytes[i] << (8 * i));
    }
    return 1;
}

/**
 * @brief unpacks an SFL (Sequence, Format, Length) header from the stream
 * @param stream the input stream to read from
 * @param seq pointer to store the full sequence number (with selector)
 * @param format pointer to store the format code
 * @param length pointer to store the payload length
 * @return 1 on success, 0 on failure
 */
static int unpack_sfl(FILE* stream, uint64_t* seq, uint8_t* format, uint64_t* length) {
    if (!unpack_nnint(stream, seq)) return 0;
    uint8_t format_and_flags;
    if (fread(&format_and_flags, 1, 1, stream) != 1) return 0;
    *format = format_and_flags >> 4;
    if (!unpack_nnint(stream, length)) return 0;
    return 1;
}

/**
 * @brief decodes a full sequence number into its sequence part and selector bit
 * @param seq the full sequence number from the SFL header
 * @param sequence_num pointer to store the sequence number part
 * @param selector pointer to store the selector bit (0 or 1)
 */
static void decode_sequence_number(const uint64_t seq, uint64_t* sequence_num, uint8_t* selector) {
    *sequence_num = seq >> 1;
    *selector = seq & 0x01;
}

/**
 * @brief prints the JSON property name (e.g., "Key":) for a given dictionary entry
 * @param entry the dictionary entry whose name to print
 * @param output_stream the stream to write to
 */
static void decode_name(const bej_dict_entry_t* entry, FILE* output_stream) {
    if (entry->name) {
        fprintf(output_stream, "\"%s\":", entry->name);
    }
}

/**
 * @brief finds a dictionary entry by its sequence number within a specific subset of a dictionary
 * @param dict the dictionary to search in
 * @param child_ptr the starting index of the subset to search
 * @param child_count the number of entries in the subset
 * @param seq the sequence number to find
 * @param entry pointer to store the found dictionary entry
 * @return 1 on success, 0 if not found
 */
static int get_entry_by_seq(const bej_dictionary_t* dict, const uint16_t child_ptr,
                            const uint16_t child_count, const uint64_t seq, bej_dict_entry_t* entry) {
    if (dict == NULL) return 0;
    bej_dict_stream_t subset_stream;
    bej_dict_stream_init_subset(&subset_stream, dict, child_ptr, child_count);
    while (bej_dict_stream_next(&subset_stream, entry)) {
        if (entry->sequence == seq) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief unpacks an Integer payload from the stream and prints it as a JSON number
 * @param out the output stream to write the JSON number to
 * @param in the input stream to read the BEJ payload from
 * @return 1 on success, 0 on failure
 */
static int unpack_integer_value(FILE* out, FILE* in) {
    uint64_t val_len;
    if (!unpack_nnint(in, &val_len)) return 0;

    int64_t value = 0;
    uint8_t bytes[8];
    if (val_len == 0 || val_len > 8 || fread(bytes, 1, val_len, in) != val_len) return 0;

    for (uint64_t i = 0; i < val_len; i++) {
        value |= ((uint64_t)bytes[i] << (8 * i));
    }
    // sign-extend if negative
    if (val_len < sizeof(int64_t) && (bytes[val_len - 1] & 0x80)) {
        const uint64_t shift = (sizeof(int64_t) - val_len) * 8;
        value = (value << shift) >> shift;
    }
    fprintf(out, "%" PRId64, value);
    return 1;
}

/**
 * @brief unpacks a String payload from the stream and prints it as a JSON string
 * @param out the output stream to write the JSON string to
 * @param in the input stream to read the BEJ payload from
 * @return 1 on success, 0 on failure
 */
static int unpack_string_value(FILE* out, FILE* in) {
    uint64_t str_len;
    if (!unpack_nnint(in, &str_len)) return 0;

    if (str_len == 0) {
        fprintf(out, "\"\"");
        return 1;
    }
    char* buf = malloc(str_len);
    if (!buf) return 0;
    if (fread(buf, 1, str_len, in) != str_len) {
        free(buf);
        return 0;
    }
    buf[str_len - 1] = '\0'; // bej strings are null-terminated
    fprintf(out, "\"%s\"", buf);
    free(buf);
    return 1;
}

/**
 * @brief unpacks a Boolean payload from the stream and prints it as a JSON boolean
 * @param out the output stream to write the JSON boolean to
 * @param in the input stream to read the BEJ payload from
 * @return 1 on success, 0 on failure
 */
static int unpack_boolean_value(FILE* out, FILE* in) {
    uint64_t bool_len;
    if (!unpack_nnint(in, &bool_len) || bool_len != 1) return 0;
    uint8_t b;
    if (fread(&b, 1, 1, in) != 1) return 0;
    fprintf(out, b ? "true" : "false");
    return 1;
}

/**
 * @brief unpacks an Enum payload from the stream and prints it as a JSON string
 * @param out the output stream to write the JSON string to
 * @param in the input stream to read the BEJ payload from
 * @param dict the dictionary to find the enum value in
 * @param entry the dictionary entry for this Enum property
 * @return 1 on success, 0 on failure
 */
static int unpack_enum_value(FILE* out, FILE* in,
                             const bej_dictionary_t* dict,
                             const bej_dict_entry_t* entry) {
    uint64_t len, enum_val;
    if (!unpack_nnint(in, &len) || !unpack_nnint(in, &enum_val)) return 0;

    bej_dict_stream_t stream;
    bej_dict_stream_init_subset(&stream, dict, entry->child_pointer, entry->child_count);
    bej_dict_entry_t enum_entry;
    while (bej_dict_stream_next(&stream, &enum_entry)) {
        if (enum_entry.sequence == enum_val) {
            fprintf(out, "\"%s\"", enum_entry.name);
            return 1;
        }
    }
    return 0;
}

/**
 * @brief decodes an Array payload. Finds the element type and decodes each element in a loop
 * @param out the output stream
 * @param in the input stream containing the array payload
 * @param schema_dict the main schema dictionary
 * @param annot_dict the annotation dictionary
 * @param entry the dictionary entry for this Array property
 * @return 1 on success, 0 on failure
 */
static int decode_array(FILE* out, FILE* in,
                        const bej_dictionary_t* schema_dict,
                        const bej_dictionary_t* annot_dict,
                        const bej_dict_entry_t* entry) {
    uint64_t count;
    if (!unpack_nnint(in, &count)) return 0;
    fprintf(out, "[");

    const bej_dictionary_t* dict_for_children = (entry->name[0] == '@') ? annot_dict : schema_dict;

    bej_dict_stream_t subset_stream;
    bej_dict_stream_init_subset(&subset_stream, dict_for_children,
                                entry->child_pointer, entry->child_count);
    bej_dict_entry_t element_entry;
    if (!bej_dict_stream_next(&subset_stream, &element_entry)) {
        fprintf(out, "]");
        return 1; // array with no element type definition
    }

    // decode each element in the array
    for (uint64_t i = 0; i < count; i++) {
        uint64_t elem_seq, elem_len;
        uint8_t elem_fmt;
        if (unpack_sfl(in, &elem_seq, &elem_fmt, &elem_len) == 0) return 0;

        decode_value(out, in, schema_dict, annot_dict, &element_entry, elem_len);
        if (i < count - 1) fprintf(out, ",");
    }

    fprintf(out, "]");
    return 1;
}

/**
 * @brief decodes a Set (object) payload by recursively calling the main stream decoder for its properties
 * @param out the output stream
 * @param in the input stream containing the set payload
 * @param schema_dict the main schema dictionary
 * @param annot_dict the annotation dictionary
 * @param entry the dictionary entry for this Set property
 * @return 1 on success, 0 on failure
 */
static int decode_set(FILE* out, FILE* in,
                      const bej_dictionary_t* schema_dict,
                      const bej_dictionary_t* annot_dict,
                      const bej_dict_entry_t* entry) {
    uint64_t count;
    if (!unpack_nnint(in, &count)) return 0;
    fprintf(out, "{");

    const bej_dictionary_t* dict_for_children = (entry->name[0] == '@') ? annot_dict : schema_dict;

    if (count > 0) {
        // recursively decode the inner properties
        bej_decode_stream_internal(out, in, schema_dict, annot_dict,
                                   dict_for_children, entry->child_pointer, entry->child_count,
                                   count, 1);
    }

    fprintf(out, "}");
    return 1;
}

/**
 * @brief the central dispatcher function. Decodes a single property's value based on its format
 * @param out the output stream
 * @param in the input stream, positioned at the start of the property's payload
 * @param schema_dict the main schema dictionary
 * @param annot_dict the annotation dictionary
 * @param entry the dictionary entry for the property being decoded
 * @param length the total length of the payload to be consumed
 * @return 1 on success, 0 on failure
 */
static int decode_value(FILE* out, FILE* in,
                        const bej_dictionary_t* schema_dict,
                        const bej_dictionary_t* annot_dict,
                        const bej_dict_entry_t* entry,
                        const uint64_t length) {
    switch (entry->format) {
    case BEJ_FORMAT_SET:
        return decode_set(out, in, schema_dict, annot_dict, entry);
    case BEJ_FORMAT_ARRAY:
        return decode_array(out, in, schema_dict, annot_dict, entry);
    case BEJ_FORMAT_INTEGER:
        return unpack_integer_value(out, in);
    case BEJ_FORMAT_STRING:
        return unpack_string_value(out, in);
    case BEJ_FORMAT_BOOLEAN:
        return unpack_boolean_value(out, in);
    case BEJ_FORMAT_ENUM:
        {
            const bej_dictionary_t* dict_to_use = (entry->name[0] == '@') ? annot_dict : schema_dict;
            return unpack_enum_value(out, in, dict_to_use, entry);
        }
    case BEJ_FORMAT_NULL:
        fprintf(out, "null");
        return 1;
    default:
        fseek(in, length, SEEK_CUR); // skip unknown types
        return 1;
    }
}

/**
 * @brief the main recursive function. Decodes a sequence of properties within a Set
 * @param output_stream the main output stream
 * @param input_stream the main input stream
 * @param schema_dict the main schema dictionary
 * @param annot_dict the annotation dictionary
 * @param current_dict the dictionary for the current context (either schema_dict or annot_dict)
 * @param child_ptr the starting index for the property lookup in the current context
 * @param child_count the number of properties in the current context
 * @param prop_count the number of properties to decode from the stream
 * @param add_name flag indicating whether to print property names (true for objects, false for arrays)
 * @return 1 on success, 0 on failure
 */
static int bej_decode_stream_internal(FILE* output_stream, FILE* input_stream,
                                      const bej_dictionary_t* schema_dict,
                                      const bej_dictionary_t* annot_dict,
                                      const bej_dictionary_t* current_dict,
                                      const uint16_t child_ptr,
                                      const uint16_t child_count,
                                      const uint64_t prop_count,
                                      const int add_name) {
    for (uint64_t i = 0; i < prop_count; i++) {
        uint64_t seq, length;
        uint8_t format;
        if (unpack_sfl(input_stream, &seq, &format, &length) == 0) return 0;

        uint64_t seq_num;
        uint8_t selector;
        decode_sequence_number(seq, &seq_num, &selector);

        bej_dict_entry_t entry;
        if (selector == 0) { // search in schema context
            if (!get_entry_by_seq(current_dict, child_ptr, child_count, seq_num, &entry)) return 0;
        } else { // search in annotation dictionary (globally)
            if (!get_entry_by_seq(annot_dict, 0, annot_dict->size, seq_num, &entry)) return 0;
        }

        if (add_name) {
            decode_name(&entry, output_stream);
        }

        if (!decode_value(output_stream, input_stream, schema_dict, annot_dict, &entry, length)) return 0;

        if (i < prop_count - 1) {
            fprintf(output_stream, ",");
        }
    }
    return 1;
}

int bej_decode_stream(FILE* output_stream, FILE* input_stream,
                      const bej_dictionary_t* schema_dict,
                      const bej_dictionary_t* annot_dict) {
    if (!output_stream || !input_stream || !schema_dict || !annot_dict) return 0;

    // skip 7-byte BEJ header
    fseek(input_stream, 7, SEEK_SET);

    // get the root entry from the dictionary
    bej_dict_stream_t ds;
    bej_dict_stream_init(&ds, schema_dict);
    bej_dict_entry_t root_entry;
    if (!bej_dict_stream_next(&ds, &root_entry)) return 0;

    // the entire payload is one large SET, so we call decode_value for it
    uint64_t seq, length;
    uint8_t format;
    if (unpack_sfl(input_stream, &seq, &format, &length) == 0 || format != BEJ_FORMAT_SET) return 0;

    root_entry.format = BEJ_FORMAT_SET;
    return decode_value(output_stream, input_stream, schema_dict, annot_dict, &root_entry, length);
}

json_value_t* bej_decode_buffer(const uint8_t* data, const size_t size,
                                const bej_dictionary_t* schema_dict,
                                const bej_dictionary_t* annot_dict) {
    // this function is a convenient wrapper around bej_decode_stream
    FILE* input_stream = fmemopen((void*)data, size, "r");
    if (!input_stream) return NULL;

    FILE* output_stream = tmpfile();
    if (!output_stream) {
        fclose(input_stream);
        return NULL;
    }

    const int success = bej_decode_stream(output_stream, input_stream, schema_dict, annot_dict);

    json_value_t* result = NULL;
    if (success) {
        fseek(output_stream, 0, SEEK_END);
        const long json_size = ftell(output_stream);
        rewind(output_stream);
        if (json_size > 0) {
            char* json_str = malloc(json_size + 1);
            if (json_str) {
                if (fread(json_str, 1, json_size, output_stream) == (size_t)json_size) {
                    json_str[json_size] = '\0';
                    result = json_parse(json_str);
                }
                free(json_str);
            }
        }
    }

    fclose(input_stream);
    fclose(output_stream);
    return result;
}
