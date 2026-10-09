# Builds the entry screen's layered logo from the original artwork (assets/teamforge_logo.png):
# the TF monogram is cropped at full resolution and split into two transparent layers, so the UI
# can give them depth (parallax) on the dark background:
#   assets/teamforge_mark_light.png  the T and the lower F bar, recoloured light
#   assets/teamforge_mark_red.png    the red F bar
#   assets/teamforge_mark_depth.png  the whole monogram as a dark silhouette (stacked behind the
#                                    other two to give the logo thickness)
# The artwork's own alpha is kept, so the anti-aliased edges stay smooth. Deterministic; re-run after changing the artwork:
#     powershell -ExecutionPolicy Bypass -File tools/make_logo_layers.ps1

$root = Split-Path -Parent $PSScriptRoot
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class LogoLayers
{
    // Crops `area` from `source` and writes the red ink and the dark ink as separate layers.
    public static void Split(string source, Rectangle area, string lightPath, Color light, string redPath, Color red,
                             string depthPath, Color depth)
    {
        using (var original = new Bitmap(source))
        using (var cropped = original.Clone(area, PixelFormat.Format32bppArgb))
        using (var lightLayer = new Bitmap(area.Width, area.Height, PixelFormat.Format32bppArgb))
        using (var redLayer = new Bitmap(area.Width, area.Height, PixelFormat.Format32bppArgb))
        using (var depthLayer = new Bitmap(area.Width, area.Height, PixelFormat.Format32bppArgb))
        {
            var rect = new Rectangle(0, 0, area.Width, area.Height);
            var src = cropped.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var lt = lightLayer.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            var rd = redLayer.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            var dp = depthLayer.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            int bytes = src.Stride * area.Height;
            byte[] input = new byte[bytes], outLight = new byte[bytes], outRed = new byte[bytes], outDepth = new byte[bytes];
            Marshal.Copy(src.Scan0, input, 0, bytes);
            for (int i = 0; i < bytes; i += 4)
            {
                int g = input[i + 1], r = input[i + 2], alpha = input[i + 3];
                // The artwork has a transparent background: keep its coverage (alpha), and sort
                // each pixel by ink: red keeps a high red channel, the dark ink is grey.
                bool isRed = r - g > 60;
                byte[] target = isRed ? outRed : outLight;
                Color tone = isRed ? red : light;
                target[i] = tone.B; target[i + 1] = tone.G; target[i + 2] = tone.R; target[i + 3] = (byte)alpha;
                outDepth[i] = depth.B; outDepth[i + 1] = depth.G; outDepth[i + 2] = depth.R; outDepth[i + 3] = (byte)alpha;
            }
            Marshal.Copy(outLight, 0, lt.Scan0, bytes);
            Marshal.Copy(outRed, 0, rd.Scan0, bytes);
            Marshal.Copy(outDepth, 0, dp.Scan0, bytes);
            cropped.UnlockBits(src);
            lightLayer.UnlockBits(lt);
            redLayer.UnlockBits(rd);
            depthLayer.UnlockBits(dp);
            lightLayer.Save(lightPath, ImageFormat.Png);
            redLayer.Save(redPath, ImageFormat.Png);
            depthLayer.Save(depthPath, ImageFormat.Png);
        }
    }
}
'@

[LogoLayers]::Split((Join-Path $root 'assets/teamforge_logo.png'),
    (New-Object System.Drawing.Rectangle 612, 104, 844, 454),
    (Join-Path $root 'assets/teamforge_mark_light.png'), [System.Drawing.Color]::FromArgb(236, 238, 242),
    (Join-Path $root 'assets/teamforge_mark_red.png'), [System.Drawing.Color]::FromArgb(242, 58, 65),
    (Join-Path $root 'assets/teamforge_mark_depth.png'), [System.Drawing.Color]::FromArgb(58, 64, 78))
