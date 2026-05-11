namespace SDT_App.Core;

public enum LogLevel { Info, Ok, Warn, Error }

public record LogEntry(DateTime Timestamp, string Message, LogLevel Level)
{
    public string FormattedTime => Timestamp.ToString("HH:mm:ss");
}
