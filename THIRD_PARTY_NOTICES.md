# 第三方代码与素材

- firmware/CORE：ARM CMSIS相关内核与启动文件；保留文件头原版权及许可说明。
- firmware/FWLIB：STMicroelectronics STM32F4标准外设库；保留文件头原版权及许可说明，仅在其原许可范围内使用。
- firmware/USER中的STM32设备头和系统初始化文件同样包含ST/ARM代码，见原文件头。
- web使用React、Vinext、Vite、Cloudflare/Wrangler、Drizzle、Recharts等依赖；版本和来源见package-lock.json，适用各自许可证。
- web/scripts/vite/sites-vite-plugin.ts为脚手架提供的MIT组件，许可证保留在同目录sites-vite-plugin.LICENSE；web/vendor中的许可也予以保留。
- assets/lcd-sensors-layout.png来自本项目的排版验证素材，图中为模拟数据；assets/system-architecture.svg为依据当前结构绘制的架构图。

本仓库未以一份新许可证覆盖或改变第三方文件的原许可。公开展示不意味着能够忽略原许可条款。
