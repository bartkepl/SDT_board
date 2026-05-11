using System.Globalization;

namespace SDT_App.Core;

internal static class Scpi
{
    // IEEE 488.2
    public const string Idn        = "*IDN?";
    public const string Rst        = "*RST";
    public const string Cls        = "*CLS";
    public const string Tst        = "*TST?";
    public const string EseSet     = "*ESE";
    public const string EseQuery   = "*ESE?";
    public const string SreSet     = "*SRE";
    public const string SreQuery   = "*SRE?";
    public const string EsrQuery   = "*ESR?";
    public const string StbQuery   = "*STB?";
    public const string OpcQuery   = "*OPC?";
    public const string OpcSet     = "*OPC";
    public const string Wai        = "*WAI";

    // SYSTem
    public const string SysVersion         = "SYSTem:VERSion?";
    public const string SysErrorQuery      = "SYSTem:ERRor?";
    public const string SysErrorCount      = "SYSTem:ERRor:COUNt?";
    public const string SysIdShort         = "SYSTem:ID? SHORT";
    public const string SysIdLong          = "SYSTem:ID? LONG";
    public const string SysBootloaderEnter = "SYSTem:BOOTloader:ENter";
    public const string SysRst             = "SYSTem:RST";

    // SENSor
    public const string SensorTemp         = "SENSor:TEMPerature?";
    public const string SensorHum          = "SENSor:HUMidity?";
    public const string SensorType         = "SENSor:TYPE?";
    public const string SensorId           = "SENSor:ID?";
    public const string SensorReadPeriod   = "SENSor:READperiod";
    public const string SensorReadPeriodQ  = "SENSor:READperiod?";
    public const string SensorAverage      = "SENSor:AVErage";
    public const string SensorAverageQ     = "SENSor:AVErage?";
    public const string SensorPrecision    = "SENSor:PRECision";
    public const string SensorPrecisionQ   = "SENSor:PRECision?";
    public const string SensorHeater       = "SENSor:HEATer";
    public const string SensorSoftReset    = "SENSor:SOFTReset";
    public const string SensorAlertHigh    = "SENSor:ALERt:HIGH";
    public const string SensorAlertHighQ   = "SENSor:ALERt:HIGH?";
    public const string SensorAlertLow     = "SENSor:ALERt:LOW";
    public const string SensorAlertLowQ    = "SENSor:ALERt:LOW?";
    public const string SensorAlertStatus  = "SENSor:ALERt:STATus?";
    public const string SensorMode         = "SENSor:MODe";
    public const string SensorModeQ        = "SENSor:MODe?";
    public const string SensorConvRate     = "SENSor:CONVrate";
    public const string SensorConvRateQ    = "SENSor:CONVrate?";

    // DISPlay
    public const string DisplayBrightness  = "DISPlay:BRIGhtness";
    public const string DisplayBrightnessQ = "DISPlay:BRIGhtness?";
    public const string DisplayState       = "DISPlay:STATe";
    public const string DisplayStateQ      = "DISPlay:STATe?";
    public const string DisplaySource      = "DISPlay:SOURce";
    public const string DisplaySourceQ     = "DISPlay:SOURce?";
    public const string DisplayText        = "DISPlay:TEXT";
    public const string DisplayTextQ       = "DISPlay:TEXT?";

    // The threshold above which a float response is treated as NaN/invalid
    public const double NanThreshold = 9e36;

    public static string Set(string cmd, object value) =>
        $"{cmd} {(value is double d ? d.ToString(CultureInfo.InvariantCulture) : value)}";

    public static double? ParseFloat(string? text)
    {
        if (text is null) return null;
        if (double.TryParse(text.Trim(), NumberStyles.Float,
                            CultureInfo.InvariantCulture, out var d))
        {
            if (double.IsNaN(d) || Math.Abs(d) > NanThreshold) return null;
            return d;
        }
        return null;
    }

    public static int? ParseInt(string? text) =>
        text is not null && int.TryParse(text.Trim(), out var i) ? i : null;
}
