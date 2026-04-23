#include "PlistBuddy.h"

int VerifyCommandToken(const char *text, const char *token, const char **remaining)
{
    size_t token_len = strlen(token);

    if (strncasecmp(text, token, token_len) != 0) {
        return 0;
    }

    *remaining = &text[token_len];
    return 1;
}

PBPropType PropertyTypeFromString(const char *text)
{
    if (strcasecmp(text, "string") == 0) {
        return PB_PROP_STRING;
    }

    if (strcasecmp(text, "array") == 0) {
        return PB_PROP_ARRAY;
    }

    if (strcasecmp(text, "dict") == 0) {
        return PB_PROP_DICT;
    }

    if (strcasecmp(text, "bool") == 0) {
        return PB_PROP_BOOL;
    }

    if (strcasecmp(text, "real") == 0) {
        return PB_PROP_REAL;
    }

    if (strcasecmp(text, "integer") == 0) {
        return PB_PROP_INTEGER;
    }

    if (strcasecmp(text, "date") == 0) {
        return PB_PROP_DATE;
    }

    return strcasecmp(text, "data") == 0 ? PB_PROP_DATA : PB_PROP_INVALID;
}

int ParseAtom(const char *text, char *buffer, int *offset, int single_word)
{
    bool escaped = false;
    bool in_single_quote = false;
    bool in_double_quote = false;
    int out_len = 0;

    while (1) {
        unsigned char ch = (unsigned char)text[*offset];

        if (escaped) {
            if (ch == 't') {
                if (buffer != NULL) {
                    buffer[out_len] = '\t';
                }
                escaped = false;
                ++out_len;
            } else if (ch == 'n') {
                if (buffer != NULL) {
                    buffer[out_len] = '\n';
                }
                escaped = false;
                ++out_len;
            } else if (ch == ':') {
                if (buffer != NULL) {
                    buffer[out_len] = '\\';
                    buffer[out_len + 1] = ':';
                }
                escaped = false;
                out_len += 2;
            } else {
                if (buffer != NULL) {
                    buffer[out_len] = (char)ch;
                }
                escaped = false;
                ++out_len;
            }
        } else if (ch == '\\') {
            escaped = true;
        } else if (!single_word || in_single_quote || in_double_quote) {
            if (ch == '"') {
                in_double_quote = !in_double_quote;
            } else if (ch == '\'') {
                in_single_quote = !in_single_quote;
            } else {
                if (buffer != NULL) {
                    buffer[out_len] = (char)ch;
                }
                ++out_len;
            }
        } else if (ch == '"') {
            in_double_quote = !in_double_quote;
        } else if (ch == '\t' || ch == ' ') {
            in_single_quote = false;
            in_double_quote = false;
            break;
        } else if (ch == '\'') {
            in_single_quote = !in_single_quote;
        } else {
            if (buffer != NULL) {
                buffer[out_len] = (char)ch;
            }
            ++out_len;
        }

        if (text[*offset] == '\0') {
            break;
        }
        ++(*offset);
    }

    if (in_single_quote || in_double_quote) {
        puts("Parse Error: Unclosed Quotes");
        return 0;
    }

    if (buffer != NULL) {
        buffer[out_len] = '\0';
    }

    return out_len;
}

char *ExtractAtom(const char *text, const char **next, int single_word)
{
    int offset = 0;
    int length;
    char *atom;

    while (*text == ' ' || *text == '\t') {
        ++text;
    }

    length = ParseAtom(text, NULL, &offset, single_word);
    if (length < 1) {
        return NULL;
    }

    atom = malloc((size_t)length + 1);
    if (atom == NULL) {
        return NULL;
    }

    offset = 0;
    ParseAtom(text, atom, &offset, single_word);
    if (next != NULL) {
        *next = text + offset;
    }

    return atom;
}

PBPropType ExtractPropertyType(const char *text, const char **next, const char *error_message)
{
    char *atom = ExtractAtom(text, next, 1);
    PBPropType type = PB_PROP_INVALID;

    if (atom != NULL) {
        type = PropertyTypeFromString(atom);
        if (type == PB_PROP_INVALID) {
            printf("Unrecognized Type: %s\n", atom);
        }
        free(atom);
        return type;
    }

    if (error_message != NULL) {
        printf("%s", error_message);
    }

    return PB_PROP_INVALID;
}

char *ExtractEntry(const char *text, const char **next, const char *error_message)
{
    char *entry = ExtractAtom(text, next, 1);

    if (error_message != NULL && entry == NULL) {
        printf("%s", error_message);
    }

    return entry;
}

