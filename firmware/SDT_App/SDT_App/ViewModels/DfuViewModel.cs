using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Windows;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Win32;
using SDT_App.Core;

namespace SDT_App.ViewModels;

public partial class DfuViewModel : ObservableObject
{
    private readonly ScpiDevice _device;
    private readonly AppState   _state;
    private readonly Action<string, LogLevel> _log;

    private string? _detectedTool;
    private string? _detectedToolPath;

    [ObservableProperty] private string _firmwarePath    = "";
    [ObservableProperty] private string _toolStatusText  = "Click 'Detect' to find DFU tool";
    [ObservableProperty] private bool   _toolFound;
    [ObservableProperty] private bool   _isFlashing;
    [ObservableProperty] private string _flashStepText   = "Idle";
    [ObservableProperty] private bool   _isConnected;

    public ObservableCollection<string> FlashLogLines { get; } = new();

    private static readonly (string Name, string Exe)[] DfuCandidates =
    {
        ("dfu-util",             "dfu-util"),
        ("STM32_Programmer_CLI", "STM32_Programmer_CLI"),
    };

    private static readonly string[] Stm32ProgrammerPaths =
    {
        @"STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        @"STMicroelectronics\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
    };

    public DfuViewModel(ScpiDevice device, AppState state, Action<string, LogLevel> log)
    {
        _device = device;
        _state  = state;
        _log    = log;
    }

    [RelayCommand]
    private void BrowseFile()
    {
        var dlg = new OpenFileDialog
        {
            Title = "Select Firmware File",
            Filter = "Firmware files (*.bin;*.hex;*.elf;*.dfu)|*.bin;*.hex;*.elf;*.dfu|All files (*.*)|*.*",
        };
        if (dlg.ShowDialog() == true) FirmwarePath = dlg.FileName;
    }

    [RelayCommand]
    private async Task DetectToolAsync()
    {
        _detectedTool     = null;
        _detectedToolPath = null;
        ToolFound = false;
        ToolStatusText = "Detecting…";

        foreach (var (name, exe) in DfuCandidates)
        {
            var path = await Task.Run(() => FindExecutable(exe));
            if (path is null) continue;

            bool ok = await Task.Run(() =>
            {
                try
                {
                    var psi = new ProcessStartInfo(path, "--version")
                    {
                        RedirectStandardOutput = true,
                        RedirectStandardError  = true,
                        UseShellExecute = false,
                        CreateNoWindow = true,
                    };
                    using var proc = Process.Start(psi)!;
                    var stdout = proc.StandardOutput.ReadToEnd();
                    var stderr = proc.StandardError.ReadToEnd();
                    proc.WaitForExit();
                    var combined = stdout + stderr;
                    return proc.ExitCode == 0 || combined.Contains("version", StringComparison.OrdinalIgnoreCase)
                                              || combined.Contains("dfu", StringComparison.OrdinalIgnoreCase);
                }
                catch { return true; } // found the exe, even if --version fails
            });

            if (!ok) continue;

            _detectedTool     = name;
            _detectedToolPath = path;
            ToolFound         = true;
            ToolStatusText    = $"Found: {name}  ({path})";
            _log($"DFU tool detected: {name} at {path}", LogLevel.Ok);
            FlashCommand.NotifyCanExecuteChanged();
            return;
        }

        ToolStatusText = "No DFU tool found in PATH. Install dfu-util or STM32CubeProgrammer.";
        _log("DFU tool not found", LogLevel.Warn);
        FlashCommand.NotifyCanExecuteChanged();
    }

