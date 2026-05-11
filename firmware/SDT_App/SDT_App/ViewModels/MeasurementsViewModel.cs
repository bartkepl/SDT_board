using System.Globalization;
using System.IO;
using System.Windows;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Win32;
using OxyPlot;
using OxyPlot.Axes;
using OxyPlot.Series;
using SDT_App.Core;

namespace SDT_App.ViewModels;

public partial class MeasurementsViewModel : ObservableObject
{
    private readonly ScpiDevice _device;
    private readonly AppState   _state;
    private readonly Action<string, LogLevel> _log;

    private CancellationTokenSource? _pollCts;
    private StreamWriter? _csvWriter;
    private readonly LineSeries _tempSeries;
    private readonly LineSeries _humSeries;

    [ObservableProperty] private bool   _isPolling;
    [ObservableProperty] private string _intervalText    = "2.0";
    [ObservableProperty] private string _liveTemperature = "—";
    [ObservableProperty] private string _liveHumidity    = "—";
    [ObservableProperty] private int    _sampleCount;
    [ObservableProperty] private bool   _isConnected;
    [ObservableProperty] private string _csvPath         = $"measurements_{DateTime.Now:yyyyMMdd}.csv";
    [ObservableProperty] private string _csvStatusText   = "Not logging";
    [ObservableProperty] private bool   _csvLogging;

    public PlotModel PlotModel { get; }

    public MeasurementsViewModel(ScpiDevice device, AppState state, Action<string, LogLevel> log)
    {
        _device = device;
        _state  = state;
        _log    = log;

        PlotModel = new PlotModel
        {
            Background = OxyColors.Transparent,
            PlotAreaBorderColor = OxyColor.FromRgb(100, 100, 100),
        };

        var timeAxis = new LinearAxis
        {
            Position = AxisPosition.Bottom,
            Title = "Time [s]",
            TitleColor = OxyColors.LightGray,
            TextColor = OxyColors.LightGray,
            TicklineColor = OxyColor.FromRgb(100, 100, 100),
        };
        var tempAxis = new LinearAxis
        {
            Position = AxisPosition.Left,
            Title = "Temperature [°C]",
            Key = "temp",
            TitleColor = OxyColor.FromRgb(0xE0, 0x66, 0x33),
            TextColor = OxyColor.FromRgb(0xE0, 0x88, 0x66),
            TicklineColor = OxyColor.FromRgb(100, 100, 100),
        };
        var humAxis = new LinearAxis
        {
            Position = AxisPosition.Right,
            Title = "Humidity [%RH]",
            Key = "hum",
            TitleColor = OxyColor.FromRgb(0x44, 0x88, 0xCC),
            TextColor = OxyColor.FromRgb(0x66, 0xAA, 0xDD),
            TicklineColor = OxyColor.FromRgb(100, 100, 100),
        };

        _tempSeries = new LineSeries
        {
            Title = "Temperature",
            Color = OxyColor.FromRgb(0xE0, 0x66, 0x33),
            MarkerType = MarkerType.Circle,
            MarkerSize = 3,
            YAxisKey = "temp",
        };
        _humSeries = new LineSeries
        {
            Title = "Humidity",
            Color = OxyColor.FromRgb(0x44, 0x88, 0xCC),
            MarkerType = MarkerType.Square,
            MarkerSize = 3,
            YAxisKey = "hum",
        };

        PlotModel.Axes.Add(timeAxis);
        PlotModel.Axes.Add(tempAxis);
        PlotModel.Axes.Add(humAxis);
        PlotModel.Series.Add(_tempSeries);
        PlotModel.Series.Add(_humSeries);
        PlotModel.IsLegendVisible = true;
    }

