# Installs or removes the test-signed MyPods AAP driver. ONLY in the test VM (see docs/WINDOWS-DRIVER.md):
# it needs test signing, which is off (and stays off) on a normal system.
#   powershell -ExecutionPolicy Bypass -File install.ps1            install
#   powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall remove
param([switch]$Uninstall)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole('Administrators')) {
    throw 'Run as administrator.'
}
# Refuse to run anywhere but a VM, so a slip can't put a test driver on the real machine
$model = (Get-CimInstance Win32_ComputerSystem).Model
if ($model -notmatch 'VirtualBox|Virtual Machine|VMware') {
    throw "Not a virtual machine ($model). The test driver is installed only in the test VM."
}

if ($Uninstall) {
    .\aaptool.exe unregister
    $published = pnputil /enum-drivers | Select-String -Context 1,0 'mypodsaap.inf' |
        ForEach-Object { ($_.Context.PreContext[0] -split ':\s+')[1] }
    foreach ($inf in $published) { pnputil /delete-driver $inf /uninstall /force }
    return
}

if (-not (bcdedit /enum '{current}' | Select-String 'testsigning\s+Yes')) {
    bcdedit /set testsigning on | Out-Null
    throw 'Test signing was off and is now on. Reboot the VM, then run this script again.'
}
# Trust the test certificate on this machine only (the VM)
Import-Certificate -FilePath .\mypods-test.cer -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
Import-Certificate -FilePath .\mypods-test.cer -CertStoreLocation Cert:\LocalMachine\TrustedPublisher | Out-Null

pnputil /add-driver .\mypodsaap.inf /install
.\aaptool.exe register
Start-Sleep -Seconds 2
Get-PnpDevice -FriendlyName 'MyPods AirPods (AAP)' -ErrorAction SilentlyContinue | Format-Table Status, FriendlyName, InstanceId -AutoSize
