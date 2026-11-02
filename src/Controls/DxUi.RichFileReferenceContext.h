#pragma once
#include <DxUi/RichTextCommands.h>
#include <optional>

namespace DxUi::RichText::Detail
{
struct FileReferenceContext
{
    FileReferenceContext() : continuation(0u, L' ')
    {
    }
    FileReferenceContext(const FileReferenceContext&)            = default;
    FileReferenceContext& operator=(const FileReferenceContext&) = default;
    std::wstring continuation;
    bool table = false;
};
// Uses the command planner's source/container validation; a null result is an
// ambiguous or cross-container selection and must not publish a partial list.
std::optional<FileReferenceContext> FileReferenceInsertionContext(const SourceSnapshot& source,
                                                                  std::uint64_t revision,
                                                                  std::shared_ptr<const MarkdownSyntax> syntax,
                                                                  Selection selection);
std::wstring EscapeFileReferenceLiteral(std::wstring_view value);
} // namespace DxUi::RichText::Detail
