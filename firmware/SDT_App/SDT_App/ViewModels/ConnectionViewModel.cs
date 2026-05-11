using System.Collections.ObjectModel;
using System.Windows;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SDT_App.Core;

namespace SDT_App.ViewModels;

public partial class ConnectionViewModel : ObservableObject
{
    private readonly ScpiDevice _device;
    private readonly AppState   _state;
    private readonly Action<string, LogLevel> _log;

    public event Action<string, string?>? Connected;
    public event Action?                  Disconnected;

    public ObservableCollection<string> Resources { get; } = new();

    [ObservableProperty] private string? _selectedResource;
    [ObservableProperty] private string  _statusText  = "Not connected";
    [ObservableProperty] private string  _idnText     = "—";
    [ObservableProperty] private string  _versionText = "—";
    [ObservableProperty] private bool    _isConnected;

    public ConnectionViewModel(ScpiDevice device, AppState state, Action<string, LogLevel> log)
    {
        _device = device;
        _state  = state;
        _log    = log;
    }

    [RelayCommand]
    private async Task RefreshAsync()
    {
        Resources.Clear();
        try
        {
            var list = await Task.Run(() => _device.ListResources());
            foreach (var r in list) Resources.Add(r);
            if (Resources.Count > 0) SelectedResource = Resources[0];
            _log($"Found {Resources.Count} resource(s)", LogLevel.Info);
        }
        catch (Exception ex)
        {
            _log($"Refresh error: {ex.Message}", LogLevel.Error);
            MessageBox.Show(
                $"Could not list VISA resources.\n\n{ex.Message}\n\nMake sure NI-VISA or another VISA runtime is installed.",
                "VISA Error", MessageBoxButton.OK, MessageBoxImage.Warning);
        }
    }

    [RelayCommand(CanExecute = nameof(CanConnect))]
    private async Task ConnectAsync()
    {
        if (SelectedResource is null) return;
        try
        {
            string? sensorType = null;
            await Task.Run(() =>
            {
                _device.Connect(SelectedResource);
                sensorType = _device.Query(Scpi.SensorType);
            });

            _state.IsConnected       = true;
            _state.ConnectedResource = SelectedResource;
            _state.SensorType        = sensorType;
            IsConnected  = true;
            StatusText   = $"Connected: {SelectedResource}";
            IdnText      = "—";
            VersionText  = "—";
            _log($"Connected to {SelectedResource} (sensor: {sensorType ?? "unknown"})", LogLevel.Ok);
            Connected?.Invoke(SelectedResource, sensorType);
        }
        catch (Exception ex)
        {
            _log($"Connect failed: {ex.Message}", LogLevel.Error);
            MessageBox.Show($"Connection failed:\n{ex.Message}", "Connect Error",
                MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private bool CanConnect() => !IsConnected && SelectedResource is not null;

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private void Disconnect()
    {
        _device.Disconnect();
        _state.IsConnected       = false;
        _state.ConnectedResource = null;
        IsConnected  = false;
        StatusText   = "Disconnected";
        _log("Disconnected", LogLevel.Info);
        Disconnected?.Invoke();
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task QueryIdnAsync()
    {
        try
        {
            var resp = await Task.Run(() => _device.Query(Scpi.Idn));
            IdnText = resp;
            _state.LastCommand = Scpi.Idn;
            _log($"{Scpi.Idn} → {resp}", LogLevel.Ok);
        }
        catch (Exception ex) { _log($"IDN error: {ex.Message}", LogLevel.Error); }
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task QueryVersionAsync()
    {
        try
        {
            var resp = await Task.Run(() => _device.Query(Scpi.SysVersion));
            VersionText = resp;
            _state.LastCommand = Scpi.SysVersion;
            _log($"{Scpi.SysVersion} → {resp}", LogLevel.Ok);
        }
        catch (Exception ex) { _log($"Version error: {ex.Message}", LogLevel.Error); }
    }

    partial void OnIsConnectedChanged(bool value)
    {
        ConnectCommand.NotifyCanExecuteChanged();
        DisconnectCommand.NotifyCanExecuteChanged();
        QueryIdnCommand.NotifyCanExecuteChanged();
        QueryVersionCommand.NotifyCanExecuteChanged();
    }

    partial void OnSelectedResourceChanged(string? value) =>
        ConnectCommand.NotifyCanExecuteChanged();
}
