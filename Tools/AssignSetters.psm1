<#
.SYNOPSIS
Finds public control setters under src/Controls that assign their property without Control::Assign.

.DESCRIPTION
One way to assign a property (Specs/UI/UI_ControlsAndLayout.md, "Assigning a property"): a public Set* member function of a
public control class (a class of include/DxUi derived from Control) stores its value through Control::Assign, which does
nothing on an equal value and otherwise routes exactly the declared Affects. A setter is compliant when the body of every
definition of it under src/Controls (or inline in its header) calls Assign. Two kinds need no Assign:

- a callback registration: a setter whose parameter list takes a std::function or a type named ...Callback. A callable
  has no equality, nothing reads it to paint, measure, hit-test or describe the control, and storing it invalidates
  nothing;
- an entry of the reviewed allow list below, each with its one-line reason.

A finding is a setter that is neither; a stale allow-list entry (one that now uses Assign or no longer exists) is a finding
too, so the list cannot outlive its reasons.
#>
Set-StrictMode -Version Latest

# The reviewed allow list: 'Class::Setter' = 'why it does not use Assign'. Keep it small; every entry is reviewed.
$script:AllowList = [ordered]@{
    # Tree state and geometry the host itself tracks: equality-guarded, they move the interaction revision (not a property route).
    'Control::SetBounds'                       = 'arrange output: moves the interaction revision and re-places text input; equality-guarded'
    'Control::SetVisible'                      = 'tree state: interaction revision, preparation, frame schedule and focus loss; equality-guarded'
    'Control::SetEnabled'                      = 'tree state: interaction revision and OnEnabledChanged; equality-guarded'
    'Control::SetFlowDirection'                = 'inherited property: re-announced to descendants under the geometry guard; equality-guarded'
    'Control::SetDensity'                      = 'inherited property: re-announced to descendants under the geometry guard; equality-guarded'
    'Control::SetAccessibleFormProperties'     = 'three values in a record allocated on demand, with a weakly held label; equality-guarded, returns S_FALSE'
    'PageHost::SetPage'                        = 'transfers ownership of a page (no equality) and starts the page transition'
    'Tree::SetRowHeightDip'                    = 'row geometry under the geometry guard (interaction revision) and density re-derivation; equality-guarded'
    'Tree::SetIndentDip'                       = 'row geometry under the geometry guard (interaction revision); equality-guarded'
    'Grid::SetRowHeightDip'                    = 'row geometry under the geometry guard (interaction revision) and density re-derivation; equality-guarded'
    'Grid::SetEffectiveRowHeightDip'           = 'row geometry under the geometry guard (interaction revision) and density re-derivation; equality-guarded'
    'Grid::SetHeaderHeightDip'                 = 'header geometry under the geometry guard (interaction revision) and density re-derivation; equality-guarded'
    # Bindings: re-setting the same model or delegate is the documented way to rebind and reload.
    'Tree::SetModel'                           = 'rebinding reloads data and cancels gestures even for the same model (NotifyDataChanged contract)'
    'Tree::SetDelegate'                        = 'rebinding retires in-flight model queries even for the same delegate (binding revision)'
    'Grid::SetModel'                           = 'rebinding reloads data, reconciles selection and notifies observers even for the same model'
    'Grid::SetDelegate'                        = 'rebinding retires in-flight model queries even for the same delegate (binding revision)'
    # Selection and focus: they reveal the row, reconcile against the model and notify observers on every call.
    'Tree::SetSelectedItemId'                  = 'resolves the id against the model and scrolls it into view on every call'
    'Tree::SetFocusedItemId'                   = 'resolves the id against the model and scrolls it into view on every call'
    'Tree::SetSelectedItemIds'                 = 'a list resolved against the visible rows; reveals the last one on every call'
    'Grid::SetSelectedRowIds'                  = 'a list resolved against the visible rows; keeps the selection epoch when equal and reveals the last row'
    'Grid::SetFocusedRowId'                    = 'resolves the id against the model and publishes focus on every call'
    'Grid::SetSelectionMode'                   = 'collapses a multi-selection and notifies selection observers (application callbacks); equality-guarded'
    'RadioButton::SetChecked'                  = 'group exclusivity through AssignChecked, shared with RadioButtons::SetSelectedIndex; publishes only on change'
    'RadioButtons::SetSelectedIndex'           = 'synchronizes every child even for the same index (children added later join it); publishes only on change'
    # Text editing state: a programmatic set resets caret, selection, undo and composition on every call by contract.
    'TextField::SetText'                       = 'resets caret, selection, undo history, scroll and text input on every call (editing contract)'
    'TextField::SetTextAndNotify'              = 'SetText then the change notification, on every call'
    'TextField::SetSelectionRange'             = 'restarts the caret blink and synchronizes text input on every call (caret state machine)'
    'ComboBox::SetText'                        = 'resets caret, selection, undo history and text input on every call (editing contract)'
    'ComboBox::SetTextAndNotify'               = 'SetText then the change notification, on every call'
    'ComboBox::SetEditableSelectionRange'      = 'restarts the selection gesture and synchronizes text input on every call (caret state machine)'
    'ComboBox::SetEditable'                    = 'forwards to SetVariant, which assigns; also ends a selection gesture'
    'TagPicker::SetInputText'                  = 'forwards to its ComboBox part''s SetText'
    'NumericStepper::SetValue'                 = 'reconciles an edit in progress (explicit-state-machines plan 1.15); outside one it re-syncs its text'
    'NumericStepper::SetStepButtonNames'       = 'forwards to its two button parts'' SetAccessibleName, which assign'
    'ColorPicker::SetLabels'                   = 'distributes its labels to its parts (steppers, field, buttons), each of which assigns'
    'FormField::SetLabel'                      = 'the text lives on its Label part, which assigns; the field relays out and publishes its form properties'
    'FormField::SetDescription'                = 'the text lives on its Label part, which assigns; the field relays out and publishes its form properties'
    'FormField::SetValidation'                 = 'state and text on its Label part, which assigns; equality-guarded, returns S_FALSE'
    # Transitions and deadlines: these schedule work and answer whether anything changed.
    'Button::SetDisclosureExpanded'            = 'the first call snaps, later ones start the rotation and raise the expand/collapse event; equality-guarded'
    'TooltipLayer::SetTooltip'                 = 'cancels pending show and hide deadlines; returns whether anything changed'
    'TooltipLayer::SetTooltipDelayed'          = 'schedules the show deadline; returns whether anything changed'
    # The rich text editor: document and presentation options rebuild semantic layout through its own revisions.
    'RichTextEditor::SetText'                  = 'forwards to TrySetText: a source edit with history and revisions'
    'RichTextEditor::SetDocumentFilePath'      = 'forwards to TrySetDocumentFilePath: rebinds relative image resolution'
    'RichTextEditor::SetSelection'             = 'forwards to TrySetSelection: the selection state machine'
    'RichTextEditor::SetViewMode'              = 'forwards to TrySetViewMode: rebuilds the presentation'
    'RichTextEditor::SetAppearance'            = 'forwards to TrySetAppearance: rebuilds layout through the appearance revision'
    'RichTextEditor::SetZoom'                  = 'forwards to TrySetZoom: rebuilds layout and keeps the viewport anchor'
    'RichTextEditor::SetLinkUnderlinePolicy'   = 'forwards to TrySetLinkUnderlinePolicy: rebuilds link ink'
    'RichTextEditor::SetSourceHighlights'      = 'forwards to TrySetSourceHighlights: a list validated against the source'
    'RichTextEditor::SetSearchMatches'         = 'forwards to TrySetSearchMatches: a list validated against an expected source snapshot'
    'RichTextEditor::SetDecorations'           = 'forwards to TrySetDecorations: a list validated against an expected source snapshot'
    'RichTextEditor::SetDefaultContextMenuLabels' = 'forwards to TrySetDefaultContextMenuLabels: copies labels from a borrowed pointer'
    'RichTextEditor::SetMarkdownAnalysisProfile' = 'forwards to TrySetMarkdownAnalysisProfile: re-analyzes the source'
    'RichTextEditor::SetMarkdownAnalysisExtensions' = 'forwards to TrySetMarkdownAnalysisExtensions: re-analyzes the source'
    'RichTextEditor::SetMarkdownRenderingEnabled' = 'forwards to TrySetMarkdownRenderingEnabled: rebuilds the presentation'
    'RichTextEditor::SetTaskTogglePolicy'      = 'forwards to TrySetTaskTogglePolicy'
    'RichTextEditor::SetSearchQuery'           = 'forwards to TrySetSearchQuery: searches the source'
    'RichTextEditor::SetRemoteImagesEnabled'   = 'a policy stored in the image resolver; retires semantic accessibility and layout; equality-guarded'
    'RichTextEditor::SetDataImagesEnabled'     = 'a policy stored in the image resolver; retires semantic accessibility and layout; equality-guarded'
    'RichTextEditor::SetSvgImagesEnabled'      = 'a policy stored in the image resolver; retires semantic accessibility and layout; equality-guarded'
    'RichTextEditor::SetAccessibilityLabels'   = 'labels stored in the semantic builder; rebuilds semantic identity; equality-guarded by identity'
    'RichTextEditor::SetCodeTokenizer'         = 'a tokenizer is a callable (no equality); replacing it restarts code ink preparation'
    'RichTextEditor::SetScrollOffsetDip'       = 'clamps against prepared layout, ends caret reveal on every call and returns whether it scrolled'
    'RichTextEditor::SetHorizontalScrollOffsetDip' = 'clamps against prepared layout, ends caret reveal on every call and returns whether it scrolled'
    'RichTextEditor::SetSemanticDisclosure'    = 'keyed by a semantic element of an expected document; returns whether it applied'
    'RichTextEditor::SetReadOnly'              = 'ends the typing run and republishes the selection on every call'
}

