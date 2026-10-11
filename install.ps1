# Installs OUI Code, oui_run and oui_calc on Windows from the latest release: each one .exe, with nothing to install
# beside it and no administrator needed. Or downloads a whole package, the programs with their licences.
#
#   irm https://raw.githubusercontent.com/iganinja/olagarro/master/install.ps1 | iex
#
# With no options it shows a menu of what there is and asks. Options choose without asking; through irm they go
# after the script made a script block:
#
#   -Program oui_code, oui_run or oui_calc   those programs (oui_code unless given)
#   -All                                     the three
#   -Gui                                     those that also open in a window of their own, under the same names,
#                                            rather than those for the terminal only
#   -Package                                 the whole package, the three with LICENSE and THIRD_PARTY_NOTICES.txt,
#                                            unpacked into the folder it is run from rather than installed
#
#   & ([scriptblock]::Create((irm https://raw.githubusercontent.com/iganinja/olagarro/master/install.ps1))) -All -Gui
#
# The programs go in %LOCALAPPDATA%\Programs\olagarro, which is added to the user's PATH when it is not there. Every
# download is checked against the release's SHA256SUMS before it is used. OUI_INSTALL_DIR says where programs go
# instead; OUI_DOWNLOAD_URL where they come from, the latest release unless set. Works in Windows PowerShell 5.1 and
# in PowerShell 7.
param(
    [ValidateSet("oui_code", "oui_run", "oui_calc")]
    [string[]]$Program,
    [switch]$All,
    [switch]$Gui,
    [switch]$Package
)

$ErrorActionPreference = "Stop"
# Invoke-WebRequest's progress bar slows a download down several times over in Windows PowerShell.
$ProgressPreference = "SilentlyContinue"
# Windows PowerShell 5.1 may not offer TLS 1.2 unless told, and GitHub takes nothing older.
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

# A failure is thrown, to be shown at the end: exit would close the whole window of a script run through iex.
function Fail([string]$message) {
    throw $message
}

function Install-Olagarro {
    if ($env:OS -ne "Windows_NT") {
        Fail "This installer is for Windows. On Linux, install.sh does the same: see the README."
    }
    # Windows on ARM runs the x86_64 programs through its emulation.
    if ($env:PROCESSOR_ARCHITECTURE -ne "AMD64" -and $env:PROCESSOR_ARCHITECTURE -ne "ARM64") {
        Fail "The releases have programs for x86_64 only, and this is $env:PROCESSOR_ARCHITECTURE."
    }

    $source = $env:OUI_DOWNLOAD_URL
    if (-not $source) { $source = "https://github.com/iganinja/olagarro/releases/latest/download" }
    $target = $env:OUI_INSTALL_DIR
    if (-not $target) { $target = Join-Path $env:LOCALAPPDATA "Programs\olagarro" }

    # What to get: the programs, the kind, terminal or gui, and whether the whole package.
    $programs = @()
    if ($Program) { $programs = $Program }
    if ($All) { $programs = @("oui_code", "oui_run", "oui_calc") }
    $kind = "terminal"
    if ($Gui) { $kind = "gui" }
    $wholePackage = [bool]$Package

    if (-not ($Program -or $All -or $Gui -or $Package)) {
        Write-Host @"
Olagarro: what would you like?

  Installed in $target, one .exe each:
   1) OUI Code, the text editor                    terminal
   2) OUI Code, the text editor                    terminal and GUI
   3) oui_run, which runs JavaScript programs      terminal
   4) oui_run, which runs JavaScript programs      terminal and GUI
   5) oui_calc, a calculator                       terminal
   6) oui_calc, a calculator                       terminal and GUI
   7) All three                                    terminal
   8) All three                                    terminal and GUI

  The whole package, with its licences, unpacked into this folder:
   9) All three                                    terminal
  10) All three                                    terminal and GUI

   0) Nothing

