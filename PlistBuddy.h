#ifndef PLISTBUDDY_H
#define PLISTBUDDY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#else
typedef const void *CFTypeRef;
extern void CFRelease(CFTypeRef cf);
#endif

typedef enum PBOutputMode {
    PB_OUTPUT_READABLE = 0,
    PB_OUTPUT_XML = 1,
} PBOutputMode;

typedef enum PBPropType {
    PB_PROP_INVALID = 0,
    PB_PROP_STRING = 1,
    PB_PROP_ARRAY = 2,
    PB_PROP_DICT = 3,
    PB_PROP_BOOL = 4,
    PB_PROP_REAL = 5,
    PB_PROP_INTEGER = 6,
    PB_PROP_DATE = 7,
    PB_PROP_DATA = 8,
} PBPropType;

typedef enum PBCommandKind {
    PB_CMD_INVALID = 0,
    PB_CMD_HELP = 1,
    PB_CMD_EXIT = 2,
    PB_CMD_SAVE = 3,
    PB_CMD_REVERT = 4,
    PB_CMD_CLEAR = 5,
    PB_CMD_PRINT = 6,
    PB_CMD_SET = 7,
    PB_CMD_ADD = 8,
    PB_CMD_COPY = 9,
    PB_CMD_DELETE = 10,
    PB_CMD_MERGE = 11,
    PB_CMD_IMPORT = 12,
    PB_CMD_CD = 13,
} PBCommandKind;

typedef struct PBState {
    PBOutputMode output_mode;
    uint8_t interactive;
    uint8_t dirty;
    uint16_t reserved;
    CFTypeRef plist;
    CFTypeRef current;
    char *path;
} PBState;

typedef struct PBCommand {
    PBCommandKind kind;
    PBPropType value_type;
    char *entry;
    char *entry2;
    int64_t payload_len;
    void *payload;
    const char *options;
} PBCommand;

extern int returnCode;

PBState *InitState(PBState *state);
void CleanupState(PBState *state);
int ParseArgs(int argc, char **argv, PBState *state, PBCommand **commands);
PBCommand *CreateCommand(void);
void DisposeCommand(PBCommand *cmd);
int ReadCommand(PBCommand *cmd);
void FixWorkingContainer(PBState *state, CFTypeRef entry);

void SavePlist(CFTypeRef plist, const char *path);
CFDataRef ReadData(char *path);
CFPropertyListRef ReadPlist(char *path);
int RevertPlist(PBState *state);

int VerifyCommandToken(const char *text, const char *token, const char **remaining);
PBPropType PropertyTypeFromString(const char *text);
int ParseAtom(const char *text, char *buffer, int *offset, int single_word);
char *ExtractAtom(const char *text, const char **next, int single_word);
PBPropType ExtractPropertyType(const char *text, const char **next, const char *error_message);
char *ExtractEntry(const char *text, const char **next, const char *error_message);
int ParseCmnd(const char *text, PBCommand *cmd);

PBPropType PropertyTypeOfEntry(CFTypeRef entry);
char *GetAbsoluteTimeString(CFAbsoluteTime absolute_time);
double GetAbsoluteTimeFromString(const char *text);
char *FixUpName(const char *name);
const char *GetEntryName(const char *entry);
CFTypeRef GetEntryInPlist(CFTypeRef cf, const char *name, int allow_empty);
CFTypeRef ResolveEntryParent(CFTypeRef plist, CFTypeRef current, const char *entry, int create_missing);
CFTypeRef ResolveEntryStringToRef(const PBState *state, const char *entry);
CFTypeRef CreateEntryWithValue(PBPropType type, CFIndex length, char *text);
int AddEntryToPlist(CFTypeRef plist, const void *entry, const char *name);
int AddMultipleEntriesToPlist(CFTypeRef dst, const void *src, int recursive);
CFTypeRef CreateCopyOfEntry(CFTypeRef entry);
int RemoveEntryFromPlist(CFTypeRef plist, const char *name);

void PrintUsage(char **argv);
void PrintHelp(void);
void PrintEntryReadable(char *format, CFTypeRef entry);
void PrintEntryXML(CFPropertyListRef property_list);
void PrintEntry(const PBState *state, CFPropertyListRef property_list);

void CommandAdd(PBState *state, PBCommand *cmd);
void CommandCopy(PBState *state, PBCommand *cmd);
void CommandMerge(PBState *state, PBCommand *cmd);
void CommandImport(PBState *state, PBCommand *cmd);
void CommandSet(PBState *state, PBCommand *cmd);
void CommandPrint(PBState *state, PBCommand *cmd);
void CommandDelete(PBState *state, PBCommand *cmd);
void CommandCD(PBState *state, PBCommand *cmd);
void ExecuteCommand(PBState *state, PBCommand *cmd);

#endif
