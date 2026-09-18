# ESP32-S3 仿手机 UI 项目

这是一个基于 **ESP-IDF** 框架和 ESP32-S3 的仿手机用户界面（UI）项目。该项目旨在为嵌入式设备提供一个直观、美观且功能丰富的交互界面，模仿现代智能手机的操作体验。

## 项目演示

您可以在 Bilibili 上观看项目的演示视频：
https://www.bilibili.com/video/BV1utey6aE6M/?share_source=copy_web&vd_source=c5b28031d15a1478a256a684112e9e98

## 截图展示

![项目截图](Screenshot/IMG20260915064909.jpg)

## 主要功能

目前项目已实现以下功能：

*   **MP3 播放器**：支持音频文件的播放与控制。
*   **NES 游戏机模拟器**：可以运行经典的 NES (任天堂娱乐系统) 游戏。
*   **中文日历**：显示日期、时间，并支持中文界面。
*   **WiFi STA 模式**：支持连接到无线局域网，实现网络通信功能。

## 未来计划

更多功能正在开发和更新中，敬请期待！

## 硬件连接说明

本项目使用以下外设模块，请按照下表连接引脚：

### 屏幕模块展示

![屏幕模块](Screenshot/tb_image_share_1789697037630.png)

### 1. 显示屏与触摸
*   **屏幕驱动**: ILI9341 (SPI 接口)
*   **触摸驱动**: FT6336G (I2C 接口)

#### 显示屏引脚连接

| 功能 | ESP32-S3 GPIO | 备注 |
| :--- | :---: | :--- |
| LCD SCLK | GPIO 40 | SPI 时钟 |
| LCD MOSI | GPIO 41 | SPI 数据输出 |
| LCD MISO | GPIO 38 | SPI 数据输入 |
| LCD CS | GPIO 1 | 片选 |
| LCD DC | GPIO 42 | 数据/命令选择 |
| LCD RST | GPIO 2 | 复位 |
| LCD BL | GPIO 39 | 背光控制 |

#### 触摸屏引脚连接

| 功能 | ESP32-S3 GPIO | 备注 |
| :--- | :---: | :--- |
| I2C SCL | GPIO 21 | I2C 时钟线 |
| I2C SDA | GPIO 47 | I2C 数据线 |
| Touch RST | GPIO 11 | 触摸复位 |

### 2. 音频输出

#### 音频模块展示

![音频模块](Screenshot/tb_image_share_1789698535189.png)

*   **功放模块**: MAX98357 (I2S 接口)

| 功能 | ESP32-S3 GPIO | 备注 |
| :--- | :---: | :--- |
| BCLK | GPIO 21 | 位时钟 |
| WS (LRC) | GPIO 14 | 字选择/左右声道 |
| DIN | GPIO 10 | 数据输入 |

### 3. TF 卡模块 (SD Card)
*   **接口**: SPI

| 功能 | ESP32-S3 GPIO | 备注 |
| :--- | :---: | :--- |
| SCK | GPIO 5 | 时钟 |
| MISO | GPIO 4 | 主入从出 |
| MOSI | GPIO 6 | 主出从入 |
| CS | GPIO 7 | 片选 |

### 4. NES 游戏按键
NES 模拟器的按键映射逻辑如下：
*   **电平逻辑**：`1` 代表抬起（未按下），`0` 代表按下。
*   **按键对应关系**：

| NES 按键 | ESP32-S3 GPIO | 备注 |
| :--- | :---: | :--- |
| A | GPIO 9 | Bit 0 |
| B | GPIO 8 | Bit 1 |
| SELECT | GPIO 46 | Bit 2 |
| START | GPIO 3 | Bit 3 |
| UP | GPIO 17 | Bit 4 |
| DOWN | GPIO 16 | Bit 5 |
| LEFT | GPIO 15 | Bit 6 |
| RIGHT | GPIO 18 | Bit 7 |

*(注：按键状态通过 `key_board` 变量的位操作进行更新，具体逻辑参考 `video_audio.c`)*

## 环境搭建

本项目使用 **ESP-IDF v6.0.2** 进行开发。请确保您已经安装了该版本的 ESP-IDF 环境。

1.  参考 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html) 安装 ESP-IDF。
2.  切换或安装 **v6.0.2** 版本以确保兼容性。

## 如何编译和上传

### 方法一：使用命令行

1.  克隆本仓库到您的本地机器：
    ```bash
    git clone <repository-url>
    cd xiaocai_esp32_project
    ```

2.  设置 ESP-IDF 环境：
    ```bash
    . $IDF_PATH/export.sh  # Linux/macOS
    %IDF_PATH%\export.bat  # Windows
    ```

3.  配置项目（可选）：
    ```bash
    idf.py menuconfig
    ```

4.  编译项目：
    ```bash
    idf.py build
    ```

5.  连接您的 ESP32-S3 开发板，然后上传代码并监控串口输出：
    ```bash
    idf.py -p PORT flash monitor
    ```
    请将 `PORT` 替换为您的开发板对应的串口端口（例如 `/dev/ttyUSB0` 或 `COM3`）。

### 方法二：使用 VS Code ESP-IDF 插件

1.  在 VS Code 中安装 "Espressif IDF" 插件。
2.  打开项目文件夹。
3.  配置 ESP-IDF 环境（选择 v6.0.2）。
4.  使用插件提供的功能进行编译、烧录和监控。

## 贡献

欢迎任何形式的贡献，包括问题报告、功能请求和代码提交。

## 许可证

本项目采用 **非商业性使用** 许可协议。仅供个人学习、研究和交流使用，严禁用于任何商业用途。详情请参阅 [LICENSE](LICENSE) 文件。
