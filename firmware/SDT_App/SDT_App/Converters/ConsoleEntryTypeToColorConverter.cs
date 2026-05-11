using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;
using SDT_App.ViewModels;

namespace SDT_App.Converters;

public class ConsoleEntryTypeToColorConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        => (ConsoleEntryType)value switch
        {
            ConsoleEntryType.Sent     => new SolidColorBrush(Color.FromRgb(0x66, 0xAA, 0xFF)),
            ConsoleEntryType.Received => new SolidColorBrush(Color.FromRgb(0x44, 0xBB, 0x66)),
            ConsoleEntryType.Error    => new SolidColorBrush(Color.FromRgb(0xEE, 0x44, 0x44)),
            _                         => new SolidColorBrush(Color.FromRgb(0xAA, 0xAA, 0xAA)),
        };

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        => throw new NotSupportedException();
}
