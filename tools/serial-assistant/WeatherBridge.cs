using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

public sealed class WeatherBridge : Form {
    readonly SerialPort port = new SerialPort("COM3",115200,Parity.None,8,StopBits.One);
    readonly HttpClient http = new HttpClient();
    readonly ComboBox ports = new ComboBox();
    readonly Button connect = new Button(), refresh = new Button();
    readonly CheckBox cloud = new CheckBox();
    readonly Label connection = new Label(), summary = new Label(), hint = new Label();
    readonly Label rainfall = new Label();
    readonly RichTextBox output = new RichTextBox();
    readonly Timer timer = new Timer();
    readonly string logPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"weather_bridge.log");
    Task<string> request;
    DateTime nextFetch = DateTime.MinValue, nextSend = DateTime.MinValue, nextConnect = DateTime.MinValue;
    bool autoConnect=true;
    string pending="";
    long observed;
    int temp10,humidity,wind10,code;
    bool weatherValid;
    int[] rainDays10;
    long rainStart,rainFetched;
    string rainCaption;
    const string Url="https://api.open-meteo.com/v1/forecast?latitude=28.2282&longitude=112.9388&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&daily=precipitation_sum&precipitation_unit=mm&wind_speed_unit=ms&timeformat=unixtime&timezone=Asia%2FShanghai&past_days=1&forecast_days=2";

    [STAThread] public static void Main(string[] args) {
        ServicePointManager.SecurityProtocol=SecurityProtocolType.Tls12;
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new WeatherBridge(Array.IndexOf(args,"--cloud-ready")>=0));
    }
    WeatherBridge(bool ready) {
        Text="长沙天气转发 + 中文串口助手";
        Size=new Size(1000,720); MinimumSize=new Size(820,540);
        StartPosition=FormStartPosition.CenterScreen;
        Font=new Font("Microsoft YaHei UI",10);
        ports.SetBounds(16,16,110,30); ports.DropDownStyle=ComboBoxStyle.DropDownList;
        ports.Items.AddRange(SerialPort.GetPortNames());
        if(ports.Items.Contains("COM3")) ports.SelectedItem="COM3";
        else if(ports.Items.Count>0) ports.SelectedIndex=0;
        connect.Text="连接串口";connect.SetBounds(140,13,110,34);
        refresh.Text="刷新天气";refresh.SetBounds(264,13,110,34);
        cloud.Text="天气物模型已配置，启用天气云端上报";
        cloud.SetBounds(395,17,460,27);cloud.Checked=ready;
        connection.Text="等待串口连接：115200 / 8N1";connection.SetBounds(16,57,950,28);
        summary.Text="正在获取长沙天气……";summary.SetBounds(16,91,950,54);
        rainfall.Text="降水统计：正在获取昨天、今天、明天的预报量……";
        rainfall.SetBounds(16,149,950,64);rainfall.ForeColor=Color.FromArgb(0,95,125);
        hint.Text="K0循环切换三页，K-UP设置位移与倾角基准；天气每10分钟更新，每30秒转发。";
        hint.SetBounds(16,218,950,28);
        foreach(Label label in new[]{connection,summary,rainfall,hint})label.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;
        output.SetBounds(16,253,950,406);output.Anchor=AnchorStyles.Top|AnchorStyles.Bottom|AnchorStyles.Left|AnchorStyles.Right;
        output.ReadOnly=true;output.MaxLength=2000000;output.Font=new Font("Microsoft YaHei UI",10);
        output.BackColor=Color.FromArgb(20,26,36);output.ForeColor=Color.FromArgb(170,240,210);
        Controls.AddRange(new Control[]{ports,connect,refresh,cloud,connection,summary,rainfall,hint,output});
        port.Handshake=Handshake.None;port.DtrEnable=false;port.RtsEnable=false;port.WriteTimeout=1000;
        http.Timeout=TimeSpan.FromSeconds(25);
        connect.Click+=delegate {if(port.IsOpen){autoConnect=false;ClosePort();}else{autoConnect=true;OpenPort();}};
        refresh.Click+=delegate {nextFetch=DateTime.MinValue;};
        cloud.CheckedChanged+=delegate {nextSend=DateTime.MinValue;};
        timer.Interval=100;timer.Tick+=OnTick;
        Shown+=delegate {timer.Start();};
        FormClosing+=delegate {timer.Stop();if(port.IsOpen)port.Close();port.Dispose();http.Dispose();};
    }
    void Append(string text) {
        // Keep control bytes (including BEL) out of the Windows rich edit control.
        var visible=new StringBuilder(text.Length);
        foreach(char c in text)
            if(c=='\r'||c=='\n'||c=='\t'||!char.IsControl(c))visible.Append(c);
        // SelectedText attempts to edit a protected selection in a read-only
        // RichTextBox and can trigger Windows' warning sound on each trim.
        // Replace Text programmatically instead; retain only recent history.
        const int historyLimit=100000, retainedHistory=50000;
        string incoming=visible.ToString();
        if(incoming.Length>retainedHistory)
            incoming=incoming.Substring(incoming.Length-retainedHistory);
        if(output.TextLength+incoming.Length>historyLimit){
            string history=output.Text;
            int start=Math.Max(0,history.Length-retainedHistory);
            int newline=history.IndexOf('\n',start);
            if(newline>=0)start=newline+1;
            output.Text=history.Substring(start);
        }
        output.SelectionStart=output.TextLength;output.SelectionLength=0;
        output.AppendText(incoming);
        output.SelectionStart=output.TextLength;output.ScrollToCaret();
    }
    void Log(string text) {
        string line="["+DateTime.Now.ToString("HH:mm:ss")+"] "+text+"\r\n";
        Append(line);
        try{File.AppendAllText(logPath,line,Encoding.UTF8);}catch{}
    }
    void ClosePort(){if(port.IsOpen)port.Close();ports.Enabled=true;connect.Text="连接串口";connection.Text="串口已断开";}
    void OpenPort(){
        nextConnect=DateTime.UtcNow.AddSeconds(5);
        if(ports.SelectedItem==null)return;
        try{
            port.PortName=ports.SelectedItem.ToString();port.Open();ports.Enabled=false;connect.Text="断开串口";
            connection.Text=port.PortName+" 已连接 | 115200 / 8N1 | 等待开发板确认天气数据";
            nextSend=DateTime.MinValue;Log("串口已连接 "+port.PortName);
        }catch(Exception e){Log("串口连接失败："+e.Message+"；请关闭其他软件对同一端口的占用。");}
    }
    static string WeatherText(int c){
        if(c==0)return "晴";if(c<=3)return "少云/多云";if(c==45||c==48)return "雾";
        if(c>=51&&c<=57)return "毛毛雨";if(c>=61&&c<=67)return "雨";
        if(c>=71&&c<=77)return "雪";if(c>=80&&c<=82)return "阵雨";
        if(c==85||c==86)return "阵雪";if(c>=95)return "雷暴";return "未知";
    }
    static long UnixNow(){return (long)(DateTime.UtcNow-new DateTime(1970,1,1,0,0,0,DateTimeKind.Utc)).TotalSeconds;}
    static double Number(Dictionary<string,object> d,string key){
        if(!d.ContainsKey(key)||d[key]==null)throw new FormatException("天气API缺少字段 "+key);
        return Convert.ToDouble(d[key],CultureInfo.InvariantCulture);
    }
    // Yesterday, today and tomorrow in Beijing calendar days.
    public static int[] ParseRainForecast(string json,long now,out long start){
        var root=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(json);
        if(!root.ContainsKey("daily")||!root.ContainsKey("daily_units"))throw new FormatException("缺少三日降水统计");
        var units=(Dictionary<string,object>)root["daily_units"];
        if(!units.ContainsKey("precipitation_sum")||Convert.ToString(units["precipitation_sum"])!="mm")throw new FormatException("降水单位不是毫米");
        var daily=(Dictionary<string,object>)root["daily"];
        if(!daily.ContainsKey("time")||!daily.ContainsKey("precipitation_sum"))throw new FormatException("降水日期或数据缺失");
        var times=(System.Collections.IList)daily["time"];
        var rain=(System.Collections.IList)daily["precipitation_sum"];
        if(times.Count!=rain.Count)throw new FormatException("降水日期和数据数量不一致");
        start=((now+28800)/86400-1)*86400-28800;
        var result=new int[3];
        for(int day=0;day<3;day++){
            int found=0;
            for(int i=0;i<times.Count;i++){
                if(times[i]==null||Convert.ToInt64(times[i],CultureInfo.InvariantCulture)!=start+day*86400)continue;
                if(rain[i]==null)throw new FormatException("三日降水统计含缺测值");
                double value=Convert.ToDouble(rain[i],CultureInfo.InvariantCulture);
                if(double.IsNaN(value)||double.IsInfinity(value)||value<0||value>2000)throw new FormatException("日降水统计超出范围");
                result[day]=(int)Math.Round(value*10,MidpointRounding.AwayFromZero);found++;
            }
            if(found!=1)throw new FormatException("三日降水统计日期缺失或重复");
        }
        return result;
    }
    void AcceptWeather(string json){
        var root=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(json);
        if(!root.ContainsKey("current"))throw new FormatException("天气API未返回current");
        var current=(Dictionary<string,object>)root["current"];
        double t=Number(current,"temperature_2m"),h=Number(current,"relative_humidity_2m"),w=Number(current,"wind_speed_10m"),c=Number(current,"weather_code");
        long time=checked((long)Number(current,"time"));
        if(t < -80||t>65||h<0||h>100||w<0||w>150||c<0||c>99||UnixNow()-time<0||UnixNow()-time>3600)
            throw new FormatException("天气API数据超出范围或超过1小时");
        temp10=(int)Math.Round(t*10,MidpointRounding.AwayFromZero);
        humidity=(int)h;wind10=(int)Math.Round(w*10,MidpointRounding.AwayFromZero);code=(int)c;observed=time;weatherValid=true;
        string when=new DateTime(1970,1,1,0,0,0,DateTimeKind.Utc).AddSeconds(observed).AddHours(8).ToString("yyyy-MM-dd HH:mm");
        summary.Text="长沙 | "+WeatherText(code)+" | 气温 "+t.ToString("0.0",CultureInfo.InvariantCulture)+" ℃ | 空气湿度 "+humidity+" % | 风速 "+w.ToString("0.0",CultureInfo.InvariantCulture)+" 米/秒\r\n天气观测时间："+when+"（北京时间）；来源 Open-Meteo，城市天气模型数据";
        nextSend=DateTime.MinValue;
        Log("天气获取成功："+when+"（北京时间），"+WeatherText(code)+"，气温 "+t.ToString("0.0",CultureInfo.InvariantCulture)+"℃，空气湿度 "+humidity+"%，风速 "+w.ToString("0.0",CultureInfo.InvariantCulture)+" 米/秒。");
        rainDays10=null;
        try{
            long fetched=UnixNow(),start;
            int[] days=ParseRainForecast(json,fetched,out start);
            rainDays10=days;rainStart=start;rainFetched=fetched;
            int sum=days[0]+days[1]+days[2];
            string period=new DateTime(1970,1,1,0,0,0,DateTimeKind.Utc).AddSeconds(start).AddHours(8).ToString("MM月dd日")+"至"+
                new DateTime(1970,1,1,0,0,0,DateTimeKind.Utc).AddSeconds(start+172800).AddHours(8).ToString("MM月dd日");
            string detail=String.Format(CultureInfo.InvariantCulture,"三日降水统计（{0}）：{1:0.0}、{2:0.0}、{3:0.0} mm；累计 {4:0.0} mm；日均 {5:0.0} mm/日；降水权重 8%。",period,days[0]/10.0,days[1]/10.0,days[2]/10.0,sum/10.0,((sum+1)/3)/10.0);
            rainCaption=String.Format(CultureInfo.InvariantCulture,"降水统计（{0}）：昨天 {1:0.0} / 今天 {2:0.0} / 明天 {3:0.0} mm\r\n三日累计：{4:0.0} mm　日均降水：{5:0.0} mm/日　范围：昨天/今天/明天",period,days[0]/10.0,days[1]/10.0,days[2]/10.0,sum/10.0,((sum+1)/3)/10.0);
            rainfall.Text=rainCaption;
            Log(detail);
        }catch(Exception e){rainfall.Text="降水统计暂不可用："+e.Message;Log("三日降水统计暂不可用："+e.Message);}
    }
    void SendWeather(){
        nextSend=DateTime.UtcNow.AddSeconds(30);
        if(!weatherValid||!port.IsOpen)return;
        long now=UnixNow();
        if(now-observed>3600||now<observed){Log("缓存天气已过期，等待API更新。");return;}
        string body=String.Format(CultureInfo.InvariantCulture,"WX,{0},{1},{2},{3},{4},{5}",temp10,humidity,wind10,code,observed,now);
        int check=0;foreach(byte b in Encoding.ASCII.GetBytes(body))check^=b;
        string frame=body+"*"+check.ToString("X2");
        port.Write("WXCLOUD,"+(cloud.Checked?"1":"0")+"\r\n"+frame+"\r\n");
        if(rainDays10!=null&&now>=rainFetched&&now-rainFetched<=3600&&rainStart==((now+28800)/86400-1)*86400-28800){
            string rainBody=String.Format(CultureInfo.InvariantCulture,"WX3C,{0},{1},{2},{3},{4},{5}",rainDays10[0],rainDays10[1],rainDays10[2],rainStart,rainFetched,now);
            check=0;foreach(byte b in Encoding.ASCII.GetBytes(rainBody))check^=b;
            port.Write(rainBody+"*"+check.ToString("X2")+"\r\n");
        }
        Log("天气数据已发送；云端上报"+(cloud.Checked?"已启用":"未启用")+"，等待开发板确认。");
    }
    // Translate complete received lines only; serial packets may split a label
    // or number at any byte. Keep original ASCII commands for protocol parsing.
    public static string TranslateLine(string line){
        var risk=Regex.Match(line,@"^RISK LEVEL=([012]) VALID=([01]) BASE=([01]) CONFIRM=([01]) SCORE100=(\d+) D100=(\d+) A100=(\d+)$");
        if(risk.Success){
            string level=risk.Groups[3].Value=="0"?"待设基准":risk.Groups[1].Value=="2"?"危险":risk.Groups[1].Value=="1"?"预警":risk.Groups[2].Value=="0"?"数据不足":risk.Groups[4].Value=="1"?"确认中":"安全";
            return "实验评分等级："+level+"　综合分数："+(risk.Groups[2].Value=="1"?(int.Parse(risk.Groups[5].Value)/100.0).ToString("0.00",CultureInfo.InvariantCulture):"--")+
                "　位移相对变化："+(int.Parse(risk.Groups[6].Value)/100.0).ToString("0.00",CultureInfo.InvariantCulture)+" mm　倾角相对变化："+(int.Parse(risk.Groups[7].Value)/100.0).ToString("0.00",CultureInfo.InvariantCulture)+"°";
        }
        line=Regex.Replace(line,@"^POS_RATE\(mm/min\)=([-\d.]+) WINDOW=5S$",
            "位移变化率：$1 毫米/分钟（与倾角共用5秒窗口）");
        if(line=="POS_RATE=WAIT_5S")return "位移变化率：等待5秒有效数据（与倾角同步）";
        line=Regex.Replace(line,@"^SEQ=(\d+)\s+RAIN=([-\d.]+)%\s+SOIL=([-\d.]+)%\s+POS=([-\d.]+)\s+mm\s+POS_ADC=(-?\d+)$",
            "第 $1 组监测数据\r\n雨滴湿润度：$2%　土壤相对湿度：$3%\r\n位移：$4 毫米　位移原始采样值：$5");
        line=Regex.Replace(line,@"^ACC\(mg\): X=([-\d.]+) Y=([-\d.]+) Z=([-\d.]+)$",
            "加速度（mg）：X轴 $1　Y轴 $2　Z轴 $3");
        line=Regex.Replace(line,@"^GYRO\(deg/s\): X=([-\d.]+) Y=([-\d.]+) Z=([-\d.]+)$",
            "角速度（°/秒）：X轴 $1　Y轴 $2　Z轴 $3");
        line=Regex.Replace(line,@"^IMU_TEMP=([-\d.]+) C$","芯片温度：$1℃");
        var key=Regex.Match(line,@"^K1_RAW=([01]) BUZZER=([01]) PA8_OUT=([01])$");
        if(key.Success)return "按键K1："+(key.Groups[1].Value=="0"?"按下":"松开")+
            "　蜂鸣器："+(key.Groups[2].Value=="1"?"开启":"关闭")+
            "　PA8输出："+(key.Groups[3].Value=="1"?"高电平":"低电平");
        line=Regex.Replace(line,@"^TILT\(deg\)=([-\d.]+) ROLL=([-\d.]+) PITCH=([-\d.]+) RATE\(deg/min\)=([-\d.]+)$",
            "合成倾角：$1°　横滚角：$2°　俯仰角：$3°\r\n倾角变化率：$4°/分钟");
        line=Regex.Replace(line,@"^TILT\(deg\)=([-\d.]+) ROLL=([-\d.]+) PITCH=([-\d.]+) RATE=WAIT_5S$",
            "合成倾角：$1°　横滚角：$2°　俯仰角：$3°\r\n倾角变化率：等待5秒有效数据");
        if(line.StartsWith("TILT=INVALID"))return "倾角数据无效：传感器运动干扰或读取失败";
        if(line=="WX_OK")return "开发板已确认收到天气数据";
        if(line=="WX3_OK")return "开发板已确认三日降水统计，LCD已启用累计量和日均值";
        if(line=="WX3_REJECTED")return "开发板拒绝三日降水统计：检查校验、日期或数据范围";
        if(line=="WX_REJECTED")return "开发板拒绝天气数据：请检查数据格式、校验或时间";
        if(line=="WX_PUBLISHED")return "开发板已将天气数据交给4G模块发送";
        if(line=="WX_CLOUD=1")return "开发板天气云端上报：已启用";
        if(line=="WX_CLOUD=0")return "开发板天气云端上报：已关闭";
        if(line=="K0 PAGE=WEATHER")return "按键K0：已切换到天气页面";
        if(line=="K0 PAGE=RISK")return "按键K0：已切换到评分报警页面";
        if(line=="BASELINE_OK")return "按键K-UP：位移和倾角基准已设置，重新建立评分窗口";
        if(line=="BASELINE_WAIT")return "按键K-UP：基准未设置，请保持静止并等待完整5秒有效数据后再按";
        if(line=="MANUAL_BLOCKED")return "自动报警优先：当前手动蜂鸣器指令未执行，待报警解除后可试鸣";
        if(line=="K0 PAGE=SENSORS")return "按键K0：已切换到传感器页面";
        if(line=="CLOUD BUZZER=ON")return "云端控制：蜂鸣器已开启";
        if(line=="CLOUD BUZZER=OFF")return "云端控制：蜂鸣器已关闭";
        if(line=="CLOUD_RX_OVERFLOW")return "4G下行接收缓存溢出，已丢弃不完整命令";
        if(line=="K1 BUZZER=PULSE")return "按键K1：蜂鸣器滴滴间歇响；再按持续响，再按关闭";
        if(line=="K1 BUZZER=ON")return "按键K1：蜂鸣器持续响；再按关闭";
        if(line=="K1 BUZZER=OFF")return "按键K1：蜂鸣器已关闭";
        if(line=="BOOT_START")return "开发板正在启动";
        if(line=="BOOT_KEY_OK")return "开发板按键与系统计时初始化完成";
        line=Regex.Replace(line,@"^BOOT_LCD_MS=(\d+) K0=PE4 ACTIVE_LOW$","屏幕初始化完成：启动计时 $1 毫秒；K0按键为PE4，低电平有效");
        line=Regex.Replace(line,@"^BOOT_FIRST_FRAME_MS=(\d+)$","首屏数据刷新完成：启动计时 $1 毫秒");
        if(line.StartsWith("STM32 SENSORS R12"))return "多模态监测终端已启动｜串口1：PA9/PA10，115200，8位数据、无校验、1位停止位";
        if(line.StartsWith("RAIN/SOIL: relative"))return "单位：湿润度/相对湿度 %，位移 毫米，加速度 mg，角速度 °/秒";
        return line;
    }
    void ReadPort(){
        pending+=port.ReadExisting();
        int end;
        while((end=pending.IndexOf('\n'))>=0){
            string line=pending.Substring(0,end).Trim();pending=pending.Substring(end+1);
            if(line=="WX_OK"){
                connection.Text=port.PortName+" 已连接 | 115200 / 8N1 | 开发板已确认天气数据";
            }
            if(line=="WX_OK"||line.StartsWith("WX3_")||line=="WX_REJECTED"||line=="WX_PUBLISHED"||line.StartsWith("WX_CLOUD=")||line.StartsWith("K0 PAGE=")||line.StartsWith("BASELINE_")||line=="MANUAL_BLOCKED"||line.StartsWith("K1 BUZZER=")||line.StartsWith("CLOUD ")||line=="CLOUD_RX_OVERFLOW"||line.StartsWith("BOOT_"))Log(TranslateLine(line));
            else Append(TranslateLine(line)+"\r\n");
            if(line.StartsWith("STM32 SENSORS R12"))nextSend=DateTime.MinValue;
        }
        if(pending.Length>4096){pending="";Log("收到的串口数据行过长，已丢弃，等待下一组完整数据。");}
    }
    void OnTick(object sender,EventArgs args){
        try{
            DateTime now=DateTime.UtcNow;
            if(rainDays10!=null){
                long unix=UnixNow();
                bool valid=unix>=rainFetched&&unix-rainFetched<=3600&&rainStart==((unix+28800)/86400-1)*86400-28800;
                rainfall.Text=valid?rainCaption:"降水统计已过期，等待获取新的三日预报；请检查网络或点击刷新天气。";
            }
            if(autoConnect&&!port.IsOpen&&now>=nextConnect)OpenPort();
            if(request==null&&now>=nextFetch){request=http.GetStringAsync(Url);nextFetch=now.AddMinutes(10);Log("正在获取长沙天气。");}
            if(request!=null&&request.IsCompleted){
                try{AcceptWeather(request.GetAwaiter().GetResult());}
                catch(Exception e){Log("天气获取失败："+e.Message+"；60秒后重试。");nextFetch=now.AddSeconds(60);}
                finally{request=null;}
            }
            if(port.IsOpen&&port.BytesToRead>0)ReadPort();
            if(port.IsOpen&&weatherValid&&now>=nextSend)SendWeather();
        }catch(Exception e){Log("转发异常："+e.Message);try{ClosePort();}catch{}nextConnect=DateTime.UtcNow.AddSeconds(5);}
    }
}
