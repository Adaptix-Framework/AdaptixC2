#include <CXXCompleter.h>
#include <Language.h>
#include <QFile>
#include <QStringListModel>
#include <QDir>
#include <QFileInfo>

CXXCompleter::CXXCompleter(QObject* parent) : QCompleter(parent)
{
    Q_INIT_RESOURCE(codeeditor_resources);
    QFile fl(":/languages/cpp.xml");

    if (!fl.open(QIODevice::ReadOnly))
        return;

    Language language(&fl);

    if (!language.isLoaded())
        return;

    QStringList allNames;
    auto keys = language.keys();
    for (auto&& key : keys) {
        allNames.append(language.names(key));
    }

    // Add common C/C++ identifiers and WinAPI types
    allNames.append({
        // Preprocessor directives (without #)
        "include", "define", "ifdef", "ifndef", "endif", "elif",
        "undef", "pragma", "error", "warning", "line", "defined",
        "pragma once", "pragma pack",

        // BOF/C Script API
        "BeaconPrintf", "BeaconOutput", "BeaconDownload",
        "BeaconDataParse", "BeaconDataExtract", "BeaconDataInt",
        "BeaconDataShort", "BeaconDataLength", "BeaconDataPtr",
        "BeaconFormatAlloc", "BeaconFormatAppend", "BeaconFormatFree",
        "BeaconFormatInt", "BeaconFormatPrintf", "BeaconFormatReset",
        "BeaconFormatToString",
        "BeaconUseToken", "BeaconRevertToken", "BeaconIsAdmin",
        "BeaconGetSpawnTo", "BeaconSpawnTemporaryProcess",
        "BeaconInjectProcess", "BeaconInjectTemporaryProcess",
        "BeaconCleanupProcess",
        "BeaconDataStoreGetItem", "BeaconDataStoreProtectItem",
        "BeaconDataStoreUnprotectItem", "BeaconDataStoreMaxEntries",
        "BeaconInformation", "BeaconAddValue", "BeaconGetValue",
        "BeaconRemoveValue", "BeaconGetCustomUserData",
        "BeaconGetSyscallInformation",
        "BeaconVirtualAlloc", "BeaconVirtualAllocEx",
        "BeaconVirtualProtect", "BeaconVirtualProtectEx",
        "BeaconVirtualFree", "BeaconGetThreadContext",
        "BeaconSetThreadContext", "BeaconResumeThread",
        "BeaconOpenProcess",
        "toWideChar", "DbgPrintf",
        "CALLBACK_OUTPUT", "CALLBACK_OUTPUT_OEM",
        "CALLBACK_ERROR", "CALLBACK_OUTPUT_UTF8",
        "datap", "formatp", "PDATA_STORE_OBJECT",
        "BEACON_INFO", "BEACON_SYSCALLS",

        // FFI atom types
        "cstr", "ptr", "i8", "i16", "u16", "i32", "u32", "i64", "u64",

        // Common macros
        "NULL", "TRUE", "FALSE", "MAX_PATH", "INFINITE",
        "S_OK", "S_FALSE", "E_FAIL", "E_POINTER", "E_OUTOFMEMORY",
        "INVALID_HANDLE_VALUE", "WAIT_OBJECT_0", "WAIT_TIMEOUT",
        "INT_MAX", "INT_MIN", "UINT_MAX", "SIZE_MAX", "UINTPTR_MAX",
        "EXIT_SUCCESS", "EXIT_FAILURE", "SEEK_SET", "SEEK_CUR", "SEEK_END",
        "stdin", "stdout", "stderr", "EOF", "BUFSIZ",
        "assert", "offsetof", "container_of", "ARRAY_SIZE", "ARRAYSIZE",

        // WinAPI functions
        "GetLastError", "SetLastError", "FormatMessage",
        "CreateFile", "ReadFile", "WriteFile", "CloseHandle",
        "VirtualAlloc", "VirtualFree", "VirtualProtect",
        "LoadLibrary", "GetProcAddress", "FreeLibrary",
        "CreateProcess", "CreateThread", "WaitForSingleObject",
        "RegOpenKey", "RegQueryValue", "RegCloseKey",
        "MessageBox", "GetModuleHandle", "GetModuleFileName",
        "GetUserNameExA", "GetUserNameExW", "GetComputerNameExA",

        // C standard library
        "malloc", "calloc", "realloc", "free",
        "printf", "sprintf", "snprintf", "fprintf", "vsnprintf",
        "scanf", "sscanf", "fscanf",
        "strlen", "strcpy", "strncpy", "strcmp", "strncmp", "strcat", "strncat",
        "strstr", "strchr", "strrchr", "strtok",
        "memcpy", "memset", "memmove", "memcmp",
        "fopen", "fclose", "fread", "fwrite", "fgets", "fputs",
        "fseek", "ftell", "rewind", "feof", "ferror",
        "atoi", "atof", "atol", "strtol", "strtoul", "strtod",
        "abort", "exit", "atexit", "system", "getenv",
        "time", "clock", "difftime", "mktime", "strftime",

        // C++ casts
        "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast",

        // STL common
        "make_shared", "make_unique", "make_pair", "make_tuple",
        "move", "forward", "swap", "declval",
        "begin", "end", "size", "empty", "clear", "insert", "erase",
        "push_back", "pop_back", "front", "back", "at", "data",
        "find", "count", "lower_bound", "upper_bound", "equal_range",
        "sort", "reverse", "transform", "for_each", "accumulate",
        "min", "max", "min_element", "max_element", "clamp",
        "distance", "advance", "next", "prev", "back_inserter",

        // Threading
        "thread", "mutex", "lock_guard", "unique_lock", "shared_lock",
        "condition_variable", "atomic", "future", "promise", "async",
        "packaged_task", "this_thread", "yield", "sleep_for",

        // Qt common
        "Q_OBJECT", "Q_PROPERTY", "Q_SIGNAL", "Q_SLOT", "Q_EMIT",
        "Q_DECL_OVERRIDE", "Q_DECL_FINAL", "Q_DECL_NOEXCEPT",
        "Q_UNUSED", "Q_ASSERT", "Q_UNREACHABLE",
        "emit", "signals", "slots",
        "foreach", "forever"
    });

    allNames.removeDuplicates();
    allNames.sort(Qt::CaseInsensitive);

    auto model = new QStringListModel(allNames, this);
    setModel(model);
    setCompletionMode(QCompleter::CompletionMode::PopupCompletion);
    setCaseSensitivity(Qt::CaseSensitive);
    setWrapAround(true);
}
