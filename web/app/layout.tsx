import type {Metadata} from 'next';
import './globals.css';
export const metadata:Metadata={title:'多模态微形变监测 | 私有监测空间',description:'STM32与OneNET多传感器监测：实时读数、实验评分、天气预报和历史曲线。',icons:{icon:'/favicon.svg'}};
export default function RootLayout({children}:Readonly<{children:React.ReactNode}>){return <html lang="zh-CN"><body>{children}</body></html>;}
