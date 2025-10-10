/**
* @file json.c
* @brief implementation of the JSON parser and utility library
*
* This file contains the full implementation for parsing, creating, freeing,
* and serializing JSON data structures
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "json.h"

// Prototypes for static functions
static json_value_t* parse_value(json_parser_t* parser);
static json_value_t* parse_object(json_parser_t* parser);
static json_value_t* parse_array(json_parser_t* parser);
static json_value_t* parse_string(json_parser_t* parser);
static json_value_t* parse_number(json_parser_t* parser);
static json_value_t* parse_literal(json_parser_t* parser);
static void skip_whitespace(json_parser_t* parser);
static bool match_string(json_parser_t* parser, const char* str);
static bool resize_array(json_array_t* array, size_t new_capacity);
static bool resize_object(json_object_t* object, size_t new_capacity);
static char* read_file_contents(const char* filename, size_t* size);

json_value_t* json_create(const json_type_t type) {
    json_value_t* value = calloc(1, sizeof(json_value_t));
    if (!value) return NULL;

    value->type = type;

    if (value->type == JSON_ARRAY) {
        // init array data
        value->data.array.items = malloc(JSON_INITIAL_CAPACITY * sizeof(json_value_t*));
        if (!value->data.array.items) {
            free(value);
            return NULL;
        }
        value->data.array.count = 0;
        value->data.array.capacity = JSON_INITIAL_CAPACITY;
    } else if (value->type == JSON_OBJECT) {
        // init object data
        value->data.object.entries = malloc(JSON_INITIAL_CAPACITY * sizeof(json_object_entry_t));
        if (!value->data.object.entries) {
            free(value);
            return NULL;
        }
        value->data.object.count = 0;
        value->data.object.capacity = JSON_INITIAL_CAPACITY;
    }

    return value;
}

void json_free(json_value_t* value) {
    if (!value) return;

    switch (value->type) {
        case JSON_STRING:
            free(value->data.string);
            break;
        case JSON_ARRAY:
            // recursively free all items in the array
            for (size_t i = 0; i < value->data.array.count; i++)
                json_free(value->data.array.items[i]);
            free(value->data.array.items);
            break;
        case JSON_OBJECT:
            // free each key and recursively free each value
            for (size_t i = 0; i < value->data.object.count; i++) {
                free(value->data.object.entries[i].key);
                json_free(value->data.object.entries[i].value);
            }
            free(value->data.object.entries);
            break;
        default:
            // for NULL, NUMBER, BOOL no extra data needs to be free
            break;
    }

    free(value);
}

/**
 * @brief parse any JSON value (object, array, string, number, literal)
 * @param parser parser context
 * @return parsed json_value_t or NULL on error
 */
static json_value_t* parse_value(json_parser_t* parser) {
    skip_whitespace(parser);

    if (!*parser->current) {
        parser->error = JSON_ERROR_PARSE_ERROR;
        return NULL;
    }

    const char c = *parser->current;

    // determine a value type by the first char
    switch (c) {
        case '{': return parse_object(parser);
        case '[': return parse_array(parser);
        case '"': return parse_string(parser);
        case 't': case 'f': case 'n': return parse_literal(parser);
        default:
            if (c == '-' || isdigit((unsigned char)c)) return parse_number(parser);
            parser->error = JSON_ERROR_PARSE_ERROR;
            return NULL;
    }
}

/**
 * @brief parse a JSON object
 * @param parser parser context
 * @return JSON object or NULL on error
 */
