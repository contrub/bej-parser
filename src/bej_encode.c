/**
 * @file bej_encode.c
 * @brief Implements the BEJ (Binary Encoded JSON) encoder.
 *
 * This file contains the internal (static) functions for encoding a JSON tree
 * into the BEJ binary format, according to a schema dictionary.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "bej_encode.h"
#include "bej_dictionary.h"
#include "json.h"

// Prototypes for static functions
static int encode_properties(FILE* out, const json_value_t* json_object,
                             const bej_dict_entry_t* parent_entry,
                             const bej_dictionary_t* schema_dict,
                             const bej_dictionary_t* annot_dict);

static int encode_value(FILE* out, const bej_dict_entry_t* entry,
                        uint8_t selector, const json_value_t* json_value,
                        const bej_dictionary_t* schema_dict,
                        const bej_dictionary_t* annot_dict);

/**
 * @brief packs a 64-bit unsigned integer into the nnint format
 * @param out the output stream
 * @param value the value to pack
 */
static void pack_nnint(FILE* out, const uint64_t value) {
    uint8_t tmp[9];
    size_t n = 0;
    if (value == 0) {
        tmp[0] = 1; tmp[1] = 0;
        fwrite(tmp, 1, 2, out);
        return;
    }
    uint64_t v = value;
    while (v) {
        tmp[++n] = (uint8_t)(v & 0xFF);
        v >>= 8;
    }
    tmp[0] = (uint8_t)n;
    fwrite(tmp, 1, n + 1, out);
}

/**
 * @brief packs an SFL (Sequence, Format, Length) header
 * @param out the output stream
 * @param seq_with_selector the sequence number combined with the selector bit
 * @param format the format code (BEJ_FORMAT_*)
 * @param len the length of the payload (value)
 */
static void pack_sfl(FILE* out,
                     const uint64_t seq_with_selector,
                     const uint8_t format,
                     const uint64_t len) {
    pack_nnint(out, seq_with_selector);
    const uint8_t fmt = (uint8_t)(format << 4);
    fwrite(&fmt, 1, 1, out);
    pack_nnint(out, len);
}

/**
 * @brief packs the payload for an Integer value
 * @param out the output stream
 * @param value the value to pack
 * @return 1 on success
 */
static int pack_integer_value(FILE* out, const int64_t value) {
    uint8_t buf[8];
    const uint64_t u = (uint64_t)value;
    for (int i = 0; i < 8; ++i) {
        buf[i] = (uint8_t)(u >> (8 * i));
    }
    int num = 8;
    while (num > 1) {
        const uint8_t msb_next = buf[num - 1];
        const uint8_t msb = buf[num - 2];
        if ((value >= 0 && msb_next == 0x00 && (msb & 0x80) == 0) ||
            (value < 0 && msb_next == 0xFF && (msb & 0x80) != 0)) {
            num--;
        } else {
            break;
        }
    }
    pack_nnint(out, num);
    fwrite(buf, 1, (size_t)num, out);
    return 1;
}

/**
 * @brief packs the payload for a String value
 * @param out the output stream
 * @param str the string to pack
 * @return 1 on success
 */
static int pack_string_value(FILE* out, const char* str) {
    const size_t len = strlen(str) + 1; // including null terminator
    pack_nnint(out, len);
    fwrite(str, 1, len, out);
    return 1;
}

/**
 * @brief packs the payload for a Boolean value
 * @param out the output stream
 * @param value the value (0 or 1)
 * @return 1 on success
 */
static int pack_boolean_value(FILE* out, const int value) {
    pack_nnint(out, 1);
    const uint8_t b = value ? 1u : 0u;
    fwrite(&b, 1, 1, out);
    return 1;
}

/**
 * @brief packs the payload for an Enum value
 * @param out the output stream
 * @param dict the dictionary to find the enum value in
 * @param entry the dictionary entry for this Enum property
 * @param enum_name the string name of the enum value
 * @return 1 on success, 0 if the value is not found
 */
