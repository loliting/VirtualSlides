// Module for talking with the host

use std::cmp::max;
use std::fs::File;
use std::io::{BufRead, BufReader, Write};
use std::time::{Duration, Instant};
use anyhow::{Result, anyhow};
use serde::{Deserialize, Serialize};
use indicatif::{HumanBytes, ProgressBar};

const WRITE_CONSOLE_PATH: &str = "/dev/hvc0";
const READ_CONSOLE_PATH: &str = "/dev/hvc1";

pub struct HostBridge {
    read_console: File,
    write_console: File
}

#[derive(Serialize, Deserialize)]
pub enum RequestType {
    #[serde(rename = "reboot")] 
    Reboot,
    #[serde(rename = "download-test")] 
    DownloadTest,
    #[serde(rename = "hostname")] 
    Hostname,
}

#[derive(Serialize, Deserialize, PartialEq, Eq)]
enum Status {
    #[serde(rename = "ok")] 
    Ok,
    #[serde(rename = "err")] 
    Err
}

#[derive(Serialize, Deserialize)]
pub struct Response {
    pub(in self) status: Status,
    pub(in self) error: Option<String>,

    pub hostname: Option<String>,
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

    fn write(&mut self, request: RequestType) -> Result<()>  {
        let mut message = serde_json::json!({"type": request}).to_string();
        message.push_str("\x1e\n");
        
        self.write_console.write(message.as_bytes())?;
        
        Ok(())
    }
    
    fn read(&mut self) -> Result<Response> {
        let mut reader = BufReader::new(&self.read_console);
        let now = Instant::now();
        let progress_bar = ProgressBar::new_spinner();
        progress_bar.enable_steady_tick(Duration::from_millis(150));

        let mut buf: Vec<u8> = Vec::new();
        reader.read_until(b'\x1e', &mut buf)?;
        progress_bar.finish();
        buf.pop();

        let response: Response = serde_json::from_slice(&buf)?;
        if response.status == Status::Err {
            return Err(anyhow!(response.error.unwrap_or_default()));
        }
        if buf.len() <= 256{
            println!("buf: {:?}", String::from_utf8(buf.to_vec())?);
        }

        let bytes_recived: u64 = buf.len().try_into().unwrap();
        let speed = bytes_recived / max(1, now.elapsed().as_secs());

        println!("\nRead {} in: {:.2?} ({}/s)", HumanBytes(bytes_recived), now.elapsed(), HumanBytes(speed));

        Ok(response)
    }

    pub fn message_host(&mut self, request: RequestType) -> Result<Response> {
        self.write(request)?;
        Ok(self.read()?)
    }

}