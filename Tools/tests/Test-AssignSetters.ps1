<# .SYNOPSIS Proves the one-way-to-assign matcher (Tools/AssignSetters.psm1; UI_ControlsAndLayout.md, "Assigning a property"): every
public Set* of a public control class under src/Controls assigns through Control::Assign, registers a callback, or sits on the
reviewed allow list with a reason; the allow list holds no stale entry; and every setter that uses Assign is a row of the native
gate on FX5. Seeded fixtures fail for each kind of violation and pass once it is repaired. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $root 'Tools/AssignSetters.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force

$header = @'
class Control
{
public:
    void SetVisible(bool visible) noexcept;
protected:
    void SetParent(Panel* parent) noexcept;
};
class Widget final : public Control
{
public:
    Widget();
    void SetWidth(float width) noexcept;
    [[nodiscard]] HRESULT SetLabel(std::wstring text, std::wstring tooltip = {}) noexcept;
    void SetOnClick(std::function<void()> onClick);
    void SetOnHover(HoverCallback onHover);
    void SetInline(bool value) noexcept
    {
        Assign(_inline, value, Affects::Paint);
    }
#if DXUI_ENABLE_DIAGNOSTICS
    void DebugSetSomething() noexcept;
#endif

private:
    void SetSecret(int value) noexcept;
    struct Part
    {
        void SetText(std::wstring text);
    };
};
struct Options
{
    void SetFlag(bool flag) noexcept;
};
'@

$compliant = @'
void Control::SetVisible(bool visible) noexcept
{
    _visible = visible;
}
void Widget::SetWidth(float width) noexcept
{
    Assign(_width, width, Affects::Layout | Affects::Paint);
}
HRESULT Widget::SetLabel(std::wstring text, std::wstring tooltip) noexcept
{
    // Assign(_label, ...) in a comment does not count
    return AssignAccess::Assign(*this, HitRevisionSite::None, _label, std::move(text), Affects::Paint, []() noexcept {}) ? S_OK : S_FALSE;
}
void Widget::SetOnClick(std::function<void()> onClick)
{
    _onClick = std::move(onClick);
}
void Widget::SetOnHover(HoverCallback onHover)
{
    _onHover = std::move(onHover);
}
void Widget::SetSecret(int value) noexcept
{
    _secret = value;
}
'@

function Inventory([string] $Header = $header, [string] $Source = $compliant, [System.Collections.IDictionary] $AllowList = [ordered]@{ 'Control::SetVisible' = 'tree state' }) {
    , @(Get-AssignSetterInventory -Headers @{ 'include/DxUi/DxUi.h' = $Header } -Sources @{ 'src/Controls/Widget.cpp' = $Source } -AllowList $AllowList -SideWrites ([ordered]@{}))
}
function Findings([string] $Header = $header, [string] $Source = $compliant, [System.Collections.IDictionary] $AllowList = [ordered]@{ 'Control::SetVisible' = 'tree state' }, [System.Collections.IDictionary] $SideWrites = [ordered]@{}, [System.Collections.IDictionary] $OnStoredReviewed = [ordered]@{}) {
    , @(Find-AssignSetterFindings -Headers @{ 'include/DxUi/DxUi.h' = $Header } -Sources @{ 'src/Controls/Widget.cpp' = $Source } -AllowList $AllowList -SideWrites $SideWrites -OnStoredReviewed $OnStoredReviewed)
}

Invoke-TestCase 'every public control setter of the repository assigns, registers a callback or is allowed with a reason' {
    $inputs = Get-AssignSetterInputs -Root $root
    $findings = @(Find-AssignSetterFindings -Headers $inputs.Headers -Sources $inputs.Sources)
    foreach ($finding in $findings) { Write-Host "     $finding" }
    Assert-Equal 0 $findings.Count 'setters outside the one way to assign a property'
    $inventory = @(Get-AssignSetterInventory -Headers $inputs.Headers -Sources $inputs.Sources)
    $allowed = @($inventory | Where-Object Kind -eq 'Allowed').Count
    $assigned = @($inventory | Where-Object Kind -eq 'Assign').Count
    Write-Host ("     {0} public control setters: {1} assign, {2} callbacks, {3} allowed" -f $inventory.Count, $assigned, @($inventory | Where-Object Kind -eq 'Callback').Count, $allowed)
    Assert-True ($assigned -gt $allowed) 'most setters assign through Control::Assign'
    Assert-True ($inventory.Setter -contains 'ProgressBar::SetValue') 'the inventory sees the classes of include/DxUi'
}