    [RelayCommand(CanExecute = nameof(CanStartPolling))]
    private async Task StartPollingAsync()
    {
        if (!double.TryParse(IntervalText, NumberStyles.Float, CultureInfo.InvariantCulture, out var interval)
            || interval < 0.1)
        {
            MessageBox.Show("Interval must be >= 0.1 seconds.", "Validation",
                MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        _state.ClearMeasurements();
        _tempSeries.Points.Clear();
        _humSeries.Points.Clear();
        SampleCount     = 0;
        LiveTemperature = "—";
        LiveHumidity    = "—";

        _pollCts = new CancellationTokenSource();
        IsPolling = true;
        StartPollingCommand.NotifyCanExecuteChanged();
        StopPollingCommand.NotifyCanExecuteChanged();

        _log($"Polling started (interval {interval:F1} s)", LogLevel.Ok);

        try { await PollLoopAsync(interval, _pollCts.Token); }
        catch (OperationCanceledException) { }
        catch (Exception ex) { _log($"Polling error: {ex.Message}", LogLevel.Error); }
        finally
        {
            IsPolling = false;
            StartPollingCommand.NotifyCanExecuteChanged();
            StopPollingCommand.NotifyCanExecuteChanged();
            _log("Polling stopped", LogLevel.Info);
        }
    }

    private bool CanStartPolling() => IsConnected && !IsPolling;

    [RelayCommand(CanExecute = nameof(IsPolling))]
    private void StopPolling() => _pollCts?.Cancel();

    // Called by MainViewModel on window close
    public void StopPollingNow() => _pollCts?.Cancel();

    [RelayCommand]
    private void ClearData()
    {
        _state.ClearMeasurements();
        _tempSeries.Points.Clear();
        _humSeries.Points.Clear();
        SampleCount = 0;
        LiveTemperature = "—";
        LiveHumidity = "—";
        PlotModel.InvalidatePlot(true);
    }

    [RelayCommand]
    private void BrowseCsv()
    {
        var dlg = new SaveFileDialog
        {
            Title = "Select CSV file",
            Filter = "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
            FileName = System.IO.Path.GetFileName(CsvPath),
        };
        if (dlg.ShowDialog() == true) CsvPath = dlg.FileName;
    }

    [RelayCommand(CanExecute = nameof(CanStartCsv))]
    private void StartCsv()
    {
        try
        {
            _csvWriter = new StreamWriter(CsvPath, append: false, System.Text.Encoding.UTF8)
            { AutoFlush = true };
            _csvWriter.WriteLine("timestamp_iso,elapsed_s,temperature_c,humidity_pct");
            CsvLogging = true;
            CsvStatusText = $"Logging → {System.IO.Path.GetFileName(CsvPath)}";
            _log($"CSV logging started: {CsvPath}", LogLevel.Ok);
            StartCsvCommand.NotifyCanExecuteChanged();
            StopCsvCmdCommand.NotifyCanExecuteChanged();
        }
        catch (Exception ex)
        {
            _log($"CSV open error: {ex.Message}", LogLevel.Error);
            MessageBox.Show($"Cannot open CSV file:\n{ex.Message}", "CSV Error",
                MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private bool CanStartCsv() => !CsvLogging;

    public void StopCsv()
    {
        try { _csvWriter?.Dispose(); } catch { }
        _csvWriter = null;
        CsvLogging = false;
        CsvStatusText = "Not logging";
        StartCsvCommand.NotifyCanExecuteChanged();
        StopCsvCmdCommand.NotifyCanExecuteChanged();
        _log("CSV logging stopped", LogLevel.Info);
    }

    [RelayCommand(CanExecute = nameof(CsvLogging))]
    private void StopCsvCmd() => StopCsv();

    private async Task PollLoopAsync(double intervalSecs, CancellationToken ct)
    {
        _state.StartTime = Environment.TickCount64 / 1000.0;

        while (!ct.IsCancellationRequested)
        {
            var loopStart = DateTime.UtcNow;
            double? temp = null, hum = null;

            try
            {
                var tResp = await Task.Run(() => _device.Query(Scpi.SensorTemp), ct);
                temp = Scpi.ParseFloat(tResp);

                var sType = _state.SensorType?.ToUpperInvariant();
                if (sType is not "TMP117")
                {
                    var hResp = await Task.Run(() => _device.Query(Scpi.SensorHum), ct);
                    hum = Scpi.ParseFloat(hResp);
                }
            }
            catch (OperationCanceledException) { break; }
            catch (Exception ex)
            {
                _log($"Poll error: {ex.Message}", LogLevel.Error);
            }

            var elapsed = Environment.TickCount64 / 1000.0 - _state.StartTime!.Value;
            var now = DateTime.Now;

            Application.Current.Dispatcher.Invoke(() =>
            {
                if (temp.HasValue)
                {
                    _state.Timestamps.Add(elapsed);
                    _state.TempValues.Add(temp.Value);
                    _tempSeries.Points.Add(new DataPoint(elapsed, temp.Value));
                    LiveTemperature = $"{temp.Value:F4} °C";
                    SampleCount = _state.TempValues.Count;
                }
                if (hum.HasValue)
                {
                    _state.HumValues.Add(hum.Value);
                    _humSeries.Points.Add(new DataPoint(elapsed, hum.Value));
                    LiveHumidity = $"{hum.Value:F2} %RH";
                }
                if (temp.HasValue || hum.HasValue)
                    PlotModel.InvalidatePlot(true);
            });

            WriteCsvRow(now, elapsed, temp, hum);

            var remaining = intervalSecs - (DateTime.UtcNow - loopStart).TotalSeconds;
            if (remaining > 0.01)
            {
                try { await Task.Delay(TimeSpan.FromSeconds(remaining), ct); }
                catch (OperationCanceledException) { break; }
            }
        }
    }

    private void WriteCsvRow(DateTime ts, double elapsed, double? temp, double? hum)
    {
        if (_csvWriter is null) return;
        try
        {
            var tempStr = temp.HasValue ? temp.Value.ToString("F4", CultureInfo.InvariantCulture) : "";
            var humStr  = hum.HasValue  ? hum.Value.ToString("F2",  CultureInfo.InvariantCulture) : "";
            _csvWriter.WriteLine(
                $"{ts:O},{elapsed.ToString("F3", CultureInfo.InvariantCulture)},{tempStr},{humStr}");
        }
        catch (Exception ex) { _log($"CSV write error: {ex.Message}", LogLevel.Warn); }
    }

    partial void OnIsConnectedChanged(bool value)
    {
        StartPollingCommand.NotifyCanExecuteChanged();
    }

    partial void OnIsPollingChanged(bool value)
    {
        StopPollingCommand.NotifyCanExecuteChanged();
        StartPollingCommand.NotifyCanExecuteChanged();
    }

    partial void OnCsvLoggingChanged(bool value)
    {
        StartCsvCommand.NotifyCanExecuteChanged();
        StopCsvCmdCommand.NotifyCanExecuteChanged();
    }
}
