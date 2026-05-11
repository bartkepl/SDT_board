using System.Collections.Specialized;
using System.Windows.Controls;
using SDT_App.ViewModels;

namespace SDT_App.Views;

public partial class DfuView : UserControl
{
    public DfuView()
    {
        InitializeComponent();
        Loaded += (_, _) =>
        {
            if (DataContext is DfuViewModel vm)
                vm.FlashLogLines.CollectionChanged += FlashLogChanged;
        };
    }

    private void FlashLogChanged(object? sender, NotifyCollectionChangedEventArgs e)
    {
        if (e.Action == NotifyCollectionChangedAction.Add && FlashLogList.Items.Count > 0)
            FlashLogList.ScrollIntoView(FlashLogList.Items[FlashLogList.Items.Count - 1]);
    }
}
