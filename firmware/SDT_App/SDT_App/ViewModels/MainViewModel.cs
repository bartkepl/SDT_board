using System.Collections.ObjectModel;
using System.Windows;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SDT_App.Core;

namespace SDT_App.ViewModels;

public partial class MainViewModel : ObservableObject
{
    private readonly ScpiDevice _device;
    private readonly AppState   _state;

    public ConnectionViewModel   ConnectionVm   { get; }
    public SensorViewModel       SensorVm       { get; }
    public Tmp117ViewModel       Tmp117Vm       { get; }
    public DisplayViewModel      DisplayVm      { get; }
    public SystemViewModel       SystemVm       { get; }
    public MeasurementsViewModel MeasurementsVm { get; }
    public DfuViewModel          DfuVm          { get; }
    public ConsoleViewModel      ConsoleVm      { get; }

    public ObservableCollection<LogEntry> LogEntries { get; } = new();

    [ObservableProperty] private string _statusText        = "Disconnected";
    [ObservableProperty] private string _sensorTypeDisplay = "—";
    [ObservableProperty] private string _lastCommandDisplay = "—";
    [ObservableProperty] private bool   _isConnected;

    public MainViewModel()
    {
        _device = new ScpiDevice();
        _state  = new AppState();

        _state.PropertyChanged += (_, e) =>
        {
            if (e.PropertyName is nameof(AppState.IsConnected))
            {
                IsConnected  = _state.IsConnected;
                StatusText   = _state.IsConnected
                    ? $"Connected: {_state.ConnectedResource}"
                    : "Disconnected";
            }
            if (e.PropertyName is nameof(AppState.SensorType))
                SensorTypeDisplay = _state.SensorType ?? "—";
            if (e.PropertyName is nameof(AppState.LastCommand))
                LastCommandDisplay = _state.LastCommand ?? "—";
        };

        ConnectionVm   = new ConnectionViewModel(_device, _state, AddLog);
        SensorVm       = new SensorViewModel(_device, _state, AddLog);
        Tmp117Vm       = new Tmp117ViewModel(_device, _state, AddLog);
        DisplayVm      = new DisplayViewModel(_device, _state, AddLog);
        SystemVm       = new SystemViewModel(_device, _state, AddLog);
        MeasurementsVm = new MeasurementsViewModel(_device, _state, AddLog);
        DfuVm          = new DfuViewModel(_device, _state, AddLog);
        ConsoleVm      = new ConsoleViewModel(_device, _state, AddLog);

        // Propagate connect/disconnect to child VMs
        ConnectionVm.Connected    += OnConnected;
        ConnectionVm.Disconnected += OnDisconnected;
        SystemVm.DeviceDisconnected += OnDisconnected;
    }

    private void OnConnected(string resource, string? sensorType)
    {
        void UpdateAll(bool v)
        {
            SensorVm.IsConnected       = v;
            Tmp117Vm.IsConnected       = v;
            DisplayVm.IsConnected      = v;
            SystemVm.IsConnected       = v;
            MeasurementsVm.IsConnected = v;
            DfuVm.IsConnected          = v;
            ConsoleVm.IsConnected      = v;
        }
        UpdateAll(true);
        Tmp117Vm.UpdateSensorAvailability(sensorType);
    }

    private void OnDisconnected()
    {
        SensorVm.IsConnected       = false;
        Tmp117Vm.IsConnected       = false;
        DisplayVm.IsConnected      = false;
        SystemVm.IsConnected       = false;
        MeasurementsVm.IsConnected = false;
        DfuVm.IsConnected          = false;
        ConsoleVm.IsConnected      = false;
    }

    [RelayCommand]
    private void ClearLog() => LogEntries.Clear();

    public void AddLog(string message, LogLevel level)
    {
        Application.Current.Dispatcher.Invoke(() =>
        {
            LogEntries.Add(new LogEntry(DateTime.Now, message, level));
            while (LogEntries.Count > 2000)
                LogEntries.RemoveAt(0);
        });
    }

    public void OnWindowClosing()
    {
        MeasurementsVm.StopPollingNow();
        MeasurementsVm.StopCsv();
        _device.Disconnect();
    }
}
