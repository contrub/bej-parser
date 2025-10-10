/**
 * @file bej_dictionary.h
 * @brief API for handling BEJ (Binary Encoded JSON) dictionaries
 *
 * This file defines the structures and public functions for loading, parsing,
 * and iterating over BEJ schema dictionaries from binary files
 */
#ifndef BEJ_DICTIONARY_H
#define BEJ_DICTIONARY_H

#include <stdint.h>

/**
 * @name BEJ Format Codes
 * @{
 */
#define BEJ_FORMAT_SET                  0x00 /**< identifies a set (JSON object) */
#define BEJ_FORMAT_ARRAY                0x01 /**< identifies an array */
#define BEJ_FORMAT_NULL                 0x02 /**< identifies a null value */
#define BEJ_FORMAT_INTEGER              0x03 /**< identifies an integer */
#define BEJ_FORMAT_ENUM                 0x04 /**< identifies an enumeration value */
#define BEJ_FORMAT_STRING               0x05 /**< identifies a UTF-8 string */
#define BEJ_FORMAT_REAL                 0x06 /**< identifies a real number (floating-point) */
#define BEJ_FORMAT_BOOLEAN              0x07 /**< identifies a boolean value */
#define BEJ_FORMAT_PROPERTY_ANNOTATION  0x0A /**< identifies a property annotation */
#define BEJ_FORMAT_RESOURCE_LINK        0x0E /**< identifies a resource link */
/** @} */

/**
 * @name BEJ Flags
 * @{
 */
#define BEJ_FLAG_DEFERRED                    (1u << 0) /**< indicates a deferred binding */
#define BEJ_FLAG_NESTED_TOP_LEVEL_ANNOTATION (1u << 1) /**< indicates a nested top-level annotation */
/** @} */

/**
 * @name Dictionary Selectors
 * @{
 */
#define BEJ_DICTIONARY_SELECTOR_MAJOR_SCHEMA 0x00 /**< selector for the main schema dictionary */
#define BEJ_DICTIONARY_SELECTOR_ANNOTATION   0x01 /**< selector for the annotation dictionary */
/** @} */

/**
 * @name Dictionary Format Constants
 * @{
 */
#define BEJ_DICTIONARY_HEADER_SIZE 12 /**< size of the dictionary header in bytes */
#define BEJ_DICTIONARY_ENTRY_SIZE  10 /**< size of a single dictionary entry in bytes */
#define BEJ_DICTIONARY_MAGIC_SIZE  4  /**< size of the magic number in the header */
/** @} */

/**
 * @name Dictionary Structure Offsets
 * @{
 */
enum {
    BEJ_OFFSET_VERSION = 0,       /**< offset to the version field */
    BEJ_OFFSET_FLAGS = 1,         /**< offset to the flags field */
    BEJ_OFFSET_ENTRY_COUNT = 2,   /**< offset to the entry count field */
    BEJ_OFFSET_DICT_SIZE = 4,     /**< offset to the dictionary size field */
    BEJ_OFFSET_RESERVED = 8,      /**< offset to the reserved area */
    BEJ_OFFSET_ENTRIES_START = 12 /**< offset where the first entry begins */
};
/** @} */

/**
 * @name Entry Structure Offsets
 * @brief offsets relative to the start of an entry
 * @{
 */
enum {
    BEJ_ENTRY_OFFSET_FORMAT_FLAGS = 0,  /**< offset to format and flags byte */
    BEJ_ENTRY_OFFSET_SEQUENCE = 1,      /**< offset to the sequence number */
    BEJ_ENTRY_OFFSET_CHILD_POINTER = 3, /**< offset to the child pointer */
    BEJ_ENTRY_OFFSET_CHILD_COUNT = 5,   /**< offset to the child count */
    BEJ_ENTRY_OFFSET_NAME_LEN = 7,      /**< offset to the name length */
    BEJ_ENTRY_OFFSET_NAME_OFFSET = 8    /**< offset to the name offset */
};
/** @} */

