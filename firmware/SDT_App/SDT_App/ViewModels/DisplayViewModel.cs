using System.Windows;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SDT_App.Core;

namespace SDT_App.ViewModels;

public partial class DisplayViewModel : ObservableObject
{
    private readonly ScpiDevice _device;
    private readonly AppState   _state;
    private readonly Action<string, LogLevel> _log;

    [ObservableProperty] private int    _brightness     = 5;
    [ObservableProperty] private bool   _displayOn      = true;
    [ObservableProperty] private bool   _sourceMeasurement = true;
    [ObservableProperty] private string _displayText    = "        ";
    [ObservableProperty] private string _currentText    = "—";
    [ObservableProperty] private bool   _isConnected;

    public string BrightnessLabel => $"{Brightness}%";

    public DisplayViewModel(ScpiDevice device, AppState state, Action<string, LogLevel> log)
    {
        _device = device;
        _state  = state;
        _log    = log;
    }

    partial void OnBrightnessChanged(int value) => OnPropertyChanged(nameof(BrightnessLabel));

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
    private async Task SetBrightnessAsync() =>
        await WriteAsync(Scpi.Set(Scpi.DisplayBrightness, Brightness));

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetBrightnessAsync()
    {
        var r = await QueryAsync(Scpi.DisplayBrightnessQ);
        var v = Scpi.ParseInt(r);
        if (v.HasValue) Brightness = Math.Clamp(v.Value, 1, 100);
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SetStateAsync() =>
        await WriteAsync(Scpi.Set(Scpi.DisplayState, DisplayOn ? 1 : 0));

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetStateAsync()
    {
        var r = await QueryAsync(Scpi.DisplayStateQ);
        if (r is "1" or "ON") DisplayOn = true;
        else if (r is "0" or "OFF") DisplayOn = false;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SetSourceAsync() =>
        await WriteAsync(Scpi.Set(Scpi.DisplaySource, SourceMeasurement ? 0 : 1));

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetSourceAsync()
    {
        var r = await QueryAsync(Scpi.DisplaySourceQ);
        if (r is "0") SourceMeasurement = true;
        else if (r is "1") SourceMeasurement = false;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task WriteTextAsync()
    {
        var text = DisplayText.Length > 8 ? DisplayText[..8] : DisplayText;
        await WriteAsync($"{Scpi.DisplayText} \"{text}\"");
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task ReadTextAsync()
    {
        var r = await QueryAsync(Scpi.DisplayTextQ);
        if (r is not null)
        {
            CurrentText  = r;
            DisplayText  = r.Trim('"');
        }
    }

    partial void OnIsConnectedChanged(bool value)
    {
        SetBrightnessCommand.NotifyCanExecuteChanged();
        GetBrightnessCommand.NotifyCanExecuteChanged();
        SetStateCommand.NotifyCanExecuteChanged();
        GetStateCommand.NotifyCanExecuteChanged();
        SetSourceCommand.NotifyCanExecuteChanged();
        GetSourceCommand.NotifyCanExecuteChanged();
        WriteTextCommand.NotifyCanExecuteChanged();
        ReadTextCommand.NotifyCanExecuteChanged();
    }
}
