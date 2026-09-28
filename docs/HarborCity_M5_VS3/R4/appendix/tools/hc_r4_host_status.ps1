param([string]$OutputFile='')
$ErrorActionPreference='Stop'
Add-Type @'
using System;using System.Runtime.InteropServices;
public class HCR4HostStatus {
 [StructLayout(LayoutKind.Sequential)] public struct Power {public byte AC,Flag,Percent,Reserved;public uint Life,Full;}
 [DllImport("kernel32.dll")] public static extern bool GetSystemPowerStatus(out Power p);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
}
'@
$power=[HCR4HostStatus+Power]::new()
[void][HCR4HostStatus]::GetSystemPowerStatus([ref]$power)
$foregroundProcessId=[uint32]0
[void][HCR4HostStatus]::GetWindowThreadProcessId([HCR4HostStatus]::GetForegroundWindow(),[ref]$foregroundProcessId)
$games=@(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe' OR Name='HarborCity-Win64-Shipping.exe'" | Select-Object ProcessId,Name,ExecutablePath)
$foregroundProcessName=(Get-Process -Id $foregroundProcessId -ErrorAction SilentlyContinue).ProcessName
$data=[ordered]@{time=(Get-Date -Format o);ac_line=$power.AC;battery_percent=$power.Percent;foreground_pid=$foregroundProcessId;foreground_process=$foregroundProcessName;games=$games;disks=@(Get-PSDrive C,D,E|Select-Object Name,@{n='free_GiB';e={[math]::Round($_.Free/1GB,2)}})}
$json=$data|ConvertTo-Json -Depth 5
if($OutputFile){$json|Set-Content -LiteralPath $OutputFile -Encoding utf8}
$json
