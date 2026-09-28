# Only inspect/restore windows belonging to the explicitly supplied UE process.
if(-not ('HCR3WindowGuard' -as [type])) {
Add-Type @'
using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
public static class HCR3WindowGuard {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 public delegate bool EnumProc(IntPtr h,IntPtr p);
 [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc c,IntPtr p);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder b,int n);
 public static string WindowClass(IntPtr h){var b=new StringBuilder(128);GetClassName(h,b,128);return b.ToString();}
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int z,uint f);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int c);
 [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h,int c);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int k);
 public static IntPtr[] Windows(int pid) {
  var a=new List<IntPtr>(); EnumWindows((h,p)=>{uint id;GetWindowThreadProcessId(h,out id);if(id==pid)a.Add(h);return true;},IntPtr.Zero);return a.ToArray();
 }
}
'@
}
function Test-HCR3Window {
 param([int]$GameProcessId,[string]$EvidencePath,[switch]$KeepForeground)
 $observed=@()
 foreach($handle in [HCR3WindowGuard]::Windows($GameProcessId)) {
  if(-not [HCR3WindowGuard]::IsWindow($handle)){continue}
  if([HCR3WindowGuard]::WindowClass($handle) -ne 'UnrealWindow'){continue}
  $rect=New-Object HCR3WindowGuard+RECT
  if(-not [HCR3WindowGuard]::GetClientRect($handle,[ref]$rect)){continue}
  $width=$rect.R-$rect.L;$height=$rect.B-$rect.T
  if($KeepForeground){[void][HCR3WindowGuard]::SetWindowPos($handle,[IntPtr](-1),0,0,0,0,0x53)}
  # Ignore utility/message windows; a minimized main window may keep its size.
  if(-not [HCR3WindowGuard]::IsWindowVisible($handle) -and $width -eq 0 -and $height -eq 0){continue}
  if([HCR3WindowGuard]::IsIconic($handle) -or $width -lt 64 -or $height -lt 64 -or -not [HCR3WindowGuard]::IsWindowVisible($handle)) {
   [void][HCR3WindowGuard]::ShowWindowAsync($handle,9)
   [void][HCR3WindowGuard]::SetForegroundWindow($handle)
   Start-Sleep -Seconds 1
   # Startup can replace a splash/initial viewport while the one-second restore wait runs.
   # A destroyed handle is not a minimized live game window. Inspect its successor next poll.
   if(-not [HCR3WindowGuard]::IsWindow($handle)){continue}
   [void][HCR3WindowGuard]::GetClientRect($handle,[ref]$rect)
   $ok= -not [HCR3WindowGuard]::IsIconic($handle) -and [HCR3WindowGuard]::IsWindowVisible($handle) -and ($rect.R-$rect.L) -ge 64 -and ($rect.B-$rect.T) -ge 64
   $row=[ordered]@{time=(Get-Date).ToString('o');pid=$GameProcessId;handle=$handle.ToInt64();before=@($width,$height);after=@(($rect.R-$rect.L),($rect.B-$rect.T));status=if($ok){'RESTORED_WAITED_1S'}else{'BLOCKED_WINDOW'}}
   if($EvidencePath){$row|ConvertTo-Json -Compress|Add-Content -LiteralPath $EvidencePath -Encoding utf8}
   $observed+=$row
   if(-not $ok){return $false}
  }
 }
 return $true
}
