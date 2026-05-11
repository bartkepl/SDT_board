using System.Windows;
using SDT_App.ViewModels;

namespace SDT_App;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        var vm   = new MainViewModel();
        var win  = new MainWindow { DataContext = vm };
        MainWindow = win;
        win.Show();
    }
}
