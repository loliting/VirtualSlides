use super::RequestType;

use vsock::{get_local_cid, VsockAddr, VsockStream};
use anyhow::Result;
use std::io::{BufRead, BufReader, Write};


pub struct HostBridge {
    stream: VsockStream
}


impl HostBridge {
    pub fn new() -> Result<Self> {
        let cid = get_local_cid()?;
        let sock = VsockAddr::new(2, cid);
        let stream = VsockStream::connect(&sock)?;
        Ok(HostBridge { stream })
    }

    pub(in super) fn do_read(&mut self) -> Result<Vec<u8>> {
        let mut buf: Vec<u8> = Vec::new();

        let mut reader = BufReader::new(&self.stream);
        reader.read_until(b'\x1e', &mut buf)?;
        buf.pop();
        
        Ok(buf)
    }

    pub(in super) fn do_write(&mut self, request: RequestType) -> Result<()> {
        let mut message = serde_json::json!({"type": request}).to_string();
        message.push_str("\x1e");
        self.stream.write(message.as_bytes())?;
        Ok(())
    }
}