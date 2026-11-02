#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <DbgHelp.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <wil/resource.h>
#include <windows.h>

namespace DxUiFailureTests::DumpReader
{
// Reads only the owned dump, never an exited child's address space. Unwind
// metadata comes from the dump's exact module list and local PE/PDB images.
class View final
{
public:
    View() noexcept              = default;
    View(const View&)            = delete;
    View& operator=(const View&) = delete;
    View(View&&)                 = delete;
    View& operator=(View&&)      = delete;
    bool Open(const std::wstring& path) noexcept
    {
        file_.reset(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        LARGE_INTEGER length{};
        if (! file_ || ! GetFileSizeEx(file_.get(), &length) || length.QuadPart < static_cast<LONGLONG>(sizeof(MINIDUMP_HEADER)) ||
            length.QuadPart > 1024ll * 1024ll * 1024ll)
            return false;
        size_ = static_cast<size_t>(length.QuadPart);
        mapping_.reset(CreateFileMappingW(file_.get(), nullptr, PAGE_READONLY, 0, 0, nullptr));
        if (! mapping_)
            return false;
        view_.reset(MapViewOfFile(mapping_.get(), FILE_MAP_READ, 0, 0, 0));
        const auto* header = At<MINIDUMP_HEADER>(0);
        return header && header->Signature == MINIDUMP_SIGNATURE && header->StreamDirectoryRva <= size_ &&
               header->NumberOfStreams <= (size_ - header->StreamDirectoryRva) / sizeof(MINIDUMP_DIRECTORY);
    }
    template <class T> const T* At(uint64_t offset, size_t count = 1u) const noexcept
    {
        if (! view_ || offset > size_ || count > (size_ - static_cast<size_t>(offset)) / sizeof(T))
            return nullptr;
        return reinterpret_cast<const T*>(static_cast<const std::byte*>(view_.get()) + static_cast<size_t>(offset));
    }
    template <class T> const T* Stream(ULONG kind, ULONG& bytes) const noexcept
    {
        PMINIDUMP_DIRECTORY directory{};
        void* data{};
        if (! MiniDumpReadDumpStream(view_.get(), kind, &directory, &data, &bytes) || bytes < sizeof(T))
            return nullptr;
        const auto address = reinterpret_cast<uintptr_t>(data), start = reinterpret_cast<uintptr_t>(view_.get());
        if (address < start || address - start > size_ || bytes > size_ - (address - start))
            return nullptr;
        return static_cast<const T*>(data);
    }
    std::wstring Name(RVA offset) const
    {
        const auto* length = At<ULONG32>(offset);
        if (! length || (*length % sizeof(wchar_t)) || *length > 65536u)
            return {};
        const auto* text = At<wchar_t>(static_cast<uint64_t>(offset) + sizeof(ULONG32), *length / sizeof(wchar_t));
        return text ? std::wstring(text, *length / sizeof(wchar_t)) : std::wstring{};
    }
    bool Read(uint64_t address, void* output, DWORD count) const noexcept
    {
        if (! count)
            return true;
        if (! output)
            return false;
        ULONG bytes{};
        if (const auto* memory = Stream<MINIDUMP_MEMORY_LIST>(MemoryListStream, bytes))
        {
            if (memory->NumberOfMemoryRanges > (bytes - offsetof(MINIDUMP_MEMORY_LIST, MemoryRanges)) / sizeof(MINIDUMP_MEMORY_DESCRIPTOR))
                return false;
            for (ULONG32 index = 0; index < memory->NumberOfMemoryRanges; ++index)
                if (Copy(memory->MemoryRanges[index].StartOfMemoryRange,
                         memory->MemoryRanges[index].Memory.DataSize,
                         memory->MemoryRanges[index].Memory.Rva,
                         address,
                         output,
                         count))
                    return true;
        }
        if (const auto* threads = Stream<MINIDUMP_THREAD_LIST>(ThreadListStream, bytes))
        {
            if (threads->NumberOfThreads > (bytes - offsetof(MINIDUMP_THREAD_LIST, Threads)) / sizeof(MINIDUMP_THREAD))
                return false;
            for (ULONG32 index = 0; index < threads->NumberOfThreads; ++index)
                if (Copy(threads->Threads[index].Stack.StartOfMemoryRange,
                         threads->Threads[index].Stack.Memory.DataSize,
                         threads->Threads[index].Stack.Memory.Rva,
                         address,
                         output,
                         count))
                    return true;
        }
        if (const auto* memory = Stream<MINIDUMP_MEMORY64_LIST>(Memory64ListStream, bytes))
        {
            if (memory->NumberOfMemoryRanges > (bytes - offsetof(MINIDUMP_MEMORY64_LIST, MemoryRanges)) / sizeof(MINIDUMP_MEMORY_DESCRIPTOR64))
                return false;
            uint64_t offset = memory->BaseRva;
            for (uint64_t index = 0; index < memory->NumberOfMemoryRanges; ++index)
            {
                const auto& range = memory->MemoryRanges[index];
                if (Copy(range.StartOfMemoryRange, range.DataSize, offset, address, output, count))
                    return true;
                if (range.DataSize > (std::numeric_limits<uint64_t>::max)() - offset)
                    return false;
                offset += range.DataSize;
            }
        }
        return false;
    }
    void AddImage(HANDLE file, uint64_t base, uint64_t size)
    {
        images_.push_back({file, base, size});
    }
    bool ReadImage(uint64_t address, void* output, DWORD count) const noexcept
    {
        if (! output || ! count)
            return false;
        for (const auto& image : images_)
        {
            if (address < image.base || address - image.base >= image.size || count > image.size - (address - image.base))
                continue;
            const auto rva = address - image.base;
            IMAGE_DOS_HEADER dos{};
            IMAGE_NT_HEADERS64 headers{};
            if (! ReadFileAt(image.file, 0, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 ||
                ! ReadFileAt(image.file, static_cast<uint64_t>(dos.e_lfanew), &headers, sizeof(headers)) || headers.Signature != IMAGE_NT_SIGNATURE ||
                headers.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
                return false;
            if (rva < headers.OptionalHeader.SizeOfHeaders && count <= headers.OptionalHeader.SizeOfHeaders - rva)
                return ReadFileAt(image.file, rva, output, count);
            const uint64_t sectionOffset =
                static_cast<uint64_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + headers.FileHeader.SizeOfOptionalHeader;
            for (unsigned index = 0; index < headers.FileHeader.NumberOfSections; ++index)
            {
                IMAGE_SECTION_HEADER section{};
                if (! ReadFileAt(image.file, sectionOffset + static_cast<uint64_t>(index) * sizeof(section), &section, sizeof(section)))
                    return false;
                if ((section.Characteristics & IMAGE_SCN_MEM_WRITE) || rva < section.VirtualAddress)
                    continue;
                const auto offset      = rva - section.VirtualAddress;
                const auto virtualSize = section.Misc.VirtualSize ? section.Misc.VirtualSize : section.SizeOfRawData;
                if (offset < virtualSize && count <= virtualSize - offset && offset < section.SizeOfRawData && count <= section.SizeOfRawData - offset)
                    return ReadFileAt(image.file, static_cast<uint64_t>(section.PointerToRawData) + offset, output, count);
            }
            return false;
        }
        return false;
    }

private:
    struct Image
    {
        HANDLE file;
        uint64_t base;
        uint64_t size;
    };
    static bool ReadFileAt(HANDLE file, uint64_t offset, void* output, size_t count) noexcept
    {
        if (offset > static_cast<uint64_t>((std::numeric_limits<LONGLONG>::max)()) || count > (std::numeric_limits<DWORD>::max)())
            return false;
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset);
        DWORD read{};
        return SetFilePointerEx(file, position, nullptr, FILE_BEGIN) && ReadFile(file, output, static_cast<DWORD>(count), &read, nullptr) && read == count;
    }
    bool Copy(uint64_t start, uint64_t bytes, uint64_t rva, uint64_t address, void* output, DWORD count) const noexcept
    {
        if (address < start || address - start > bytes || count > bytes - (address - start) || rva > (std::numeric_limits<uint64_t>::max)() - (address - start))
            return false;
        const auto* input = At<std::byte>(rva + address - start, count);
        if (! input)
            return false;
        std::memcpy(output, input, count);
        return true;
    }
    wil::unique_hfile file_;
    wil::unique_handle mapping_;
    wil::unique_mapview_ptr<void> view_;
    size_t size_{};
    std::vector<Image> images_;
};
inline thread_local const View* activeView{};
inline BOOL CALLBACK ReadMemory(HANDLE, DWORD64 address, PVOID output, DWORD bytes, LPDWORD read) noexcept
{
    const auto success = activeView && (activeView->Read(address, output, bytes) || activeView->ReadImage(address, output, bytes));
    if (! success)
        std::fprintf(stderr, "Dump unwind read rejected: address=%llx bytes=%lu\n", address, bytes);
    if (read)
        *read = success ? bytes : 0u;
    return success ? TRUE : FALSE;
}
inline PVOID CALLBACK FunctionTable(HANDLE process, DWORD64 address) noexcept
{
    return SymFunctionTableAccess64(process, address);
}
inline DWORD64 CALLBACK ModuleBase(HANDLE process, DWORD64 address) noexcept
{
    return SymGetModuleBase64(process, address);
}
inline BOOL CALLBACK SymbolCallback(HANDLE, ULONG action, ULONG64 data, ULONG64 context) noexcept
{
    if (action != CBA_READ_MEMORY || ! data || ! context)
        return FALSE;
    auto* request    = reinterpret_cast<IMAGEHLP_CBA_READ_MEMORY*>(data);
    const auto* view = reinterpret_cast<const View*>(context);
    if (request->bytesread)
        *request->bytesread = 0u;
    if (! request->buf && request->bytes)
        return FALSE;
    const bool success = view->Read(request->addr, request->buf, request->bytes) || view->ReadImage(request->addr, request->buf, request->bytes);
    if (! success)
        std::fprintf(stderr, "Dump symbol read rejected: address=%llx bytes=%lu\n", request->addr, request->bytes);
    if (request->bytesread)
        *request->bytesread = success ? request->bytes : 0u;
    return success ? TRUE : FALSE;
}

// Keep each verified image open without write/delete sharing until the walk
// finishes, so DbgHelp cannot reopen a replaced image at the same path.
inline bool OpenMatchingImage(const std::wstring& name, const MINIDUMP_MODULE& module, wil::unique_hfile& output) noexcept
{
    wil::unique_hfile file(CreateFileW(name.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    LARGE_INTEGER length{};
    if (! file || ! GetFileSizeEx(file.get(), &length) || length.QuadPart < static_cast<LONGLONG>(sizeof(IMAGE_DOS_HEADER)))
        return false;
    IMAGE_DOS_HEADER dos{};
    DWORD bytes{};
    if (! ReadFile(file.get(), &dos, static_cast<DWORD>(sizeof(dos)), &bytes, nullptr) || bytes != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < 0 || static_cast<uint64_t>(dos.e_lfanew) > static_cast<uint64_t>(length.QuadPart) ||
        sizeof(IMAGE_NT_HEADERS64) > static_cast<uint64_t>(length.QuadPart) - static_cast<uint64_t>(dos.e_lfanew))
        return false;
    LARGE_INTEGER position{};
    position.QuadPart = dos.e_lfanew;
    IMAGE_NT_HEADERS64 headers{};
    if (! SetFilePointerEx(file.get(), position, nullptr, FILE_BEGIN) ||
        ! ReadFile(file.get(), &headers, static_cast<DWORD>(sizeof(headers)), &bytes, nullptr) || bytes != sizeof(headers) ||
        headers.Signature != IMAGE_NT_SIGNATURE || headers.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
        headers.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || headers.FileHeader.TimeDateStamp != module.TimeDateStamp ||
        headers.OptionalHeader.SizeOfImage != module.SizeOfImage || headers.OptionalHeader.CheckSum != module.CheckSum)
        return false;
    output = std::move(file);
    return true;
}

// Caller serializes DbgHelp in its process. Require the originating symbol in
// a real unwind of the dump's exception context, with the exact module's PDB.
inline bool Verify(
    const std::wstring& path, HANDLE symbolKey, DWORD expectedThread, uint64_t expectedIp, const std::wstring& expectedModule, std::string_view expectedOrigin)
{
    View dump;
    if (! dump.Open(path) || activeView)
        return false;
    ULONG bytes{};
    const auto* exception = dump.Stream<MINIDUMP_EXCEPTION_STREAM>(ExceptionStream, bytes);
    if (! exception || exception->ThreadId != expectedThread || exception->ThreadContext.DataSize < sizeof(CONTEXT))
        return false;
    const auto* saved = dump.At<CONTEXT>(exception->ThreadContext.Rva);
    if (! saved)
        return false;
    CONTEXT context;
    std::memcpy(&context, saved, sizeof(context));
    constexpr DWORD requiredContext = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if ((context.ContextFlags & requiredContext) != requiredContext)
        return false;
    STACKFRAME64 frame{};
    DWORD machine{};
#if defined(_M_X64)
    machine                = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
#elif defined(_M_ARM64)
    machine                = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset    = context.Pc;
    frame.AddrFrame.Offset = context.Fp;
    frame.AddrStack.Offset = context.Sp;
#else
    return false;
#endif
    if (! frame.AddrPC.Offset || frame.AddrPC.Offset != expectedIp)
        return false;
    frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;
    const auto* modules                                             = dump.Stream<MINIDUMP_MODULE_LIST>(ModuleListStream, bytes);
    if (! modules || modules->NumberOfModules > (bytes - offsetof(MINIDUMP_MODULE_LIST, Modules)) / sizeof(MINIDUMP_MODULE))
        return false;
    const auto options = SymGetOptions();
    SymSetOptions(options | SYMOPT_UNDNAME | SYMOPT_EXACT_SYMBOLS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS | SYMOPT_DEFERRED_LOADS);
    const auto restore = wil::scope_exit([&] { SymSetOptions(options); });
    const auto slash   = expectedModule.find_last_of(L"\\/");
    const auto search  = slash == std::wstring::npos ? std::wstring{} : expectedModule.substr(0, slash);
    activeView         = &dump;
    const auto clear   = wil::scope_exit([] { activeView = nullptr; });
    if (! SymInitializeW(symbolKey, search.c_str(), FALSE))
        return false;
    const auto cleanup = wil::scope_exit([&] { SymCleanup(symbolKey); });
    if (! SymRegisterCallbackW64(symbolKey, SymbolCallback, reinterpret_cast<ULONG64>(&dump)))
        return false;
    DWORD64 targetBase{};
    GUID targetSignature{};
    DWORD targetAge{};
    std::vector<wil::unique_hfile> verifiedImages;
    verifiedImages.reserve(modules->NumberOfModules);
    for (ULONG32 index = 0; index < modules->NumberOfModules; ++index)
    {
        const auto& module = modules->Modules[index];
        const auto name    = dump.Name(module.ModuleNameRva);
        if (name.empty())
            return false;
        wil::unique_hfile image;
        if (! module.BaseOfImage || ! module.SizeOfImage || module.BaseOfImage > (std::numeric_limits<uint64_t>::max)() - module.SizeOfImage ||
            ! OpenMatchingImage(name, module, image))
        {
            std::fprintf(stderr, "Dump image identity unavailable or mismatched: %ls\n", name.c_str());
            return false;
        }
        verifiedImages.push_back(std::move(image));
        dump.AddImage(verifiedImages.back().get(), module.BaseOfImage, module.SizeOfImage);
        SetLastError(ERROR_SUCCESS);
        const auto loaded = SymLoadModuleExW(symbolKey, nullptr, name.c_str(), nullptr, module.BaseOfImage, module.SizeOfImage, nullptr, 0);
        if ((loaded && loaded != module.BaseOfImage) ||
            (! loaded && (GetLastError() != ERROR_SUCCESS || SymGetModuleBase64(symbolKey, module.BaseOfImage) != module.BaseOfImage)))
        {
            std::fprintf(stderr, "Dump unwind image load failed: %ls (error %lu)\n", name.c_str(), GetLastError());
            return false;
        }
        const bool target = _wcsicmp(name.c_str(), expectedModule.c_str()) == 0;
        if (target)
        {
            const auto* cv = dump.At<std::byte>(module.CvRecord.Rva, module.CvRecord.DataSize);
            if (! cv || module.CvRecord.DataSize < 24u || std::memcmp(cv, "RSDS", 4u))
                return false;
            std::memcpy(&targetSignature, cv + 4u, sizeof(targetSignature));
            std::memcpy(&targetAge, cv + 20u, sizeof(targetAge));
            targetBase = module.BaseOfImage;
        }
    }
    if (! targetBase)
        return false;
    const auto threadKey = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(expectedThread));
    if (! threadKey)
        return false;
    std::array<DWORD64, 256u> walked{};
    unsigned walkedCount{};
    for (unsigned step = 0; step < walked.size(); ++step)
    {
        walked[walkedCount++] = frame.AddrPC.Offset;
        if (SymGetModuleBase64(symbolKey, frame.AddrPC.Offset) == targetBase)
        {
            alignas(SYMBOL_INFO) std::array<std::byte, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> storage{};
            auto* symbol         = reinterpret_cast<SYMBOL_INFO*>(storage.data());
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen   = MAX_SYM_NAME;
            DWORD64 displacement{};
            if (SymFromAddr(symbolKey, frame.AddrPC.Offset, &displacement, symbol) &&
                std::string_view(symbol->Name, symbol->NameLen).find(expectedOrigin) != std::string_view::npos)
            {
                IMAGEHLP_MODULEW64 details{};
                details.SizeOfStruct = sizeof(details);
                return SymGetModuleInfoW64(symbolKey, targetBase, &details) && details.SymType == SymPdb && ! details.PdbUnmatched &&
                       details.LoadedPdbName[0] && ! std::memcmp(&targetSignature, &details.PdbSig70, sizeof(targetSignature)) && targetAge == details.PdbAge;
            }
        }
        const auto pc = frame.AddrPC.Offset, sp = frame.AddrStack.Offset;
        if (! StackWalk64(machine, symbolKey, threadKey, &frame, &context, ReadMemory, FunctionTable, ModuleBase, nullptr) || ! frame.AddrPC.Offset ||
            (step != 0u && frame.AddrPC.Offset == pc && frame.AddrStack.Offset == sp))
            break;
    }
    std::fprintf(stderr,
                 "Dump origin absent from saved-context unwind: %.*s (%u frames)\n",
                 static_cast<int>(expectedOrigin.size()),
                 expectedOrigin.data(),
                 walkedCount);
    for (unsigned index = 0; index < walkedCount; ++index)
    {
        alignas(SYMBOL_INFO) std::array<std::byte, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> storage{};
        auto* symbol         = reinterpret_cast<SYMBOL_INFO*>(storage.data());
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen   = MAX_SYM_NAME;
        DWORD64 displacement{};
        if (SymFromAddr(symbolKey, walked[index], &displacement, symbol))
            std::fprintf(stderr, "  %llx %.*s+%llx\n", walked[index], static_cast<int>(symbol->NameLen), symbol->Name, displacement);
        else
            std::fprintf(stderr, "  %llx (unresolved)\n", walked[index]);
    }
    return false;
}
} // namespace DxUiFailureTests::DumpReader
