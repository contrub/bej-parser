/**
 * @file cli_encode.c
 * @brief implementation of the command-line BEJ encoder
 */

#include <stdio.h>
#include <stdlib.h>

#include "cli_encode.h"
#include "bej_encode.h"
#include "bej_dictionary.h"
#include "json.h"

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
int cli_run_encode(const args_t *args)
{
    // parse the input JSON file into a value tree
    json_value_t *root = json_parse_file(args->input_path);
    if (!root)
    {
        fprintf(stderr, "Failed to parse JSON: %s\n", args->input_path);
        return 1;
    }

    // load the main schema dictionary
    bej_dictionary_t *schema = bej_dictionary_load_map(args->schema_path);
    if (!schema)
    {
        fprintf(stderr, "Failed to load schema dictionary\n");
        json_free(root);
        return 1;
    }

    // optionally, load the annotation dictionary
    bej_dictionary_t *annot = NULL;
    if (args->annot_path)
    {
        annot = bej_dictionary_load_map(args->annot_path);
        if (!annot)
        {
            fprintf(stderr, "Failed to load annotation dictionary\n");
            json_free(root);
            bej_dictionary_free(schema);
            return 1;
        }
    }

    // determine output stream (file or stdout)
    FILE *out = args->output_path ? fopen(args->output_path, "wb") : stdout;
    if (!out)
    {
        perror("fopen output");
        json_free(root);
        bej_dictionary_free(schema);
        if (annot) bej_dictionary_free(annot);
        return 1;
    }

    // call the main encoder function from the library
    const int ok = bej_encode_stream(out, root, schema, annot);

    // cleanup all allocated resources
    if (out != stdout) fclose(out);
    json_free(root);
    bej_dictionary_free(schema);
    if (annot) bej_dictionary_free(annot);

    return ok ? 0 : 1;
}
