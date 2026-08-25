# build_atlas.ps1 - Genera l'atlas texture della PSP dagli asset PNG ORIGINALI di Abisso 2.0.
# Replica fedelmente SPRITE_SHEET_INFO / ENTITY_SPRITES / SPRITE_CROP dell'index.html:
#  - sheets divisi in celle (4 eroi/mostri, 2 forzieri)
#  - crop dei margini trasparenti delle immagini AI ([x,y,w,h])
#  - tile pre-ridotti a 64x64 come prepareTileTextures()
# Output: assets/atlas.rle (RGBA8888 RLE), src/atlas_data.h, src/atlas_rects.c
param(
    [string]$SrcRoot = "$env:TEMP\opencode\Abisso-2.0",
    [string]$OutRoot = "$PSScriptRoot\.."
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$spritesDir = Join-Path $SrcRoot 'assets\sprites'
$specialDir = Join-Path $SrcRoot 'assets\speciali'

function Get-TrimRect([System.Drawing.Bitmap]$b){
    $rect = New-Object System.Drawing.Rectangle(0,0,$b.Width,$b.Height)
    $fmt  = [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    $bd   = $b.LockBits($rect,[System.Drawing.Imaging.ImageLockMode]::ReadOnly,$fmt)
    $minX=$b.Width; $minY=$b.Height; $maxX=-1; $maxY=-1
    $bytes = New-Object byte[] ($b.Width*$b.Height*4)
    [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$bytes,0,$bytes.Length)
    for ($y=0; $y -lt $b.Height; $y++){
        $rowOff = $y*$bd.Stride
        for ($x=0; $x -lt $b.Width; $x++){
            $a = $bytes[$rowOff + $x*4 + 3]
            if ($a -gt 8){
                if ($x -lt $minX){$minX=$x}; if ($x -gt $maxX){$maxX=$x}
                if ($y -lt $minY){$minY=$y}; if ($y -gt $maxY){$maxY=$y}
            }
        }
    }
    $b.UnlockBits($bd)
    if ($maxX -lt 0){ return @{X=0;Y=0;W=1;H=1} }
    return @{X=$minX;Y=$minY;W=$maxX-$minX+1;H=$maxY-$minY+1}
}

function New-Scaled([System.Drawing.Bitmap]$src,[int]$tw,[int]$th){
    $out = New-Object System.Drawing.Bitmap($tw,$th,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($out)
    $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.DrawImage($src,(New-Object System.Drawing.Rectangle(0,0,$tw,$th)))
    $g.Dispose()
    return $out
}

# Ritaglia una cella: bitmap sorgente -> bitmap ritagliata (crop opzionale + autotrim)
function Get-CellBitmap([System.Drawing.Bitmap]$img,[int]$cx,[int]$cy,[int]$cw,[int]$ch,[int[]]$crop){
    $sx=$cx; $sy=$cy; $sw=$cw; $sh=$ch
    if ($crop){ $sx+= $crop[0]; $sy+=$crop[1]; $sw=$crop[2]; $sh=$crop[3] }
    $region = New-Object System.Drawing.Rectangle($sx,$sy,$sw,$sh)
    $cell = $img.Clone($region,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $t = Get-TrimRect $cell
    $trimmed = $cell.Clone((New-Object System.Drawing.Rectangle($t.X,$t.Y,$t.W,$t.H)),[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $cell.Dispose()
    return ,$trimmed
}

# Scala mantenendo l'aspetto dentro un box (lato lungo = maxSide)
function Fit-Box([System.Drawing.Bitmap]$src,[int]$maxSide){
    $ratio = [Math]::Min($maxSide / $src.Width, $maxSide / $src.Height)
    if ($ratio -gt 1){ $ratio = 1 }
    $tw = [Math]::Max(1,[int][Math]::Round($src.Width * $ratio))
    $th = [Math]::Max(1,[int][Math]::Round($src.Height* $ratio))
    return (New-Scaled $src $tw $th)
}

$entries = New-Object System.Collections.ArrayList   # {Name,Bmp}

function Add-Entry([string]$name,[System.Drawing.Bitmap]$bmp){
    [void]$entries.Add(@{Name=$name; Bmp=$bmp})
}

function Add-Single([string]$name,[string]$path,[int]$maxSide,[int[]]$crop){
    Write-Host " - $name"
    $img = [System.Drawing.Bitmap]::FromFile($path)
    try {
        $cell = Get-CellBitmap $img 0 0 $img.Width $img.Height $crop
        $fit  = Fit-Box $cell $maxSide
        $cell.Dispose()
        Add-Entry $name $fit
    } finally { $img.Dispose() }
}

function Add-SheetCells([string]$prefix,[string[]]$names,[string]$path,[int]$cols,[int]$maxSide){
    $img = [System.Drawing.Bitmap]::FromFile($path)
    try {
        $cw = [int]($img.Width / $cols); $chh = $img.Height
        for ($i=0; $i -lt $names.Count; $i++){
            $cell = Get-CellBitmap $img ($i*$cw) 0 $cw $chh $null
            $fit  = Fit-Box $cell $maxSide
            $cell.Dispose()
            Add-Entry ($prefix + $names[$i]) $fit
        }
    } finally { $img.Dispose() }
}

Write-Host "== Caricamento asset originali =="

# --- Tile (pre-ridotti a 64x64 con crop, identici a prepareTileTextures) ---
$tiles = @{
    floor_dirt = @(420,92,555,601)
    floor_stone= @(350,67,709,638)
    wall_brick = @(368,45,643,678)
    wall_stone = @(186,0,1037,768)
}
foreach ($k in @('floor_dirt','floor_stone','wall_brick','wall_stone')){
    Add-Single $k (Join-Path $spritesDir "$k.png") 64 $tiles[$k]
}

# --- Props ---
Add-Single 'stairs'   (Join-Path $spritesDir 'stairs.png')   72 @(486,111,437,542)
Add-Single 'torch'    (Join-Path $spritesDir 'torch.png')    88 @(516,132,401,480)
Add-Single 'merchant' (Join-Path $spritesDir 'merchant.png') 76 $null
Add-SheetCells 'chest_' @('closed','open') (Join-Path $spritesDir 'chest_sheet.png') 2 56

# --- Eroi (heroes_sheet: guerriero ladro mago ranger + singoli) ---
Add-SheetCells 'hero_' @('guerriero','ladro','mago','ranger') (Join-Path $spritesDir 'heroes_sheet.png') 4 84
Add-Single 'hero_prof'       (Join-Path $specialDir 'prof.png') 84 $null
foreach ($h in @('paladino','negromante','bardo','monaco')){
    Add-Single "hero_$h" (Join-Path $spritesDir "hero_$h.png") 84 $null
}

# --- Mostri ---
Add-SheetCells 'mon_' @('ratto','pipistrello','goblin','scheletro') (Join-Path $spritesDir 'monsters_sheet1.png') 4 80
Add-SheetCells 'mon_' @('melma','gelatina','zombie','ragno')        (Join-Path $spritesDir 'monsters_sheet2.png') 4 80
foreach ($m in @('spettro','drago','orco','serpente','arpia','cavaliere','cavaliere_alt','cultista','golem','mantide','sciamano')){
    Add-Single "mon_$m" (Join-Path $spritesDir "mon_$m.png") 80 $null
}

# --- Boss ---
foreach ($b in @('boss_golem','boss_lich','boss_melme','boss_ragno','boss_ratti')){
    Add-Single $b (Join-Path $spritesDir "$b.png") 120 $null
}

# --- Icone ---
Add-Single 'icon_gold'        (Join-Path $spritesDir 'icon_gold.png')          48 $null
Add-Single 'icon_gem_blue'    (Join-Path $spritesDir 'icon_gem_blue.png')      48 $null
Add-Single 'icon_potion_hp'   (Join-Path $spritesDir 'icon_potion_hp.png')     48 $null
Add-Single 'icon_potion_mana' (Join-Path $spritesDir 'icon_potion_mana.png')   48 $null
Add-Single 'pw_furia'         (Join-Path $spritesDir 'furia.png')              48 $null
Add-Single 'pw_shield'        (Join-Path $spritesDir 'icon_shield_buff.png')   48 $null
Add-Single 'pw_haste'         (Join-Path $spritesDir 'potenziamento_fretta.png')48 $null
Add-Single 'pw_focus'         (Join-Path $spritesDir 'powerupfocus.png')       48 $null
Add-Single 'icon_lightning'   (Join-Path $spritesDir 'icon_lightning.png')     48 @(538,116,345,548)
foreach ($e in @('equip_helm','equip_necklace','equip_armor','equip_ring','equip_greaves')){
    Add-Single $e (Join-Path $spritesDir "$e.png") 48 $null
}

# Glow radiale generato (per torce/luci/bagliori: sostituisce i radialGradient del canvas)
& {
    $sz=64; $glow = New-Object System.Drawing.Bitmap($sz,$sz,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bd = $glow.LockBits((New-Object System.Drawing.Rectangle(0,0,$sz,$sz)),[System.Drawing.Imaging.ImageLockMode]::WriteOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $pix = New-Object byte[] ($sz*$sz*4)
    $half = ($sz-1)/2.0
    for ($y=0;$y -lt $sz;$y++){ for ($x=0;$x -lt $sz;$x++){
        $dx=($x-$half)/$half; $dy=($y-$half)/$half
        $d=[Math]::Sqrt($dx*$dx+$dy*$dy)
        $a=[Math]::Max(0,[Math]::Min(1,1-$d))
        $a=[Math]::Pow($a,1.6)
        $off=($y*$sz+$x)*4
        $pix[$off]=255; $pix[$off+1]=255; $pix[$off+2]=255; $pix[$off+3]=[byte][Math]::Round($a*255)
    }}
    [System.Runtime.InteropServices.Marshal]::Copy($pix,0,$bd.Scan0,$pix.Length)
    $glow.UnlockBits($bd)
    Add-Entry 'glow' $glow
}

# ================= Packing 1024 x H =================
$AW = 1024
$pad = 2
$placed = @{}
$x=0; $y=0; $rowH=0
foreach ($e in $entries){
    $w=$e.Bmp.Width; $h=$e.Bmp.Height
    if ($x+$w+$pad -gt $AW){ $x=0; $y+=$rowH+$pad; $rowH=0 }
    $placed[$e.Name] = @{X=$x;Y=$y;W=$w;H=$h}
    $x += $w+$pad
    if ($h -gt $rowH){ $rowH=$h }
}
$usedH = $y+$rowH+$pad
$AH = 512; while ($AH -lt $usedH){ $AH *= 2 }

Write-Host ("Atlas: {0}x{1} ({2} sprite)" -f $AW,$AH,$entries.Count)

# ================= Rasterizzazione =================
$pixels = New-Object uint32[] ($AW*$AH)
foreach ($e in $entries){
    $p = $placed[$e.Name]
    $bmp = $e.Bmp
    $rect = New-Object System.Drawing.Rectangle(0,0,$bmp.Width,$bmp.Height)
    $bd = $bmp.LockBits($rect,[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bytes = New-Object byte[] ($bmp.Width*$bmp.Height*4)
    [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$bytes,0,$bytes.Length)
    $bmp.UnlockBits($bd)
    for ($yy=0; $yy -lt $bmp.Height; $yy++){
        $dstRow = ($p.Y+$yy)*$AW + $p.X
        $srcRow = $yy*$bmp.Width*4
        for ($xx=0; $xx -lt $bmp.Width; $xx++){
            $b=$bytes[$srcRow+$xx*4]; $g=$bytes[$srcRow+$xx*4+1]; $r=$bytes[$srcRow+$xx*4+2]; $a=$bytes[$srcRow+$xx*4+3]
            $pixels[$dstRow+$xx] = [uint32](($a -shl 24) -bor ($b -shl 16) -bor ($g -shl 8) -bor $r)
        }
    }
}

# ================= RLE =================
$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)
$total = $AW*$AH
$i=0
while ($i -lt $total){
    $v = $pixels[$i]; $run=1
    while (($i+$run -lt $total) -and ($pixels[$i+$run] -eq $v) -and ($run -lt 65535)){ $run++ }
    $bw.Write([uint16]$run)
    $bw.Write([uint32]$v)
    $i += $run
}
$bw.Flush()
$rleBytes = $ms.ToArray()
$outAssets = Join-Path $OutRoot 'assets'
New-Item -ItemType Directory -Force -Path $outAssets | Out-Null
[System.IO.File]::WriteAllBytes((Join-Path $outAssets 'atlas.rle'), $rleBytes)
Write-Host ("atlas.rle: {0} KB (da {1} KB raw)" -f [int]($rleBytes.Length/1024), [int]($total*4/1024))

# ================= Header + rects =================
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('/* Generato da tools/build_atlas.ps1 - atlas degli asset originali di Abisso 2.0 */')
[void]$sb.AppendLine('#ifndef ABISSO_ATLAS_DATA_H')
[void]$sb.AppendLine('#define ABISSO_ATLAS_DATA_H')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#define ATLAS_W '+$AW)
[void]$sb.AppendLine('#define ATLAS_H '+$AH)
[void]$sb.AppendLine('')
[void]$sb.AppendLine('typedef struct { unsigned short u,v,w,h; } ARect;')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('enum {')
for ($idx=0; $idx -lt $entries.Count; $idx++){
    $enumName = 'AR_' + ($entries[$idx].Name.ToUpper() -replace '[^A-Z0-9_]','_')
    $comma = if ($idx -lt $entries.Count-1) { ',' } else { '' }
    [void]$sb.AppendLine("    $enumName = $idx$comma")
}
[void]$sb.AppendLine('};')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('extern const ARect g_atlasRects['+$entries.Count+'];')
[void]$sb.AppendLine('int atlasLoad(void);   /* decomprime atlas.rle nel buffer texture */')
[void]$sb.AppendLine('unsigned int* atlasPixels(void);')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#endif')
[System.IO.File]::WriteAllText((Join-Path $OutRoot 'src\atlas_data.h'), $sb.ToString())

$rc = New-Object System.Text.StringBuilder
[void]$rc.AppendLine('/* Generato da tools/build_atlas.ps1 */')
[void]$rc.AppendLine('#include "atlas_data.h"')
[void]$rc.AppendLine('')
[void]$rc.AppendLine('const ARect g_atlasRects['+$entries.Count+'] = {')
$lines = @()
foreach ($e in $entries){
    $p = $placed[$e.Name]
    $lines += ('    {'+$p.X+','+$p.Y+','+$p.W+','+$p.H+'} /* '+$e.Name+' */')
}
[void]$rc.AppendLine(($lines -join ",`n") + '};')
[System.IO.File]::WriteAllText((Join-Path $OutRoot 'src\atlas_rects.c'), $rc.ToString())

Write-Host "OK: src/atlas_data.h, src/atlas_rects.c scritti."
