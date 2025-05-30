#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */
/*                                INPUT BUFFER                                */
/* -------------------------------------------------------------------------- */

typedef struct {
    char *buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

InputBuffer *input_buffer_new() {
    InputBuffer *input_buffer = malloc(sizeof(InputBuffer));

    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}

void input_buffer_free(InputBuffer *input_buffer) {
    free(input_buffer->buffer);
    free(input_buffer);
}

void input_buffer_read(InputBuffer *input_buffer) {
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);
    if (bytes_read <= 0) {
        fprintf(stderr, "Unable to read input.\n");
        exit(EXIT_FAILURE);
    }

    // Ignore trailing newline
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

/* -------------------------------------------------------------------------- */
/*                                META COMMANDS                               */
/* -------------------------------------------------------------------------- */

typedef enum {
    META_SUCCESS,
    META_UNRECOGNIZED_COMMAND,
} MetaResult;

MetaResult meta_command(InputBuffer *input_buffer) {
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        exit(EXIT_SUCCESS);
    } else {
        return META_UNRECOGNIZED_COMMAND;
    }
}

/* -------------------------------------------------------------------------- */
/*                                 STATEMENTS                                 */
/* -------------------------------------------------------------------------- */

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_STATEMENT,
} PrepareResult;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT,
} StatementType;

typedef struct {
    StatementType type;
} Statement;

PrepareResult statement_prepare(InputBuffer *input_buffer, Statement *statement) {
    if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
        statement->type = STATEMENT_INSERT;
        return PREPARE_SUCCESS;
    }

    if (strcmp(input_buffer->buffer, "select") == 0) {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}

void statement_execute(Statement *statement) {
    switch (statement->type) {
        case STATEMENT_INSERT:
            printf("Insert.\n");
            break;

        case STATEMENT_SELECT:
            printf("Select.\n");
            break;
    }
}

/* -------------------------------------------------------------------------- */
/*                                    REPL                                    */
/* -------------------------------------------------------------------------- */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    InputBuffer *input_buffer = input_buffer_new();

    while (true) {
        printf("db > ");
        input_buffer_read(input_buffer);

        if (input_buffer->buffer[0] == '.') {
            switch (meta_command(input_buffer)) {
                case META_SUCCESS:
                    continue;
                
                case META_UNRECOGNIZED_COMMAND:
                    fprintf(stderr, "Unrecognized command: '%s'.\n", input_buffer->buffer);
                    continue;
            }
        }

        Statement statement;
        switch (statement_prepare(input_buffer, &statement)) {
            case PREPARE_SUCCESS:
                break;

            case PREPARE_UNRECOGNIZED_STATEMENT:
                fprintf(stderr, "Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
                continue;
        }

        statement_execute(&statement);
        printf("Executed.\n");
    }

    return EXIT_SUCCESS;
}