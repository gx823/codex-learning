#requires -Version 7.0
param([Parameter(Mandatory=$true)][int]$GameProcessId,[Parameter(Mandatory=$true)][string]$OutputFile,[switch]$Click,[int]$ClientX=0,[int]$ClientY=0)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'hc_r3_window_guard.ps1')
. (Join-Path $PSScriptRoot 'hc_r4_stop_guard.ps1')
$process=Get-Process -Id $GameProcessId
if($process.ProcessName -notin @('UnrealEditor','HarborCity-Win64-Shipping')){throw 'Unexpected target executable'}
if(Test-HCR4DesktopStopped){throw 'ESC STOP (latched)'}
if(-not (Test-HCR3Window -GameProcessId $GameProcessId -EvidencePath ($OutputFile+'.guard.jsonl'))){throw 'BLOCKED_WINDOW'}
Add-Type -AssemblyName System.Drawing
if(-not ('HCR4Input' -as [type])){Add-Type @'
using System;using System.Runtime.InteropServices;
public static class HCR4Input {
 [StructLayout(LayoutKind.Sequential)] public struct POINT {public int x,y;}
 [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT {public int dx,dy;public uint data,flags,time;public UIntPtr extra;}
 [StructLayout(LayoutKind.Sequential)] public struct INPUT {public uint type;public MOUSEINPUT mouse;}
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr w,ref POINT p);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] static extern uint SendInput(uint n,INPUT[] input,int size);
 public static bool Click(){var a=new INPUT[2];a[0].mouse.flags=2;a[1].mouse.flags=4;return SendInput(2,a,Marshal.SizeOf(typeof(INPUT)))==2;}
}
'@}
$windows=@([HCR3WindowGuard]::Windows($GameProcessId)|Where-Object {[HCR3WindowGuard]::IsWindowVisible($_)})
$targetHandle=[IntPtr]::Zero;$width=0;$height=0
foreach($w in $windows){$r=New-Object HCR3WindowGuard+RECT;[void][HCR3WindowGuard]::GetClientRect($w,[ref]$r);if(($r.R-$r.L)*($r.B-$r.T) -gt $width*$height){$targetHandle=$w;$width=$r.R-$r.L;$height=$r.B-$r.T}}
if($width -lt 64 -or $height -lt 64){throw 'BLOCKED_WINDOW'}
[void][HCR4Input]::SetForegroundWindow($targetHandle);Start-Sleep -Milliseconds 300
if([HCR4Input]::GetForegroundWindow() -ne $targetHandle){throw 'BLOCKED_FOCUS'}
$point=New-Object HCR4Input+POINT;[void][HCR4Input]::ClientToScreen($targetHandle,[ref]$point)
if($Click){
 if($ClientX -lt 0 -or $ClientY -lt 0 -or $ClientX -ge $width -or $ClientY -ge $height){throw 'Coordinates outside actual client'}
 if(Test-HCR4DesktopStopped){throw 'ESC STOP (latched)'}
 [void][HCR4Input]::SetCursorPos($point.x+$ClientX,$point.y+$ClientY)
 if(-not [HCR4Input]::Click()){throw 'SendInput refused'}
 Start-Sleep -Milliseconds 300
}
if(-not (Test-HCR3Window -GameProcessId $GameProcessId -EvidencePath ($OutputFile+'.guard.jsonl'))){throw 'BLOCKED_WINDOW'}
$bmp=New-Object Drawing.Bitmap($width,$height);$graphics=[Drawing.Graphics]::FromImage($bmp)
try{$graphics.CopyFromScreen($point.x,$point.y,0,0,$bmp.Size);$bmp.Save($OutputFile,[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bmp.Dispose()}
[ordered]@{level='C-OS injection, not human click';pid=$GameProcessId;client=@($width,$height);click=[bool]$Click;xy=@($ClientX,$ClientY);screenshot=$OutputFile;time=(Get-Date -Format o)}|ConvertTo-Json|Set-Content -LiteralPath ($OutputFile+'.json') -Encoding utf8
