#include "PlistBuddy.h"

int returnCode = 0;

int main(int argc, char **argv)
{
    PBState state;
    PBCommand *commands[15];
    int parsed;

    InitState(&state);
    memset(commands, 0, sizeof(commands));

    parsed = ParseArgs(argc, argv, &state, commands);
    if (parsed) {
        if (commands[0] != NULL) {
            for (int index = 0; commands[index] != NULL; ++index) {
                ExecuteCommand(&state, commands[index]);
                DisposeCommand(commands[index]);
            }

            if (state.dirty) {
                SavePlist(state.plist, state.path);
            }
        } else if (state.interactive) {
            do {
                PBCommand *cmd = CreateCommand();

                if (ReadCommand(cmd)) {
                    ExecuteCommand(&state, cmd);
                }
                DisposeCommand(cmd);
            } while (state.interactive);
        }
    } else {
        for (int index = 0; commands[index] != NULL; ++index) {
            DisposeCommand(commands[index]);
        }
        returnCode = 1;
    }

    CleanupState(&state);
    return returnCode;
}