function Get-AssignSetterAllowList { $script:AllowList }

# Comments and literals, leftmost first (the same rule as Tools/CountedCreation.psm1).
$script:LiteralPattern = [regex]::new(
    'R"(?<delimiter>[^()\\\s"]{0,16})\((?s:.*?)\)\k<delimiter>"' +
    '|//[^\n]*' +
    '|/\*(?s:.*?)(?:\*/|\z)' +
    '|"(?:\\.|[^"\\\n])*(?:"|$)' +
    "|(?<=^|[^0-9A-Za-z_]|(?<![0-9A-Za-z_])(?:L|u|U|u8))'(?:\\.|[^'\\\n])*(?:'|$)",
    [Text.RegularExpressions.RegexOptions]::Multiline)

# Preprocessor directives, continuation lines included: an #if/#endif pair around declarations is not code.
$script:DirectivePattern = [regex]::new('^[ \t]*#(?:[^\n]*\\\r?\n)*[^\n]*', [Text.RegularExpressions.RegexOptions]::Multiline)

# The text with every comment, literal and preprocessor directive replaced by spaces; line breaks are kept.
function Remove-AssignCommentsAndLiterals([Parameter(Mandatory)][AllowEmptyString()][string] $Text) {
    $blank = [Text.RegularExpressions.MatchEvaluator] { param($match) [regex]::Replace($match.Value, '[^\r\n]', ' ') }
    $script:DirectivePattern.Replace($script:LiteralPattern.Replace($Text, $blank), $blank)
}

