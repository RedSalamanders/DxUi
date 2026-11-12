# Engine acquisition/integrity for paired diagnostics; no assignment or verdict policy.
Set-StrictMode -Version Latest

function Initialize-PairedEngineInputs {
    <# Historical sources keep their own CI pin. Restore only an absent generated checkout;
       an existing dirty/wrong-pin/foreign engine is preserved and refused. The current
       harness reader is a separate frozen input, never copied into a historical engine. #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Source,[Parameter(Mandatory)][string]$Target)
    if(-not(Test-Path -LiteralPath (Join-Path $Target '.ci/project.json') -PathType Leaf)){return $null}
    $reader=Join-Path $Source '.ci-engine'
    Import-Module (Join-Path $Source '.ci-engine/Tools/Modules/Ci.SourceInputs.psm1') -ErrorAction Stop
    Import-Module (Join-Path $Source '.ci-engine/Tools/Modules/Ci.Policy.psm1') -ErrorAction Stop
    $readerIdentity=Get-CiPinnedEngineInputsIdentity -Root $Source -EngineRoot $reader
    $project=Read-CiProject $Target
    $engine=Assert-CiContainedPath $Target (Join-Path $Target '.ci-engine')
    $created=$false
    if(-not(Test-Path -LiteralPath $engine)){
        $null=Invoke-CiGit $Target @('-c','core.longpaths=true','clone','--quiet','--no-hardlinks',$reader,$engine)
        $created=$true
        $null=Invoke-CiGit $engine @('config','core.longpaths','true')
        # Hosted readers are shallow and need not contain the side's old commit.
        # Acquire only that exact pin from its declared canonical repository;
        # neither the historical side nor the common reader may advance.
        $present=Invoke-CiGit $engine @('cat-file','-e',($project.engine.ref+'^{commit}')) -AllowFailure
        if($present.exitCode -ne 0){
            $remote='https://github.com/'+$project.engine.repository+'.git'
            $null=Invoke-CiGit $engine @('fetch','--quiet','--no-tags','--depth=1',$remote,$project.engine.ref)
        }
        $null=Invoke-CiGit $engine @('checkout','--quiet','--detach',$project.engine.ref)
    }
    $actualIdentity=Get-CiPinnedEngineInputsIdentity -Root $Target -EngineRoot $engine
    if((Get-CiPinnedEngineInputsIdentity -Root $Source -EngineRoot $reader) -cne $readerIdentity){throw 'Paired harness engine changed during side preparation.'}
    return [ordered]@{ReaderRoot=$reader;ReaderInputsIdentity=$readerIdentity;ActualInputsIdentity=$actualIdentity;ActualPin=$project.engine.ref;Created=$created}
}

function Assert-PairedEngineInputs {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Target,[AllowNull()]$Context)
    if($null -eq $Context){
        if(Test-Path -LiteralPath (Join-Path $Target '.ci/project.json')){throw 'The side engine requirement changed after paired preparation.'}
        return
    }
    $reader=$Context['ReaderRoot']
    Import-Module (Join-Path (Split-Path $reader -Parent) '.ci-engine/Tools/Modules/Ci.SourceInputs.psm1') -ErrorAction Stop
    $currentReader=Get-CiPinnedEngineInputsIdentity -Root (Split-Path $reader -Parent) -EngineRoot $reader
    $currentSide=Get-CiPinnedEngineInputsIdentity -Root $Target -EngineRoot (Join-Path $Target '.ci-engine')
    if($currentReader -cne $Context['ReaderInputsIdentity'] -or $currentSide -cne $Context['ActualInputsIdentity']){
        throw 'Actual side or common reader engine inputs changed during the paired study.'
    }
}

Export-ModuleMember -Function Initialize-PairedEngineInputs,Assert-PairedEngineInputs
