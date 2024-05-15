#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <map>

using namespace std;

// 函数用于解析文件中的键值对，并返回指定键的值
string parseUEventFile(const string& filePath, const string& key) {
    string line, keyValue;
    ifstream file(filePath);

    if (file.is_open()) {
        while (getline(file, line)) {
            if (line.find(key) != string::npos) {
                istringstream iss(line);
                getline(iss, keyValue, '=');
                getline(iss, keyValue);
                break;
            }
        }
        file.close();
    } else {
        cerr << "错误：无法打开文件" << filePath << endl;
    }

    return keyValue;
}

// 修改键值对文件中的某个键的值
//   input_file_path: 输入文件路径
//   output_file_path: 输出文件路径
//   key_value_to_modify: 需要修改的键值对 <键, 新值>
// 返回值:
//   成功返回 true，失败返回 false
bool modifyKeyValuePair(const std::string& input_file_path, const std::string& output_file_path, const std::pair<std::string, std::string>& key_value_to_modify) {
    // 打开输入文件
    std::ifstream infile(input_file_path);
    // 检查文件是否成功打开
    if (!infile.is_open()) {
        std::cerr << "错误：打开输入文件时出错" << std::endl;
        return false;
    }

    // 创建一个map来存储键值对
    std::map<std::string, std::string> key_value_pairs;
    std::string line;

    // 逐行读取文件内容
    while (std::getline(infile, line)) {
        // 查找等号的位置
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            // 提取键和值
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            // 将键值对存储到map中
            key_value_pairs[key] = value;
        }
    }

    // 修改指定键的值
    key_value_pairs[key_value_to_modify.first] = key_value_to_modify.second;

    // 关闭输入文件流
    infile.close();

    // 打开输出文件，以写入模式
    std::ofstream outfile(output_file_path);
    // 检查文件是否成功打开
    if (!outfile.is_open()) {
        std::cerr << "错误：打开输出文件时出错" << std::endl;
        return false;
    }

    // 将修改后的键值对写回到文件中
    for (const auto& pair : key_value_pairs) {
        outfile << pair.first << "=" << pair.second << std::endl;
    }

    // 关闭输出文件流
    outfile.close();

    return true;
}


//计算准确时间
void exact_time(double hours, int& wholeHours, int& minutes) {
    wholeHours = static_cast<int>(hours); // 取整数部分作为小时
    minutes = static_cast<int>((hours - wholeHours) * 60); // 将小数部分转换为分钟
}