# Braces, statement ends and access labels of comment-free code, in order, and the index just past the brace closing each
# opening brace.
function Get-CodeTokens([string] $Code) {
    $tokens = @([regex]::Matches($Code, '[{};]|(?<![\w:])(?:public|protected|private)\s*:(?!:)'))
    $closeOf = @{}
    $stack = [Collections.Generic.Stack[int]]::new()
    foreach ($token in $tokens) {
        if ($token.Value -eq '{') { $stack.Push($token.Index) }
        elseif ($token.Value -eq '}' -and $stack.Count -gt 0) { $closeOf[$stack.Pop()] = $token.Index + 1 }
    }
    [pscustomobject]@{ Tokens = $tokens; CloseOf = $closeOf }
}

# The parameter list of a declaration starting at the '(' at $Open, without the parentheses.
function Get-ParameterList([string] $Code, [int] $Open) {
    $depth = 0
    for ($index = $Open; $index -lt $Code.Length; $index++) {
        if ($Code[$index] -eq '(') { $depth++ }
        elseif ($Code[$index] -eq ')') {
            $depth--
            if ($depth -eq 0) { return $Code.Substring($Open + 1, $index - $Open - 1) }
        }
    }
    return $Code.Substring($Open + 1)
}

$script:ClassHead = [regex]::new('(?<!enum\s+)\b(?<kind>class|struct)\s+(?:DXUI_API\s+)?(?<name>[A-Za-z_]\w*)\s*(?:final\s*)?(?::\s*(?<bases>[^{;()]*))?\{')
$script:SetterDeclaration = [regex]::new('(?<![\w:~])(?<name>Set[A-Z]\w*)\s*\(')

