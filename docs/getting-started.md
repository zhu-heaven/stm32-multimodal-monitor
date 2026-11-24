# 构建与运行

## 固件

1. 使用Keil5、ARM Compiler 5打开firmware/USER/LCD_ST7789.uvproj。
2. 目标STM32F407VE，程序Flash从0x08000000开始；设备包、Flash算法和ST-Link设置按自己的板卡配置。
3. 编译，不需要云端账号。接线见wiring.md。
4. 若启用云端控制，在firmware/USER/project_config.h填写自己的产品ID和设备名，DTU配置115200/8N1、MQTT及对应OneJSON主题。凭据仅填写在DTU平台或服务器。
5. 烧录后保持静止，等待完整窗口，按K-UP设基准。

## 电脑串口工具

在Windows上执行tools/serial-assistant/build.cmd和start.cmd。默认串口COM3，可按实际选择；同一串口不可同时被多个程序占用。工具每10分钟更新天气、每30秒向已连接开发板转发，天气有效性仍需通过校验。

## 网页

要求Node.js≥22.13。源码保留Vinext/Vite、Cloudflare Workers和D1适配，不是仅用静态服务器就能运行的HTML页面。

```powershell
cd web
npm ci
npm run build
npx wrangler d1 migrations apply DB --local --config wrangler.local.jsonc --persist-to .wrangler/state
npm run dev
```

打开控制台打印的本地地址。游客可以查看页面；未配置OneNET时传感器值为空。天气直接读取城市网格API，可能受网络或接口限流影响。

管理员设置仅用于你自己的本地数据库或部署环境。公开副本没有已有账号或密码：需要按运行环境初始化密码，不要沿用本地开发用的示例pepper部署到公网。

## 可选云端接入

在自己的服务端环境设置：ONENET_PRODUCT_ID、ONENET_DEVICE_NAME、ONENET_ACCESS_KEY（或ONENET_API_TOKEN）、SITE_PASSWORD_PEPPER。示例格式见web/.env.example；实际值不能写进前端、Git提交或截图。

公开副本的web/.openai/hosting.json只保留本地DB绑定，没有原部署项目ID。若使用Sites或其他Workers托管，需要为自己的项目重新注册部署及数据库绑定，不会自动连接原作者运行中的系统。

cloud/onenet-model.json和cloud/dtu-control.lua是脱敏示例。根据自己的平台版本导入/配置，并核对主题中的产品ID、设备名和下发/回执格式。
