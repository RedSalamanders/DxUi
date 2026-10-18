[CmdletBinding()] param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$draft=Join-Path $Root '.build/reports/rich-incremental-preparation-20261018'
function Read-Text([string]$Path){[IO.File]::ReadAllText($Path).Replace("`r`n","`n")}
function Write-Text([string]$Path,[string]$Text){[IO.File]::WriteAllText($Path,$Text.Replace("`r`n","`n").Replace("`n","`r`n"))}
$header=Join-Path $Root 'include/DxUi/RichMarkdown.h'
$text=Read-Text $header
if($text.Contains('class PreparedProjection')){throw 'Projection service is already installed.'}
$text=$text.Replace('bool ToggleMarker(Document& document, std::wstring_view marker);',
 (Read-Text (Join-Path $draft 'ProjectionHeader.Draft.inc'))+"`nbool ToggleMarker(Document& document, std::wstring_view marker);")
Write-Text $header $text
$implementation=Join-Path $Root 'src/Controls/DxUi.RichMarkdownProjection.cpp'
$text=Read-Text $implementation
$text=$text.Replace('    Projection result;',
 '    Projection result{std::wstring(0u, L''\0''), std::wstring(0u, L''\0''), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u), 0u, ViewMode::Source};')
$old="} // namespace`nProjection Project(SourceSnapshot source, std::uint64_t revision, ViewMode mode)"
if(-not $text.Contains($old)){throw 'Canonical projection entry point changed.'}
$text=$text.Replace($old,'Projection ProjectText(std::wstring text, std::uint64_t revision, ViewMode mode)')
$text=$text.Replace('result.source = source.Text();','result.source = std::move(text);')
$text=$text.Replace('return std::move(result);','return std::as_const(result); // Copy avoids allocating Debug proxies inside noexcept string moves.')
$draftImplementation=Read-Text (Join-Path $draft 'ProjectionImplementation.Draft.inc')
$first=$draftImplementation.IndexOf('    if (state_ && state_->mode == mode && change.kind == SourceChangeKind::TrackedEdits)')
$last=$draftImplementation.IndexOf("    if (state_)`n        next->blocks.insert",$first)
if($first -lt 0 -or $last -le $first){throw 'Cannot isolate incremental branch for the complete-source baseline.'}
$baseline=$draftImplementation.Remove($first,$last-$first)
$insertion="} // namespace`nProjection Project(SourceSnapshot source, std::uint64_t revision, ViewMode mode)`n{`n    return ProjectText(source.Text(), revision, mode);`n}`n"+$baseline+"`nbool ToggleMarker"
$text=$text.Replace('bool ToggleMarker',$insertion)
Write-Text $implementation $text
$tests=Join-Path $Root 'Tests/Controls/Scope.Tests.RichProjectionBlocks.cpp'
$testText=@'
#include "Scope.Tests.RichProjectionBlocks.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/RichMarkdown.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
namespace
{
'@
$testText+="`n"+(Read-Text (Join-Path $draft 'ProjectionTests.Draft.inc'))+"`n"+(Read-Text (Join-Path $draft 'ProjectionTiming.Draft.inc'))
$testText+=@'

} // namespace
void RunRichProjectionBlockTests()
{
    DXUI_RUN_TEST(TestRichProjectionBlockFixedTrials);
    DXUI_RUN_TEST(TestRichProjectionBlockFullEquivalence);
    DXUI_RUN_TEST(TestRichProjectionBlockReuseAndLineJoins);
    DXUI_RUN_TEST(TestRichProjectionBlockUnicodeEditOracle);
    DXUI_RUN_TEST(TestRichProjectionBlockFallbackModesAndLifetime);
    DXUI_RUN_TEST(TestRichProjectionBlockMaximumSource);
}
'@
Write-Text $tests ($testText+"`n")
Write-Text (Join-Path $Root 'Tests/Controls/Scope.Tests.RichProjectionBlocks.h') "#pragma once`nvoid RunRichProjectionBlockTests();`n"
Write-Text (Join-Path $Root 'Tests/Embedded/Scope.Tests.RichProjectionResources.h') ("#pragma once`n"+(Read-Text (Join-Path $draft 'ProjectionResources.Draft.inc')))
$file=Join-Path $Root 'Tests/Controls/DxUi.Tests.EditorControls.cpp';$text=Read-Text $file
$text=$text.Replace('#include "Scope.Tests.RichMarkdown.h"',"#include `"Scope.Tests.RichMarkdown.h`"`n#include `"Scope.Tests.RichProjectionBlocks.h`"")
$text=$text.Replace('    RunRichMarkdownTests();',"    RunRichMarkdownTests();`n    RunRichProjectionBlockTests();")
Write-Text $file $text
$file=Join-Path $Root 'Tests/Controls/DxUi.ControlTests.vcxproj';$text=Read-Text $file
$text=$text.Replace('<ClCompile Include="Scope.Tests.RichMarkdown.cpp" />','<ClCompile Include="Scope.Tests.RichMarkdown.cpp" /><ClCompile Include="Scope.Tests.RichProjectionBlocks.cpp" />')
Write-Text $file $text
$file=Join-Path $Root 'Tests/Embedded/Embedded.Tests.Embedded.cpp';$text=Read-Text $file
$text=$text.Replace('#include "Scope.Tests.RichDocumentStorageResources.h"',"#include `"Scope.Tests.RichDocumentStorageResources.h`"`n#include `"Scope.Tests.RichProjectionResources.h`"")
$text=$text.Replace('    TestRichSourceChangeRolloverFailureAtomicity();',"    TestRichSourceChangeRolloverFailureAtomicity();`n    TestRichProjectionMappingAndUnchangedAllocationBudget();`n    TestRichProjectionPreparationAllocationFailureAtomicity();")
Write-Text $file $text
$file=Join-Path $Root 'Tests/native-test-files.json'
[string[]]$files=@((Get-Content -LiteralPath $file -Raw|ConvertFrom-Json)) + @('Tests/Controls/Scope.Tests.RichProjectionBlocks.cpp','Tests/Controls/Scope.Tests.RichProjectionBlocks.h','Tests/Embedded/Scope.Tests.RichProjectionResources.h')
[array]::Sort($files,[StringComparer]::Ordinal)
Write-Text $file ((ConvertTo-Json -InputObject $files)+"`n")
Write-Host 'Installed complete-source baseline only. Incremental branch remains inactive in the retained draft.'
