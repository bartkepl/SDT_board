using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;

namespace SDT_App.Converters;

/// <summary>
/// ConverterParameter = "#trueColor|#falseColor" (hex, e.g. "#44BB66|#EE4444")
/// </summary>
public class BoolToColorConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        var flag = value is bool b && b;
        var colors = (parameter as string)?.Split('|') ?? Array.Empty<string>();
        var hex = flag ? (colors.ElementAtOrDefault(0) ?? "#44BB66")
                       : (colors.ElementAtOrDefault(1) ?? "#888888");
        return ParseHex(hex);
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        => throw new NotSupportedException();

    private static SolidColorBrush ParseHex(string hex)
    {
        hex = hex.TrimStart('#');
        if (hex.Length == 6)
        {
            var r = System.Convert.ToByte(hex[..2], 16);
            var g = System.Convert.ToByte(hex[2..4], 16);
            var b = System.Convert.ToByte(hex[4..6], 16);
            return new SolidColorBrush(Color.FromRgb(r, g, b));
        }
        return new SolidColorBrush(Colors.Gray);
    }
}
