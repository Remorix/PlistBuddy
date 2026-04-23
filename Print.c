#include "PlistBuddy.h"

void PrintUsage(char **argv)
{
    char *program = argv[0];
    char *cursor = program;

    while (*cursor != '\0') {
        ++cursor;
    }

    if (cursor > program) {
        char *scan = cursor - 1;

        while (scan > program && *scan != '/') {
            --scan;
        }

        if (*scan == '/') {
            program = scan + 1;
        }
    }

    printf("Usage: %s [-cxh] <file.plist>\n", program);
    puts("    -c \"<command>\" execute command, otherwise run in interactive mode");
    puts("    -x output will be in the form of an xml plist where appropriate");
    puts("    -h print the complete help info, with command guide");
    putchar('\n');
}

void PrintHelp(void)
{
    puts("Command Format:");
    puts("    Help - Prints this information");
    puts("    Exit - Exits the program, changes are not saved to the file");
    puts("    Save - Saves the current changes to the file");
    puts("    Revert - Reloads the last saved version of the file");
    puts("    Clear [<Type>] - Clears out all existing entries, and creates root of Type");
    puts("    Print [<Entry>] - Prints value of Entry.  Otherwise, prints file");
    puts("    Set <Entry> <Value> - Sets the value at Entry to Value");
    puts("    Add <Entry> <Type> [<Value>] - Adds Entry to the plist, with value Value");
    puts("    Copy <EntrySrc> <EntryDst> - Copies the EntrySrc property to EntryDst");
    puts("    Delete <Entry> - Deletes Entry from the plist");
    puts("    Merge <file.plist> [<Entry>] - Adds the contents of file.plist to Entry");
    puts("    Import <Entry> <file> - Creates or sets Entry the contents of file");
    putchar('\n');
    puts("Entry Format:");
    puts("    Entries consist of property key names delimited by colons.  Array items");
    puts("    are specified by a zero-based integer index.  Examples:");
    puts("        :CFBundleShortVersionString");
    puts("        :CFBundleDocumentTypes:2:CFBundleTypeExtensions");
    putchar('\n');
    puts("Types:");
    puts("    string");
    puts("    array");
    puts("    dict");
    puts("    bool");
    puts("    real");
    puts("    integer");
    puts("    date");
    puts("    data");
    putchar('\n');
    puts("Examples:");
    puts("    Set :CFBundleIdentifier com.apple.plistbuddy");
    puts("        Sets the CFBundleIdentifier property to com.apple.plistbuddy");
    puts("    Add :CFBundleGetInfoString string \"App version 1.0.1\"");
    puts("        Adds the CFBundleGetInfoString property to the plist");
    puts("    Add :CFBundleDocumentTypes: dict");
    puts("        Adds a new item of type dict to the CFBundleDocumentTypes array");
    puts("    Add :CFBundleDocumentTypes:0 dict");
    puts("        Adds the new item to the beginning of the array");
    puts("    Delete :CFBundleDocumentTypes:0 dict");
    puts("        Deletes the FIRST item in the array");
    puts("    Delete :CFBundleDocumentTypes");
    puts("        Deletes the ENTIRE CFBundleDocumentTypes array");
    putchar('\n');
}

void PrintEntryReadable(char *format, CFTypeRef entry)
{
    char indent[8];
    char *prefix;
    char *marker;
    const char *suffix;
    CFTypeID type;

    strcpy(indent, "    ");
    prefix = calloc(1, strlen(format));
    marker = strstr(format, "%s");
    memcpy(prefix, format, (size_t)(marker - format));
    suffix = strstr(format, "%s") + 2;
    type = CFGetTypeID(entry);

    if (type == CFStringGetTypeID()) {
        char *buffer = malloc(0x1000);

        if (CFStringGetCString((CFStringRef)entry, buffer, 4096, kCFStringEncodingUTF8)) {
            printf("%s", buffer);
        }
        free(buffer);
        free(prefix);
        return;
    }

    if (type == CFArrayGetTypeID()) {
        char *nested_format = malloc(strlen(format) + 5);
        CFIndex count;

        sprintf(nested_format, "%s%s", indent, format);
        printf("Array {%s\n", suffix);
        count = CFArrayGetCount((CFArrayRef)entry);
        for (CFIndex index = 0; index < count; ++index) {
            CFTypeRef item;

            printf("%s%s", indent, prefix);
            item = CFArrayGetValueAtIndex((CFArrayRef)entry, index);
            if (item != NULL) {
                PrintEntryReadable(nested_format, item);
            }
            puts(suffix);
        }
        printf("%s}", prefix);
        free(nested_format);
        free(prefix);
        return;
    }

    if (type == CFDictionaryGetTypeID()) {
        char *nested_format = malloc(strlen(format) + 5);
        CFIndex count;
        const void **keys;
        const void **values;

        sprintf(nested_format, "%s%s", indent, format);
        printf("Dict {%s\n", suffix);
        count = CFDictionaryGetCount((CFDictionaryRef)entry);
        keys = malloc(sizeof(void *) * (size_t)count);
        values = malloc(sizeof(void *) * (size_t)count);
        CFDictionaryGetKeysAndValues((CFDictionaryRef)entry, keys, values);

        for (CFIndex index = 0; index < count; ++index) {
            printf("%s%s", indent, prefix);
            PrintEntryReadable(nested_format, (CFTypeRef)keys[index]);
            printf(" = ");
            PrintEntryReadable(nested_format, (CFTypeRef)values[index]);
            puts(suffix);
        }

        printf("%s}", prefix);
        free(keys);
        free(values);
        free(nested_format);
        free(prefix);
        return;
    }

    if (type == CFBooleanGetTypeID()) {
        printf("%s", entry == kCFBooleanTrue ? "true" : "false");
    } else if (type == CFNumberGetTypeID()) {
        if (CFNumberIsFloatType((CFNumberRef)entry)) {
            float real_value = 0.0f;

            CFNumberGetValue((CFNumberRef)entry, kCFNumberFloat32Type, &real_value);
            printf("%f", (double)real_value);
        } else {
            int64_t integer_value = 0;

            CFNumberGetValue((CFNumberRef)entry, kCFNumberLongLongType, &integer_value);
            printf("%lld", integer_value);
        }
    } else if (type == CFDateGetTypeID()) {
        char *time_string = GetAbsoluteTimeString(CFDateGetAbsoluteTime((CFDateRef)entry));

        if (time_string != NULL) {
            printf("%s", time_string);
            free(time_string);
        }
    } else if (type == CFDataGetTypeID()) {
        const UInt8 *bytes = CFDataGetBytePtr((CFDataRef)entry);
        CFIndex length = CFDataGetLength((CFDataRef)entry);

        for (CFIndex index = 0; index < length; ++index) {
            putchar((char)bytes[index]);
        }
    } else {
        printf("%s", "Unprintable Data");
    }

    free(prefix);
}

void PrintEntryXML(CFPropertyListRef property_list)
{
    CFDataRef xml_data = CFPropertyListCreateXMLData(kCFAllocatorDefault, property_list);

    if (xml_data != NULL) {
        PrintEntryReadable("%s", xml_data);
        CFRelease(xml_data);
    }
}

void PrintEntry(const PBState *state, CFPropertyListRef property_list)
{
    if (state->output_mode == PB_OUTPUT_XML) {
        PrintEntryXML(property_list);
    } else if (state->output_mode != PB_OUTPUT_READABLE) {
        CFShow(property_list);
    } else {
        PrintEntryReadable("%s", property_list);
        putchar('\n');
    }
}
