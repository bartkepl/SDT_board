using System.Windows;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SDT_App.Core;

namespace SDT_App.ViewModels;

public partial class SensorViewModel : ObservableObject
{
    private readonly ScpiDevice _device;
    private readonly AppState   _state;
    private readonly Action<string, LogLevel> _log;

    [ObservableProperty] private string _temperatureText = "—";
    [ObservableProperty] private string _humidityText    = "—";
    [ObservableProperty] private string _sensorTypeText  = "—";
    [ObservableProperty] private string _sensorIdText    = "—";
    [ObservableProperty] private string _readPeriodText  = "500";
    [ObservableProperty] private string _averageText     = "1";
    [ObservableProperty] private string _selectedPrecision = "HIGH";
    [ObservableProperty] private bool   _isConnected;

    public string[] PrecisionOptions { get; } = { "LOW", "MEDIUM", "HIGH" };

    public SensorViewModel(ScpiDevice device, AppState state, Action<string, LogLevel> log)
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
        catch (Exception ex)
        {
            _log($"ERROR [{cmd}]: {ex.Message}", LogLevel.Error);
            return null;
        }
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
    private async Task ReadTemperatureAsync()
    {
        var r = await QueryAsync(Scpi.SensorTemp);
        if (r is null) return;
        var v = Scpi.ParseFloat(r);
        TemperatureText = v.HasValue ? $"{v:F4} °C" : "N/A";
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task ReadHumidityAsync()
    {
        var r = await QueryAsync(Scpi.SensorHum);
        if (r is null) return;
        var v = Scpi.ParseFloat(r);
        HumidityText = v.HasValue ? $"{v:F2} %RH" : "N/A";
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task QuerySensorTypeAsync()
    {
        var r = await QueryAsync(Scpi.SensorType);
        if (r is not null) { SensorTypeText = r; _state.SensorType = r; }
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task QuerySensorIdAsync()
    {
        var r = await QueryAsync(Scpi.SensorId);
        if (r is not null) SensorIdText = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SetReadPeriodAsync()
    {
        if (!int.TryParse(ReadPeriodText, out var ms) || ms < 50 || ms > 60000)
        {
            MessageBox.Show("Read period must be 50–60000 ms.", "Validation",
                MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }
        await WriteAsync(Scpi.Set(Scpi.SensorReadPeriod, ms));
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetReadPeriodAsync()
    {
        var r = await QueryAsync(Scpi.SensorReadPeriodQ);
        if (r is not null) ReadPeriodText = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SetAverageAsync()
    {
        if (!int.TryParse(AverageText, out var n) || n < 1)
        {
            MessageBox.Show("Average must be a positive integer.", "Validation",
                MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }
        await WriteAsync(Scpi.Set(Scpi.SensorAverage, n));
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetAverageAsync()
    {
        var r = await QueryAsync(Scpi.SensorAverageQ);
        if (r is not null) AverageText = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SetPrecisionAsync() =>
        await WriteAsync(Scpi.Set(Scpi.SensorPrecision, SelectedPrecision));

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task GetPrecisionAsync()
    {
        var r = await QueryAsync(Scpi.SensorPrecisionQ);
        if (r is not null) SelectedPrecision = r;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task HeaterAsync() => await WriteAsync(Scpi.SensorHeater);

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SoftResetAsync() => await WriteAsync(Scpi.SensorSoftReset);

    partial void OnIsConnectedChanged(bool value) => NotifyAllCommands();

    private void NotifyAllCommands()
    {
        ReadTemperatureCommand.NotifyCanExecuteChanged();
        ReadHumidityCommand.NotifyCanExecuteChanged();
        QuerySensorTypeCommand.NotifyCanExecuteChanged();
        QuerySensorIdCommand.NotifyCanExecuteChanged();
        SetReadPeriodCommand.NotifyCanExecuteChanged();
        GetReadPeriodCommand.NotifyCanExecuteChanged();
        SetAverageCommand.NotifyCanExecuteChanged();
        GetAverageCommand.NotifyCanExecuteChanged();
        SetPrecisionCommand.NotifyCanExecuteChanged();
        GetPrecisionCommand.NotifyCanExecuteChanged();
        HeaterCommand.NotifyCanExecuteChanged();
        SoftResetCommand.NotifyCanExecuteChanged();
    }
}