function Get-BaseNames([string] $Bases) {
    @([regex]::Matches($Bases, '[A-Za-z_][\w:]*') | ForEach-Object { ($_.Value -split '::')[-1] } | Where-Object { $_ -notin @('public', 'protected', 'private', 'virtual') })
}

# Public Set* declarations of every class and struct in a header text: Class, Name, Bases, Parameters, Line, and the inline
# body when the declaration defines it.
function Get-ClassSetterDeclarations([Parameter(Mandatory)][AllowEmptyString()][string] $Text) {
    $code = Remove-AssignCommentsAndLiterals $Text
    $scan = Get-CodeTokens $code
    $tokens = $scan.Tokens
    $results = [Collections.Generic.List[object]]::new()
    foreach ($head in $script:ClassHead.Matches($code)) {
        $open = $head.Index + $head.Length - 1
        if (-not $scan.CloseOf.ContainsKey($open)) { continue }
        $close = $scan.CloseOf[$open]
        $class = [pscustomobject]@{ Name = $head.Groups['name'].Value; Bases = if ($head.Groups['bases'].Success) { Get-BaseNames $head.Groups['bases'].Value } else { @() } }
        # Walk the class body at its own depth: a nested brace (an inline body, a nested type, an initializer) is one step.
        $access = if ($head.Groups['kind'].Value -eq 'struct') { 'public' } else { 'private' }
        $statementStart = $open + 1
        $position = $open + 1
        $low = 0; $high = $tokens.Count
        while ($low -lt $high) { $middle = [int][math]::Floor(($low + $high) / 2); if ($tokens[$middle].Index -lt $position) { $low = $middle + 1 } else { $high = $middle } }
        for ($tokenIndex = $low; $tokenIndex -lt $tokens.Count; $tokenIndex++) {
            $token = $tokens[$tokenIndex]
            if ($token.Index -lt $position) { continue }
            if ($token.Index -ge $close - 1) { break }
            $value = $token.Value
            if ($value -eq '{') {
                $end = $scan.CloseOf[$token.Index]
                $statement = $code.Substring($statementStart, $token.Index - $statementStart)
                # Inside an open parameter list the brace is a default argument ({}), not a body.
                if (([regex]::Matches($statement, '\(')).Count -ne ([regex]::Matches($statement, '\)')).Count) { $position = $scan.CloseOf[$token.Index]; continue }
                Add-SetterDeclaration $results $class $access $statement $statementStart $code $token.Index $end
                $position = $end
                # A nested type or a braced initializer ends at ';'; an inline function body ends at its brace.
                if (-not ($statement -match '\b(class|struct|enum|union)\b' -or $statement -match '(=|\w)\s*$' -and $statement -notmatch '\)[\w\s]*$')) { $statementStart = $end }
                continue
            }
            if ($value -eq ';') {
                Add-SetterDeclaration $results $class $access $code.Substring($statementStart, $token.Index - $statementStart) $statementStart $code -1 -1
                $statementStart = $token.Index + 1
                continue
            }
            if ($value -eq '}') { continue }
            if ($code.Substring($statementStart, $token.Index - $statementStart).Trim().Length -eq 0) {
                $access = ($value -replace '\s*:$', '').Trim()
                $statementStart = $token.Index + $token.Length
            }
        }
    }
    $results.ToArray()
}