"@
        # In a switch, break and continue act on the switch: whether the choice was one says when the asking ends.
        $chosen = $false
        while (-not $chosen) {
            $choice = Read-Host "Your choice [1]"
            if (-not $choice) { $choice = "1" }
            $chosen = $true
            switch ($choice.Trim()) {
                "1" { $programs = @("oui_code") }
                "2" { $programs = @("oui_code"); $kind = "gui" }
                "3" { $programs = @("oui_run") }
                "4" { $programs = @("oui_run"); $kind = "gui" }
                "5" { $programs = @("oui_calc") }
                "6" { $programs = @("oui_calc"); $kind = "gui" }
                "7" { $programs = @("oui_code", "oui_run", "oui_calc") }
                "8" { $programs = @("oui_code", "oui_run", "oui_calc"); $kind = "gui" }
                "9" { $wholePackage = $true }
                "10" { $wholePackage = $true; $kind = "gui" }
                "0" { Write-Host "Nothing installed."; return }
                default { Write-Host "That is no choice: a number from 0 to 10."; $chosen = $false }
            }
        }
    }
    if ($programs.Count -eq 0) { $programs = @("oui_code") }

    $work = Join-Path ([IO.Path]::GetTempPath()) ("olagarro-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $work | Out-Null
    try {
        try {
            Invoke-WebRequest -UseBasicParsing -Uri "$source/SHA256SUMS" -OutFile (Join-Path $work "SHA256SUMS")
        } catch {
            Fail "The release's SHA256SUMS could not be downloaded from ${source}: $($_.Exception.Message)"
        }
        $sums = @{}
        foreach ($line in Get-Content (Join-Path $work "SHA256SUMS")) {
            $parts = $line -split "\s+", 2
            if ($parts.Count -eq 2) { $sums[$parts[1].TrimStart("*")] = $parts[0].ToLower() }
        }

        # A file of the release, downloaded and checked against its sum.
        function Get-ReleaseFile([string]$name) {
            Write-Host "Downloading $name..."
            $path = Join-Path $work $name
            try {
                Invoke-WebRequest -UseBasicParsing -Uri "$source/$name" -OutFile $path
            } catch {
                Fail "$name could not be downloaded from ${source}: $($_.Exception.Message)"
            }
            if (-not $sums.ContainsKey($name)) { Fail "The release's SHA256SUMS does not list ${name}: nothing done." }
            if ((Get-FileHash -Algorithm SHA256 -Path $path).Hash.ToLower() -ne $sums[$name]) {
                Fail "$name is not what the release says it is: nothing done."
            }
            return $path
        }

        if ($wholePackage) {
            $name = "olagarro-windows-x86_64-$kind"
            $folder = Join-Path (Get-Location) $name
            if (Test-Path $folder) { Fail "$name is here already: nothing done." }
            $zip = Get-ReleaseFile "$name.zip"
            Expand-Archive -Path $zip -DestinationPath (Get-Location)
            Write-Host "Unpacked into ${folder}: the programs, LICENSE and THIRD_PARTY_NOTICES.txt."
            return
        }

        # Every program downloaded and checked before any is installed: a failure leaves nothing half done.
        $downloaded = @{}
        foreach ($each in $programs) {
            $downloaded[$each] = Get-ReleaseFile "$each-windows-x86_64-$kind.exe"
        }
        New-Item -ItemType Directory -Force -Path $target | Out-Null
        $installed = @()
        foreach ($each in $programs) {
            $name = $each
            Move-Item -Force -Path $downloaded[$each] -Destination (Join-Path $target "$name.exe")
            Write-Host "Installed $(Join-Path $target "$name.exe")"
            $installed += $name
        }

        # The folder in the user's PATH, for new terminals, and in this one's.
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        $folders = @()
        if ($userPath) { $folders = $userPath -split ";" | Where-Object { $_ } }
        if ($folders -notcontains $target) {
            [Environment]::SetEnvironmentVariable("Path", (($folders + $target) -join ";"), "User")
            Write-Host "$target is now in your PATH: terminals opened from now on find the programs."
        }
        if (($env:Path -split ";") -notcontains $target) { $env:Path = "$env:Path;$target" }
        Write-Host "Run it: $($installed -join ', ')"
        if ($kind -eq "gui") { Write-Host "They open in the terminal; with --window, in a window of their own." }
    } finally {
        Remove-Item -Recurse -Force -Path $work -ErrorAction SilentlyContinue
    }
}

try {
    Install-Olagarro
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    # Run as a file it ends with a code saying so; through iex, exit would close the window.
    if ($PSCommandPath) { exit 1 }
}
