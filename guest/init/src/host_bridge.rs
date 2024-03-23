// Module for talking with the host

use std::fs::File;
use std::io::{BufRead, BufReader, Write};
use std::time::{Duration, Instant};
use anyhow::{Result, anyhow};
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
    Reboot,
    #[serde(rename = "download-test")] 
    DownloadTest,
}

#[derive(Serialize, Deserialize)]
pub struct Message {
    #[serde(rename = "type")] 
    pub mtype: MessageType,
}

impl Message {
    pub fn from_message_type(t: MessageType) -> Self {
        Message {
            mtype: t
        }
    }
}

#[derive(Serialize, Deserialize, PartialEq, Eq, Debug)]
enum Status {
    #[serde(rename = "ok")] 
    Ok,
    #[serde(rename = "err")] 
    Err
}

#[derive(Serialize, Deserialize, Debug)]
struct StatusResponse {
    status: Status,
    error: Option<String>
}

impl HostBridge {
    pub fn new() -> Result<Self> {
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

    fn write(&mut self, msg: Message) -> Result<()>  {
        let mut message = String::new();
        message.push_str(&serde_json::to_string(&msg)?);
        message.push_str("\x1e\n");
        
        self.write_console.write(message.as_bytes())?;
        
        Ok(())
    }
    
    fn read(&mut self) -> Result<()> {
        let mut reader = BufReader::new(&self.read_console);
        let now = Instant::now();

        let mut buf: Vec<u8> = Vec::new();
        reader.read_until(b'\x1e', &mut buf)?;
        buf.pop();

        let status: StatusResponse = serde_json::from_slice(&buf)?;
        if status.status == Status::Err {
            return Err(anyhow!(status.error.unwrap_or_default()));
        }

        if buf.len() <= 256{
            println!("buf: {:?}", String::from_utf8(buf.to_vec())?);
        }
        let elapsed = now.elapsed();
        println!("Read {} bytes in: {:.2?}", buf.len(), elapsed);
        Ok(())
    }

    pub fn message_host(&mut self, msg: Message) -> Result<()> {
        self.write(msg)?;
        self.read()?;
        Ok(())
    }

}