function Add-SetterDeclaration($Results, $Class, [string] $Access, [string] $Statement, [int] $Start, [string] $Code, [int] $BodyOpen, [int] $BodyEnd) {
    if ($Access -ne 'public') { return }
    if ($Statement -match '\b(class|struct|enum|union|using|typedef|friend)\b') { return }
    $match = $script:SetterDeclaration.Match($Statement)
    if (-not $match.Success) { return }
    # A declaration names the setter before its parameter list; a call inside a default argument does not count.
    if ($Statement.Substring(0, $match.Index) -match '[=(,]') { return }
    $parameters = Get-ParameterList $Statement ($match.Index + $match.Length - 1)
    $line = 1 + ([regex]::Matches($Code.Substring(0, $Start + $match.Index), "`n")).Count
    $body = if ($BodyOpen -ge 0) { $Code.Substring($BodyOpen, $BodyEnd - $BodyOpen) } else { $null }
    $Results.Add([pscustomobject]@{ Class = $Class.Name; Name = $match.Groups['name'].Value; Bases = $Class.Bases; Parameters = $parameters.Trim(); Line = $line; Body = $body })
}
# The classes derived, directly or not, from Control (Control included), from every class head of the header texts.
function Get-ControlClassNames([Parameter(Mandatory)][AllowEmptyString()][string[]] $HeaderTexts) {
    $basesOf = @{}
    foreach ($text in $HeaderTexts) {
        $code = Remove-AssignCommentsAndLiterals $text
        foreach ($head in $script:ClassHead.Matches($code)) {
            $basesOf[$head.Groups['name'].Value] = if ($head.Groups['bases'].Success) { Get-BaseNames $head.Groups['bases'].Value } else { @() }
        }
    }
    $controls = [Collections.Generic.HashSet[string]]::new([string[]] @('Control'))
    do {
        $added = $false
        foreach ($name in @($basesOf.Keys)) {
            if ($controls.Contains($name)) { continue }
            foreach ($base in $basesOf[$name]) {
                if ($controls.Contains($base)) { [void] $controls.Add($name); $added = $true; break }
            }
        }
    } while ($added)
    , @($controls)
}

# Out-of-line definitions 'Class::Set...(...) ... { body }' in one source text: Class, Name, Line, Body.
function Get-SetterDefinitions([Parameter(Mandatory)][AllowEmptyString()][string] $Text) {
    $code = Remove-AssignCommentsAndLiterals $Text
    $closeOf = (Get-CodeTokens $code).CloseOf
    $results = [Collections.Generic.List[object]]::new()
    foreach ($match in [regex]::Matches($code, '(?m)^[^\S\n]*(?:[\w:<>,\*&\[\]]+[^\S\n]+)+(?<class>[A-Za-z_]\w*)::(?<name>Set[A-Z]\w*)\s*\(')) {
        $parameters = Get-ParameterList $code ($match.Index + $match.Length - 1)
        $after = $match.Index + $match.Length - 1 + $parameters.Length + 2
        # Up to the body: qualifiers, noexcept, a member initializer list never occurs on a setter.
        $rest = $code.Substring($after)
        $head = [regex]::Match($rest, '^[^;{]*')
        if ($after + $head.Length -ge $code.Length -or $code[$after + $head.Length] -ne '{') { continue }
        $open = $after + $head.Length
        if (-not $closeOf.ContainsKey($open)) { continue }
        $end = $closeOf[$open]
        $line = 1 + ([regex]::Matches($code.Substring(0, $match.Index), "`n")).Count
        $results.Add([pscustomobject]@{ Class = $match.Groups['class'].Value; Name = $match.Groups['name'].Value; Line = $line; Body = $code.Substring($open, $end - $open) })
    }
    $results.ToArray()
}

