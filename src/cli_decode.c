/**
 * @file cli_decode.c
 * @brief implementation of the command-line BEJ decoder
 */

#include <stdio.h>
#include <stdlib.h>

#include "cli_decode.h"
#include "bej_dictionary.h"
#include "bej_decode.h"
#include "json.h"

/**
 * @brief runs the BEJ decoding process
 * * This function opens the input BEJ file, loads the necessary dictionaries,
 * decodes the data into a JSON tree, and writes the result to the
 * specified output file or to stdout
 *
 * @param args a pointer to a populated args_t structure with all required paths
 * @return 0 on success, or a non-zero value on failure
 */
int cli_run_decode(const args_t *args) {
    // read entire input file into a memory buffer
    FILE *in = fopen(args->input_path, "rb");
    if (!in) {
        perror("fopen input");
        return 1;
    }
    fseek(in, 0, SEEK_END);
    const long sz = ftell(in);
    rewind(in);

    uint8_t *buf = malloc(sz);
    if (!buf) {
        fprintf(stderr, "Failed to allocate memory for input file\n");
        fclose(in);
        return 1;
    }
    fread(buf, 1, sz, in);
    fclose(in);

    // load the main schema dictionary
    bej_dictionary_t *schema = bej_dictionary_load_map(args->schema_path);
    if (!schema) {
        fprintf(stderr, "Failed to load schema dictionary\n");
        free(buf);
        return 1;
    }

    // optionally, load the annotation dictionary
    bej_dictionary_t *annot = NULL;
    if (args->annot_path) {
        annot = bej_dictionary_load_map(args->annot_path);
        if (!annot) {
            fprintf(stderr, "Failed to load annotation dictionary\n");
            bej_dictionary_free(schema);
            free(buf);
            return 1;
        }
    }

    // call the main decoder function from the library
    json_value_t *decoded = bej_decode_buffer(buf, (size_t)sz, schema, annot);
    free(buf);

    if (!decoded) {
        fprintf(stderr, "Failed to decode BEJ\n");
        bej_dictionary_free(schema);
        if (annot) bej_dictionary_free(annot);
        return 1;
    }

    // determine output stream (file or stdout)
    FILE *out = args->output_path ? fopen(args->output_path, "w") : stdout;
    if (!out) {
        perror("fopen output");
        json_free(decoded);
        bej_dictionary_free(schema);
        if (annot) bej_dictionary_free(annot);
        return 1;
    }
    
    // write the resulting JSON to the output
    json_write_file(out, decoded);

    // cleanup all allocated resources
    if (out != stdout) fclose(out);
    json_free(decoded);
    bej_dictionary_free(schema);
    if (annot) bej_dictionary_free(annot);

    return 0;
}
