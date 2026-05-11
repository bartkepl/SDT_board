using Ivi.Visa;

namespace SDT_App.Core;

public sealed class ScpiDevice : IDisposable
{
    private IMessageBasedSession? _session;
    private readonly object _lock = new();

    public bool IsConnected
    {
        get { lock (_lock) { return _session is not null; } }
    }

    public IEnumerable<string> ListResources()
    {
        // GlobalResourceManager.Find() is a static method — no resource manager object needed
        try { return GlobalResourceManager.Find("?*INSTR").ToList(); }
        catch { return GlobalResourceManager.Find().ToList(); }
    }

    public void Connect(string resourceName, int timeoutMs = 5000)
    {
        lock (_lock)
        {
            var session = GlobalResourceManager.Open(resourceName);
            _session = (IMessageBasedSession)session;
            // TimeoutMilliseconds is on IVisaSession (parent of IMessageBasedSession)
            ((IVisaSession)_session).TimeoutMilliseconds = timeoutMs;
            _session.SendEndEnabled = true;
        }
    }

    public void Disconnect()
    {
        lock (_lock)
        {
            try { _session?.Dispose(); } catch { }
            _session = null;
        }
    }

    public string Query(string cmd)
    {
        lock (_lock)
        {
            if (_session is null) throw new InvalidOperationException("Not connected");
            _session.FormattedIO.WriteLine(cmd);
            return _session.FormattedIO.ReadLine().Trim();
        }
    }

    public void Write(string cmd)
    {
        lock (_lock)
        {
            if (_session is null) throw new InvalidOperationException("Not connected");
            _session.FormattedIO.WriteLine(cmd);
        }
    }

    public void Dispose() => Disconnect();
}