function Test-CallbackParameters([string] $Parameters) {
    $Parameters -match '\bstd::function\s*<' -or $Parameters -match '\b\w+Callback\b'
}

function Test-UsesAssign([string] $Body) {
    $Body -match '(?<![\w.>])Assign\s*\('
}

<#
Every public setter of every public control class, with its compliance: Setter ('Class::Name'), Kind ('Assign',
'Callback', 'Allowed' or 'Finding'), Reason, Declaration ('path:line'), Definitions ('path:line' each).
Headers: path => text of the public headers; Sources: path => text of the sources that define them.
#>
function Get-AssignSetterInventory([Parameter(Mandatory)][hashtable] $Headers, [Parameter(Mandatory)][hashtable] $Sources, [System.Collections.IDictionary] $AllowList = $script:AllowList) {
    $controls = Get-ControlClassNames -HeaderTexts @($Headers.Values)
    $declarations = [ordered]@{}
    foreach ($path in ($Headers.Keys | Sort-Object)) {
        foreach ($declaration in Get-ClassSetterDeclarations $Headers[$path]) {
            if ($controls -notcontains $declaration.Class) { continue }
            $key = '{0}::{1}' -f $declaration.Class, $declaration.Name
            if (-not $declarations.Contains($key)) {
                $declarations[$key] = [pscustomobject]@{ Setter = $key; Parameters = [Collections.Generic.List[string]]::new(); Declaration = ('{0}:{1}' -f $path, $declaration.Line); Bodies = [Collections.Generic.List[object]]::new() }
            }
            $declarations[$key].Parameters.Add($declaration.Parameters)
            if ($null -ne $declaration.Body) { $declarations[$key].Bodies.Add([pscustomobject]@{ Where = ('{0}:{1}' -f $path, $declaration.Line); Body = $declaration.Body }) }
        }
    }
    foreach ($path in ($Sources.Keys | Sort-Object)) {
        foreach ($definition in Get-SetterDefinitions $Sources[$path]) {
            $key = '{0}::{1}' -f $definition.Class, $definition.Name
            if ($declarations.Contains($key)) { $declarations[$key].Bodies.Add([pscustomobject]@{ Where = ('{0}:{1}' -f $path, $definition.Line); Body = $definition.Body }) }
        }
    }
    $inventory = [Collections.Generic.List[object]]::new()
    foreach ($entry in $declarations.Values) {
        $callback = @($entry.Parameters | Where-Object { Test-CallbackParameters $_ }).Count -eq $entry.Parameters.Count
        $bodies = @($entry.Bodies)
        $assigns = $bodies.Count -gt 0 -and @($bodies | Where-Object { -not (Test-UsesAssign $_.Body) }).Count -eq 0
        $kind, $reason = if ($assigns) { 'Assign', '' }
        elseif ($callback) { 'Callback', 'callback registration: no equality, no visual, layout, hit or semantic effect' }
        elseif ($AllowList.Contains($entry.Setter)) { 'Allowed', [string] $AllowList[$entry.Setter] }
        elseif ($bodies.Count -eq 0) { 'Finding', 'no definition found under src/Controls or inline' }
        else { 'Finding', 'assigns without Control::Assign and is not on the allow list' }
        $inventory.Add([pscustomobject]@{ Setter = $entry.Setter; Kind = $kind; Reason = $reason; Declaration = $entry.Declaration; Definitions = @($bodies | ForEach-Object { $_.Where }) })
    }
    $inventory.ToArray()
}

