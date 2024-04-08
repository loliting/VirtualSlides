// Module for talking with the host

use std::cmp::max;
use std::fs::File;
#[cfg(feature = "legacy-host-bridge")]
use std::io::{BufRead, BufReader, Write};
use std::time::{Duration, Instant};
use anyhow::{anyhow, Result};
#[cfg(not(feature = "legacy-host-bridge"))]
use nix::ioctl_read_bad;
use serde::{Deserialize, Serialize};
use indicatif::{HumanBytes, ProgressBar};
#[cfg(not(feature = "legacy-host-bridge"))]
use std::os::unix::io::AsRawFd; 

#[cfg(feature = "legacy-host-bridge")]
const WRITE_CONSOLE_PATH: &str = "/dev/hvc0";
#[cfg(feature = "legacy-host-bridge")]
const READ_CONSOLE_PATH: &str = "/dev/hvc1";

#[cfg(not(feature = "legacy-host-bridge"))]
const VSOCK_PATH: &str = "/dev/vsock";
#[cfg(not(feature = "legacy-host-bridge"))]
const IOCTL_VM_SOCKETS_GET_LOCAL_CID: usize = 0x7b9;

pub struct HostBridge {
    #[cfg(feature = "legacy-host-bridge")]
    read_console: File,
    #[cfg(feature = "legacy-host-bridge")]
    write_console: File,
    #[cfg(not(feature = "legacy-host-bridge"))]
    cid: u32
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
        let host_bridge: HostBridge;

        #[cfg(feature = "legacy-host-bridge")] {
            let read_console = File::options()
            .read(true)
            .write(false)
            .open(READ_CONSOLE_PATH)?;
    
            let write_console = File::options()
            .read(false)
            .write(true)
            .open(WRITE_CONSOLE_PATH)?;

            host_bridge = HostBridge {
                read_console: read_console,
                write_console: write_console
            };
        }

        #[cfg(not(feature = "legacy-host-bridge"))] {
            host_bridge = HostBridge {
                cid: HostBridge::get_local_cid()?
            };
    
            println!("Our CID: {}", host_bridge.cid);
        }
        
        Ok(host_bridge)
    }

    #[cfg(not(feature = "legacy-host-bridge"))]
    fn get_local_cid() -> Result<u32> {
        let mut cid: u32 = 0;
        let vsock = File::open(VSOCK_PATH)?;

        ioctl_read_bad!(
            get_local_cid,
            IOCTL_VM_SOCKETS_GET_LOCAL_CID,
            u32
        );
        
        unsafe{ get_local_cid(vsock.as_raw_fd(), &mut cid) }?;
        
        Ok(cid)
    }

    #[cfg(not(feature = "legacy-host-bridge"))]
    fn do_write(&mut self, _request: RequestType) -> Result<()> {
        Err(anyhow!("Not implemented"))
    }

    #[cfg(feature = "legacy-host-bridge")]
    fn do_write(&mut self, request: RequestType) -> Result<()>  {
        let mut message = serde_json::json!({"type": request}).to_string();
        message.push_str("\x1e\n");
        
        self.write_console.write(message.as_bytes())?;
        
        Ok(())
    }
    
    fn write(&mut self, request: RequestType) -> Result<()> {
        self.do_write(request)?;
        Ok(())
    }
    
    #[cfg(not(feature = "legacy-host-bridge"))]
    fn do_read(&mut self) -> Result<Vec<u8>> {
        Err(anyhow!("Not implemented"))
    }

    #[cfg(feature = "legacy-host-bridge")]
    fn do_read(&mut self) -> Result<Vec<u8>>  {
        let mut reader = BufReader::new(&self.read_console);
        let mut buf: Vec<u8> = Vec::new();
        reader.read_until(b'\x1e', &mut buf)?;
        buf.pop();
        
        Ok(buf)
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