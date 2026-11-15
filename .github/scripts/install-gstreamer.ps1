$ErrorActionPreference = 'Stop'
# 1.28.8 is the latest published MSVC binary installer (1.28.9 is source-only).
$version = '1.28.8'
$filename = "gstreamer-1.0-msvc-x86_64-$version.exe"
$url = "https://gstreamer.freedesktop.org/data/pkg/windows/$version/msvc/$filename"
$hash = 'e4c41ee6ee9c6c32a45d198463ec08d79bac3bd5fa58adc93d13fe4a08abc9d9'
$installer = Join-Path $env:RUNNER_TEMP $filename
$prefix = Join-Path $env:RUNNER_TEMP 'gstreamer-sdk'
$log = Join-Path $env:RUNNER_TEMP 'gstreamer-install.log'
Invoke-WebRequest $url -OutFile $installer
if ((Get-FileHash $installer -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hash) {
    throw "Checksum mismatch for $filename"
}
# GStreamer 1.28 combines runtime and development files in an Inno Setup installer.
$arguments = "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /CURRENTUSER /TYPE=devel /DIR=`"$prefix`" /LOG=`"$log`""
$process = Start-Process $installer -ArgumentList $arguments -Wait -PassThru
if ($process.ExitCode -notin @(0, 3010)) {
    if (Test-Path $log) { Get-Content $log -Tail 60 }
    throw "GStreamer installation failed with exit code $($process.ExitCode)"
}
foreach ($file in @('bin/gst-inspect-1.0.exe', 'lib/pkgconfig/gstreamer-webrtc-1.0.pc')) {
    if (-not (Test-Path (Join-Path $prefix $file))) {
        throw "GStreamer runtime/development file is missing: $file"
    }
}
"VACUUM_GSTREAMER_ROOT=$prefix" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
