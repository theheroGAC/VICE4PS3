Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$fontPath = Join-Path $root "assets\data\font.ttf"
$outPath = Join-Path $root "assets\data\splash_hd.png"

$width = 1920
$height = 1080

$bitmap = New-Object -TypeName System.Drawing.Bitmap -ArgumentList @($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

$rect = New-Object -TypeName System.Drawing.Rectangle -ArgumentList @(0, 0, $width, $height)
$c1 = [System.Drawing.Color]::FromArgb(255, 8, 10, 31)
$c2 = [System.Drawing.Color]::FromArgb(255, 20, 43, 76)
$background = New-Object -TypeName System.Drawing.Drawing2D.LinearGradientBrush -ArgumentList @($rect, $c1, $c2, 42)
$graphics.FillRectangle($background, $rect)

$glowColor = [System.Drawing.Color]::FromArgb(42, 33, 121, 176)
$glowBrush = New-Object -TypeName System.Drawing.SolidBrush -ArgumentList @($glowColor)
$graphics.FillEllipse($glowBrush, 720, -260, 850, 900)
$glowBrush.Dispose()

$colors = @(
    [System.Drawing.Color]::FromArgb(235, 220, 60, 50),
    [System.Drawing.Color]::FromArgb(235, 235, 130, 40),
    [System.Drawing.Color]::FromArgb(235, 240, 210, 50),
    [System.Drawing.Color]::FromArgb(235, 60, 190, 80),
    [System.Drawing.Color]::FromArgb(235, 60, 180, 230),
    [System.Drawing.Color]::FromArgb(235, 80, 100, 220)
)

for ($i = 0; $i -lt 6; $i++) {
    $bottomX = 1160 + ($i * 72)
    $topX = 1575 + ($i * 72)
    $p1 = New-Object -TypeName System.Drawing.PointF -ArgumentList @($bottomX, 1080)
    $p2 = New-Object -TypeName System.Drawing.PointF -ArgumentList @(($bottomX + 58), 1080)
    $p3 = New-Object -TypeName System.Drawing.PointF -ArgumentList @(($topX + 58), 0)
    $p4 = New-Object -TypeName System.Drawing.PointF -ArgumentList @($topX, 0)
    $points = [System.Drawing.PointF[]]@($p1, $p2, $p3, $p4)
    $band = New-Object -TypeName System.Drawing.SolidBrush -ArgumentList @($colors[$i])
    $graphics.FillPolygon($band, $points)
    $band.Dispose()
}

$fontCollection = New-Object -TypeName System.Drawing.Text.PrivateFontCollection
$fontCollection.AddFontFile($fontPath)
$family = $fontCollection.Families[0]
$regular = [System.Drawing.FontStyle]::Regular
$pixel = [System.Drawing.GraphicsUnit]::Pixel

$titleFont = New-Object -TypeName System.Drawing.Font -ArgumentList @($family, 196, $regular, $pixel)
$subtitleFont = New-Object -TypeName System.Drawing.Font -ArgumentList @($family, 52, $regular, $pixel)
$badgeFont = New-Object -TypeName System.Drawing.Font -ArgumentList @($family, 88, $regular, $pixel)
$footerFont = New-Object -TypeName System.Drawing.Font -ArgumentList @($family, 34, $regular, $pixel)

$center = New-Object -TypeName System.Drawing.StringFormat
$center.Alignment = [System.Drawing.StringAlignment]::Center
$center.LineAlignment = [System.Drawing.StringAlignment]::Center

$titleRect = New-Object -TypeName System.Drawing.RectangleF -ArgumentList @(0, 260, 1920, 240)
$titleShadowColor = [System.Drawing.Color]::FromArgb(170, 0, 3, 12)
$titleShadowBrush = New-Object -TypeName System.Drawing.SolidBrush -ArgumentList @($titleShadowColor)
$titleShadowRect = New-Object -TypeName System.Drawing.RectangleF -ArgumentList @(5, 266, 1920, 240)
$graphics.DrawString("VICE4PS3", $titleFont, $titleShadowBrush, $titleShadowRect, $center)
$titleShadowBrush.Dispose()

$white = New-Object -TypeName System.Drawing.SolidBrush -ArgumentList @([System.Drawing.Color]::FromArgb(255, 250, 252, 255))
$graphics.DrawString("VICE4PS3", $titleFont, $white, $titleRect, $center)

$subtitleRect = New-Object -TypeName System.Drawing.RectangleF -ArgumentList @(0, 500, 1920, 90)
$subtitleColor = [System.Drawing.Color]::FromArgb(255, 112, 190, 230)
$subtitleBrush = New-Object -TypeName System.Drawing.SolidBrush -ArgumentList @($subtitleColor)
$graphics.DrawString("Commodore 64 HD Frontend", $subtitleFont, $subtitleBrush, $subtitleRect, $center)
$subtitleBrush.Dispose()

$badgeX = 760
$badgeY = 650
$badgeW = 400
$badgeH = 120
$radius = 54
$badgePath = New-Object -TypeName System.Drawing.Drawing2D.GraphicsPath
$badgePath.AddArc($badgeX, $badgeY, $radius, $radius, 180, 90)
$badgePath.AddArc(($badgeX + $badgeW - $radius), $badgeY, $radius, $radius, 270, 90)
$badgePath.AddArc(($badgeX + $badgeW - $radius), ($badgeY + $badgeH - $radius), $radius, $radius, 0, 90)
$badgePath.AddArc($badgeX, ($badgeY + $badgeH - $radius), $radius, $radius, 90, 90)
$badgePath.CloseFigure()

$badgeRectI = New-Object -TypeName System.Drawing.Rectangle -ArgumentList @($badgeX, $badgeY, $badgeW, $badgeH)
$badgeC1 = [System.Drawing.Color]::FromArgb(255, 28, 43, 74)
$badgeC2 = [System.Drawing.Color]::FromArgb(255, 52, 88, 120)
$badgeFill = New-Object -TypeName System.Drawing.Drawing2D.LinearGradientBrush -ArgumentList @($badgeRectI, $badgeC1, $badgeC2, 90)
$graphics.FillPath($badgeFill, $badgePath)
$badgeFill.Dispose()

$badgeBorderColor = [System.Drawing.Color]::FromArgb(255, 160, 225, 245)
$badgeBorder = New-Object -TypeName System.Drawing.Pen -ArgumentList @($badgeBorderColor, 5)
$graphics.DrawPath($badgeBorder, $badgePath)
$badgeBorder.Dispose()

$badgeRect = New-Object -TypeName System.Drawing.RectangleF -ArgumentList @($badgeX, $badgeY, $badgeW, $badgeH)
$graphics.DrawString("C64", $badgeFont, $white, $badgeRect, $center)

$footerRect = New-Object -TypeName System.Drawing.RectangleF -ArgumentList @(0, 900, 1920, 60)
$footerColor = [System.Drawing.Color]::FromArgb(200, 150, 175, 210)
$footerBrush = New-Object -TypeName System.Drawing.SolidBrush -ArgumentList @($footerColor)
$graphics.DrawString("PLAYSTATION 3  -  HD EDITION", $footerFont, $footerBrush, $footerRect, $center)
$footerBrush.Dispose()

$graphics.Dispose()
$bitmap.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()
$titleFont.Dispose()
$subtitleFont.Dispose()
$badgeFont.Dispose()
$footerFont.Dispose()
$fontCollection.Dispose()
$background.Dispose()
$white.Dispose()
$center.Dispose()
$badgePath.Dispose()

Write-Output "Wrote $outPath"
