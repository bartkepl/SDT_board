using System.Windows;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SDT_App.Core;

namespace SDT_App.ViewModels;

public partial class SystemViewModel : ObservableObject
{
    private readonly ScpiDevice _device;
    private readonly AppState   _state;
    private readonly Action<string, LogLevel> _log;

    public event Action? DeviceDisconnected;

    [ObservableProperty] private string _eseInput       = "0";
    [ObservableProperty] private string _esrText        = "—";
    [ObservableProperty] private string _sreInput       = "0";
    [ObservableProperty] private string _stbText        = "—";
    [ObservableProperty] private string _selfTestResult = "—";
    [ObservableProperty] private string _scpiVersion    = "—";
    [ObservableProperty] private string _systemIdText   = "—";
    [ObservableProperty] private bool   _idTypeLong;
    [ObservableProperty] private string _errorQueueText = "";
    [ObservableProperty] private bool   _isConnected;

    public SystemViewModel(ScpiDevice device, AppState state, Action<string, LogLevel> log)
    {
        _device = device;
        _state  = state;
        _log    = log;
    }

    private async Task<string?> QueryAsync(string cmd)
    {
        try
        {
            var resp = await Task.Run(() => _device.Query(cmd));
            _state.LastCommand = cmd;
            _log($"{cmd} → {resp}", LogLevel.Ok);
            return resp;
        }
        catch (Exception ex) { _log($"ERROR [{cmd}]: {ex.Message}", LogLevel.Error); return null; }
    }

    private async Task WriteAsync(string cmd)
    {
        try
        {
            await Task.Run(() => _device.Write(cmd));
            _state.LastCommand = cmd;
            _log($"Write: {cmd}", LogLevel.Ok);
        }
        catch (Exception ex) { _log($"ERROR [{cmd}]: {ex.Message}", LogLevel.Error); }
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SetEseAsync()
    {
        if (int.TryParse(EseInput, out var v))
            await WriteAsync(Scpi.Set(Scpi.EseSet, v));
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetEseAsync()
    {
        var r = await QueryAsync(Scpi.EseQuery);
        if (r is not null) EseInput = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetEsrAsync()
    {
        var r = await QueryAsync(Scpi.EsrQuery);
        if (r is not null) EsrText = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SetSreAsync()
    {
        if (int.TryParse(SreInput, out var v))
            await WriteAsync(Scpi.Set(Scpi.SreSet, v));
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetSreAsync()
    {
        var r = await QueryAsync(Scpi.SreQuery);
        if (r is not null) SreInput = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetStbAsync()
    {
        var r = await QueryAsync(Scpi.StbQuery);
        if (r is not null) StbText = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task RunSelfTestAsync()
    {
        var r = await QueryAsync(Scpi.Tst);
        if (r is not null)
            SelfTestResult = r == "0" ? "PASS (0)" : $"FAIL ({r})";
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetVersionAsync()
    {
        var r = await QueryAsync(Scpi.SysVersion);
        if (r is not null) ScpiVersion = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetSystemIdAsync()
    {
        var cmd = IdTypeLong ? Scpi.SysIdLong : Scpi.SysIdShort;
        var r = await QueryAsync(cmd);
        if (r is not null) SystemIdText = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task ReadErrorQueueAsync()
    {
        ErrorQueueText = "";
        var sb = new System.Text.StringBuilder();
        for (var i = 0; i < 20; i++)
        {
            var r = await QueryAsync(Scpi.SysErrorQuery);
            if (r is null) break;
            sb.AppendLine(r);
            var parts = r.Split(',');
            if (parts.Length > 0 && parts[0].Trim() == "0") break;
        }
        ErrorQueueText = sb.ToString().TrimEnd();
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetErrorCountAsync()
    {
        var r = await QueryAsync(Scpi.SysErrorCount);
        if (r is not null) _log($"Error count: {r}", LogLevel.Info);
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SendClsAsync() => await WriteAsync(Scpi.Cls);

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SendRstAsync() => await WriteAsync(Scpi.Rst);

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SendOpcAsync() => await WriteAsync(Scpi.OpcSet);

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task QueryOpcAsync()
    {
        var r = await QueryAsync(Scpi.OpcQuery);
        if (r is not null) _log($"OPC = {r}", LogLevel.Info);
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SendWaiAsync() => await WriteAsync(Scpi.Wai);

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task EnterBootloaderAsync()
    {
        var result = MessageBox.Show(
            "This will send SYSTem:BOOTloader:ENter and disconnect.\nThe device will restart in DFU mode.\n\nContinue?",
            "Enter Bootloader", MessageBoxButton.YesNo, MessageBoxImage.Question);
        if (result != MessageBoxResult.Yes) return;

        await WriteAsync(Scpi.SysBootloaderEnter);
        await Task.Delay(200);
        _device.Disconnect();
        _state.IsConnected = false;
        _log("Entered bootloader — device disconnected", LogLevel.Warn);
        DeviceDisconnected?.Invoke();
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task RestartDeviceAsync()
    {
        var result = MessageBox.Show(
            "Send SYSTem:RST to restart the device?",
            "Restart Device", MessageBoxButton.YesNo, MessageBoxImage.Question);
        if (result != MessageBoxResult.Yes) return;
        await WriteAsync(Scpi.SysRst);
        _log("Restart command sent", LogLevel.Ok);
    }

    partial void OnIsConnectedChanged(bool value)
    {
        SetEseCommand.NotifyCanExecuteChanged();
        GetEseCommand.NotifyCanExecuteChanged();
        GetEsrCommand.NotifyCanExecuteChanged();
        SetSreCommand.NotifyCanExecuteChanged();
        GetSreCommand.NotifyCanExecuteChanged();
        GetStbCommand.NotifyCanExecuteChanged();
        RunSelfTestCommand.NotifyCanExecuteChanged();
        GetVersionCommand.NotifyCanExecuteChanged();
        GetSystemIdCommand.NotifyCanExecuteChanged();
        ReadErrorQueueCommand.NotifyCanExecuteChanged();
        GetErrorCountCommand.NotifyCanExecuteChanged();
        SendClsCommand.NotifyCanExecuteChanged();
        SendRstCommand.NotifyCanExecuteChanged();
        SendOpcCommand.NotifyCanExecuteChanged();
        QueryOpcCommand.NotifyCanExecuteChanged();
        SendWaiCommand.NotifyCanExecuteChanged();
        EnterBootloaderCommand.NotifyCanExecuteChanged();
        RestartDeviceCommand.NotifyCanExecuteChanged();
    }
}
