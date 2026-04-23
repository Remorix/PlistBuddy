#include "PlistBuddy.h"

PBState *InitState(PBState *state)
{
    state->output_mode = PB_OUTPUT_READABLE;
    state->interactive = 1;
    state->dirty = 0;
    state->reserved = 0;
    state->plist = NULL;
    state->current = NULL;
    state->path = NULL;
    return state;
}

void CleanupState(PBState *state)
{
    if (state->plist != NULL) {
        CFRelease(state->plist);
    }

    if (state->path != NULL) {
        free(state->path);
    }
}

int ParseArgs(int argc, char **argv, PBState *state, PBCommand **commands)
{
    int command_count = 0;

    *commands = NULL;

    for (int argi = 1; argi < argc; ++argi) {
        const char *arg = argv[argi];

        if (strcmp(arg, "-h") == 0 ||
            strcmp(arg, "--help") == 0 ||
            strcmp(arg, "-help") == 0) {
            PrintHelp();
            return 0;
        }

        if (strcmp(arg, "-c") == 0) {
            PBCommand *cmd = malloc(sizeof(*cmd));

            if (cmd == NULL) {
                return 0;
            }

            memset(cmd, 0, sizeof(*cmd));
            commands[command_count] = cmd;

            ++argi;
            if (argi == argc) {
                break;
            }

            if (!ParseCmnd(argv[argi], cmd)) {
                DisposeCommand(commands[command_count]);
                commands[command_count] = NULL;
                return 0;
            }

            ++command_count;
            commands[command_count] = NULL;
            continue;
        }

        if (strcmp(arg, "-x") == 0) {
            state->output_mode = PB_OUTPUT_XML;
            continue;
        }

        if (state->path != NULL) {
            puts("Invalid Arguments\n");
            return 0;
        }

        state->path = malloc(strlen(arg) + 1);
        if (state->path == NULL) {
            return 0;
        }

        strcpy(state->path, arg);
        if (!RevertPlist(state)) {
            return 0;
        }
    }

    if (state->plist != NULL) {
        return 1;
    }

    PrintUsage(argv);
    return 0;
}

PBCommand *CreateCommand(void)
{
    PBCommand *cmd = malloc(sizeof(*cmd));

    if (cmd != NULL) {
        memset(cmd, 0, sizeof(*cmd));
    }

    return cmd;
}

void DisposeCommand(PBCommand *cmd)
{
    if (cmd->entry != NULL) {
        free(cmd->entry);
    }

    if (cmd->entry2 != NULL) {
        free(cmd->entry2);
    }

    if (cmd->payload != NULL) {
        free(cmd->payload);
    }

    free(cmd);
}

int ReadCommand(PBCommand *cmd)
{
    char text[512];
    char *cursor = text;

    printf("Command: ");

    do {
        if (scanf("%c", cursor) == -1) {
            cmd->kind = PB_CMD_EXIT;
            return 1;
        }

        if (*cursor == '\n') {
            break;
        }

        ++cursor;
    } while (cursor < &text[sizeof(text) - 1]);

    *cursor = '\0';

    return text[0] != '\0' && ParseCmnd(text, cmd);
}

void FixWorkingContainer(PBState *state, CFTypeRef entry)
{
    if (state->current == entry) {
        puts("Working Container has become Invalid.  Setting to :");
        state->current = state->plist;
    }
}