int ParseCmnd(const char *text, PBCommand *cmd)
{
    const char *cursor = NULL;

    if (strncasecmp(text, "Help", 4) == 0 || strncasecmp(text, "?", 1) == 0) {
        cmd->kind = PB_CMD_HELP;
        return 1;
    }

    if (strncasecmp(text, "Exit", 4) == 0 || strncasecmp(text, "Quit", 4) == 0) {
        cursor = text + 4;
    } else if (!VerifyCommandToken(text, "Bye", &cursor)) {
        if (VerifyCommandToken(text, "Save", &cursor)) {
            cmd->kind = PB_CMD_SAVE;
            return 1;
        }

        if (VerifyCommandToken(text, "Revert", &cursor)) {
            cmd->kind = PB_CMD_REVERT;
            return 1;
        }

        if (VerifyCommandToken(text, "Clear", &cursor)) {
            cmd->kind = PB_CMD_CLEAR;
            cmd->value_type = ExtractPropertyType(cursor, &cursor, NULL);
            return 1;
        }

        if (VerifyCommandToken(text, "Print", &cursor) ||
            VerifyCommandToken(text, "ls", &cursor)) {
            cmd->kind = PB_CMD_PRINT;
            cmd->entry = ExtractAtom(cursor, &cursor, 1);
            return 1;
        }

        if (VerifyCommandToken(text, "Set", &cursor) ||
            VerifyCommandToken(text, "=", &cursor)) {
            cmd->kind = PB_CMD_SET;
            cmd->entry = ExtractEntry(cursor, &cursor, "Entry Required for Set Command\n");
            if (cmd->entry == NULL) {
                return 0;
            }

            cmd->payload = ExtractAtom(cursor, &cursor, 0);
            if (cmd->payload != NULL) {
                cmd->payload_len = (int64_t)strlen((const char *)cmd->payload);
                return 1;
            }

            cmd->payload_len = 0;
            puts("Value Required for Set Command");
            return 0;
        }

        if (VerifyCommandToken(text, "Add", &cursor) ||
            VerifyCommandToken(text, "+", &cursor)) {
            cmd->kind = PB_CMD_ADD;
            cmd->entry = ExtractEntry(cursor, &cursor, "Entry Required for Add Command\n");
            if (cmd->entry == NULL) {
                return 0;
            }

            cmd->value_type = ExtractPropertyType(cursor, &cursor, "Type Required for Add Command\n");
            if (cmd->value_type == PB_PROP_INVALID) {
                return 0;
            }

            cmd->payload = ExtractAtom(cursor, NULL, 0);
            if (cmd->payload != NULL) {
                cmd->payload_len = (int64_t)strlen((const char *)cmd->payload);
            } else {
                cmd->payload_len = 0;
            }

            return 1;
        }

        if (VerifyCommandToken(text, "Copy", &cursor) ||
            VerifyCommandToken(text, "cp", &cursor)) {
            cmd->kind = PB_CMD_COPY;
            cmd->entry = ExtractEntry(cursor, &cursor, "Entry Required for Copy Command\n");
            if (cmd->entry == NULL) {
                return 0;
            }

            cmd->entry2 = ExtractEntry(cursor, &cursor, "Two Entries Required for Copy Command\n");
            return cmd->entry2 != NULL;
        }

        if (VerifyCommandToken(text, "Delete", &cursor) ||
            VerifyCommandToken(text, "Remove", &cursor) ||
            VerifyCommandToken(text, "rm", &cursor) ||
            VerifyCommandToken(text, "-", &cursor)) {
            cmd->kind = PB_CMD_DELETE;
            cmd->entry = ExtractEntry(cursor, &cursor, "Entry Required for Delete Command\n");
            return cmd->entry != NULL;
        }

        if (VerifyCommandToken(text, "Merge", &cursor)) {
            cmd->kind = PB_CMD_MERGE;
            cmd->payload = ExtractAtom(cursor, &cursor, 1);
            if (cmd->payload != NULL) {
                cmd->payload_len = (int64_t)strlen((const char *)cmd->payload);
                if (strcasecmp((const char *)cmd->payload, "-r") == 0) {
                    cmd->options = "r";
                    cmd->payload = ExtractAtom(cursor, &cursor, 1);
                    if (cmd->payload != NULL) {
                        cmd->payload_len = (int64_t)strlen((const char *)cmd->payload);
                    } else {
                        cmd->payload_len = 0;
                        puts("File required for Merge Command");
                        return 0;
                    }
                }

                cmd->entry = ExtractAtom(cursor, NULL, 1);
                return 1;
            }

            cmd->payload_len = 0;
            puts("File required for Merge Command");
            return 0;
        }

        if (VerifyCommandToken(text, "Import", &cursor)) {
            cmd->kind = PB_CMD_IMPORT;
            cmd->entry = ExtractEntry(cursor, &cursor, "Entry Required for Import Command\n");
            if (cmd->entry == NULL) {
                return 0;
            }

            cmd->payload = ExtractAtom(cursor, &cursor, 1);
            if (cmd->payload != NULL) {
                cmd->payload_len = (int64_t)strlen((const char *)cmd->payload);
                return 1;
            }

            cmd->payload_len = 0;
            puts("File required for Import Command");
            return 0;
        }

        if (VerifyCommandToken(text, "CD", &cursor)) {
            cmd->kind = PB_CMD_CD;
            cmd->entry = ExtractEntry(cursor, &cursor, "Entry Required for CD Command\n");
            return cmd->entry != NULL;
        }

        puts("Unrecognized Command");
        return 0;
    }

    cmd->kind = PB_CMD_EXIT;
    return 1;
}
