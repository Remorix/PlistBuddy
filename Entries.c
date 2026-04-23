#include "PlistBuddy.h"

PBPropType PropertyTypeOfEntry(CFTypeRef entry)
{
    CFTypeID type = CFGetTypeID(entry);

    if (type == CFStringGetTypeID()) {
        return PB_PROP_STRING;
    }

    if (type == CFArrayGetTypeID()) {
        return PB_PROP_ARRAY;
    }

    if (type == CFDictionaryGetTypeID()) {
        return PB_PROP_DICT;
    }

    if (type == CFBooleanGetTypeID()) {
        return PB_PROP_BOOL;
    }

    if (type == CFNumberGetTypeID()) {
        return CFNumberIsFloatType((CFNumberRef)entry) ? PB_PROP_REAL : PB_PROP_INTEGER;
    }

    if (type == CFDateGetTypeID()) {
        return PB_PROP_DATE;
    }

    return type == CFDataGetTypeID() ? PB_PROP_DATA : PB_PROP_INVALID;
}

char *GetAbsoluteTimeString(CFAbsoluteTime absolute_time)
{
    CFTimeZoneRef timezone = CFTimeZoneCopyDefault();
    CFGregorianDate gregorian = CFAbsoluteTimeGetGregorianDate(absolute_time, timezone);
    struct tm tm_value;
    char *buffer = malloc(0x400);

    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_year = gregorian.year - 1900;
    tm_value.tm_mon = gregorian.month - 1;
    tm_value.tm_mday = gregorian.day;
    tm_value.tm_wday = CFAbsoluteTimeGetDayOfWeek(absolute_time, timezone) % 7;
    tm_value.tm_hour = gregorian.hour;
    tm_value.tm_min = gregorian.minute;
    tm_value.tm_sec = (int)gregorian.second;

    strftime(buffer, 0x400, "%a %b %d %H:%M:%S %Z %Y", &tm_value);
    CFRelease(timezone);
    return buffer;
}

double GetAbsoluteTimeFromString(const char *text)
{
    struct tm tm_value;

    memset(&tm_value, 0, sizeof(tm_value));
    if (strptime(text, "%a %b %d %H:%M:%S %Z %Y", &tm_value) != NULL ||
        strptime(text, "%c", &tm_value) != NULL ||
        strptime(text, "%D", &tm_value) != NULL) {
        CFTimeZoneRef timezone = CFTimeZoneCopyDefault();
        CFGregorianDate date;
        CFAbsoluteTime absolute_time;

        date.year = tm_value.tm_year + 1900;
        date.month = tm_value.tm_mon + 1;
        date.day = tm_value.tm_mday;
        date.hour = tm_value.tm_hour;
        date.minute = tm_value.tm_min;
        date.second = tm_value.tm_sec;
        absolute_time = CFGregorianDateGetAbsoluteTime(date, timezone);
        CFRelease(timezone);
        return absolute_time;
    }

    puts("Unrecognized Date Format");
    return 0.0;
}

char *FixUpName(const char *name)
{
    int name_len = (int)strlen(name);
    char *fixed = malloc((size_t)name_len + 1);
    size_t out = 0;

    for (int i = 0; i < name_len; ++i) {
        if (name[i] == '\\') {
            if (name[i + 1] == ':') {
                fixed[out++] = ':';
                ++i;
            } else {
                fixed[out++] = '\\';
            }
        } else {
            fixed[out++] = name[i];
        }
    }

    fixed[out] = '\0';
    return fixed;
}

const char *GetEntryName(const char *entry)
{
    const char *name = entry;
    int index = (int)strlen(entry) - 1;

    while (index >= 0) {
        if (name[index] == ':' && (index == 0 || name[index - 1] != '\\')) {
            name += index + 1;
            break;
        }
        --index;
    }

    return name;
}

CFTypeRef GetEntryInPlist(CFTypeRef cf, const char *name, int allow_empty)
{
    int index = 0;

    if (name != NULL && (allow_empty || *name != '\0')) {
        CFTypeID type = CFGetTypeID(cf);
        const void *value = NULL;

        if (type == CFDictionaryGetTypeID()) {
            CFStringRef key = CFStringCreateWithCString(
                kCFAllocatorDefault, name, kCFStringEncodingUTF8);
            value = CFDictionaryGetValue((CFDictionaryRef)cf, key);
            CFRelease(key);
        }

        if (type == CFArrayGetTypeID() && !allow_empty) {
            CFIndex count;

            if (*name != '\0') {
                sscanf(name, "%i", &index);
            } else {
                index = (int)CFArrayGetCount((CFArrayRef)cf) - 1;
            }

            count = CFArrayGetCount((CFArrayRef)cf);
            if (count > index && index >= 0) {
                return (CFTypeRef)CFArrayGetValueAtIndex((CFArrayRef)cf, index);
            }
            return NULL;
        }

        return (CFTypeRef)value;
    }

    return cf;
}

