// Module for talking with the host

#[cfg(not(feature = "legacy-host-bridge"))]
mod host_bridge_imp;
#[cfg(not(feature = "legacy-host-bridge"))]
pub use host_bridge_imp::HostBridge;


#[cfg(feature = "legacy-host-bridge")]
mod legacy_host_bridge_imp;
#[cfg(feature = "legacy-host-bridge")]
pub use legacy_host_bridge_imp::HostBridge;

use std::cmp::max;
use std::time::{Duration, Instant};
use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
use indicatif::{HumanBytes, ProgressBar};

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

        let bytes_recived = buf.len().try_into().unwrap();
        let speed = bytes_recived / max(1, now.elapsed().as_secs());

        println!("\nRead {} in: {:.2?} ({}/s)", HumanBytes(bytes_recived), now.elapsed(), HumanBytes(speed));

        Ok(response)
    }

    pub fn message_host(&mut self, request: RequestType) -> Result<Response> {
        self.write(request)?;
        Ok(self.read()?)
    }

}