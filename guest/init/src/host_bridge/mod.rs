// Module for talking with the host

mod install_file;
mod init_script;
mod task;

use std::io::{BufRead, BufReader, Write};
// use std::time::Duration;
use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
// use indicatif::ProgressBar;
use vsock::{get_local_cid, VsockAddr, VsockStream};

pub use install_file::InstallFile;
pub use init_script::InitScript;
pub use task::*;

pub struct HostBridge {
    stream: VsockStream
}

#[derive(Serialize, Deserialize, Debug)]
#[serde(rename_all = "camelCase")]
pub enum RequestType {
    Reboot,
    DownloadTest,
    GetHostname,
    GetInstallFiles,
    GetInitScripts,
    GetTasks,
    GetTermSize,
    FinishSubtask,
}

#[derive(Serialize, Deserialize, PartialEq, Eq, Debug)]
#[serde(rename_all = "camelCase")]
enum Status {
    Ok,
    Err
}

#[derive(Serialize, Deserialize, Debug)]
#[serde(rename_all = "camelCase")]
pub struct Response {
    pub(in self) status: Status,
    pub(in self) error: Option<String>,

    pub hostname: Option<String>,
    pub install_files: Option<Vec<InstallFile>>,
    pub init_scripts: Option<Vec<InitScript>>,
    pub tasks: Option<Vec<Task>>,

    pub term_height: Option<u32>,
    pub term_width: Option<u32>,
}

#[derive(Serialize, Deserialize, Debug)]
#[serde(rename_all = "camelCase")]
pub struct Request {
    #[serde(rename = "type")]
    pub request_type: RequestType,

    pub task_id: Option<String>,
    pub subtask_id: Option<String>,
}

impl HostBridge {
    pub fn new() -> Result<Self> {
        let cid = get_local_cid()?;
        let sock = VsockAddr::new(2, cid);
        let stream = VsockStream::connect(&sock)?;
        Ok(HostBridge { stream })
    }

    fn write(&mut self, request: Request) -> Result<()> {
        let mut message = serde_json::json!(request).to_string();
        message.push_str("\x1e");
        self.stream.write(message.as_bytes())?;
        Ok(())
    }
    
    fn read(&mut self) -> Result<Response> {
        // let progress_bar = ProgressBar::new_spinner();
        
        // progress_bar.enable_steady_tick(Duration::from_millis(150));
        
        let mut buf: Vec<u8> = Vec::new();
        let mut reader = BufReader::new(&self.stream);
        reader.read_until(b'\x1e', &mut buf)?;
        buf.pop();

        // progress_bar.finish_and_clear();

        let response: Response = serde_json::from_slice(&buf)?;
        if response.status == Status::Err {
            return Err(anyhow!(response.error.unwrap_or_default()));
        }
        
        Ok(response)
    }

    pub fn message_host_simple(&mut self, request_type: RequestType) -> Result<Response> {
        let request = Request {
            request_type,
            task_id: None,
            subtask_id: None
        };

        self.write(request)?;
        Ok(self.read()?)
    }

    pub fn message_host(&mut self, request: Request) -> Result<Response> {
        self.write(request)?;
        Ok(self.read()?)
    }

}