Invoke-TestCase 'every setter that assigns is a row of the native gate on FX5' {
    $inputs = Get-AssignSetterInputs -Root $root
    $inventory = @(Get-AssignSetterInventory -Headers $inputs.Headers -Sources $inputs.Sources)
    $gate = [IO.File]::ReadAllText((Join-Path $root 'Tests/Support/Support.Tests.AssignSetterRows.h'))
    $findings = @(Find-AssignGateCoverageFindings -Inventory $inventory -GateText $gate)
    foreach ($finding in $findings) { Write-Host "     $finding" }
    Assert-Equal 0 $findings.Count 'setters the gate does not exercise'
}

Invoke-TestCase 'the parser finds public setters of control classes only, with their kinds' {
    $inventory = Inventory
    $names = @($inventory | ForEach-Object Setter)
    Assert-Equal 'Control::SetVisible,Widget::SetWidth,Widget::SetLabel,Widget::SetOnClick,Widget::SetOnHover,Widget::SetInline' ($names -join ',') 'public setters of Control and its subclass, in declaration order'
    $kinds = @{}; foreach ($entry in $inventory) { $kinds[$entry.Setter] = $entry.Kind }
    Assert-Equal 'Allowed' $kinds['Control::SetVisible'] 'an allowed setter'
    Assert-Equal 'Assign' $kinds['Widget::SetWidth'] 'Assign in an out-of-line body'
    Assert-Equal 'Assign' $kinds['Widget::SetLabel'] 'the hit-site form, with a default argument'
    Assert-Equal 'Callback' $kinds['Widget::SetOnClick'] 'a std::function registration'
    Assert-Equal 'Callback' $kinds['Widget::SetOnHover'] 'a ...Callback registration'
    Assert-Equal 'Assign' $kinds['Widget::SetInline'] 'Assign in an inline body'
    Assert-Equal 0 (Findings).Count 'the compliant fixture has no finding'
}

Invoke-TestCase 'a setter that stores without Assign is a finding' {
    $seeded = $compliant.Replace('Assign(_width, width, Affects::Layout | Affects::Paint);', '_width = width; RequestInvalidate();')
    $findings = Findings -Source $seeded
    Assert-Equal 1 $findings.Count 'one finding'
    Assert-True ($findings[0] -like 'Widget::SetWidth (include/DxUi/DxUi.h:*)*') "the finding names the setter and its declaration: $($findings[0])"
    $commented = $compliant.Replace('Assign(_width, width, Affects::Layout | Affects::Paint);', '// Assign(_width, width, Affects::Paint);')
    Assert-Equal 1 (Findings -Source $commented).Count 'Assign in a comment does not count'
    Assert-Equal 1 (Findings -Source $compliant.Replace('Assign(_width,', 'AssignChecked(_width,')).Count 'another function whose name starts with Assign does not count'
}

Invoke-TestCase 'a setter that calls Assign for a side field but stores its property itself is a finding (review probes)' {
    $raw = $compliant.Replace('Assign(_width, width, Affects::Layout | Affects::Paint);', '_width = width; RequestInvalidate(); Assign(_dirty, true, Affects::None);')
    $findings = Findings -Source $raw
    Assert-Equal 1 $findings.Count 'one finding'
    Assert-True ($findings[0] -like 'Widget::SetWidth (*): uses Assign but writes _width itself*') $findings[0]
    $branch = $compliant.Replace('Assign(_width, width, Affects::Layout | Affects::Paint);', 'if (width > 0) Assign(_width, width, Affects::Layout); else { _width = 0; InvalidateMeasure(); }')
    Assert-Equal 1 (Findings -Source $branch).Count 'a raw store on another path'
    foreach ($write in @('_state->width = width;', 'this->_width += width;', '++_count;', '_items[0].text = text;')) {
        Assert-Equal 1 (Findings -Source $compliant.Replace('Assign(_width, width, Affects::Layout | Affects::Paint);', "Assign(_width, width, Affects::Paint); $write")).Count "a raw write outside Assign: $write"
    }
    $bookkeeping = $compliant.Replace('Assign(_width, width, Affects::Layout | Affects::Paint);', 'const float previous = _width; Assign(_width, width, Affects::Paint, [this]() noexcept { ++_revision; _cache = nullptr; }); if (previous == _width) {}')
    Assert-Equal 0 (Findings -Source $bookkeeping).Count 'reads, comparisons and onStored bookkeeping are not raw writes'
    $allowed = Findings -Source $raw -SideWrites ([ordered]@{ 'Widget::SetWidth' = 'reviewed' })
    Assert-Equal 0 $allowed.Count 'a reviewed side write passes'
    $stale = Findings -SideWrites ([ordered]@{ 'Widget::SetGone' = 'removed' })
    Assert-True ($stale -contains 'Widget::SetGone: side-write entry names no setter that uses Assign') ($stale -join '; ')
}

