/**
 * @file bej_dictionary.c
 * @brief implementation of the BEJ dictionary parser and iterator
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bej_dictionary.h"

/**
 * @brief reads a little-endian 16-bit unsigned integer from a byte buffer
 * @param p pointer to the buffer
 * @return the 16-bit value
 */
static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief reads a little-endian 32-bit unsigned integer from a byte buffer
 * @param p pointer to the buffer
 * @return the 32-bit value
 */
static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief validates that the dictionary buffer is not NULL and has a minimum size
 * @param dict the dictionary to validate
 * @return 1 if valid, 0 otherwise
 */
static int validate_dictionary_header(const bej_dictionary_t *dict) {
    return dict->size >= BEJ_DICTIONARY_HEADER_SIZE;
}

/**
 * @brief reads header fields from the dictionary
 * @param dict the dictionary to read from
 * @param entry_count pointer to store the number of entries
 * @param name_table_offset pointer to store the calculated offset of the name table
 * @return 1 on success, 0 on failure
 */
static int read_dictionary_header(const bej_dictionary_t *dict,
                                 uint16_t *entry_count,
                                 size_t *name_table_offset) {
    if (!validate_dictionary_header(dict))
        return 0;

    *entry_count = read_u16(dict->bytes + BEJ_OFFSET_ENTRY_COUNT);

    const size_t entries_size = (size_t)(*entry_count) * BEJ_DICTIONARY_ENTRY_SIZE;
    *name_table_offset = BEJ_OFFSET_ENTRIES_START + entries_size;

    return (*name_table_offset <= dict->size);
}

bej_dictionary_t *bej_dictionary_load(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;

    // get file size by seeking to the end
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    const long file_size = ftell(file);
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    // allocate a single buffer for the entire file
    uint8_t *buffer = malloc((size_t)file_size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)file_size, file) != (size_t)file_size) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    fclose(file);

    // perform a temporary cast to validate the header before final allocation
    const bej_dictionary_t temp_dict = {.bytes = buffer, .size = (size_t)file_size};
    if (!validate_dictionary_header(&temp_dict)) {
        free(buffer);
        return NULL;
    }

    bej_dictionary_t *dictionary = malloc(sizeof(*dictionary));
    if (!dictionary) {
        free(buffer);
        return NULL;
    }

    dictionary->bytes = buffer;
    dictionary->size = (size_t)file_size;
    return dictionary;
}

bej_dictionary_t *bej_dictionary_load_map(const char *path) {
    const char *extension = strrchr(path, '.');

    if (extension && strcmp(extension, ".bin") == 0)
        return bej_dictionary_load(path);

    // if a .map file is given, construct the .bin path and load that
    if (extension && strcmp(extension, ".map") == 0) {
        const size_t base_name_length = (size_t)(extension - path);
        char *bin_path = malloc(base_name_length + 5); // for ".bin" and null terminator
        if (!bin_path)
            return NULL;

        memcpy(bin_path, path, base_name_length);
        memcpy(bin_path + base_name_length, ".bin", 5);

        bej_dictionary_t *dictionary = bej_dictionary_load(bin_path);
        free(bin_path);
        return dictionary;
    }

    // try to load the path as is
    return bej_dictionary_load(path);
}

void bej_dictionary_free(bej_dictionary_t *dict) {
    if (!dict)
        return;
    free(dict->bytes); // free the raw data buffer
    free(dict);        // free the struct itself
}

void bej_dict_stream_init(bej_dict_stream_t *s, const bej_dictionary_t *dict) {
    s->bytes = dict ? dict->bytes : NULL;
    s->size = dict ? dict->size : 0;
    s->index = 0;
    s->child_count = 0;
    s->current_entry = 0;

    if (dict && validate_dictionary_header(dict)) {
        uint16_t entry_count;
        size_t name_table_offset;

        if (read_dictionary_header(dict, &entry_count, &name_table_offset)) {
            s->index = BEJ_OFFSET_ENTRIES_START;
            s->child_count = 1; // top-level has a single root entry
        }
    }
}

void bej_dict_stream_init_subset(bej_dict_stream_t *s,
                                const bej_dictionary_t *dict,
                                const uint16_t offset,
                                const uint16_t child_count) {
    s->bytes = dict ? dict->bytes : NULL;
    s->size = dict ? dict->size : 0;
    s->index = offset;
    s->child_count = (child_count == BEJ_CHILD_COUNT_WILDCARD) ? -1 : (int)child_count;
    s->current_entry = 0;
}

int bej_dict_stream_has_entry(const bej_dict_stream_t *s) {
    if (!s || !s->bytes) {
        return 0;
    }
    
    // if child_count is a wildcard, check bounds
    if (s->child_count < 0)
        return (s->index + BEJ_DICTIONARY_ENTRY_SIZE) <= s->size;

    // otherwise, check if we've iterated through all children
    return s->current_entry < s->child_count;
}

int bej_dict_stream_next(bej_dict_stream_t *s, bej_dict_entry_t *dest) {
    if (!bej_dict_stream_has_entry(s))
        return 0;

    if (s->index + BEJ_DICTIONARY_ENTRY_SIZE > s->size)
        return 0;

    // get a pointer to the start of the raw entry data
    const uint8_t *entry_data = s->bytes + s->index;

    const uint8_t format_flags = entry_data[BEJ_ENTRY_OFFSET_FORMAT_FLAGS];
    dest->format = (uint8_t)(format_flags >> 4); // upper 4 bits
    dest->flags = (uint8_t)(format_flags & 0x0F);  // lower 4 bits
    dest->sequence = read_u16(entry_data + BEJ_ENTRY_OFFSET_SEQUENCE);
    dest->child_pointer = read_u16(entry_data + BEJ_ENTRY_OFFSET_CHILD_POINTER);
    dest->child_count = read_u16(entry_data + BEJ_ENTRY_OFFSET_CHILD_COUNT);

    const uint8_t name_length = entry_data[BEJ_ENTRY_OFFSET_NAME_LEN];
    const uint16_t name_offset = read_u16(entry_data + BEJ_ENTRY_OFFSET_NAME_OFFSET);

    dest->name = NULL;
    if (name_length > 0 && name_offset < s->size)
        dest->name = (const char*)(s->bytes + name_offset);

    // advance the stream to the next entry
    s->index += BEJ_DICTIONARY_ENTRY_SIZE;

    if (s->child_count >= 0)
        s->current_entry++;

    return 1;
}

int bej_dict_find_child_by_name(const bej_dictionary_t *dict,
                               const uint16_t offset,
                               const uint16_t child_count,
                               const char *name,
                               bej_dict_entry_t *dest) {
    if (!dict || !name || !dest)
        return 0;

    bej_dict_stream_t stream;
    bej_dict_stream_init_subset(&stream, dict, offset, child_count);

    // linear search through the subset
    bej_dict_entry_t entry;
    while (bej_dict_stream_next(&stream, &entry)) {
        if (entry.name && strcmp(entry.name, name) == 0) {
            *dest = entry;
            return 1;
        }
    }

    return 0;
}
