#include "PlistBuddy.h"

void CommandAdd(PBState *state, PBCommand *cmd)
{
    CFTypeRef entry = CreateEntryWithValue(
        cmd->value_type, (CFIndex)cmd->payload_len, (char *)cmd->payload);
    CFTypeRef parent;
    const char *entry_name;
    char *fixed;

    if (entry == NULL) {
        return;
    }

    parent = ResolveEntryParent(state->plist, state->current, cmd->entry, 1);
    entry_name = GetEntryName(cmd->entry);
    if (entry_name != NULL) {
        fixed = FixUpName(entry_name);
        if (parent == NULL) {
            fprintf(stderr, "Add: Entry, \"%s\", Incorrectly Specified\n", cmd->entry);
            goto done;
        }
    } else {
        fixed = NULL;
        if (parent == NULL) {
            fprintf(stderr, "Add: Entry, \"%s\", Incorrectly Specified\n", cmd->entry);
            goto done;
        }
    }

    if (GetEntryInPlist(parent, fixed, 1) != NULL) {
        fprintf(stderr, "Add: \"%s\" Entry Already Exists\n", cmd->entry);
        returnCode = 1;
        goto done;
    }

    if (AddEntryToPlist(parent, entry, fixed) != 0) {
        fprintf(stderr, "Add: Can't Add Entry, \"%s\", to Parent\n", cmd->entry);
        returnCode = 1;
    }

done:
    if (fixed != NULL) {
        free(fixed);
    }
    CFRelease(entry);
}

void CommandCopy(PBState *state, PBCommand *cmd)
{
    CFTypeRef source = ResolveEntryStringToRef(state, cmd->entry);
    CFTypeRef entry_copy;
    CFTypeRef parent;
    const char *entry_name;
    char *fixed;

    if (source == NULL) {
        fprintf(stderr, "Copy: Entry, \"%s\", Does Not Exist\n", cmd->entry);
        returnCode = 1;
        return;
    }

    entry_copy = CreateCopyOfEntry(source);
    parent = ResolveEntryParent(state->plist, state->current, cmd->entry2, 1);
    entry_name = GetEntryName(cmd->entry2);
    if (entry_name != NULL) {
        fixed = FixUpName(entry_name);
        if (parent == NULL) {
            fprintf(stderr, "Copy: Entry, \"%s\", Incorrectly Specified\n", cmd->entry2);
            goto done;
        }
    } else {
        fixed = NULL;
        if (parent == NULL) {
            fprintf(stderr, "Copy: Entry, \"%s\", Incorrectly Specified\n", cmd->entry2);
            goto done;
        }
    }

    if (GetEntryInPlist(parent, fixed, 1) != NULL) {
        fprintf(stderr, "Copy: \"%s\" Entry Already Exists\n", cmd->entry2);
        returnCode = 1;
        goto done;
    }

    if (AddEntryToPlist(parent, entry_copy, fixed) != 0) {
        fprintf(stderr, "Copy: Can't Add Entry, \"%s\", to Parent\n", cmd->entry2);
        returnCode = 1;
    }

done:
    if (fixed != NULL) {
        free(fixed);
    }
    CFRelease(entry_copy);
}

void CommandMerge(PBState *state, PBCommand *cmd)
{
    CFTypeRef current;
    CFPropertyListRef plist;
    PBPropType type;

    if (cmd->entry != NULL) {
        current = ResolveEntryStringToRef(state, cmd->entry);
        if (current == NULL) {
            fprintf(stderr, "Merge: \"%s\" Entry Does Not Exist\n", cmd->entry);
            returnCode = 1;
            return;
        }

        type = PropertyTypeOfEntry(current);
        cmd->value_type = type;
        if ((type & ~1U) != PB_PROP_ARRAY) {
            fwrite("Merge: Specified Entry Must Be a Container\n", 43, 1, stderr);
            returnCode = 1;
            return;
        }
    } else {
        current = state->current;
    }

    plist = ReadPlist((char *)cmd->payload);
    if (plist == NULL) {
        fprintf(stderr, "Merge: Error Reading File: %s\n", (const char *)cmd->payload);
        returnCode = 1;
        return;
    }

    type = PropertyTypeOfEntry(plist);
    if (type == cmd->value_type || type != PB_PROP_DICT) {
        if (type == cmd->value_type || type != PB_PROP_ARRAY) {
            AddMultipleEntriesToPlist(
                current,
                plist,
                cmd->options != NULL && strchr(cmd->options, 'r') != NULL);
        } else {
            fwrite("Merge: Can't Add array Entries to dict\n", 39, 1, stderr);
            returnCode = 1;
        }
    } else {
        fwrite("Merge: Can't Add dict Entries to array\n", 39, 1, stderr);
        returnCode = 1;
    }

    CFRelease(plist);
}

