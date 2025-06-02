#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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
/*                                    ROWS                                    */
/* -------------------------------------------------------------------------- */

#define COLUMN_USERNAME_SIZE    32
#define COLUMN_EMAIL_SIZE       255

typedef struct {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
} Row;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

const uint32_t ID_SIZE          = size_of_attribute(Row, id);
const uint32_t ID_OFFSET        = 0;
const uint32_t USERNAME_SIZE    = size_of_attribute(Row, username);
const uint32_t USERNAME_OFFSET  = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_SIZE       = size_of_attribute(Row, email);
const uint32_t EMAIL_OFFSET     = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE         = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

void row_serialize(Row *source, void *destination) {
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void row_deserialize(void *source, Row *destination) {
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

void row_print(Row *row) {
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

/* -------------------------------------------------------------------------- */
/*                                   TABLES                                   */
/* -------------------------------------------------------------------------- */

#define TABLE_MAX_PAGES 100

const uint32_t PAGE_SIZE        = 4096;
const uint32_t ROWS_PER_PAGE    = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS   = ROWS_PER_PAGE * TABLE_MAX_PAGES;

typedef struct {
    uint32_t num_rows;
    void *pages[TABLE_MAX_PAGES];
} Table;

Table *table_new() {
    Table *table = malloc(sizeof(Table));

    table->num_rows = 0;

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        table->pages[i] = NULL;
    }

    return table;
}

void table_free(Table *table) {
    for (int i = 0; table->pages[i]; i++) {
        free(table->pages[i]);
    }

    free(table);
}

void *table_row_slot(Table *table, uint32_t row_num) {
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void *page = table->pages[page_num];
    if (page == NULL) {
        // Allocate memory only when we try to access page
        page = table->pages[page_num] = malloc(PAGE_SIZE);
    }

    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;

    return page + byte_offset;
}

/* -------------------------------------------------------------------------- */
/*                                 STATEMENTS                                 */
/* -------------------------------------------------------------------------- */

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,
    PREPARE_STRING_TOO_LONG,
    PREPARE_UNRECOGNIZED_STATEMENT,
} PrepareResult;

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL,
} ExecuteResult;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT,
} StatementType;

typedef struct {
    StatementType type;
    Row row_to_insert;  // Only used by INSERT
} Statement;

PrepareResult statement_prepare_insert(InputBuffer *input_buffer, Statement *statement) {
    statement->type = STATEMENT_INSERT;

    char *keyword = strtok(input_buffer->buffer, " ");
    (void)keyword;
    
    char *id_str = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *email = strtok(NULL, " ");

    if (id_str == NULL || username == NULL || email == NULL) return PREPARE_SYNTAX_ERROR;
    if (strlen(username) > COLUMN_USERNAME_SIZE || strlen(email) > COLUMN_EMAIL_SIZE) return PREPARE_STRING_TOO_LONG;

    int id = atoi(id_str);
    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);

    return PREPARE_SUCCESS;
}

PrepareResult statement_prepare(InputBuffer *input_buffer, Statement *statement) {
    if (strncmp(input_buffer->buffer, "insert", 6) == 0)
        return statement_prepare_insert(input_buffer, statement);

    if (strcmp(input_buffer->buffer, "select") == 0) {  
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult statement_execute_insert(Statement *statement, Table *table) {
    if (table->num_rows >= TABLE_MAX_ROWS) return EXECUTE_TABLE_FULL;

    Row *row_to_insert = &(statement->row_to_insert);
    row_serialize(row_to_insert, table_row_slot(table, table->num_rows));
    table->num_rows++;

    return EXECUTE_SUCCESS;
}

ExecuteResult statement_execute_select(Statement *statement, Table *table) {
    (void)statement;
    Row row;

    for (uint32_t i = 0; i < table->num_rows; i++) {
        row_deserialize(table_row_slot(table, i), &row);
        row_print(&row);
    }

    return EXECUTE_SUCCESS;
}

ExecuteResult statement_execute(Statement *statement, Table *table) {
    switch (statement->type) {
        case STATEMENT_INSERT:
            return statement_execute_insert(statement, table);

        case STATEMENT_SELECT:
            return statement_execute_select(statement, table);
    }

    return EXECUTE_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/*                                    REPL                                    */
/* -------------------------------------------------------------------------- */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    Table *table = table_new();
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

            case PREPARE_SYNTAX_ERROR:
                fprintf(stderr, "Syntax error. Could not parse statement.\n");
                continue;
            
            case PREPARE_STRING_TOO_LONG:
                fprintf(stderr, "String is too long.\n");
                continue;

            case PREPARE_UNRECOGNIZED_STATEMENT:
                fprintf(stderr, "Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
                continue;
        }

        switch (statement_execute(&statement, table)) {
            case EXECUTE_SUCCESS:
                printf("Executed.\n");
                break;
            
            case EXECUTE_TABLE_FULL:
                fprintf(stderr, "Error: Table full.\n");
                break;
        }
    }

    return EXIT_SUCCESS;
}