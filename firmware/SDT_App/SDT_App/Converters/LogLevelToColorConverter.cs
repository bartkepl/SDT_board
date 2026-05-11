using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;
using SDT_App.Core;

namespace SDT_App.Converters;

public class LogLevelToColorConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        => (LogLevel)value switch
        {
            LogLevel.Ok    => new SolidColorBrush(Color.FromRgb(0x44, 0xBB, 0x66)),
            LogLevel.Error => new SolidColorBrush(Color.FromRgb(0xEE, 0x44, 0x44)),
            LogLevel.Warn  => new SolidColorBrush(Color.FromRgb(0xDD, 0x99, 0x00)),
            _              => new SolidColorBrush(Color.FromRgb(0xCC, 0xCC, 0xCC)),
        };

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        => throw new NotSupportedException();
}
