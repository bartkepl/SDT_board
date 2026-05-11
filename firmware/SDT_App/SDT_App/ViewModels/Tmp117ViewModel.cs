using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SDT_App.Core;

namespace SDT_App.ViewModels;

public partial class Tmp117ViewModel : ObservableObject
{
    private readonly ScpiDevice _device;
    private readonly AppState   _state;
    private readonly Action<string, LogLevel> _log;

    [ObservableProperty] private string _alertHighInput   = "85.0";
    [ObservableProperty] private string _alertHighCurrent = "—";
    [ObservableProperty] private string _alertLowInput    = "0.0";
    [ObservableProperty] private string _alertLowCurrent  = "—";
    [ObservableProperty] private string _alertStatusText  = "—";
    [ObservableProperty] private string _selectedMode     = "CONTINUOUS";
    [ObservableProperty] private int    _convRate         = 4;
    [ObservableProperty] private string _convRateLabel    = "→ 1 s";
    [ObservableProperty] private bool   _isConnected;
    [ObservableProperty] private bool   _isTmp117Available;

    public string[] ModeOptions { get; } = { "CONTINUOUS", "SHUTDOWN", "ONESHOT" };

    private static readonly Dictionary<int, string> ConvRateLabels = new()
    {
        { 0, "15.5 ms" }, { 1, "125 ms" }, { 2, "250 ms" }, { 3, "500 ms" },
        { 4, "1 s" },     { 5, "4 s" },    { 6, "8 s" },    { 7, "16 s" },
    };

    public Tmp117ViewModel(ScpiDevice device, AppState state, Action<string, LogLevel> log)
    {
        _device = device;
        _state  = state;
        _log    = log;
    }

    public void UpdateSensorAvailability(string? sensorType) =>
        IsTmp117Available = sensorType is "TMP117" or "DUAL" or null;

    partial void OnConvRateChanged(int value) =>
        ConvRateLabel = ConvRateLabels.TryGetValue(value, out var lbl) ? $"→ {lbl}" : "→ ?";

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
    private async Task SetAlertHighAsync()
    {
        if (!double.TryParse(AlertHighInput, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var v))
        { _log("Invalid alert high value", LogLevel.Warn); return; }
        await WriteAsync(Scpi.Set(Scpi.SensorAlertHigh, v));
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetAlertHighAsync()
    {
        var r = await QueryAsync(Scpi.SensorAlertHighQ);
        if (r is not null) AlertHighCurrent = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SetAlertLowAsync()
    {
        if (!double.TryParse(AlertLowInput, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var v))
        { _log("Invalid alert low value", LogLevel.Warn); return; }
        await WriteAsync(Scpi.Set(Scpi.SensorAlertLow, v));
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetAlertLowAsync()
    {
        var r = await QueryAsync(Scpi.SensorAlertLowQ);
        if (r is not null) AlertLowCurrent = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetAlertStatusAsync()
    {
        var r = await QueryAsync(Scpi.SensorAlertStatus);
        if (r is not null) AlertStatusText = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SetModeAsync() =>
        await WriteAsync(Scpi.Set(Scpi.SensorMode, SelectedMode));

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetModeAsync()
    {
        var r = await QueryAsync(Scpi.SensorModeQ);
        if (r is not null && ModeOptions.Contains(r)) SelectedMode = r;
        else if (r is not null) _log($"Unknown mode: {r}", LogLevel.Warn);
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SetConvRateAsync() =>
        await WriteAsync(Scpi.Set(Scpi.SensorConvRate, ConvRate));

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetConvRateAsync()
    {
        var r = await QueryAsync(Scpi.SensorConvRateQ);
        var v = Scpi.ParseInt(r);
        if (v.HasValue && v >= 0 && v <= 7) ConvRate = v.Value;
    }

    partial void OnIsConnectedChanged(bool value)
    {
        SetAlertHighCommand.NotifyCanExecuteChanged();
        GetAlertHighCommand.NotifyCanExecuteChanged();
        SetAlertLowCommand.NotifyCanExecuteChanged();
        GetAlertLowCommand.NotifyCanExecuteChanged();
        GetAlertStatusCommand.NotifyCanExecuteChanged();
        SetModeCommand.NotifyCanExecuteChanged();
        GetModeCommand.NotifyCanExecuteChanged();
        SetConvRateCommand.NotifyCanExecuteChanged();
        GetConvRateCommand.NotifyCanExecuteChanged();
    }
}