Invoke-TestCase 'a macro at class scope is a finding and does not hide the next declaration (review probes)' {
    foreach ($macro in @('    DXUI_SETTER(Height, float)', '    DXUI_SETTER(Height, float);')) {
        $header2 = $header.Replace("    void SetWidth(float width) noexcept;", "$macro`n    void SetWidth(float width) noexcept;")
        $inventory = Inventory -Header $header2
        Assert-True (@($inventory | Where-Object Setter -eq 'Widget::macro DXUI_SETTER' | Where-Object Kind -eq 'Finding').Count -eq 1) "the macro is a finding: $macro"
        Assert-True (@($inventory | Where-Object Setter -eq 'Widget::SetWidth' | Where-Object Kind -eq 'Assign').Count -eq 1) "the declaration after it is still seen: $macro"
    }
}

Invoke-TestCase 'definitions split across lines and templates defined in a header are found (review probes)' {
    $split = $compliant.Replace('void Widget::SetWidth(float width) noexcept', "void`nWidget::SetWidth(float width) noexcept")
    Assert-Equal 0 (Findings -Source $split).Count 'a return type on its own line'
    $header2 = $header.Replace('    void SetWidth(float width) noexcept;', '    template <typename T> void SetWidth(T width) noexcept;') + "`ntemplate <typename T> inline void Widget::SetWidth(T width) noexcept { _width = width; RequestInvalidate(); }`n"
    $source2 = $compliant.Replace('void Widget::SetWidth(float width) noexcept', 'void Widget::Resize(float width) noexcept')
    $findings = Findings -Header $header2 -Source $source2
    Assert-True ($findings[0] -like 'Widget::SetWidth (*): assigns without Control::Assign*') ($findings -join '; ')
}
Invoke-TestCase 'an onStored that goes on after a call that can run application code is a finding (W2.10 review, NumericStepper::SetUnit)' {
    $base = 'Assign(_width, width, Affects::Layout | Affects::Paint);'
    $unsafe = @(
        'Assign(_width, width, Affects::Layout, [this]() noexcept { _field->SetAccessibleName(_label); Arrange(); });',
        'Assign(_width, width, Affects::Paint, [this]() noexcept { if (_multiline) InvalidateMeasure(); RegenerateEpoch(); });',
        'Assign(_width, width, Affects::Paint, [this]() noexcept { RefreshComboItems(); RebuildDisplayTags(); LayoutParts(); });')
    foreach ($onStored in $unsafe) {
        $findings = Findings -Source $compliant.Replace($base, $onStored)
        Assert-Equal 1 $findings.Count "a step after application code: $onStored"
        Assert-True ($findings[0] -like '*its onStored goes on after*') $findings[0]
    }
    $safe = @(
        'Assign(_width, width, Affects::Layout, [this]() noexcept { ResetCache(); Arrange(); });',
        'Assign(_width, width, Affects::Layout, [this]() noexcept { const auto lifetime = GetLifetimeToken(); RefreshComboItems(); if (lifetime.expired()) return; LayoutParts(); });',
        'Assign(_width, width, Affects::Paint, [this]() noexcept { if (_host) { _host->RequestAnimation(*this); } else { _done = true; } });',
        'Assign(_width, width, Affects::Paint, [this]() noexcept { if (_a) SetMode(1); else if (_b) SetMode(2); });')
    foreach ($onStored in $safe) {
        Assert-Equal 0 (Findings -Source $compliant.Replace($base, $onStored)).Count "last step, checked step or other branch: $onStored"
    }
    Assert-Equal 0 (Findings -Source $compliant.Replace($base, $unsafe[0]) -OnStoredReviewed ([ordered]@{ 'Widget::SetWidth' = 'reviewed' })).Count 'a reviewed onStored passes'
}
Invoke-TestCase 'a setter with no definition is a finding' {
    $findings = Findings -Source $compliant.Replace('void Widget::SetWidth(float width) noexcept', 'void Widget::Resize(float width) noexcept')
    Assert-Equal 1 $findings.Count 'one finding'
    Assert-True ($findings[0] -like '*no definition found*') $findings[0]
}

