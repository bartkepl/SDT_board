using System.Collections.Specialized;
using System.Windows.Controls;
using System.Windows.Input;
using SDT_App.ViewModels;

namespace SDT_App.Views;

public partial class ConsoleView : UserControl
{
    public ConsoleView()
    {
        InitializeComponent();
        Loaded += (_, _) =>
        {
            if (DataContext is ConsoleViewModel vm)
                vm.OutputLines.CollectionChanged += OutputLinesChanged;
        };
    }

    private void OnCommandKeyDown(object sender, KeyEventArgs e)
    {
        if (DataContext is not ConsoleViewModel vm) return;
        if (e.Key == Key.Up)   { vm.HistoryUp();   e.Handled = true; }
        if (e.Key == Key.Down) { vm.HistoryDown(); e.Handled = true; }
    }

    private void OutputLinesChanged(object? sender, NotifyCollectionChangedEventArgs e)
    {
        if (e.Action == NotifyCollectionChangedAction.Add && OutputList.Items.Count > 0)
            OutputList.ScrollIntoView(OutputList.Items[OutputList.Items.Count - 1]);
    }
}
