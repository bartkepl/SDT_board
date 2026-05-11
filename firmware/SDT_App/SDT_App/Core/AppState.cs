using CommunityToolkit.Mvvm.ComponentModel;

namespace SDT_App.Core;

public partial class AppState : ObservableObject
{
    [ObservableProperty] private bool   _isConnected;
    [ObservableProperty] private string? _connectedResource;
    [ObservableProperty] private string? _sensorType;
    [ObservableProperty] private string? _lastCommand;

    // Measurement series — mutate only on UI thread
    public List<double> Timestamps { get; } = new();
    public List<double> TempValues { get; } = new();
    public List<double> HumValues  { get; } = new();
    public double?      StartTime  { get; set; }

    public void ClearMeasurements()
    {
        Timestamps.Clear();
        TempValues.Clear();
        HumValues.Clear();
        StartTime = null;
    }
}
