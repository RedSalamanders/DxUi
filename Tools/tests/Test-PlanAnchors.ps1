<# .SYNOPSIS Evidence anchors in active plans: every `path:line` in Specs/Plans/WIP/*.md resolves in the checked-out tree, and
   seeded fixture plans fail for a missing file, an out-of-range line, a drifted symbol and an unresolved continuation. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PlanAnchors.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

# A five-line source file under src/Controls, a deeper header and a seeded plan per case.
$sample = "int Alpha();`nstruct Widget {};`nvoid Widget_Tick();`nint _counter = 0;`nint Beta();`n"
function New-AnchorFixture([string] $Root, [string] $Plan) {
    Set-FixtureFile $Root 'src/Controls/Sample.cpp' $sample
    Set-FixtureFile $Root 'src/Support/Deep.h' "#pragma once`nclass Gamma {};`n"
    Set-FixtureFile $Root 'Specs/Plans/WIP/Plan.md' $Plan
    return Get-DxUiPlanAnchorReport -Root $Root -PlanPath (Join-Path $Root 'Specs/Plans/WIP/Plan.md')
}
function Get-Kinds([object] $Report) { return @($Report.Problems | ForEach-Object Kind) }

Invoke-TestCase 'every anchor in Specs/Plans/WIP resolves at HEAD' {
    $plans = @(Get-ChildItem -LiteralPath (Join-Path $repository 'Specs/Plans/WIP') -Filter '*.md' -File | ForEach-Object FullName)
    Assert-True ($plans.Count -gt 0) 'active plans exist'
    $report = Get-DxUiPlanAnchorReport -Root $repository -PlanPath $plans
    foreach ($plan in $report.Plans) { Write-Host ('     {0}: {1} anchors checked, {2} skipped in ticked items' -f $plan.Plan, $plan.Checked, $plan.Skipped) }
    foreach ($problem in $report.Problems) { Write-Host "     $($problem.Message)" }
    Assert-Equal 0 $report.Problems.Count 'plan anchor problems'
    Assert-True (@($report.Plans | Where-Object Checked).Count -gt 0) 'at least one plan has anchors to check'
}

