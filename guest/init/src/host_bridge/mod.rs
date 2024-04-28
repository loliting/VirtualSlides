// Module for talking with the host

use std::io::{BufRead, BufReader, Write};
use std::time::{Duration, Instant};
use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
use indicatif::{HumanBytes, ProgressBar};
use vsock::{get_local_cid, VsockAddr, VsockStream};


pub struct HostBridge {
    stream: VsockStream
}

#[derive(Serialize, Deserialize)]
pub enum RequestType {
    #[serde(rename = "reboot")] 
    Reboot,
    #[serde(rename = "download-test")] 
    DownloadTest,
    #[serde(rename = "get-hostname")] 
    GetHostname,
    #[serde(rename = "get-motd")] 
    GetMotd,
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
    pub motd: Option<String>,
}

impl HostBridge {
    pub fn new() -> Result<Self> {
        let cid = get_local_cid()?;
        let sock = VsockAddr::new(2, cid);
        let stream = VsockStream::connect(&sock)?;
        Ok(HostBridge { stream })
    }

    fn do_read(&mut self) -> Result<Vec<u8>> {
        let mut buf: Vec<u8> = Vec::new();

        let mut reader = BufReader::new(&self.stream);
        reader.read_until(b'\x1e', &mut buf)?;
        buf.pop();
        
        Ok(buf)
    }

    fn do_write(&mut self, request: RequestType) -> Result<()> {
        let mut message = serde_json::json!({"type": request}).to_string();
        message.push_str("\x1e");
        self.stream.write(message.as_bytes())?;
        Ok(())
    }

    fn write(&mut self, request: RequestType) -> Result<()> {
        self.do_write(request)?;
        Ok(())
    }
    
    fn read(&mut self) -> Result<Response> {
        let now = Instant::now();
        let progress_bar = ProgressBar::new_spinner();
        
        progress_bar.enable_steady_tick(Duration::from_millis(150));
        let buf: Vec<u8> = self.do_read()?;
        progress_bar.finish();

        let response: Response = serde_json::from_slice(&buf)?;
        if response.status == Status::Err {
            return Err(anyhow!(response.error.unwrap_or_default()));
        }
        let bytes_recived: u32 = buf.len() as u32;
        let speed = bytes_recived as f64 / now.elapsed().as_secs_f64();

        println!("\nRead {} in: {:.2?} ({}/s)",
            HumanBytes(bytes_recived as u64),
            now.elapsed(),
            HumanBytes(speed as u64)
        );

        Ok(response)
    }

    pub fn message_host(&mut self, request: RequestType) -> Result<Response> {
        self.write(request)?;
        Ok(self.read()?)
    }

}