static int pack_enum_value(FILE* out,
                           const bej_dictionary_t* dict,
                           const bej_dict_entry_t* entry,
                           const char* enum_name) {
    bej_dict_stream_t st;
    bej_dict_stream_init_subset(&st, dict, entry->child_pointer, entry->child_count);
    bej_dict_entry_t v;
    uint16_t value = 0;
    int found = 0;
    while (bej_dict_stream_next(&st, &v)) {
        if (v.name && strcmp(v.name, enum_name) == 0) {
            value = v.sequence;
            found = 1;
            break;
        }
    }
    if (!found) return 0;

    // enum value length is its nnint representation length
    uint8_t tmp[3]; // uint16_t + length byte
    size_t n = 0;
    if (value == 0) {
        tmp[0] = 1; tmp[1] = 0; n = 2;
    } else {
        uint16_t v_tmp = value;
        tmp[0] = 0; // placeholder
        while (v_tmp) {
            tmp[++n] = (uint8_t)(v_tmp & 0xFF);
            v_tmp >>= 8;
        }
        tmp[0] = (uint8_t)n;
        n++;
    }
    pack_nnint(out, n); // length
    fwrite(tmp, 1, n, out); // value
    return 1;
}

/**
 * @brief encodes the payload for an Array
 * @param out the output stream
 * @param array_entry the dictionary entry for this Array
 * @param json_array the JSON array
 * @param schema_dict the main schema dictionary
 * @param annot_dict the annotation dictionary
 * @return 1 on success, 0 on failure
 */