void CommandImport(PBState *state, PBCommand *cmd)
{
    CFTypeRef entry = ResolveEntryStringToRef(state, cmd->entry);
    CFDataRef data;

    if (entry != NULL) {
        cmd->value_type = PropertyTypeOfEntry(entry);
        if ((cmd->value_type & ~1U) == PB_PROP_ARRAY) {
            fwrite("Import: Specified Entry Must Not Be a Container\n", 48, 1, stderr);
            returnCode = 1;
            return;
        }
    } else {
        cmd->value_type = PB_PROP_DATA;
    }

    data = ReadData((char *)cmd->payload);
    if (data == NULL) {
        fprintf(stderr, "Import: Error Reading File: %s\n", (const char *)cmd->payload);
        returnCode = 1;
        return;
    }

    free(cmd->payload);
    cmd->payload_len = CFDataGetLength(data);
    cmd->payload = malloc((size_t)cmd->payload_len);
    if (cmd->payload_len > 0) {
        CFRange range;

        range.location = 0;
        range.length = CFDataGetLength(data);
        CFDataGetBytes(data, range, (UInt8 *)cmd->payload);
    }
    CFRelease(data);

    if (entry != NULL) {
        CommandSet(state, cmd);
    } else {
        CommandAdd(state, cmd);
    }
}

void CommandSet(PBState *state, PBCommand *cmd)
{
    CFTypeRef parent = ResolveEntryParent(state->plist, state->current, cmd->entry, 0);
    char *fixed;
    CFTypeRef existing;
    CFTypeRef replacement;

    if (GetEntryName(cmd->entry) != NULL) {
        fixed = FixUpName(GetEntryName(cmd->entry));
        if (parent == NULL) {
            fprintf(stderr, "Set: Entry, \"%s\", Does Not Exist\n", cmd->entry);
            returnCode = 1;
            goto done;
        }
    } else {
        fixed = NULL;
        if (parent == NULL) {
            fprintf(stderr, "Set: Entry, \"%s\", Does Not Exist\n", cmd->entry);
            returnCode = 1;
            goto done;
        }
    }

    existing = GetEntryInPlist(parent, fixed, 0);
    if (existing == NULL) {
        fprintf(stderr, "Set: Entry, \"%s\", Does Not Exist\n", cmd->entry);
        returnCode = 1;
        goto done;
    }

    cmd->value_type = PropertyTypeOfEntry(existing);
    if ((cmd->value_type & ~1U) == PB_PROP_ARRAY) {
        fwrite("Set: Cannot Perform Set On Containers\n", 38, 1, stderr);
        returnCode = 1;
        goto done;
    }

    replacement = CreateEntryWithValue(
        cmd->value_type, (CFIndex)cmd->payload_len, (char *)cmd->payload);
    if (replacement != NULL) {
        if (existing == state->plist) {
            CFRelease(existing);
            state->plist = replacement;
            state->current = replacement;
        } else {
            RemoveEntryFromPlist(parent, fixed);
            if (AddEntryToPlist(parent, replacement, fixed) != 0) {
                fwrite("Set: Internal Error Occurred\n", 29, 1, stderr);
                returnCode = 1;
            }
            CFRelease(replacement);
        }
    }

done:
    if (fixed != NULL) {
        free(fixed);
    }
}