    private static string? FindExecutable(string name)
    {
        // Check PATH
        var fromPath = FindInPath(name);
        if (fromPath is not null) return fromPath;

        // Check known STM32 install locations
        foreach (var pf in new[] { Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
                                    Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86) })
        {
            foreach (var suffix in Stm32ProgrammerPaths)
            {
                var full = Path.Combine(pf, suffix);
                if (File.Exists(full)) return full;
            }
        }
        return null;
    }

    private static string? FindInPath(string name)
    {
        var pathExt = Environment.GetEnvironmentVariable("PATHEXT") ?? ".exe;.cmd;.bat";
        var dirs = (Environment.GetEnvironmentVariable("PATH") ?? "").Split(';');
        foreach (var dir in dirs)
        {
            foreach (var ext in pathExt.Split(';'))
            {
                var full = Path.Combine(dir.Trim(), name + ext);
                if (File.Exists(full)) return full;
            }
            // Also try exact name (for Linux-style exes on PATH)
            var exact = Path.Combine(dir.Trim(), name);
            if (File.Exists(exact)) return exact;
        }
        return null;
    }

    [RelayCommand(CanExecute = nameof(CanFlash))]
    private async Task FlashAsync()
    {
        var result = MessageBox.Show(
            $"Flash firmware:\n{FirmwarePath}\n\nUsing tool: {_detectedTool}\n\n" +
            "The device will enter bootloader mode and be flashed.\nContinue?",
            "Confirm Flash", MessageBoxButton.YesNo, MessageBoxImage.Question);
        if (result != MessageBoxResult.Yes) return;

        IsFlashing = true;
        FlashLogLines.Clear();
        FlashCommand.NotifyCanExecuteChanged();

        try
        {
            // Step 1: Enter bootloader
            if (_device.IsConnected)
            {
                FlashStepText = "Sending SYSTem:BOOTloader:ENter…";
                AppendLog("[INFO] Sending bootloader entry command");
                try
                {
                    await Task.Run(() => _device.Write(Scpi.SysBootloaderEnter));
                    await Task.Delay(200);
                }
                catch (Exception ex) { AppendLog($"[WARN] {ex.Message}"); }
                _device.Disconnect();
                _state.IsConnected = false;
                AppendLog("[INFO] Disconnected from VISA");
            }

            // Step 2: Wait for DFU enumeration
            FlashStepText = "Waiting 3 s for USB DFU enumeration…";
            AppendLog("[INFO] Waiting 3 s for device to enumerate as USB DFU…");
            await Task.Delay(3000);

            // Step 3: Build command
            var args = BuildFlashArgs(_detectedTool!, _detectedToolPath!, FirmwarePath);
            AppendLog($"[CMD] {_detectedToolPath} {args}");
            FlashStepText = "Flashing…";

            // Step 4: Run process
            var psi = new ProcessStartInfo(_detectedToolPath!, args)
            {
                RedirectStandardOutput = true,
                RedirectStandardError  = true,
                UseShellExecute = false,
                CreateNoWindow = true,
            };

            using var proc = new Process { StartInfo = psi, EnableRaisingEvents = true };
            proc.OutputDataReceived += (_, e) =>
            {
                if (e.Data is not null)
                    Application.Current.Dispatcher.Invoke(() => AppendLog(e.Data));
            };
            proc.ErrorDataReceived += (_, e) =>
            {
                if (e.Data is not null)
                    Application.Current.Dispatcher.Invoke(() => AppendLog($"[ERR] {e.Data}"));
            };

            proc.Start();
            proc.BeginOutputReadLine();
            proc.BeginErrorReadLine();
            await Task.Run(() => proc.WaitForExit());

            if (proc.ExitCode == 0)
            {
                FlashStepText = "Done — Flash successful!";
                AppendLog("[OK] Flash completed successfully.");
                _log("DFU flash successful. Reconnect device.", LogLevel.Ok);
            }
            else
            {
                FlashStepText = $"Failed (exit code {proc.ExitCode})";
                AppendLog($"[ERROR] Tool exited with code {proc.ExitCode}");
                _log($"DFU flash failed (exit code {proc.ExitCode})", LogLevel.Error);
            }
        }
        catch (Exception ex)
        {
            FlashStepText = $"Error: {ex.Message}";
            AppendLog($"[ERROR] {ex.Message}");
            _log($"Flash error: {ex.Message}", LogLevel.Error);
        }
        finally
        {
            IsFlashing = false;
            FlashCommand.NotifyCanExecuteChanged();
        }
    }

    private bool CanFlash() => !IsFlashing && !string.IsNullOrWhiteSpace(FirmwarePath)
                                           && File.Exists(FirmwarePath) && ToolFound;

    private static string BuildFlashArgs(string toolName, string toolPath, string file)
    {
        return toolName switch
        {
            "dfu-util" =>
                $"--alt 0 --dfuse-address 0x08000000 --download \"{file}\"",
            "STM32_Programmer_CLI" =>
                $"-c port=USB1 -d \"{file}\" -s 0x08000000",
            _ => throw new InvalidOperationException($"Unknown tool: {toolName}"),
        };
    }

    [RelayCommand]
    private void ClearFlashLog() => FlashLogLines.Clear();

    private void AppendLog(string line)
    {
        FlashLogLines.Add(line);
        while (FlashLogLines.Count > 1000) FlashLogLines.RemoveAt(0);
    }

    partial void OnIsFlashingChanged(bool value) => FlashCommand.NotifyCanExecuteChanged();
    partial void OnFirmwarePathChanged(string value) => FlashCommand.NotifyCanExecuteChanged();
    partial void OnToolFoundChanged(bool value) => FlashCommand.NotifyCanExecuteChanged();
}
