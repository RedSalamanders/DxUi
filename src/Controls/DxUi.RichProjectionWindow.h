#pragma once
#include <DxUi/RichMarkdown.h>
#include <memory>
#include <string>

namespace DxUi::RichText::Detail
{
// Projects complete physical lines through the canonical v0 projection builder.
std::shared_ptr<const Projection> ProjectRichSourceWindow(std::wstring&& source);
}