CFTypeRef ResolveEntryParent(CFTypeRef plist, CFTypeRef current, const char *entry, int create_missing)
{
    const char *entry_name = GetEntryName(entry);
    int prefix_len = (int)strlen(entry) - (int)strlen(entry_name);

    if (prefix_len >= 1) {
        if (prefix_len == 1 && *entry == ':') {
            return plist;
        }

        char *parent_path = malloc((size_t)prefix_len);
        char *fixed_name;
        CFTypeRef parent;
        CFTypeRef child;

        memcpy(parent_path, entry, (size_t)(prefix_len - 1));
        parent_path[prefix_len - 1] = '\0';

        parent = ResolveEntryParent(plist, current, parent_path, create_missing);
        if (parent == NULL || GetEntryName(parent_path) == NULL) {
            free(parent_path);
            return NULL;
        }

        fixed_name = FixUpName(GetEntryName(parent_path));
        child = GetEntryInPlist(parent, fixed_name, 0);
        if (create_missing && child == NULL) {
            child = CFDictionaryCreateMutable(
                kCFAllocatorDefault,
                0,
                &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks);
            if (AddEntryToPlist(parent, child, fixed_name) != 0) {
                child = NULL;
            }
        }

        free(fixed_name);
        free(parent_path);
        return child;
    }

    return current;
}

CFTypeRef ResolveEntryStringToRef(const PBState *state, const char *entry)
{
    CFTypeRef parent = ResolveEntryParent(state->plist, state->current, entry, 0);
    const char *entry_name = GetEntryName(entry);
    char *fixed_name;
    CFTypeRef resolved;

    if (entry_name == NULL) {
        return parent;
    }

    fixed_name = FixUpName(entry_name);
    if (parent != NULL) {
        resolved = GetEntryInPlist(parent, fixed_name, 0);
    } else {
        resolved = parent;
    }
    free(fixed_name);
    return resolved;
}

CFTypeRef CreateEntryWithValue(PBPropType type, CFIndex length, char *text)
{
    float real_value = 0.0f;
    int64_t integer_value = 0;
    CFNumberRef number;

    switch (type) {
    case PB_PROP_STRING:
        if (text == NULL) {
            return NULL;
        }
        return CFStringCreateWithCString(kCFAllocatorDefault, text, kCFStringEncodingUTF8);
    case PB_PROP_ARRAY:
        return CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    case PB_PROP_DICT:
        return CFDictionaryCreateMutable(
            kCFAllocatorDefault,
            0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    case PB_PROP_BOOL:
        if (text == NULL) {
            return NULL;
        }
        if (strcasecmp(text, "true") == 0 ||
            strcasecmp(text, "yes") == 0 ||
            strcasecmp(text, "1") == 0) {
            return kCFBooleanTrue;
        }
        return kCFBooleanFalse;
    case PB_PROP_REAL:
        if (text == NULL || sscanf(text, "%f", &real_value) == 0) {
            puts("Unrecognized Real Format");
            return NULL;
        }
        number = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloat32Type, &real_value);
        if (number == NULL) {
            puts("Unrecognized Real Format");
            return NULL;
        }
        return number;
    case PB_PROP_INTEGER:
        if (text == NULL || sscanf(text, "%lld", &integer_value) == 0) {
            puts("Unrecognized Integer Format");
            return NULL;
        }
        number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &integer_value);
        if (number == NULL) {
            puts("Unrecognized Integer Format");
            return NULL;
        }
        return number;
    case PB_PROP_DATE: {
        CFAbsoluteTime absolute_time;

        if (text == NULL) {
            return NULL;
        }
        absolute_time = GetAbsoluteTimeFromString(text);
        if (absolute_time == 0.0) {
            return NULL;
        }
        return CFDateCreate(kCFAllocatorDefault, absolute_time);
    }
    case PB_PROP_DATA:
        if (text == NULL) {
            return NULL;
        }
        return CFDataCreate(kCFAllocatorDefault, (const UInt8 *)text, length);
    default:
        return NULL;
    }
}