# Findings: setters neither using Assign, nor callbacks, nor allowed; and allow-list entries that are stale or unexplained.
function Find-AssignSetterFindings([Parameter(Mandatory)][hashtable] $Headers, [Parameter(Mandatory)][hashtable] $Sources, [System.Collections.IDictionary] $AllowList = $script:AllowList) {
    $inventory = @(Get-AssignSetterInventory -Headers $Headers -Sources $Sources -AllowList $AllowList)
    $findings = [Collections.Generic.List[string]]::new()
    foreach ($entry in $inventory | Where-Object Kind -eq 'Finding') { $findings.Add(('{0} ({1}): {2}' -f $entry.Setter, $entry.Declaration, $entry.Reason)) }
    foreach ($key in $AllowList.Keys) {
        $entry = $inventory | Where-Object Setter -eq $key | Select-Object -First 1
        if (-not $entry) { $findings.Add("${key}: allow-list entry names no public control setter") }
        elseif ($entry.Kind -ne 'Allowed') { $findings.Add("${key}: allow-list entry is stale (the setter is $($entry.Kind))") }
        elseif ([string]::IsNullOrWhiteSpace([string] $AllowList[$key])) { $findings.Add("${key}: allow-list entry has no reason") }
    }
    $findings.ToArray()
}

# Setters of kind Assign that the native gate (Tests/Embedded/Embedded.Tests.AssignSetters.h) names in no row: each row names
# its setter as the string literal "Class::Setter", and the gate requires every unchanged value to route nothing.
function Find-AssignGateCoverageFindings([Parameter(Mandatory)][object[]] $Inventory, [Parameter(Mandatory)][AllowEmptyString()][string] $GateText) {
    $named = [Collections.Generic.HashSet[string]]::new([string[]] @([regex]::Matches($GateText, 'Row\{"(?<name>\w+::Set\w+)"') | ForEach-Object { $_.Groups['name'].Value }))
    $findings = [Collections.Generic.List[string]]::new()
    foreach ($entry in $Inventory | Where-Object Kind -eq 'Assign') {
        if (-not $named.Contains($entry.Setter)) { $findings.Add("$($entry.Setter): no row of the assignment gate exercises it") }
    }
    foreach ($name in $named) {
        if (-not ($Inventory | Where-Object { $_.Setter -eq $name -and $_.Kind -eq 'Assign' })) { $findings.Add("${name}: a gate row names no setter that uses Assign") }
    }
    $findings.ToArray()
}
# The repository's public headers and the sources under src/Controls, as path => text maps keyed by '/'-separated paths.
function Get-AssignSetterInputs([Parameter(Mandatory)][string] $Root) {
    $rootPath = (Resolve-Path -LiteralPath $Root).ProviderPath.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $headers = @{}
    $sources = @{}
    $include = Join-Path $Root 'include/DxUi'
    $controls = Join-Path $Root 'src/Controls'
    if (-not (Test-Path -LiteralPath $include -PathType Container)) { throw "No include/DxUi directory under $Root." }
    if (-not (Test-Path -LiteralPath $controls -PathType Container)) { throw "No src/Controls directory under $Root." }
    foreach ($file in Get-ChildItem -LiteralPath $include -Recurse -File -Filter '*.h') { $headers[$file.FullName.Substring($rootPath.Length).Replace('\', '/')] = [IO.File]::ReadAllText($file.FullName) }
    foreach ($file in Get-ChildItem -LiteralPath $controls -Recurse -File -Include '*.cpp', '*.h') { $sources[$file.FullName.Substring($rootPath.Length).Replace('\', '/')] = [IO.File]::ReadAllText($file.FullName) }
    @{ Headers = $headers; Sources = $sources }
}

Export-ModuleMember -Function Get-AssignSetterAllowList, Find-AssignGateCoverageFindings, Get-ClassSetterDeclarations, Get-ControlClassNames, Get-SetterDefinitions, Get-AssignSetterInventory, Find-AssignSetterFindings, Get-AssignSetterInputs
