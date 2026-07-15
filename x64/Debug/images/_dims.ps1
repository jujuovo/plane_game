Add-Type -AssemblyName System.Drawing
$files = @('bullet_player.png','bullet_enermy.png')
foreach ($f in $files) {
    $path = (Get-Item $f).FullName
    $img = [System.Drawing.Image]::FromFile($path)
    Write-Output ("$f : " + $img.Width + " x " + $img.Height)
    $img.Dispose()
}
