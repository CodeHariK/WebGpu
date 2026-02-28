import { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'Apple Container UI',
  description: 'Manage your container runtime environments with ease',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body>
        <main style={{ maxWidth: '1200px', margin: '0 auto', padding: '40px 20px' }}>
          {children}
        </main>
      </body>
    </html>
  );
}
