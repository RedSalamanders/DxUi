# The GitHub-hosted Windows ARM image can leave its first-sign-in privacy experience above ordinary topmost windows.
# Run only immediately before a verified hosted foreground lease. Leave every independently launched process alive.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'InteractiveRun.psm1') -Force

if (-not (Test-DxUiVerifiedGitHubHostedRunner) -or $env:RUNNER_ARCH -cne 'ARM64' -or
    [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne [System.Runtime.InteropServices.Architecture]::Arm64) {
    throw 'Hosted ARM64 desktop preparation requires a verified GitHub-hosted native ARM64 Windows runner.'
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class DxUiHostedArmDesktop
{
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr window, StringBuilder text, int capacity);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string className, string title);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window, int command);
}
'@

$foreground = [DxUiHostedArmDesktop]::GetForegroundWindow()
if ($foreground -eq [IntPtr]::Zero) {
    Write-Host 'Hosted ARM64 desktop: no foreground window to prepare.'
    return
}
$processId = [uint32]0
[void][DxUiHostedArmDesktop]::GetWindowThreadProcessId($foreground, [ref]$processId)
$class = [System.Text.StringBuilder]::new(256)
$title = [System.Text.StringBuilder]::new(256)
[void][DxUiHostedArmDesktop]::GetClassName($foreground, $class, $class.Capacity)
[void][DxUiHostedArmDesktop]::GetWindowText($foreground, $title, $title.Capacity)
$processName = try { (Get-Process -Id $processId -ErrorAction Stop).ProcessName } catch { '<exited>' }
Write-Host ("Hosted ARM64 foreground: class='{0}' title='{1}' process={2} pid={3}" -f $class, $title, $processName, $processId)
if ($processName -ine 'WWAHost' -or $class.ToString() -cne 'Windows.UI.Core.CoreWindow') { return }

# Microsoft documents DisablePrivacyExperience for the current user. The engagement flag is the hosted-image
# first-sign-in setting used by Electron's ARM CI. Both only affect this disposable GitHub-hosted user session.
$policy = 'HKCU:\Software\Policies\Microsoft\Windows\OOBE'
$engagement = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\UserProfileEngagement'
New-Item -Path $policy -Force | Out-Null
New-ItemProperty -Path $policy -Name DisablePrivacyExperience -PropertyType DWord -Value 1 -Force | Out-Null
New-Item -Path $engagement -Force | Out-Null
New-ItemProperty -Path $engagement -Name ScoobeSystemSettingEnabled -PropertyType DWord -Value 0 -Force | Out-Null

# Minimize only the exact foreground privacy host seen above. Do not terminate WWAHost or UserOOBEBroker.
if ([DxUiHostedArmDesktop]::GetForegroundWindow() -ne $foreground) {
    Write-Host 'Hosted ARM64 desktop: foreground changed before privacy host could be minimized.'
    return
}
[void][DxUiHostedArmDesktop]::ShowWindow($foreground, 6) # SW_MINIMIZE
Start-Sleep -Milliseconds 250
if ([DxUiHostedArmDesktop]::IsWindow($foreground) -and -not [DxUiHostedArmDesktop]::IsIconic($foreground)) {
    throw 'Hosted ARM64 first-sign-in CoreWindow refused minimization; the foreground lease remains fail-closed.'
}
Write-Host 'Hosted ARM64 desktop: minimized the first-sign-in CoreWindow without terminating its process.'

# The same first-sign-in flow also has a separate topmost UserOOBE broker window behind WWAHost. On the
# hosted ARM image it becomes the next WindowFromPoint result after the CoreWindow is minimized.
$oobe = [DxUiHostedArmDesktop]::FindWindowEx([IntPtr]::Zero, [IntPtr]::Zero, 'UserOOBEWindowClass', $null)
if ($oobe -ne [IntPtr]::Zero -and [DxUiHostedArmDesktop]::IsWindowVisible($oobe) -and -not [DxUiHostedArmDesktop]::IsIconic($oobe)) {
    $oobeProcessId = [uint32]0
    [void][DxUiHostedArmDesktop]::GetWindowThreadProcessId($oobe, [ref]$oobeProcessId)
    $oobeProcessName = try { (Get-Process -Id $oobeProcessId -ErrorAction Stop).ProcessName } catch { '<exited>' }
    Write-Host ("Hosted ARM64 OOBE window: class=UserOOBEWindowClass process={0} pid={1}" -f $oobeProcessName, $oobeProcessId)
    [void][DxUiHostedArmDesktop]::ShowWindow($oobe, 6) # SW_MINIMIZE
    Start-Sleep -Milliseconds 250
    if ([DxUiHostedArmDesktop]::IsWindow($oobe) -and -not [DxUiHostedArmDesktop]::IsIconic($oobe)) {
        throw 'Hosted ARM64 UserOOBEWindowClass refused minimization; the foreground lease remains fail-closed.'
    }
    Write-Host 'Hosted ARM64 desktop: minimized the first-sign-in broker window without terminating its process.'
}