static json_value_t* parse_object(json_parser_t* parser) {
    json_value_t* object = json_create(JSON_OBJECT);
    if (!object) {
        parser->error = JSON_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    parser->current++; // consume '{'
    parser->column++;
    skip_whitespace(parser);

    // handle empty object
    if (*parser->current == '}') {
        parser->current++;
        parser->column++;
        return object;
    }

    // loop through key-value pairs until '}' or an error is found
    while (1) {
        skip_whitespace(parser);

        // key must be a string
        if (*parser->current != '"') {
            parser->error = JSON_ERROR_PARSE_ERROR;
            json_free(object);
            return NULL;
        }

        json_value_t* key_value = parse_string(parser);
        if (!key_value) {
            json_free(object);
            return NULL;
        }

        char* key = strdup(key_value->data.string);
        json_free(key_value);

        if (!key) {
            parser->error = JSON_ERROR_OUT_OF_MEMORY;
            json_free(object);
            return NULL;
        }

        skip_whitespace(parser);

        // expect a colon after the key
        if (*parser->current != ':') {
            parser->error = JSON_ERROR_PARSE_ERROR;
            free(key);
            json_free(object);
            return NULL;
        }

        parser->current++; // consume ':'
        parser->column++;

        json_value_t* value = parse_value(parser);
        if (!value) {
            free(key);
            json_free(object);
            return NULL;
        }

        // resize object if you need
        if (object->data.object.count >= object->data.object.capacity &&
            !resize_object(&object->data.object, object->data.object.capacity * 2)) {
            parser->error = JSON_ERROR_OUT_OF_MEMORY;
            free(key);
            json_free(value);
            json_free(object);
            return NULL;
        }

        // add the parsed key-value pair
        const size_t idx = object->data.object.count++;
        object->data.object.entries[idx].key = key;
        object->data.object.entries[idx].value = value;

        skip_whitespace(parser);

        // check for end of object
        if (*parser->current == '}') {
            parser->current++;
            parser->column++;
            break;
        }

        // expect a comma before the next pair
        if (*parser->current == ',') {
            parser->current++;
            parser->column++;
            continue;
        }

        parser->error = JSON_ERROR_PARSE_ERROR;
        json_free(object);
        return NULL;
    }

    return object;
}

/**
 * @brief parse a JSON array
 * @param parser parser context
 * @return JSON array or NULL on error
 */
static json_value_t* parse_array(json_parser_t* parser) {
    json_value_t* array = json_create(JSON_ARRAY);
    if (!array) {
        parser->error = JSON_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    parser->current++; // consume '['
    parser->column++;
    skip_whitespace(parser);

    // handle empty array
    if (*parser->current == ']') {
        parser->current++;
        parser->column++;
        return array;
    }

    // loop through items until ']' or an error is found
    while (1) {
        json_value_t* value = parse_value(parser);
        if (!value) {
            json_free(array);
            return NULL;
        }

        // resize array if need
        if (array->data.array.count >= array->data.array.capacity) {
            if (!resize_array(&array->data.array, array->data.array.capacity * 2)) {
                parser->error = JSON_ERROR_OUT_OF_MEMORY;
                json_free(value);
                json_free(array);
                return NULL;
            }
        }

        array->data.array.items[array->data.array.count++] = value;

        skip_whitespace(parser);

        // check for end of array
        if (*parser->current == ']') {
            parser->current++;
            parser->column++;
            break;
        }

        // expect a comma before the next item
        if (*parser->current == ',') {
            parser->current++;
            parser->column++;
            continue;
        }

        parser->error = JSON_ERROR_PARSE_ERROR;
        json_free(array);
        return NULL;
    }

    return array;
}

/**
 * @brief parse a JSON string
 * @param parser parser context
 * @return JSON string value or NULL on error
 */
static json_value_t* parse_string(json_parser_t* parser) {
    parser->current++; // consume opening '"'
    parser->column++;

    const char* start = parser->current;
    size_t length = 0;

    // calculate required buffer length, handling escapes
    while (*parser->current && *parser->current != '"') {
        if (*parser->current == '\\') {
            parser->current++;
            parser->column++;
            if (!*parser->current) break; // end of string after backslash

            if (*parser->current == 'u') { // unicode escape
                parser->current += 4;
                parser->column += 4;
                length += JSON_MAX_UNICODE_LENGTH; // multi-byte UTF-8
            } else {
                length++; // simple escape sequence like \n or \"
            }
        } else {
            length++;
        }
        parser->current++;
        parser->column++;
    }

    if (*parser->current != '"') {
        parser->error = JSON_ERROR_PARSE_ERROR;
        return NULL;
    }

    char* str = malloc(length + 1);
    if (!str) {
        parser->error = JSON_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    parser->current = start; // reset for next loop
    size_t pos = 0;

    // copy chars and handle escape sequences
    while (*parser->current && *parser->current != '"') {
        if (*parser->current == '\\') {
            parser->current++;
            switch (*parser->current) {
                case '"':  str[pos++] = '"';  break;
                case '\\': str[pos++] = '\\'; break;
                case '/':  str[pos++] = '/';  break;
                case 'b':  str[pos++] = '\b'; break;
                case 'f':  str[pos++] = '\f'; break;
                case 'n':  str[pos++] = '\n'; break;
                case 'r':  str[pos++] = '\r'; break;
                case 't':  str[pos++] = '\t'; break;
                case 'u': // placeholder for Unicode
                    parser->current += 4;
                    str[pos++] = '?'; // simple placeholder
                    continue;
                default: // invalid escape
                    parser->error = JSON_ERROR_PARSE_ERROR;
                    free(str);
                    return NULL;
            }
            parser->current++;
        } else {
            str[pos++] = *parser->current++;
        }
    }

    str[pos] = '\0';
    parser->current++; // consume closing '"'
    parser->column++;

    json_value_t* value = json_create(JSON_STRING);
    if (!value) {
        parser->error = JSON_ERROR_OUT_OF_MEMORY;
        free(str);
        return NULL;
    }

    value->data.string = str;
    return value;
}

/**
 * @brief parse a JSON number
 * @param parser parser context
 * @return JSON number value or NULL on error
 */
static json_value_t* parse_number(json_parser_t* parser) {
    const char* start = parser->current;

    // optional sign
    if (*parser->current == '-') {
        parser->current++;
        parser->column++;
    }

    // integer part
    if (*parser->current == '0') {
        parser->current++;
        parser->column++;
    } else if (isdigit((unsigned char)*parser->current)) {
        while (isdigit((unsigned char)*parser->current)) {
            parser->current++;
            parser->column++;
        }
    } else {
        parser->error = JSON_ERROR_PARSE_ERROR;
        return NULL;
    }

    // fractional part
    if (*parser->current == '.') {
        parser->current++;
        parser->column++;
        if (!isdigit((unsigned char)*parser->current)) {
            parser->error = JSON_ERROR_PARSE_ERROR;
            return NULL;
        }
        while (isdigit((unsigned char)*parser->current)) {
            parser->current++;
            parser->column++;
        }
    }

    // exponent part
    if (*parser->current == 'e' || *parser->current == 'E') {
        parser->current++;
        parser->column++;
        if (*parser->current == '+' || *parser->current == '-') {
            parser->current++;
            parser->column++;
        }
        if (!isdigit((unsigned char)*parser->current)) {
            parser->error = JSON_ERROR_PARSE_ERROR;
            return NULL;
        }
        while (isdigit((unsigned char)*parser->current)) {
            parser->current++;
            parser->column++;
        }
    }

    const size_t len = parser->current - start;
    char* num_str = malloc(len + 1);
    if (!num_str) {
        parser->error = JSON_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    strncpy(num_str, start, len);
    num_str[len] = '\0';

    const double number = strtod(num_str, NULL);
    free(num_str);

    json_value_t* value = json_create(JSON_NUMBER);
    if (!value) {
        parser->error = JSON_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    value->data.number = number;
    return value;
}


/**
 * @brief parse a JSON literal (true, false, null)
 * @param parser parser context
 * @return JSON value or NULL on error
 */
static json_value_t* parse_literal(json_parser_t* parser) {
    if (match_string(parser, "true")) {
        json_value_t* value = json_create(JSON_BOOL);
        if (value) value->data.boolean = true;
        return value;
    }

    if (match_string(parser, "false")) {
        json_value_t* value = json_create(JSON_BOOL);
        if (value) value->data.boolean = false;
        return value;
    }

    if (match_string(parser, "null")) {
        return json_create(JSON_NULL);
    }

    parser->error = JSON_ERROR_PARSE_ERROR;
    return NULL;
}


/**
 * @brief skip whitespace characters in parser
 * @param parser parser context
 */
static void skip_whitespace(json_parser_t* parser) {
    while (*parser->current && isspace((unsigned char)*parser->current)) {
        // track line and column numbers
        if (*parser->current == '\n') {
            parser->line++;
            parser->column = 1;
        } else {
            parser->column++;
        }
        parser->current++;
    }
}

/**
 * @brief match a fixed string at the current parser position
 * @param parser parser context
 * @param str string to match
 * @return true if matched, false otherwise
 */
static bool match_string(json_parser_t* parser, const char* str) {
    const size_t len = strlen(str);
    if (strncmp(parser->current, str, len) == 0) {
        parser->current += len;
        parser->column += len;
        return true;
    }
    return false;
}

/**
 * @brief resize a JSON array
 * @param array array to resize
 * @param new_capacity new capacity
 * @return true on success, false on allocation failure
 */
static bool resize_array(json_array_t* array, const size_t new_capacity) {
    json_value_t** new_items = realloc(array->items, new_capacity * sizeof(json_value_t*));
    if (!new_items) return false;

    array->items = new_items;
    array->capacity = new_capacity;
    return true;
}

/**
 * @brief resize a JSON object
 * @param object object to resize
 * @param new_capacity new capacity
 * @return true on success, false on allocation failure
 */
static bool resize_object(json_object_t* object, const size_t new_capacity) {
    json_object_entry_t* new_entries = realloc(object->entries, new_capacity * sizeof(json_object_entry_t));
    if (!new_entries) return false;

    object->entries = new_entries;
    object->capacity = new_capacity;
    return true;
}

/**
 * @brief read the entire contents of a file into memory
 * @param filename file path
 * @param size pointer to store read size
 * @return allocated buffer or NULL on error
 */
static char* read_file_contents(const char* filename, size_t* size) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;

    // get file size by seeking to the end
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    const long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);

    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    const size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0'; // null-terminate the buffer

    fclose(file);

    if (size) *size = read_size;

    return buffer;
}

json_value_t* json_parse(const char* input) {
    if (!input) return NULL;

    json_parser_t parser = {
        .input = input,
        .current = input,
        .line = 1,
        .column = 1,
        .error = JSON_OK
    };

    json_value_t* result = parse_value(&parser);
    if (!result) return NULL;

    skip_whitespace(&parser);
    // check for trailing characters after the main value
    if (*parser.current != '\0') {
        json_free(result);
        return NULL;
    }

    return result;
}

json_value_t* json_parse_file(const char* filename) {
    size_t size;
    char* contents = read_file_contents(filename, &size);
    if (!contents) return NULL;

    json_value_t* result = json_parse(contents);
    free(contents);
    return result;
}

void json_write_indent(FILE* f, const int indent) {
    for (int i = 0; i < indent; i++) {
        fputc('\t', f);
    }
}

void json_write_null(FILE* f) {
    fprintf(f, "null");
}

void json_write_bool(FILE* f, const bool b) {
    fprintf(f, b ? "true" : "false");
}

void json_write_number(FILE* f, const double num) {
    fprintf(f, "%g", num);
}

void json_write_string(FILE* f, const char* s) {
    fprintf(f, "\"%s\"", s);
}

void json_write_array(FILE* f, const json_array_t* array, const int indent) {
    fprintf(f, "[");
    if (array->count > 0) {
        for (size_t i = 0; i < array->count; i++) {
            json_write_value(f, array->items[i], indent + 1);
            if (i < array->count - 1)
                fprintf(f, ", ");
        }
    }
    fprintf(f, "]");
}

void json_write_object(FILE* f, const json_object_t* object, const int indent) {
    fprintf(f, "{");
    if (object->count > 0) {
        fprintf(f, "\n");
        for (size_t i = 0; i < object->count; i++) {
            // add newline and indentation for pretty printing
            json_write_indent(f, indent + 1);
            fprintf(f, "\"%s\": ", object->entries[i].key);
            json_write_value(f, object->entries[i].value, indent + 1);
            if (i < object->count - 1)
                fprintf(f, ",");
            fprintf(f, "\n");
        }
        json_write_indent(f, indent);
    }
    fprintf(f, "}");
}

void json_write_value(FILE* f, const json_value_t* value, const int indent) {
    switch (value->type) {
        case JSON_NULL:   json_write_null(f); break;
        case JSON_BOOL:   json_write_bool(f, value->data.boolean); break;
        case JSON_NUMBER: json_write_number(f, value->data.number); break;
        case JSON_STRING: json_write_string(f, value->data.string); break;
        case JSON_ARRAY:  json_write_array(f, &value->data.array, indent); break;
        case JSON_OBJECT: json_write_object(f, &value->data.object, indent); break;
    }
}

int json_write_file(FILE* file, const json_value_t* root) {
    json_write_value(file, root, 0);
    fprintf(file, "\n");
    return 0;
}

bool json_compare(const json_value_t* a, const json_value_t* b) {
    if (!a || !b || a->type != b->type) return false;

    switch (a->type) {
        case JSON_STRING:
            return strcmp(a->data.string, b->data.string) == 0;
        case JSON_NUMBER:
            return a->data.number == b->data.number;
        case JSON_BOOL:
            return a->data.boolean == b->data.boolean;
        case JSON_NULL:
            return true; // both are null
        case JSON_ARRAY:
            if (a->data.array.count != b->data.array.count) return false;
            // recursively compare each item in the array
            for (size_t i = 0; i < a->data.array.count; ++i) {
                if (!json_compare(a->data.array.items[i], b->data.array.items[i])) return false;
            }
            return true;
        case JSON_OBJECT:
            if (a->data.object.count != b->data.object.count) return false;

            // compare objects regardless of key order using nested loops
            for (size_t i = 0; i < a->data.object.count; ++i) {
                const char* key = a->data.object.entries[i].key;
                const json_value_t* val_a = a->data.object.entries[i].value;
                bool found = false;

                for (size_t j = 0; j < b->data.object.count; ++j) {
                    if (strcmp(key, b->data.object.entries[j].key) == 0) {
                        found = true;
                        // recursively compare the values for the same key
                        if (!json_compare(val_a, b->data.object.entries[j].value)) return false;
                        break;
                    }
                }
                if (!found) return false; // key from object 'a' not found in object 'b'
            }
            return true;
        default:
            return false;
    }
}