Invoke-FixtureCase 'every anchor form resolves in a passing plan' {
    param($root)
    $report = New-AnchorFixture $root @'
# Plan

| Area | Evidence |
| --- | --- |
| one | `src/Controls/Sample.cpp:1`, `Sample.cpp:2-3`, `src/Support/Deep.h:2` |

- [ ] **W1.1 Item.**
  - **Why**: `Widget` (`Sample.cpp:2`) and [Sample.cpp](../../../src/Controls/Sample.cpp) `:4-5`, `:1, 3-4`. A later
    sentence continues the item's last file (`:5`); a chain continues the anchor (`Sample.cpp:1`, `2-3`, and `5`).
  - **Change**: `Gamma` (`src/Support/Deep.h:1-2`, `Sample.cpp:1`); an external [page](https://example.com/x.cpp) is no file.

```text
`Missing.cpp:1` inside fenced code is not an anchor
```
'@
    Assert-Equal 0 $report.Problems.Count "problems: $(@($report.Problems | ForEach-Object Message) -join ' | ')"
    Assert-Equal 12 $report.Plans[0].Checked 'anchors checked'
    $forms = @($report.Anchors | ForEach-Object Form)
    foreach ($form in 'Path', 'Link', 'Continuation', 'Chained') { Assert-Contains $forms $form 'anchor forms read' }
    $bare = @($report.Anchors | Where-Object { $_.Lines -eq '2-3' -and $_.Form -eq 'Path' })
    Assert-Equal 'src/Controls/Sample.cpp' $bare[0].Path 'a file name without a directory is under src/Controls'
    $later = @($report.Anchors | Where-Object { $_.Lines -eq '5' -and $_.Form -eq 'Continuation' })
    Assert-Equal 'src/Controls/Sample.cpp' $later[0].Path 'a continuation reaches back across a sentence in its item'
    Assert-Equal 'Widget' (@($report.Anchors | Where-Object Symbol)[0].Symbol) 'an adjacent symbol binds to its anchor'
}

Invoke-FixtureCase 'a missing file fails' {
    param($root)
    $report = New-AnchorFixture $root "- [ ] **W1.1 Item.** Gone (``src/Controls/Gone.cpp:1``) and ``Absent.h:2``; a link [x](../../../src/Nope.cpp) ``:3``.`n"
    Assert-Equal 'MissingFile,MissingFile,MissingFile' ((Get-Kinds $report) -join ',') 'missing files'
    Assert-True ($report.Problems[0].Message -like '*Plan.md:1: missingfile: src/Controls/Gone.cpp does not exist') $report.Problems[0].Message
    Assert-Equal 'src/Controls/Absent.h' $report.Problems[1].Path 'a bare name is reported at its resolved path'
}

Invoke-FixtureCase 'an out-of-range line or range fails' {
    param($root)
    $report = New-AnchorFixture $root @'
- [ ] **W1.1 Item.** `Sample.cpp:6` is past the end; `Sample.cpp:4-6` ends past it; `Sample.cpp:0` and `Sample.cpp:3-2`
  are not lines; `Sample.cpp:5` is the last line; a list `Sample.cpp:1, 9`.
'@
    Assert-Equal 'OutOfRange,OutOfRange,OutOfRange,OutOfRange,OutOfRange' ((Get-Kinds $report) -join ',') 'out-of-range anchors'
    Assert-True ($report.Problems[0].Message -like "*src/Controls/Sample.cpp:6 is outside the file's 5 lines") $report.Problems[0].Message
    Assert-True ($report.Problems[4].Message -like '*Sample.cpp:9 is*') 'each listed line is checked'
}

Invoke-FixtureCase 'a symbol missing from its anchored file is reported as drifted with file, line and symbol' {
    param($root)
    $report = New-AnchorFixture $root @'
- [ ] **W1.1 Item.**
  - **Why**: `Renamed` (`Sample.cpp:2`) drifted; `Widget::Tick` (`Sample.cpp:3`) drifted too, as `Widget_Tick` is
    not the whole word `Tick`; `Gamma` (`Sample.cpp:1`, `src/Support/Deep.h:2`) occurs in one file of its group.
'@
    Assert-Equal 'Drifted,Drifted' ((Get-Kinds $report) -join ',') 'drifted symbols'
    Assert-Equal 'Renamed' $report.Problems[0].Symbol 'symbol reported'
    Assert-Equal 'src/Controls/Sample.cpp:2' $report.Problems[0].Path 'file and line reported'
    Assert-True ($report.Problems[0].Message.EndsWith('Plan.md:2: drifted: symbol `Renamed` occurs in none of src/Controls/Sample.cpp:2')) $report.Problems[0].Message
    Assert-Equal 'Widget::Tick' $report.Problems[1].Symbol 'a qualified symbol is matched by its last component as a whole word'
}

Invoke-FixtureCase 'only a symbol written directly before an anchor binds to it' {
    param($root)
    $report = New-AnchorFixture $root @'
- [ ] **W1.1 Item.** `Unbound` names something else (`Sample.cpp:1`); `Alpha` (`Sample.cpp:1`) `Later` follows it;
  the concept `WM_MOUSEMOVE` (`Sample.cpp:1`), the parameter `lParam` (`Sample.cpp:1`) and plan id `W2.1`
  (`Sample.cpp:1`) are not symbols; `Beta` `Sample.cpp:5` binds without a parenthesis.
'@
    Assert-Equal 0 $report.Problems.Count "problems: $(@($report.Problems | ForEach-Object Message) -join ' | ')"
    Assert-Equal 'Alpha,Beta' (@($report.Anchors | Where-Object Symbol | ForEach-Object Symbol) -join ',') 'bound symbols'
}

Invoke-FixtureCase 'the symbol rule is precise' {
    param($root)
    $expected = [ordered]@{
        'Button' = 'Button'; 'Button::Tick' = 'Tick'; 'HasFocus()' = 'HasFocus'; '_selectedIndex' = '_selectedIndex'
        'std::wstring' = 'wstring'; 'IDXGIAdapter3::QueryVideoMemoryInfo' = 'QueryVideoMemoryInfo'; 'D2D1_RECT_F::left' = 'left'
    }
    foreach ($code in $expected.Keys) { Assert-Equal $expected[$code] (ConvertTo-DxUiPlanAnchorSymbol $code) "symbol of $code" }
    foreach ($code in @('W2.1', 'FX5', 'G14', 'A6', 'F0.8', '16', '1e-5', '120Hz', 'src', 'true', 'caretPhaseMs', 'lParam',
            'WM_MOUSEMOVE', 'DXGI_SCALING_STRETCH', 'D2D1_RECT_F', 'DxUi.h', 'src/Controls/DxUi.cpp', ':12', 'a + b', 'Control::Assign(field)',
            'Debug…Progress', '->Create…(', '')) {
        Assert-Equal $null (ConvertTo-DxUiPlanAnchorSymbol $code) "not a symbol: '$code'"
    }
}

Invoke-FixtureCase 'anchors in a ticked item are skipped; the next item is checked' {
    param($root)
    $report = New-AnchorFixture $root @'
- [x] **W1.1 Landed.**
  - **Why**: `Old` (`src/Controls/Gone.cpp:1`), `Sample.cpp:99` and a bare `:7`.

  | Before | Evidence |
  | --- | --- |
  | gone | `Sample.cpp:42` |
- [ ] **W1.2 Open.** `Sample.cpp:42`.
'@
    Assert-Equal 'OutOfRange' ((Get-Kinds $report) -join ',') 'only the open item fails'
    Assert-Equal 7 $report.Problems[0].PlanLine 'reported at the open item'
    Assert-Equal 4 $report.Plans[0].Skipped 'skipped anchors, the ticked item''s table included'
    Assert-Equal 1 $report.Plans[0].Checked 'checked anchors'
}

Invoke-FixtureCase 'a continuation with no file before it is unresolved, and a table row starts afresh' {
    param($root)
    $report = New-AnchorFixture $root @'
- [ ] **W1.1 Item.** Starts with `:1`; then `Sample.cpp:2`.

  | Row | Evidence |
  | --- | --- |
  | first | `:3` |
  | second | `Sample.cpp:4`, `:5` |
'@
    Assert-Equal 'Unresolved,Unresolved' ((Get-Kinds $report) -join ',') 'unresolved continuations'
    Assert-Equal '1,5' (@($report.Problems | ForEach-Object PlanLine) -join ',') 'reported at the item and the first row'
}

Complete-TestRun 'Plan evidence anchors'