void CommandPrint(PBState *state, PBCommand *cmd)
{
    CFTypeRef entry = ResolveEntryStringToRef(state, cmd->entry);

    if (entry != NULL) {
        PrintEntry(state, (CFPropertyListRef)entry);
    } else {
        fprintf(stderr, "Print: Entry, \"%s\", Does Not Exist\n", cmd->entry);
        returnCode = 1;
    }
}

void CommandDelete(PBState *state, PBCommand *cmd)
{
    CFTypeRef parent = ResolveEntryParent(state->plist, state->current, cmd->entry, 0);
    char *fixed;
    CFTypeRef entry;

    if (GetEntryName(cmd->entry) != NULL) {
        fixed = FixUpName(GetEntryName(cmd->entry));
        if (parent == NULL) {
            fprintf(stderr, "Delete: Entry, \"%s\", Does Not Exist\n", cmd->entry);
            returnCode = 1;
            if (fixed == NULL) {
                return;
            }
            goto done;
        }
    } else {
        fixed = NULL;
        if (parent == NULL) {
            fprintf(stderr, "Delete: Entry, \"%s\", Does Not Exist\n", cmd->entry);
            returnCode = 1;
            goto done;
        }
    }

    entry = GetEntryInPlist(parent, fixed, 0);
    if (entry == NULL) {
        fprintf(stderr, "Delete: Entry, \"%s\", Does Not Exist\n", cmd->entry);
        returnCode = 1;
        goto done;
    }

    if (state->current == entry) {
        puts("Working Container has become Invalid.  Setting to :");
        state->current = state->plist;
    }

    RemoveEntryFromPlist(parent, fixed);

done:
    if (fixed != NULL) {
        free(fixed);
    }
}

void CommandCD(PBState *state, PBCommand *cmd)
{
    CFTypeRef entry = ResolveEntryStringToRef(state, cmd->entry);

    if (entry != NULL) {
        cmd->value_type = PropertyTypeOfEntry(entry);
        if ((cmd->value_type & ~1U) == PB_PROP_ARRAY) {
            state->current = entry;
            return;
        }

        fprintf(stderr, "CD: Entry, \"%s\", Is Not A Container\n", cmd->entry);
    } else {
        fprintf(stderr, "CD: Entry, \"%s\", Does Not Exist\n", cmd->entry);
    }

    returnCode = 1;
}

void ExecuteCommand(PBState *state, PBCommand *cmd)
{
    switch (cmd->kind) {
    case PB_CMD_HELP:
        PrintHelp();
        return;
    case PB_CMD_EXIT:
        state->interactive = 0;
        return;
    case PB_CMD_SAVE:
        puts("Saving...");
        SavePlist(state->plist, state->path);
        state->dirty = 0;
        return;
    case PB_CMD_REVERT:
        puts("Reverting to last saved state...");
        RevertPlist(state);
        state->dirty = 0;
        return;
    case PB_CMD_CLEAR: {
        char empty = '\0';

        puts("Initializing Plist...");
        CFRelease(state->plist);
        if (cmd->value_type == PB_PROP_INVALID) {
            cmd->value_type = PB_PROP_DICT;
        }
        state->plist = CreateEntryWithValue(cmd->value_type, 1, &empty);
        state->current = state->plist;
        state->dirty = 1;
        return;
    }
    case PB_CMD_PRINT:
        CommandPrint(state, cmd);
        return;
    case PB_CMD_SET:
        CommandSet(state, cmd);
        state->dirty = 1;
        return;
    case PB_CMD_ADD:
        CommandAdd(state, cmd);
        state->dirty = 1;
        return;
    case PB_CMD_COPY:
        CommandCopy(state, cmd);
        state->dirty = 1;
        return;
    case PB_CMD_DELETE:
        CommandDelete(state, cmd);
        state->dirty = 1;
        return;
    case PB_CMD_MERGE:
        CommandMerge(state, cmd);
        state->dirty = 1;
        return;
    case PB_CMD_IMPORT:
        CommandImport(state, cmd);
        state->dirty = 1;
        return;
    case PB_CMD_CD:
        CommandCD(state, cmd);
        return;
    default:
        return;
    }
}
