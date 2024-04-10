use super::RequestType;

use std::fs::File;
use std::io::{BufRead, BufReader, Write};

use anyhow::Result;

const WRITE_CONSOLE_PATH: &str = "/dev/hvc0";
const READ_CONSOLE_PATH: &str = "/dev/hvc1";


pub struct HostBridge {
    read_console: File,
    write_console: File,
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

        Ok(HostBridge { read_console, write_console })
    }

    pub(in super) fn do_write(&mut self, request: RequestType) -> Result<()>  {
        let mut message = serde_json::json!({"type": request}).to_string();
        message.push_str("\x1e\n");
        
        self.write_console.write(message.as_bytes())?;
        
        Ok(())
    }
    
    pub(in super) fn do_read(&mut self) -> Result<Vec<u8>>  {
        let mut reader = BufReader::new(&self.read_console);
        let mut buf: Vec<u8> = Vec::new();
        reader.read_until(b'\x1e', &mut buf)?;
        buf.pop();
        
        Ok(buf)
    }
}