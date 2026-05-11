using System.Collections.Specialized;
using System.Windows;
using System.Windows.Controls;
using MahApps.Metro.Controls;
using SDT_App.ViewModels;

namespace SDT_App;

public partial class MainWindow : MetroWindow
{
    public MainWindow()
    {
        InitializeComponent();

        Loaded += (_, _) =>
        {
            if (DataContext is MainViewModel vm)
                vm.LogEntries.CollectionChanged += LogEntriesChanged;
        };
    }

    private void LogEntriesChanged(object? sender, NotifyCollectionChangedEventArgs e)
    {
        if (e.Action == NotifyCollectionChangedAction.Add)
        {
            var count = LogList.Items.Count;
            if (count > 0)
                LogList.ScrollIntoView(LogList.Items[count - 1]);
        }
    }

    private void OnWindowClosing(object sender, System.ComponentModel.CancelEventArgs e)
    {
        (DataContext as MainViewModel)?.OnWindowClosing();
    }
}
