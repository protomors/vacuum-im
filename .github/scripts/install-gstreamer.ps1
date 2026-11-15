$ErrorActionPreference = 'Stop'
$version = '1.26.9'
$base = "https://gstreamer.freedesktop.org/data/pkg/windows/$version/msvc"
$downloads = Join-Path $env:RUNNER_TEMP 'gstreamer-downloads'
$destination = Join-Path $env:RUNNER_TEMP 'gstreamer-sdk'
New-Item -ItemType Directory -Force $downloads, $destination | Out-Null
$packages = @(
    @{ Name = "gstreamer-1.0-msvc-x86_64-$version.msi"; Hash = 'ef56a1e9077c6cadcf8384369d1e435a0f3bc0d5a26594773256cffe79f40ecb' },
    @{ Name = "gstreamer-1.0-devel-msvc-x86_64-$version.msi"; Hash = '79c7ca2a5013181d94a2c823ef1c8a575f3e036e813633dc3e5ac3d393581a15' }
)
foreach ($package in $packages) {
    $installer = Join-Path $downloads $package.Name
    Invoke-WebRequest "$base/$($package.Name)" -OutFile $installer
    if ((Get-FileHash $installer -Algorithm SHA256).Hash.ToLowerInvariant() -ne $package.Hash) {
        throw "Checksum mismatch for $($package.Name)"
    }
    # Administrative extraction provides all runtime/development files without
    # installing or registering a machine-wide GStreamer runtime.
    $arguments = "/a `"$installer`" /qn /norestart TARGETDIR=`"$destination`""
    $process = Start-Process msiexec.exe -ArgumentList $arguments -Wait -PassThru
    if ($process.ExitCode -notin @(0, 3010)) {
        throw "GStreamer extraction failed with exit code $($process.ExitCode)"
    }
}
$inspectors = @(Get-ChildItem $destination -Recurse -Filter 'gst-inspect-1.0.exe')
if ($inspectors.Count -ne 1) { throw 'Could not locate the extracted GStreamer runtime' }
$prefix = Split-Path $inspectors[0].Directory.FullName -Parent
if (-not (Test-Path "$prefix/lib/pkgconfig/gstreamer-webrtc-1.0.pc")) {
    throw 'GStreamer WebRTC development files are missing'
}
"VACUUM_GSTREAMER_ROOT=$prefix" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