Invoke-TestCase 'the allow list is reviewed: an entry needs a reason, a live setter and a body without Assign' {
    Assert-Equal 1 (Findings -AllowList ([ordered]@{})).Count 'removing an entry leaves its setter a finding'
    $stale = Findings -AllowList ([ordered]@{ 'Control::SetVisible' = 'tree state'; 'Widget::SetWidth' = 'now uses Assign' })
    Assert-Equal 1 $stale.Count 'an entry whose setter assigns is stale'
    Assert-True ($stale[0] -like 'Widget::SetWidth: allow-list entry is stale*') $stale[0]
    $gone = Findings -AllowList ([ordered]@{ 'Control::SetVisible' = 'tree state'; 'Widget::SetGone' = 'removed' })
    Assert-True ($gone[0] -like 'Widget::SetGone: allow-list entry names no public control setter') $gone[0]
    $empty = Findings -AllowList ([ordered]@{ 'Control::SetVisible' = ' ' })
    Assert-True ($empty[0] -like 'Control::SetVisible: allow-list entry has no reason') $empty[0]
    foreach ($entry in (Get-AssignSetterAllowList).GetEnumerator()) {
        Assert-True (-not [string]::IsNullOrWhiteSpace([string] $entry.Value)) "a reason for $($entry.Key)"
    }
}

Invoke-TestCase 'a gate row is required for every setter that assigns, and names only such setters' {
    $inventory = Inventory
    $gate = 'Row{"Widget::SetWidth", P, false, Create, Set}, Row{"Widget::SetLabel", P, false, Create, Set}, Row{"Widget::SetInline", P, false, Create, Set},'
    Assert-Equal 0 @(Find-AssignGateCoverageFindings -Inventory $inventory -GateText $gate).Count 'every assigning setter has a row'
    $missing = @(Find-AssignGateCoverageFindings -Inventory $inventory -GateText $gate.Replace('Row{"Widget::SetLabel"', 'Row{"Widget::SetLabelOld"'))
    Assert-Equal 2 $missing.Count 'a renamed row leaves its setter unexercised and names nothing'
    Assert-True ($missing -contains 'Widget::SetLabel: no row of the assignment gate exercises it') ($missing -join '; ')
    Assert-True ($missing -contains 'Widget::SetLabelOld: a gate row names no setter that uses Assign') ($missing -join '; ')
}

Invoke-FixtureCase 'a repository tree fails for an unlisted raw setter and passes once it assigns' {
    param($fixture)
    Set-FixtureFile $fixture 'include/DxUi/DxUi.h' $header
    Set-FixtureFile $fixture 'src/Controls/Widget.cpp' ($compliant.Replace('Assign(_width, width, Affects::Layout | Affects::Paint);', '_width = width;'))
    $inputs = Get-AssignSetterInputs -Root $fixture
    $allow = [ordered]@{ 'Control::SetVisible' = 'tree state' }
    Assert-Equal 1 @(Find-AssignSetterFindings -Headers $inputs.Headers -Sources $inputs.Sources -AllowList $allow -SideWrites ([ordered]@{})).Count 'the raw store fails'
    Set-FixtureFile $fixture 'src/Controls/Widget.cpp' $compliant
    $inputs = Get-AssignSetterInputs -Root $fixture
    Assert-Equal 0 @(Find-AssignSetterFindings -Headers $inputs.Headers -Sources $inputs.Sources -AllowList $allow -SideWrites ([ordered]@{})).Count 'the repaired tree passes'
}

Invoke-FixtureCase 'missing inputs fail closed' {
    param($fixture)
    Set-FixtureFile $fixture 'src/Controls/Widget.cpp' $compliant
    Assert-Throws { Get-AssignSetterInputs -Root $fixture } 'no include/DxUi'
}

Complete-TestRun 'AssignSetters'
