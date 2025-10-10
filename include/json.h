#ifndef BEJ_PARSER_JSON_H
#define BEJ_PARSER_JSON_H

/**
 * @file json.h
 * @brief JSON parser and utility library in C
 *
 * This library parses JSON strings or files into a hierarchical value tree
 * Supports JSON types: null, boolean, number, string, array, object
 */

#include <stdbool.h>
#include <stdio.h>

#define JSON_INITIAL_CAPACITY 8    /**< Initial array/object capacity */
#define JSON_MAX_UNICODE_LENGTH 6  /**< Max estimated length for Unicode escape sequence */

/**
 * @typedef json_value_t
 * @brief Represents a JSON value of any type (null, bool, number, string, array, object)
 *
 * This type is an alias for `struct json_value` and can hold any JSON value
 */
typedef struct json_value json_value_t;

/**
 * @enum json_type_t
 * @brief JSON value types
 */
typedef enum {
    JSON_NULL = 0, /**< Null value */
    JSON_BOOL,     /**< Boolean value */
    JSON_NUMBER,   /**< Number (double) */
    JSON_STRING,   /**< String value */
    JSON_ARRAY,    /**< Array of values */
    JSON_OBJECT    /**< Object with key/value pairs */
} json_type_t;

/**
 * @enum json_error_t
 * @brief Error codes returned by JSON parser and accessors
 */
typedef enum {
    JSON_OK = 0,                   /**< Success */
    JSON_ERROR_INVALID_INPUT,      /**< Null pointer or invalid input */
    JSON_ERROR_OUT_OF_MEMORY,      /**< Memory allocation failed */
    JSON_ERROR_PARSE_ERROR,        /**< Invalid JSON format */
    JSON_ERROR_INVALID_TYPE,       /**< Type mismatch */
    JSON_ERROR_KEY_NOT_FOUND,      /**< Key isn't found in an object */
    JSON_ERROR_INDEX_OUT_OF_BOUNDS /**< Array index out of bounds */
} json_error_t;

/**
 * @brief Single key/value pair in a JSON object
 */
typedef struct {
    char *key;           /**< Key string */
    json_value_t *value; /**< Pointer to value */
} json_object_entry_t;

/**
 * @brief Dynamic array container
 */
typedef struct {
    json_value_t **items; /**< Array of value pointers */
    size_t count;         /**< Number of elements */
    size_t capacity;      /**< Allocated capacity */
} json_array_t;

/**
 * @brief Dynamic object container
 */
typedef struct {
    json_object_entry_t *entries; /**< Array of object entries */
    size_t count;                 /**< Number of entries */
    size_t capacity;              /**< Allocated capacity */
} json_object_t;

/**
 * @struct json_value
 * @brief Represents a JSON value of any type
 */
struct json_value {
    json_type_t type; /**< Type of this value */

    /**
     * @brief Value data (union depends on type)
     * - boolean: for JSON_BOOL
     * - number:  for JSON_NUMBER
     * - string:  for JSON_STRING
     * - array:   for JSON_ARRAY
     * - object:  for JSON_OBJECT
     */
    union {
        bool boolean;         /**< Boolean value */
        double number;        /**< Numeric value */
        char *string;         /**< String value */
        json_array_t array;   /**< Array container */
        json_object_t object; /**< Object container */
    } data;
};


/**
 * @struct json_parser_t
 * @brief Parser context for JSON parsing
 */
typedef struct {
    const char *input;   /**< Original input string */
    const char *current; /**< Current parse position */
    size_t line;         /**< Current line number */
    size_t column;       /**< Current column number */
    json_error_t error;  /**< Last error encountered */
} json_parser_t;


/**
 * @brief Allocate a new JSON value of a specified type
 * @param type JSON type
 * @return Pointer to allocated json_value_t or NULL on failure
 */
json_value_t *json_create(json_type_t type);

/**
 * @brief Free a JSON value and all children recursively
 * @param value JSON value to free (NULL-safe)
 */
void json_free(json_value_t *value);

/**
 * @brief Parse a JSON string into value tree
 * @param input JSON input string
 * @return Pointer to root value or NULL on error
 */
json_value_t *json_parse(const char *input);

/**
 * @brief Parse a JSON file
 * @param filename Path to JSON file
 * @return Pointer to root value or NULL on error
 */
json_value_t *json_parse_file(const char *filename);

/**
 * @brief Write indentation using tabs
 * @param f Output file stream
 * @param indent Number of indentation levels (tabs)
 */
void json_write_indent(FILE *f, int indent);

/**
 * @brief Write JSON null value
 * @param f Output file stream
 */
void json_write_null(FILE *f);

/**
 * @brief Write JSON boolean value
 * @param f Output file stream
 * @param b Boolean value (true/false)
 */
void json_write_bool(FILE *f, bool b);

/**
 * @brief Write JSON numeric value
 * @param f Output file stream
 * @param num Numeric value
 */
void json_write_number(FILE *f, double num);

/**
 * @brief Write JSON string value (without escaping)
 * @param f Output file stream
 * @param s Null-terminated UTF-8 string
 */
void json_write_string(FILE *f, const char *s);

/**
 * @brief Write JSON array value
 * @param f Output file stream
 * @param array Pointer to JSON array structure
 * @param indent Current indentation level
 */
void json_write_array(FILE *f, const json_array_t *array, int indent);

/**
 * @brief Write JSON object value
 * @param f Output file stream
 * @param object Pointer to JSON object structure
 * @param indent Current indentation level
 */
void json_write_object(FILE *f, const json_object_t *object, int indent);

/**
 * @brief Write any JSON value (object, array, string, number, bool, null)
 * @param f Output file stream
 * @param value JSON value to write
 * @param indent Current indentation level for formatted output
 */
void json_write_value(FILE *f, const json_value_t *value, int indent);

/**
 * @brief Write a JSON value to a file (formatted)
 * @param file FILE pointer
 * @param root Root JSON value
 * @return 0 on success
 */
int json_write_file(FILE* file, const json_value_t *root);

/**
 * @brief recursively compares two JSON values for equality
 * * This function performs a deep comparison of two JSON trees. Object keys
 * are compared without regard to their order
 *
 * @param a the first JSON value
 * @param b the second JSON value
 * @return true if the values are identical, false otherwise
 */
bool json_compare(const json_value_t* a, const json_value_t* b);

#endif