/**
 * @name Special Dictionary Values
 * @{
 */
enum {
    BEJ_CHILD_COUNT_WILDCARD = 0xFFFF, /**< indicates an array element definition */
    BEJ_INVALID_OFFSET = 0xFFFF      /**< indicates an invalid or non-existent offset */
};
/** @} */

/**
 * @brief represents a loaded BEJ dictionary in memory
 */
typedef struct bej_dictionary
{
    uint8_t *bytes; /**< pointer to raw dictionary bytes */
    size_t   size;  /**< size of the dictionary in bytes */
} bej_dictionary_t;

/**
 * @brief stream structure for iterating over BEJ dictionary entries
 */
typedef struct bej_dict_stream
{
    const uint8_t *bytes;         /**< pointer to dictionary bytes */
    size_t         size;          /**< total size of dictionary */
    size_t         index;         /**< current index in the stream */
    int            child_count;   /**< number of entries in the current subset (-1 = until end) */
    int            current_entry; /**< index of current entry in subset */
} bej_dict_stream_t;

/**
 * @brief represents a single entry in a BEJ dictionary
 */
typedef struct bej_dict_entry
{
    uint8_t      format;        /**< upper 4 bits of the first byte */
    uint8_t      flags;         /**< lower 4 bits */
    uint16_t     sequence;      /**< sequence number */
    uint16_t     child_pointer; /**< binary offset to children (0 if none) */
    uint16_t     child_count;   /**< number of children or 0xFFFF for an array element */
    const char  *name;          /**< pointer into dictionary bytes (null-terminated) or NULL */
} bej_dict_entry_t;

/**
 * @brief loads a BEJ dictionary from a .bin file
 * @param path path to .bin dictionary
 * @return pointer to allocated bej_dictionary_t or NULL on failure
 */
bej_dictionary_t *bej_dictionary_load(const char *path);

/**
 * @brief loads a BEJ dictionary, compatible with .map or .bin paths
 * if .map is given, attempts to load sibling .bin file
 * @param path path to .map or .bin dictionary
 * @return pointer to allocated bej_dictionary_t or NULL on failure
 */
bej_dictionary_t *bej_dictionary_load_map(const char *path);

/**
 * @brief frees memory allocated for a BEJ dictionary
 * @param dict dictionary to free
 */
void bej_dictionary_free(bej_dictionary_t *dict);

/**
 * @brief initializes a stream over the entire dictionary (skipping header)
 * @param s stream to initialize
 * @param dict dictionary to iterate over
 */
void bej_dict_stream_init(bej_dict_stream_t *s, const bej_dictionary_t *dict);

/**
 * @brief initializes a stream at a child subset starting at offset with a given count
 * @param s stream to initialize
 * @param dict dictionary to iterate over
 * @param offset binary offset of subset
 * @param child_count number of entries in a subset
 */
void bej_dict_stream_init_subset(bej_dict_stream_t *s, const bej_dictionary_t *dict, uint16_t offset, uint16_t child_count);

/**
 * @brief checks if a stream has more entries
 * @param s stream to check
 * @return 1 if there are more entries, 0 otherwise
 */
int bej_dict_stream_has_entry(const bej_dict_stream_t *s);

/**
 * @brief reads the next dictionary entry into dest
 * @param s stream to read from
 * @param dest output entry
 * @return 1 on success, 0 if no more entries
 */
int bej_dict_stream_next(bej_dict_stream_t *s, bej_dict_entry_t *dest);

/**
 * @brief finds a child entry by name within a subset
 * @param dict dictionary to search
 * @param offset binary offset of subset
 * @param child_count number of entries in a subset
 * @param name name to search for
 * @param dest output entry if found
 * @return 1 if found, 0 otherwise
 */
int bej_dict_find_child_by_name(const bej_dictionary_t *dict, uint16_t offset, uint16_t child_count, const char *name, bej_dict_entry_t *dest);

#endif /* BEJ_DICTIONARY_H */