int main() {
    string yamlFilePath = "/storage/emulated/0/Android/buttay_gh/buttay.yml";
    string ueventFilePath = "/sys/class/power_supply/battery/uevent";
    string logFilePath = "/storage/emulated/0/Android/buttay_gh/buttay_info.log";

    // 读取配置文件对应键值
    int to1to2 = stoi(parseUEventFile(yamlFilePath, "to1to2"));//是否双电芯值1或2
    int Charging_detection_time = stoi(parseUEventFile(yamlFilePath, "Charging_detection_time"));//检测是否循环延迟整数秒
    int cycle_time = stoi(parseUEventFile(yamlFilePath, "cycle_time"));//检测周期时间整数秒
    int Power_consumption_record = stoi(parseUEventFile(yamlFilePath, "Power_consumption_record"));//记录功耗数量

    ofstream logFile(logFilePath, ios::out | ios::trunc); // 打开日志文件，如果存在则清空内容
    if (!logFile.is_open()) {
        logFile << "错误：无法打开日志文件" << logFilePath << endl;
    }

    vector<double> powerHistory; // 存储历史功耗值

    while (true) {
        // 读取充电状态如果正在充电则暂时停止
        string buttaystatus = parseUEventFile(ueventFilePath, "POWER_SUPPLY_STATUS");//充电状态读取
        while (buttaystatus != "Not charging") {
            // 调用修改键值对文件函数
            std::ostringstream oss;
            oss << "你在充电吗？";
            std::string module_description = oss.str();
            std::pair<std::string, std::string> key_value_to_modify = {"description", module_description};
            if (!modifyKeyValuePair("module.prop", "module.prop", key_value_to_modify)) {
                logFile << "错误：修改键值对失败退出程序" << std::endl;
                exit(1);
            }
            logFile << "信息：充电中暂停预测" << endl;
            // 每cycle_time秒检测一次是否充电
            sleep(Charging_detection_time);
            buttaystatus = parseUEventFile(ueventFilePath, "POWER_SUPPLY_STATUS");
        }

        // 读取电流电压容量
        string buttay_max_string = parseUEventFile(ueventFilePath, "POWER_SUPPLY_CHARGE_FULL");//容量微安
        string currentNowStr = parseUEventFile(ueventFilePath, "POWER_SUPPLY_CURRENT_NOW");//电流微安
        string voltage_string = parseUEventFile(ueventFilePath, "POWER_SUPPLY_VOLTAGE_NOW");//电压毫伏
        string buttay_capacity_string = parseUEventFile(ueventFilePath, "POWER_SUPPLY_CAPACITY");//电池电量%
        // 将读取到的字符串转换为整数
        int currentNow = stoi(currentNowStr);
        // 将读取到的字符串转换整数并换算成伏v
        double voltage = stoi(voltage_string) / 1000.0;
        // 计算容量wh 微安÷1000000×电压=wh
        double buttay_max = (stod(buttay_max_string) / 1000000.0) * voltage;
        // 通过百分比直接计算当前电量wh
        double buttay_capacity = stoi(buttay_capacity_string) * 0.01 * buttay_max;
        // 计算功率（功率 = 电压 * 电流）
        double power = voltage * currentNow / 1000.0; // 单位换算为瓦

        // 如果是双电池，功耗翻倍
        if (to1to2 == 2) {
            power *= 2;
        }

        // 将实时功耗添加到历史功耗数组中
        powerHistory.push_back(power);

        // 如果历史功耗数组长度超过Power_consumption_record，则删除最旧的功耗值
        if (powerHistory.size() > Power_consumption_record) {
            powerHistory.erase(powerHistory.begin());
        }

        // 计算历史功耗值的平均功耗
        double averagePower = 0;
        for (double p : powerHistory) {
            averagePower += p;
        }
        averagePower /= powerHistory.size();

        // 计算总电量还有多久用完
        double remainingTime = buttay_max / averagePower; // 单位：小时
        // 计算剩余电量还有多久用完
        double remainTime = buttay_capacity / averagePower; // 单位：小时
        // 计算准确时间
        // 接收函数返回
        int integer1, decimal1;
        int integer2, decimal2;
        // 使用函数计算准确时间
        exact_time(remainingTime, integer1, decimal1);
        exact_time(remainTime, integer2, decimal2);

        // 获取当前时间
        auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());


        // 调用修改键值对文件函数
        std::ostringstream oss;
        //使用 fixed 设置浮点数的输出格式为固定小数位数，setprecision(2) 设置小数点后保留两位
        oss << fixed << setprecision(2);
        oss << "● " << "理论时间到:" << std::put_time(std::localtime(&now), "%H:%M:%S") << " 功耗:" << power << "w" << " 平均功耗:" << averagePower << "w" << " 总续航:" << integer1 << "h" << decimal1 << "m" << " 剩余续航:" << integer2 << "h" << decimal2 << "m" << " 充满容量:" << buttay_max << "wh" << " 剩余容量:" << buttay_capacity << "wh";

        std::string module_description = oss.str();
        std::pair<std::string, std::string> key_value_to_modify = {"description", module_description};
        if (!modifyKeyValuePair("module.prop", "module.prop", key_value_to_modify)) {
            logFile << "错误：修改键值对失败退出程序" << std::endl;
            exit(1);
        }

        // 每cycle_time分钟检测一次
        sleep(cycle_time); // 使用 sleep 函数
    }

    logFile.close(); // 关闭日志文件

    return 0;
}
