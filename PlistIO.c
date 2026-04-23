#include "PlistBuddy.h"

void SavePlist(CFTypeRef plist, const char *path)
{
    SInt32 error_code = 0;
    CFStringRef cf_path = CFStringCreateWithCString(
        kCFAllocatorDefault, path, kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault, cf_path, kCFURLPOSIXPathStyle, false);

    CFRelease(cf_path);

    if (url != NULL) {
        CFDataRef xml_data = CFPropertyListCreateXMLData(kCFAllocatorDefault, plist);

        if (xml_data != NULL) {
            CFURLWriteDataAndPropertiesToResource(url, xml_data, NULL, &error_code);
            CFRelease(xml_data);
        }

        CFRelease(url);
    }
}

CFDataRef ReadData(char *path)
{
    CFDataRef resource_data = NULL;
    SInt32 error_code = 0;
    CFStringRef cf_path = CFStringCreateWithCString(
        kCFAllocatorDefault, path, kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault, cf_path, kCFURLPOSIXPathStyle, false);
    CFDataRef data = NULL;
    Boolean ok;

    CFRelease(cf_path);

    if (url == NULL) {
        return NULL;
    }

    ok = CFURLCreateDataAndPropertiesFromResource(
        kCFAllocatorDefault, url, &resource_data, NULL, NULL, &error_code);
    if (ok && resource_data != NULL) {
        data = resource_data;
    }

    CFRelease(url);
    return data;
}

CFPropertyListRef ReadPlist(char *path)
{
    CFStringRef error_string = NULL;
    CFDataRef data = ReadData(path);
    CFPropertyListRef plist;

    if (data == NULL) {
        return NULL;
    }

    plist = CFPropertyListCreateFromXMLData(
        kCFAllocatorDefault,
        data,
        kCFPropertyListMutableContainersAndLeaves,
        &error_string);
    if (error_string != NULL) {
        CFShow(error_string);
        CFRelease(error_string);
    }

    CFRelease(data);
    return plist;
}

int RevertPlist(PBState *state)
{
    struct stat info;

    memset(&info, 0, sizeof(info));

    if (state->plist != NULL) {
        CFRelease(state->plist);
    }

    if (stat(state->path, &info) != 0) {
        printf("File Doesn't Exist, Will Create: %s\n", state->path);
        state->plist = CFDictionaryCreateMutable(
            kCFAllocatorDefault,
            0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        state->current = state->plist;
        return 1;
    }

    state->plist = ReadPlist(state->path);
    state->current = state->plist;
    if (state->plist == NULL) {
        printf("Error Reading File: %s\n", state->path);
        return 0;
    }

    return 1;
}
