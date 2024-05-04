// Module for talking with the host

mod install_file;
mod init_script;

use std::io::{BufRead, BufReader, Write};
use std::time::Duration;
#[cfg(debug_assertions)]
use std::time::Instant;
use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
use indicatif::ProgressBar;
#[cfg(debug_assertions)]
use indicatif::HumanBytes;
use vsock::{get_local_cid, VsockAddr, VsockStream};

pub use install_file::InstallFile;
pub use init_script::InitScript;

pub struct HostBridge {
    stream: VsockStream
}

#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub enum RequestType {
    Reboot,
    DownloadTest,
    GetHostname,
    GetMotd,
    GetInstallFiles,
    GetInitScripts,
}

#[derive(Serialize, Deserialize, PartialEq, Eq)]
enum Status {
    #[serde(rename = "ok")] 
    Ok,
    #[serde(rename = "err")] 
    Err
}

#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Response {
    pub(in self) status: Status,
    pub(in self) error: Option<String>,

    pub hostname: Option<String>,
    pub motd: Option<String>,
    pub install_files: Option<Vec<InstallFile>>,
    pub init_scripts: Option<Vec<InitScript>>,
}

impl HostBridge {
    pub fn new() -> Result<Self> {
        let cid = get_local_cid()?;
        let sock = VsockAddr::new(2, cid);
        let stream = VsockStream::connect(&sock)?;
        Ok(HostBridge { stream })
    }

    fn write(&mut self, request: RequestType) -> Result<()> {
        let mut message = serde_json::json!({"type": request}).to_string();
        message.push_str("\x1e");
        self.stream.write(message.as_bytes())?;
        Ok(())
    }
    
    fn read(&mut self) -> Result<Response> {
        #[cfg(debug_assertions)]
        let now = Instant::now();
        let progress_bar = ProgressBar::new_spinner();
        
        progress_bar.enable_steady_tick(Duration::from_millis(150));
        
        let mut buf: Vec<u8> = Vec::new();
        let mut reader = BufReader::new(&self.stream);
        reader.read_until(b'\x1e', &mut buf)?;
        buf.pop();

        progress_bar.finish_and_clear();

        let response: Response = serde_json::from_slice(&buf)?;
        if response.status == Status::Err {
            return Err(anyhow!(response.error.unwrap_or_default()));
        }
        
        #[cfg(debug_assertions)]
        {
            let bytes_recived: u32 = buf.len() as u32;
            let speed = bytes_recived as f64 / now.elapsed().as_secs_f64();
            
            println!("Read {} in: {:.2?} ({}/s)",
                HumanBytes(bytes_recived as u64),
                now.elapsed(),
                HumanBytes(speed as u64)
            );
        }

        Ok(response)
    }

    pub fn message_host(&mut self, request: RequestType) -> Result<Response> {
        self.write(request)?;
        Ok(self.read()?)
    }

}