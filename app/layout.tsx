import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  metadataBase: new URL('http://127.0.0.1:4173'),
  title: '星穹邮差｜夜空收集小游戏',
  description: '驾驶魔法扫帚穿越星夜，收集星笺并躲开乌云。',
  openGraph: {
    title: '星穹邮差｜夜空收集小游戏',
    description: '把今晚的星光，送到每一扇窗前。',
    images: [{ url: '/og.png', width: 1536, height: 864, alt: '星穹邮差游戏封面' }],
  },
  twitter: {
    card: 'summary_large_image',
    title: '星穹邮差｜夜空收集小游戏',
    description: '把今晚的星光，送到每一扇窗前。',
    images: ['/og.png'],
  },
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return (
    <html lang="zh-CN">
      <body>{children}</body>
    </html>
  );
}
