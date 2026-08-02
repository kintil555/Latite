# inject.ps1 - Latite DLL Injector for Minecraft GDK
param(
    [Parameter(Mandatory=$true)]
    [string]$DllPath
)

Add-Type -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError=true)]
public static extern IntPtr OpenProcess(int dwAccess, bool bInherit, int pid);

[DllImport("kernel32.dll", SetLastError=true)]
public static extern IntPtr VirtualAllocEx(IntPtr hProc, IntPtr addr, uint size, uint allocType, uint protect);

[DllImport("kernel32.dll", SetLastError=true)]
public static extern bool WriteProcessMemory(IntPtr hProc, IntPtr addr, byte[] buf, uint size, out uint written);

[DllImport("kernel32.dll", CharSet=CharSet.Ansi)]
public static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

[DllImport("kernel32.dll", CharSet=CharSet.Unicode)]
public static extern IntPtr GetModuleHandleW(string moduleName);

[DllImport("kernel32.dll", SetLastError=true)]
public static extern IntPtr CreateRemoteThread(IntPtr hProc, IntPtr attr, uint stackSize, IntPtr startAddr, IntPtr param, uint flags, out uint threadId);

[DllImport("kernel32.dll")]
public static extern uint WaitForSingleObject(IntPtr handle, uint ms);

[DllImport("kernel32.dll")]
public static extern bool CloseHandle(IntPtr handle);
'@ -Name 'Injector' -Namespace 'Win32' -PassThru | Out-Null

if (-not (Test-Path $DllPath)) {
    Write-Host " [ERROR] DLL tidak ditemukan: $DllPath" -ForegroundColor Red
    exit 1
}
$DllPath = (Resolve-Path $DllPath).Path
Write-Host " [INFO] DLL  : $DllPath" -ForegroundColor Cyan

$procs = @(Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue)
if ($procs.Count -eq 0) {
    Write-Host ' [ERROR] Minecraft.Windows.exe tidak ditemukan.' -ForegroundColor Red
    exit 1
}
$proc = $procs[0]
Write-Host " [INFO] PID  : $($proc.Id)" -ForegroundColor Cyan

$hProc = [Win32.Injector]::OpenProcess(0x1F0FFF, $false, $proc.Id)
if ($hProc -eq [IntPtr]::Zero) {
    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Write-Host " [ERROR] OpenProcess gagal (err: $err) - jalankan sebagai Administrator" -ForegroundColor Red
    exit 1
}

$bytes = [System.Text.Encoding]::Unicode.GetBytes($DllPath + [char]0)
$mem = [Win32.Injector]::VirtualAllocEx($hProc, [IntPtr]::Zero, [uint32]$bytes.Length, 0x3000, 0x40)
if ($mem -eq [IntPtr]::Zero) {
    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Write-Host " [ERROR] VirtualAllocEx gagal (err: $err)" -ForegroundColor Red
    [Win32.Injector]::CloseHandle($hProc) | Out-Null
    exit 1
}

$written = [uint32]0
[Win32.Injector]::WriteProcessMemory($hProc, $mem, $bytes, [uint32]$bytes.Length, [ref]$written) | Out-Null

$hKernel = [Win32.Injector]::GetModuleHandleW('kernel32.dll')
$loadLib  = [Win32.Injector]::GetProcAddress($hKernel, 'LoadLibraryW')
$tid      = [uint32]0
$hThread  = [Win32.Injector]::CreateRemoteThread($hProc, [IntPtr]::Zero, 0, $loadLib, $mem, 0, [ref]$tid)
if ($hThread -eq [IntPtr]::Zero) {
    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    Write-Host " [ERROR] CreateRemoteThread gagal (err: $err)" -ForegroundColor Red
    [Win32.Injector]::CloseHandle($hProc) | Out-Null
    exit 1
}

[Win32.Injector]::WaitForSingleObject($hThread, 8000) | Out-Null
[Win32.Injector]::CloseHandle($hThread) | Out-Null
[Win32.Injector]::CloseHandle($hProc)  | Out-Null

Write-Host ' [OK] Injeksi berhasil!' -ForegroundColor Green
