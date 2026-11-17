# Phase D0: native Windows sequential write speed, to compare with fio inside WSL.
#   powershell -File infra\d0\windows_write_test.ps1 [-Drive D:] [-SizeMB 2048]
# Writes SizeMB of data in 1 MiB blocks with write-through (no Windows write cache),
# reports MB/s, then deletes the file.
param([string]$Drive = "D:", [int]$SizeMB = 2048)

$path = Join-Path $Drive "arena-d0-write-test.bin"
$block = New-Object byte[] (1MB)
(New-Object Random 1).NextBytes($block)  # random data, so nothing can compress it

$stream = New-Object IO.FileStream($path, [IO.FileMode]::Create, [IO.FileAccess]::Write,
    [IO.FileShare]::None, 1MB, [IO.FileOptions]::WriteThrough)
$timer = [Diagnostics.Stopwatch]::StartNew()
for ($i = 0; $i -lt $SizeMB; $i++) { $stream.Write($block, 0, $block.Length) }
$stream.Flush($true)
$timer.Stop()
$stream.Close()
Remove-Item $path

"{0}: wrote {1} MiB in {2:N2} s = {3:N0} MiB/s" -f $Drive, $SizeMB, $timer.Elapsed.TotalSeconds, ($SizeMB / $timer.Elapsed.TotalSeconds)