static int encode_array_payload(FILE* out, const bej_dict_entry_t* array_entry,
                                const json_value_t* json_array,
                                const bej_dictionary_t* schema_dict,
                                const bej_dictionary_t* annot_dict) {
    const bej_dictionary_t* dict_to_use = (array_entry->name[0] == '@') ? annot_dict : schema_dict;

    bej_dict_stream_t st;
    bej_dict_stream_init_subset(&st, dict_to_use, array_entry->child_pointer, array_entry->child_count);
    bej_dict_entry_t element_entry;
    if (!bej_dict_stream_next(&st, &element_entry)) return 0;

    pack_nnint(out, json_array->data.array.count);
    for (size_t i = 0; i < json_array->data.array.count; ++i) {
        uint8_t selector = (array_entry->name[0] == '@') ? 1 : 0;
        element_entry.sequence = i; // seq for array elements is their index
        if (!encode_value(out, &element_entry, selector,
                          json_array->data.array.items[i], schema_dict, annot_dict)) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief encodes the payload for a Set (object)
 * @param out the output stream
 * @param set_entry the dictionary entry for this Set
 * @param json_object the JSON object
 * @param schema_dict the main schema dictionary
 * @param annot_dict the annotation dictionary
 * @return 1 on success, 0 on failure
 */
static int encode_set_payload(FILE* out, const bej_dict_entry_t* set_entry,
                              const json_value_t* json_object,
                              const bej_dictionary_t* schema_dict,
                              const bej_dictionary_t* annot_dict) {
    return encode_properties(out, json_object, set_entry, schema_dict, annot_dict);
}

/**
 * @brief The central dispatcher function. Encodes a single complete property (SFL + value).
 * @param out the output stream.
 * @param entry the dictionary entry for the property to encode.
 * @param selector the dictionary selector (0 for schema, 1 for annotation).
 * @param json_value the JSON value for this property.
 * @param schema_dict the main schema dictionary.
 * @param annot_dict the annotation dictionary.
 * @return 1 on success, 0 on failure.
 */
static int encode_value(FILE* out, const bej_dict_entry_t* entry,
                        uint8_t selector, const json_value_t* json_value,
                        const bej_dictionary_t* schema_dict,
                        const bej_dictionary_t* annot_dict) {
    FILE* tmp_payload = tmpfile();
    if (!tmp_payload) return 0;

    int success = 0;
    switch (entry->format) {
        case BEJ_FORMAT_SET:
            success = encode_set_payload(tmp_payload, entry, json_value, schema_dict, annot_dict);
            break;
        case BEJ_FORMAT_ARRAY:
            success = encode_array_payload(tmp_payload, entry, json_value, schema_dict, annot_dict);
            break;
        case BEJ_FORMAT_INTEGER:
            success = (json_value && json_value->type == JSON_NUMBER) ?
                pack_integer_value(tmp_payload, (int64_t)json_value->data.number) : 0;
            break;
        case BEJ_FORMAT_STRING:
            success = (json_value && json_value->type == JSON_STRING) ?
                pack_string_value(tmp_payload, json_value->data.string) : 0;
            break;
        case BEJ_FORMAT_BOOLEAN:
            success = (json_value && json_value->type == JSON_BOOL) ?
                pack_boolean_value(tmp_payload, json_value->data.boolean) : 0;
            break;
        case BEJ_FORMAT_ENUM: {
            const bej_dictionary_t* dict_to_use = selector ? annot_dict : schema_dict;
            success = (json_value && json_value->type == JSON_STRING) ?
                pack_enum_value(tmp_payload, dict_to_use, entry, json_value->data.string) : 0;
            break;
        }
        case BEJ_FORMAT_NULL:
            success = 1; // payload is empty
            break;
        default:
            success = 0;
    }

    if (!success) {
        fclose(tmp_payload);
        return 0;
    }

    fseek(tmp_payload, 0, SEEK_END);
    const long payload_size = ftell(tmp_payload);
    rewind(tmp_payload);

    const uint64_t seq_with_selector = ((uint64_t)entry->sequence << 1) | selector;
    pack_sfl(out, seq_with_selector, entry->format, payload_size);

    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, tmp_payload)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(tmp_payload);

    return 1;
}

/**
 * @brief the main recursive function. Encodes the payload of a Set (object).
 * @param out the output stream.
 * @param json_object the JSON object to encode.
 * @param parent_entry the dictionary entry for the parent Set.
 * @param schema_dict the main schema dictionary.
 * @param annot_dict the annotation dictionary.
 * @return 1 on success, 0 on failure.
 */
static int encode_properties(FILE* out, const json_value_t* json_object,
                             const bej_dict_entry_t* parent_entry,
                             const bej_dictionary_t* schema_dict,
                             const bej_dictionary_t* annot_dict) {
    if (json_object->type != JSON_OBJECT) return 0;

    size_t prop_count = 0;
    for (size_t i = 0; i < json_object->data.object.count; ++i) {
        const char* key = json_object->data.object.entries[i].key;
        bej_dict_entry_t child;
        const bej_dictionary_t* dict_to_search = (key[0] == '@') ? annot_dict : schema_dict;
        const uint16_t child_ptr = (key[0] == '@') ? 0 : parent_entry->child_pointer;
        const uint16_t child_cnt = (key[0] == '@') ? (annot_dict ? annot_dict->size : 0) : parent_entry->child_count;
        if (dict_to_search && bej_dict_find_child_by_name(dict_to_search, child_ptr, child_cnt, key, &child)) {
            prop_count++;
        }
    }
    pack_nnint(out, prop_count);

    for (size_t i = 0; i < json_object->data.object.count; ++i) {
        const char* key = json_object->data.object.entries[i].key;
        const json_value_t* val = json_object->data.object.entries[i].value;
        bej_dict_entry_t child;
        const uint8_t selector = (key[0] == '@') ? 1 : 0;
        const bej_dictionary_t* dict_to_search = selector ? annot_dict : schema_dict;
        const uint16_t child_ptr = selector ? 0 : parent_entry->child_pointer;
        const uint16_t child_cnt = selector ? (annot_dict ? annot_dict->size : 0) : parent_entry->child_count;

        if (dict_to_search && bej_dict_find_child_by_name(dict_to_search, child_ptr, child_cnt, key, &child)) {
            if (!encode_value(out, &child, selector, val, schema_dict, annot_dict)) {
                return 0;
            }
        }
    }
    return 1;
}

int bej_encode_stream(FILE* output_stream, const json_value_t* json_data,
                      const bej_dictionary_t* schema_dict,
                      const bej_dictionary_t* annot_dict) {
    if (!output_stream || !json_data || !schema_dict) return 0;

    // Standard 7-byte BEJ header:
    // - 4-byte Magic Number (0x00F0F1F1) identifies the file type
    // - 2-byte Flags (reserved)
    // - 1-byte Schema Class (0x00 for Major Schema)
    const uint8_t header[] = {0x00, 0xF0, 0xF1, 0xF1, 0x00, 0x00, 0x00};
    fwrite(header, 1, sizeof(header), output_stream);

    bej_dict_stream_t ds;
    bej_dict_stream_init(&ds, schema_dict);
    bej_dict_entry_t root_entry;
    if (!bej_dict_stream_next(&ds, &root_entry)) return 0;

    FILE* tmp_payload = tmpfile();
    if (!tmp_payload) return 0;

    if (!encode_properties(tmp_payload, json_data, &root_entry, schema_dict, annot_dict)) {
        fclose(tmp_payload);
        return 0;
    }

    fseek(tmp_payload, 0, SEEK_END);
    long payload_size = ftell(tmp_payload);
    rewind(tmp_payload);

    pack_sfl(output_stream, 0, BEJ_FORMAT_SET, payload_size);

    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, tmp_payload)) > 0) {
        fwrite(buf, 1, n, output_stream);
    }
    fclose(tmp_payload);

    return 1;
}
