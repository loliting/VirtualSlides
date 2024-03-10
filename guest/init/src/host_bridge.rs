// Module for talking with the host

use std::error::Error;
use std::result::Result;
use std::fs::File;
use std::io::{Read, Write};
use std::time::Instant;

use serde::{Deserialize, Serialize};

const WRITE_CONSOLE_PATH: &str = "/dev/hvc0";
const READ_CONSOLE_PATH: &str = "/dev/hvc1";


pub struct HostBridge {
    read_console: File,
    write_console: File
}

#[derive(Serialize, Deserialize)]
pub enum MessageType {
    #[serde(rename = "reboot")] 
    Reboot
}

#[derive(Serialize, Deserialize)]
pub struct Message {
    #[serde(rename = "type")] 
    pub mtype: MessageType
}

impl Message {
    pub fn new_reboot() -> Result<Self, Box<dyn Error>> {
        Ok(
            Message{
                mtype: MessageType::Reboot
            }
        )
    }
}

impl HostBridge {
    pub fn new() -> Result<Self, Box<dyn Error>> {
        let read_console = File::options()
        .read(true)
        .write(false)
        .open(READ_CONSOLE_PATH)?;

        let write_console = File::options()
        .read(false)
        .write(true)
        .open(WRITE_CONSOLE_PATH)?;

        let host_bridge = HostBridge {
            read_console: read_console,
            write_console: write_console
        };
        
        Ok(host_bridge)
    }

    fn write(&mut self, msg: Message) -> Result<(), Box<dyn std::error::Error>>  {
        let mut message = String::new();
        message.push_str(&serde_json::to_string(&msg)?);
        message.push('\n'); // We use '\n' as msg separator, because it also flushes the buffor
        
        self.write_console.write(message.as_bytes())?;
        
        Ok(())
    }
    
    fn read(&mut self) -> Result<(), Box<dyn std::error::Error>>  {
        let mut read_u8 = || -> Result<u8, Box<dyn std::error::Error>> {
            let mut buf: [u8;1] = [0];
            self.read_console.read(&mut buf)?;
            Ok(buf[0])
        };
        let now = Instant::now();

        let mut buf: Vec<u8> = Vec::new();
        let mut c: u8 = read_u8()?;
        while c != b'\n' {
            buf.push(c);
            c = read_u8()?;
        }
        if buf.len() <= 256{
            println!("buf: {}", String::from_utf8(buf.clone()).unwrap());

        }
        let elapsed = now.elapsed();
        println!("Read {} bytes in: {:.2?}", buf.len(), elapsed);
        Ok(())
    }

    pub fn message_host(&mut self, msg: Message) -> Result<(), Box<dyn std::error::Error>> {
        self.write(msg)?;
        self.read()?;
        Ok(())
    }

}