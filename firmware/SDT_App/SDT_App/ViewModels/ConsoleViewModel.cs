using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SDT_App.Core;

namespace SDT_App.ViewModels;

public enum ConsoleEntryType { Sent, Received, Error, Info }

public record ConsoleEntry(string Text, ConsoleEntryType Type);

public partial class ConsoleViewModel : ObservableObject
{
    private readonly ScpiDevice _device;
    private readonly AppState   _state;
    private readonly Action<string, LogLevel> _log;

    private readonly List<string> _history = new();
    private int _historyIndex = -1;

    [ObservableProperty] private string _commandText = "";
    [ObservableProperty] private bool   _isConnected;

    public ObservableCollection<ConsoleEntry> OutputLines { get; } = new();

    public ConsoleViewModel(ScpiDevice device, AppState state, Action<string, LogLevel> log)
    {
        _device = device;
        _state  = state;
        _log    = log;
    }

    [RelayCommand(CanExecute = nameof(IsConnected))]
    private async Task SendAsync()
    {
        var cmd = CommandText.Trim();
        if (string.IsNullOrEmpty(cmd)) return;

        _history.Add(cmd);
        _historyIndex = -1;
        CommandText = "";

        AddOutput($">>> {cmd}", ConsoleEntryType.Sent);
        bool isQuery = cmd.EndsWith('?');

        try
        {
            if (isQuery)
            {
                var resp = await Task.Run(() => _device.Query(cmd));
                _state.LastCommand = cmd;
                AddOutput($"    {resp}", ConsoleEntryType.Received);
                _log($"{cmd} → {resp}", LogLevel.Ok);
            }
            else
            {
                await Task.Run(() => _device.Write(cmd));
                _state.LastCommand = cmd;
                AddOutput("    [write OK]", ConsoleEntryType.Received);
                _log($"Write: {cmd}", LogLevel.Ok);
            }
        }
        catch (Exception ex)
        {
            AddOutput($"    ERROR: {ex.Message}", ConsoleEntryType.Error);
            _log($"Console error [{cmd}]: {ex.Message}", LogLevel.Error);
        }
    }

    [RelayCommand]
    private void ClearOutput() => OutputLines.Clear();

    public void HistoryUp()
    {
        if (_history.Count == 0) return;
        if (_historyIndex < _history.Count - 1) _historyIndex++;
        CommandText = _history[_history.Count - 1 - _historyIndex];
    }

    public void HistoryDown()
    {
        if (_historyIndex <= 0) { _historyIndex = -1; CommandText = ""; return; }
        _historyIndex--;
        CommandText = _history[_history.Count - 1 - _historyIndex];
    }

    private void AddOutput(string text, ConsoleEntryType type)
    {
        OutputLines.Add(new ConsoleEntry(text, type));
        while (OutputLines.Count > 500) OutputLines.RemoveAt(0);
    }

    partial void OnIsConnectedChanged(bool value) =>
        SendCommand.NotifyCanExecuteChanged();
}
