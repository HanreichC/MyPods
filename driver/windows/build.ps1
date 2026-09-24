# Builds the MyPods AAP driver package: mypodsaap.sys, the stamped INF, the catalog, all test-signed.
# Runs on the host and loads nothing; the package is installed only in the test VM (install.ps1).
#   powershell -File driver\windows\build.ps1
# Toolchain: %LOCALAPPDATA%\MyPodsToolchain (MSVC, SDK, WDK from NuGet). Output: build\driver\
$ErrorActionPreference = 'Stop'
$T = Join-Path $env:LOCALAPPDATA 'MyPodsToolchain'
. (Join-Path $T 'env.ps1')
$V = '10.0.26100.0'
$W = Join-Path $T 'wdk\c'
$Out = Join-Path $PSScriptRoot '..\..\build\driver'
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path

# Kernel mode: no user-mode headers or CRT (/X drops %INCLUDE%). /analyze with the WDK driver rules, findings fail the build.
$includes = "$W\Include\$V\km", "$W\Include\$V\km\crt", "$W\Include\$V\shared", "$W\Include\wdf\kmdf\1.15", "$T\sdk\Include\$V\shared", "$T\msvc\include" | ForEach-Object { "/external:I$_" }
$defines = '_AMD64_', 'AMD64', '_WIN64', 'WINNT=1', 'NTDDI_VERSION=0x0A000004', '_WIN32_WINNT=0x0A00',
           'KMDF_VERSION_MAJOR=1', 'KMDF_VERSION_MINOR=15' | ForEach-Object { "/D$_" }
& cl.exe /nologo /c /kernel /X /W4 /WX /wd4201 /wd4324 /external:W0 /analyze:external- /analyze /analyze:plugin "$W\bin\$V\x64\DRIVERS.dll" /Zi /O2 /GS /Gy /GF /Zp8 @includes @defines `
    "/Fo$Out\mypodsaap.obj" "/Fd$Out\mypodsaap_c.pdb" (Join-Path $PSScriptRoot 'mypodsaap.c')
if ($LASTEXITCODE) { throw "compile failed" }

$libs = "$W\Lib\$V\km\x64\ntoskrnl.lib", "$W\Lib\$V\km\x64\hal.lib", "$W\Lib\$V\km\x64\BufferOverflowFastFailK.lib",
        "$W\Lib\wdf\kmdf\x64\1.15\WdfLdr.lib", "$W\Lib\wdf\kmdf\x64\1.15\WdfDriverEntry.lib"
& link.exe /nologo /DRIVER /SUBSYSTEM:NATIVE,10.00 /ENTRY:FxDriverEntry /NODEFAULTLIB /MACHINE:X64 /RELEASE /DEBUG `
    /OPT:REF /OPT:ICF /SECTION:INIT,d /MERGE:_TEXT=.text /MERGE:_PAGE=PAGE `
    "/OUT:$Out\mypodsaap.sys" "/PDB:$Out\mypodsaap.pdb" "$Out\mypodsaap.obj" @libs
if ($LASTEXITCODE) { throw "link failed" }

Copy-Item (Join-Path $PSScriptRoot 'mypodsaap.inf') $Out -Force
& "$W\bin\$V\x64\stampinf.exe" -f "$Out\mypodsaap.inf" -d '*' -a amd64 -k 1.15 -v 0.1.0.0
if ($LASTEXITCODE) { throw "stampinf failed" }
Remove-Item "$Out\mypodsaap.cat" -ErrorAction SilentlyContinue
& "$W\bin\$V\x86\Inf2Cat.exe" "/driver:$Out" /os:10_X64 /uselocaltime
if ($LASTEXITCODE) { throw "inf2cat failed" }

# Test certificate, kept next to the toolchain; the VM trusts it (install.ps1), the host never does.
$pfx = Join-Path $T 'mypods-test.pfx'
if (-not (Test-Path $pfx)) {
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=MyPods Test Signing' -CertStoreLocation Cert:\CurrentUser\My -NotAfter (Get-Date).AddYears(5)
    $pw = ConvertTo-SecureString 'mypods' -AsPlainText -Force
    Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $pw | Out-Null
    Export-Certificate -Cert $cert -FilePath (Join-Path $T 'mypods-test.cer') | Out-Null
    Remove-Item "Cert:\CurrentUser\My\$($cert.Thumbprint)"
}
Copy-Item (Join-Path $T 'mypods-test.cer') $Out -Force
& signtool.exe sign /q /fd sha256 /f $pfx /p mypods "$Out\mypodsaap.sys" "$Out\mypodsaap.cat"
if ($LASTEXITCODE) { throw "signing failed" }
# Helper for the VM (register the service, D2 check); static CRT so it runs on a bare test system
& cl.exe /nologo /EHsc /O2 /W4 /WX /MT "/Fe$Out\aaptool.exe" "/Fo$Out\aaptool.obj" (Join-Path $PSScriptRoot 'aaptool.cpp') `
    /link bthprops.lib cfgmgr32.lib advapi32.lib
if ($LASTEXITCODE) { throw "aaptool failed" }
Copy-Item (Join-Path $PSScriptRoot 'install.ps1') $Out -Force
Write-Host "Driver package: $Out"
