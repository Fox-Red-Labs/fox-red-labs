//! Blink LED1 and print a counter on UART0 (PA8 TX, PA9 RX), the XDS110 back-channel port, 115200 8N1.
#![no_std]
#![no_main]

use embassy_executor::Spawner;
use embassy_mspm0::Config;
use embassy_mspm0::gpio::{Level, Output};
use embassy_mspm0::uart::{Config as UartConfig, Uart};
use embassy_time::Timer;
use panic_halt as _;

fn put_u32(buf: &mut [u8], mut n: u32) -> usize {
    let mut tmp = [0u8; 10];
    let mut i = 0;
    loop {
        tmp[i] = b'0' + (n % 10) as u8;
        n /= 10;
        i += 1;
        if n == 0 {
            break;
        }
    }
    for k in 0..i {
        buf[k] = tmp[i - 1 - k];
    }
    i
}

#[embassy_executor::main]
async fn main(_spawner: Spawner) -> ! {
    let p = embassy_mspm0::init(Config::default());
    let mut led1 = Output::new(p.PA0, Level::Low);
    led1.set_inversion(true);
    let mut uart = Uart::new_blocking(p.UART0, p.PA9, p.PA8, UartConfig::default()).unwrap();
    uart.blocking_write(b"Hello from Rust on MSPM0L1306\r\n").unwrap();
    let mut n: u32 = 0;
    loop {
        Timer::after_millis(500).await;
        led1.toggle();
        n += 1;
        let mut line = [0u8; 16];
        line[0] = b't';
        line[1] = b'=';
        let l = 2 + put_u32(&mut line[2..], n);
        line[l] = b'\r';
        line[l + 1] = b'\n';
        uart.blocking_write(&line[..l + 2]).unwrap();
    }
}