int AddEntryToPlist(CFTypeRef plist, const void *entry, const char *name)
{
    int index = 0;
    CFTypeID type = CFGetTypeID(plist);

    if (type == CFDictionaryGetTypeID()) {
        CFStringRef key = CFStringCreateWithCString(
            kCFAllocatorDefault, name, kCFStringEncodingUTF8);

        CFDictionaryAddValue((CFMutableDictionaryRef)plist, key, entry);
        CFRelease(key);
        return 0;
    }

    if (type != CFArrayGetTypeID()) {
        return -1;
    }

    if (name != NULL && *name != '\0') {
        CFIndex count;

        sscanf(name, "%i", &index);
        count = CFArrayGetCount((CFArrayRef)plist);
        if (count <= index) {
            index = (int)count;
        }
        CFArrayInsertValueAtIndex((CFMutableArrayRef)plist, index, entry);
    } else {
        CFArrayAppendValue((CFMutableArrayRef)plist, entry);
    }

    return 0;
}

int AddMultipleEntriesToPlist(CFTypeRef dst, const void *src, int recursive)
{
    CFTypeID src_type = CFGetTypeID(src);
    CFTypeID dst_type = CFGetTypeID(dst);

    if (src_type == CFArrayGetTypeID() && dst_type == CFArrayGetTypeID()) {
        CFIndex count = CFArrayGetCount((CFArrayRef)src);

        for (CFIndex index = 0; index < count; ++index) {
            const void *value = CFArrayGetValueAtIndex((CFArrayRef)src, index);

            if (value != NULL) {
                CFArrayAppendValue((CFMutableArrayRef)dst, value);
            }
        }
        return 0;
    }

    if (src_type == CFDictionaryGetTypeID() && dst_type == CFDictionaryGetTypeID()) {
        int count = (int)CFDictionaryGetCount((CFDictionaryRef)src);
        const void **keys = malloc(sizeof(void *) * (size_t)count);
        const void **values = malloc(sizeof(void *) * (size_t)count);

        CFDictionaryGetKeysAndValues((CFDictionaryRef)src, keys, values);
        for (int index = 0; index < count; ++index) {
            const void *existing = CFDictionaryGetValue((CFDictionaryRef)dst, keys[index]);

            if (existing != NULL) {
                if (recursive &&
                    CFGetTypeID(existing) == CFGetTypeID(values[index]) &&
                    (CFGetTypeID(existing) == CFArrayGetTypeID() ||
                     CFGetTypeID(existing) == CFDictionaryGetTypeID())) {
                    AddMultipleEntriesToPlist((CFTypeRef)existing, values[index], 1);
                } else {
                    printf("Duplicate Entry Was Skipped: ");
                    PrintEntryReadable("%s", (CFTypeRef)keys[index]);
                    putchar('\n');
                }
            } else {
                CFDictionaryAddValue((CFMutableDictionaryRef)dst, keys[index], values[index]);
            }
        }

        free(keys);
        free(values);
        return 0;
    }

    return AddEntryToPlist(dst, src, "");
}

CFTypeRef CreateCopyOfEntry(CFTypeRef entry)
{
    CFTypeID type = CFGetTypeID(entry);

    if (type == CFStringGetTypeID()) {
        return CFStringCreateCopy(kCFAllocatorDefault, (CFStringRef)entry);
    }

    if (type == CFArrayGetTypeID()) {
        return CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, (CFArrayRef)entry);
    }

    if (type == CFDictionaryGetTypeID()) {
        return CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, (CFDictionaryRef)entry);
    }

    if (type == CFBooleanGetTypeID()) {
        return entry;
    }

    if (type == CFNumberGetTypeID()) {
        CFNumberType number_type = CFNumberGetType((CFNumberRef)entry);
        uint8_t value[16];

        CFNumberGetValue((CFNumberRef)entry, number_type, value);
        return CFNumberCreate(kCFAllocatorDefault, number_type, value);
    }

    if (type == CFDateGetTypeID()) {
        return CFDateCreate(
            kCFAllocatorDefault, CFDateGetAbsoluteTime((CFDateRef)entry));
    }

    if (type == CFDataGetTypeID()) {
        return CFDataCreateCopy(kCFAllocatorDefault, (CFDataRef)entry);
    }

    return NULL;
}

int RemoveEntryFromPlist(CFTypeRef plist, const char *name)
{
    int index = 0;
    CFTypeID type = CFGetTypeID(plist);

    if (type == CFDictionaryGetTypeID()) {
        CFStringRef key = CFStringCreateWithCString(
            kCFAllocatorDefault, name, kCFStringEncodingUTF8);

        CFDictionaryRemoveValue((CFMutableDictionaryRef)plist, key);
        CFRelease(key);
        return 0;
    }

    if (type == CFArrayGetTypeID()) {
        int count;

        sscanf(name, "%i", &index);
        count = (int)CFArrayGetCount((CFArrayRef)plist);
        if (index >= count) {
            index = count - 1;
        }
        if (count >= 1) {
            CFArrayRemoveValueAtIndex((CFMutableArrayRef)plist, index);
        }
        return 0;
    }

    return -1;
}
